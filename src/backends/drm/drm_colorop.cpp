/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_colorop.h"
#include "drm_blob.h"
#include "drm_commit.h"
#include "drm_object.h"

#include <ranges>

namespace KWin
{

DrmAbstractColorOp::DrmAbstractColorOp(DrmAbstractColorOp *next, Features features, const QString &name)
    : m_next(next)
    , m_features(features)
    , m_name(name)
{
}

DrmAbstractColorOp::~DrmAbstractColorOp()
{
}

DrmAbstractColorOp *DrmAbstractColorOp::next() const
{
    return m_next;
}

bool DrmAbstractColorOp::canBypass() const
{
    return m_features & Feature::Bypass;
}

bool DrmAbstractColorOp::supportsMultipleOps() const
{
    return m_features & Feature::MultipleOps;
}

QString DrmAbstractColorOp::name() const
{
    return m_name;
}

bool DrmAbstractColorOp::matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline)
{
    if (m_cachedPipeline && *m_cachedPipeline == pipeline) {
        commit->merge(m_cache.get());
        return true;
    }

    DrmAbstractColorOp *currentOp = this;
    const auto needsLimitedRange = [](const ColorOp &op) {
        // KMS LUTs have an input and output range of [0, 1]
        return std::holds_alternative<ColorTransferFunction>(op.operation)
            || std::holds_alternative<InverseColorTransferFunction>(op.operation);
    };

    // first, only check if the pipeline can be programmed in the first place
    // don't calculate LUTs just yet
    std::optional<ColorOp> initialOp;
    double valueScaling = 1;
    if (!pipeline.ops.empty() && needsLimitedRange(pipeline.ops.front()) && pipeline.ops.front().input.max > 1) {
        valueScaling = 1.0 / pipeline.ops.front().input.max;
        initialOp = ColorOp{
            .input = pipeline.ops.front().input,
            .operation = ColorMultiplier{valueScaling},
            .output = ValueRange{
                .min = pipeline.ops.front().input.min * valueScaling,
                .max = 1.0,
            },
        };
        while (currentOp && !currentOp->canBeUsedFor(*initialOp, false)) {
            currentOp = currentOp->next();
        }
        if (!currentOp) {
            return false;
        }
        currentOp = currentOp->next();
    }
    for (auto it = pipeline.ops.begin(); it != pipeline.ops.end(); it++) {
        while (currentOp && !currentOp->canBeUsedFor(*it, true)) {
            currentOp = currentOp->next();
        }
        if (!currentOp) {
            return false;
        }
    }

    // now actually program the properties
    currentOp = this;
    m_cache = std::make_unique<DrmAtomicCommit>(commit->gpu());
    if (initialOp) {
        while (!currentOp->canBeUsedFor(*initialOp, false)) {
            currentOp->bypass(m_cache.get());
            currentOp = currentOp->next();
        }
        currentOp->program(m_cache.get(), std::span(&*initialOp, 1), 1, 1);
        currentOp = currentOp->next();
    }
    for (auto it = pipeline.ops.begin(); it != pipeline.ops.end();) {
        while (!currentOp->canBeUsedFor(*it, true)) {
            currentOp->bypass(m_cache.get());
            currentOp = currentOp->next();
        }
        auto firstIt = it;
        it++;
        // combine as many operations into one hardware operation as possible
        while (it != pipeline.ops.end() && currentOp->canBeUsedFor(*it, true)) {
            it++;
        }
        std::span operations(firstIt, it);
        double outputScaling = 1.0;
        if (it != pipeline.ops.end() && (needsLimitedRange(operations.front()) || needsLimitedRange(operations.back()) || needsLimitedRange(*it))) {
            // if this or the next operation needs a limited range, or we need limited range for the output, make it happen
            outputScaling = 1.0 / operations.back().output.max;
        }
        currentOp->program(m_cache.get(), operations, valueScaling, outputScaling);
        valueScaling = outputScaling;
        currentOp = currentOp->next();
    }
    while (currentOp) {
        currentOp->bypass(m_cache.get());
        currentOp = currentOp->next();
    }
    commit->merge(m_cache.get());
    m_cachedPipeline = pipeline;
    return true;
}

DrmLutColorOp::DrmLutColorOp(DrmAbstractColorOp *next, DrmProperty *prop, uint32_t maxSize)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | Feature::Bypass, QStringLiteral("1D LUT"))
    , m_prop(prop)
    , m_maxSize(maxSize)
    , m_components(m_maxSize)
{
}

bool DrmLutColorOp::canBeUsedFor(const ColorOp &op, bool scaling)
{
    constexpr double eta = 0.0001;
    // if scaling is true, we can assume the input to be bounded to [0; 1] already
    if ((!scaling && op.input.max > 1 + eta) || op.input.min < -eta) {
        return false;
    }
    if (std::holds_alternative<ColorTransferFunction>(op.operation) || std::holds_alternative<InverseColorTransferFunction>(op.operation)
        || std::holds_alternative<ColorTonemapper>(op.operation) || std::holds_alternative<std::shared_ptr<ColorTransformation>>(op.operation)) {
        // the required resolution depends heavily on the function and on the input and output ranges / multipliers
        // but this is good enough for now
        return m_maxSize >= 1024;
    } else if (std::holds_alternative<ColorMultiplier>(op.operation)) {
        return true;
    }
    return false;
}

void DrmLutColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale)
{
    for (uint32_t i = 0; i < m_maxSize; i++) {
        const double input = i / double(m_maxSize - 1);
        const double scaledInput = input / inputScale;
        QVector3D output(scaledInput, scaledInput, scaledInput);
        for (const auto &op : operations) {
            if (auto tf = std::get_if<ColorTransferFunction>(&op.operation)) {
                output = tf->tf.encodedToNits(output);
            } else if (auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
                output = tf->tf.nitsToEncoded(output);
            } else if (auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
                output *= mult->factors;
            } else if (auto tonemap = std::get_if<ColorTonemapper>(&op.operation)) {
                output.setX(tonemap->map(output.x()));
            } else if (auto lut1d = std::get_if<std::shared_ptr<ColorTransformation>>(&op.operation)) {
                output = (*lut1d)->transform(output);
            } else {
                Q_UNREACHABLE();
            }
        }
        m_components[i] = {
            .red = uint16_t(std::round(std::clamp(output.x() * outputScale, 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
            .green = uint16_t(std::round(std::clamp(output.y() * outputScale, 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
            .blue = uint16_t(std::round(std::clamp(output.z() * outputScale, 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
            .reserved = 0,
        };
    }
    commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), m_components.data(), sizeof(drm_color_lut) * m_maxSize));
}

void DrmLutColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addBlob(*m_prop, nullptr);
}

LegacyMatrixColorOp::LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | Feature::Bypass, QStringLiteral("legacy matrix"))
    , m_prop(prop)
{
}

bool LegacyMatrixColorOp::canBeUsedFor(const ColorOp &op, bool scaling)
{
    // this isn't necessarily true, but let's keep things simple for now
    if (auto matrix = std::get_if<ColorMatrix>(&op.operation)) {
        return std::abs(matrix->mat(3, 0) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(3, 1) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(3, 2) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(3, 3) - 1) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(0, 3) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(1, 3) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(2, 3) - 0) < ColorPipeline::s_maxResolution;
    } else if (std::holds_alternative<ColorMultiplier>(op.operation)) {
        return true;
    }
    return false;
}

static uint64_t doubleToFixed(double value)
{
    // ctm values are in S31.32 sign-magnitude format
    uint64_t ret = std::abs(value) * (1ull << 32);
    if (value < 0) {
        ret |= 1ull << 63;
    }
    return ret;
}

void LegacyMatrixColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale)
{
    // NOTE that matrix operations have to be added in reverse order to get the correct result!
    QMatrix4x4 result;
    result.scale(outputScale);
    for (const auto &op : operations | std::views::reverse) {
        if (auto matrix = std::get_if<ColorMatrix>(&op.operation)) {
            result *= matrix->mat;
        } else if (auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
            result.scale(mult->factors.x(), mult->factors.y(), mult->factors.z());
        } else {
            Q_UNREACHABLE();
        }
    }
    result.scale(1.0 / inputScale);
    drm_color_ctm data = {
        .matrix = {
            doubleToFixed(result(0, 0)), doubleToFixed(result(0, 1)), doubleToFixed(result(0, 2)), //
            doubleToFixed(result(1, 0)), doubleToFixed(result(1, 1)), doubleToFixed(result(1, 2)), //
            doubleToFixed(result(2, 0)), doubleToFixed(result(2, 1)), doubleToFixed(result(2, 2)), //
        },
    };
    commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), &data, sizeof(data)));
}

void LegacyMatrixColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addBlob(*m_prop, nullptr);
}

}
