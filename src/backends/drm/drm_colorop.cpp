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

namespace KWin
{

DrmAbstractColorOp::DrmAbstractColorOp(DrmAbstractColorOp *next)
    : m_next(next)
{
}

DrmAbstractColorOp::~DrmAbstractColorOp()
{
}

DrmAbstractColorOp *DrmAbstractColorOp::next() const
{
    return m_next;
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
        while (currentOp && !currentOp->canBeUsedFor(*initialOp)) {
            currentOp = currentOp->next();
        }
        if (!currentOp) {
            return false;
        }
        currentOp = currentOp->next();
    }
    for (auto it = pipeline.ops.begin(); it != pipeline.ops.end(); it++) {
        while (currentOp && !currentOp->canBeUsedFor(*it)) {
            currentOp = currentOp->next();
        }
        if (!currentOp) {
            return false;
        }
        currentOp = currentOp->next();
    }

    // now actually program the properties
    currentOp = this;
    m_cache = std::make_unique<DrmAtomicCommit>(commit->gpu());
    if (initialOp) {
        while (!currentOp->canBeUsedFor(*initialOp)) {
            currentOp->bypass(m_cache.get());
            currentOp = currentOp->next();
        }
        currentOp->program(m_cache.get(), *initialOp, 1, 1);
        currentOp = currentOp->next();
    }
    for (auto it = pipeline.ops.begin(); it != pipeline.ops.end(); it++) {
        while (!currentOp->canBeUsedFor(*it)) {
            currentOp->bypass(m_cache.get());
            currentOp = currentOp->next();
        }
        if (it == pipeline.ops.end() - 1) {
            // this is the last op, we need to un-do the factor
            // this assumes that the output is always limited range
            currentOp->program(m_cache.get(), *it, valueScaling, 1.0);
            valueScaling = 1.0;
        } else if (needsLimitedRange(*it) || needsLimitedRange(*(it + 1))) {
            // this op can only output limited range or the next op needs a limited range input,
            // adjust the factor to make it happen
            currentOp->program(m_cache.get(), *it, valueScaling, 1.0 / it->output.max);
            valueScaling = 1.0 / it->output.max;
        } else {
            // this and the next op are both fine with extended range, set the factor to 1.0 to use all the resolution we can get
            currentOp->program(m_cache.get(), *it, valueScaling, 1.0);
            valueScaling = 1.0;
        }
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

LegacyLutColorOp::LegacyLutColorOp(DrmAbstractColorOp *next, DrmProperty *prop, uint32_t maxSize)
    : DrmAbstractColorOp(next)
    , m_prop(prop)
    , m_maxSize(maxSize)
    , m_components(m_maxSize)
{
}

bool LegacyLutColorOp::canBeUsedFor(const ColorOp &op)
{
    if (std::holds_alternative<ColorTransferFunction>(op.operation) || std::holds_alternative<InverseColorTransferFunction>(op.operation)) {
        // the required resolution depends heavily on the function and on the input and output ranges / multipliers
        // but this is good enough for now
        return m_maxSize >= 1024;
    } else if (std::holds_alternative<ColorMultiplier>(op.operation)) {
        return true;
    }
    return false;
}

void LegacyLutColorOp::program(DrmAtomicCommit *commit, const ColorOp &op, double inputScale, double outputScale)
{
    if (auto tf = std::get_if<ColorTransferFunction>(&op.operation)) {
        for (uint32_t i = 0; i < m_maxSize; i++) {
            const double nits = tf->tf.encodedToNits(i / double(m_maxSize - 1) / inputScale, tf->referenceLuminance);
            const uint16_t output = std::round(std::clamp(nits * outputScale, 0.0, 1.0) * std::numeric_limits<uint16_t>::max());
            m_components[i] = {
                .red = output,
                .green = output,
                .blue = output,
                .reserved = 0,
            };
        }
        commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), m_components.data(), sizeof(drm_color_lut) * m_components.size()));
    } else if (auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
        for (uint32_t i = 0; i < m_maxSize; i++) {
            const double nits = tf->tf.nitsToEncoded(i / double(m_maxSize - 1) / inputScale, tf->referenceLuminance);
            const uint16_t output = std::round(std::clamp(nits * outputScale, 0.0, 1.0) * std::numeric_limits<uint16_t>::max());
            m_components[i] = {
                .red = output,
                .green = output,
                .blue = output,
                .reserved = 0,
            };
        }
        commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), m_components.data(), sizeof(drm_color_lut) * m_components.size()));
    } else if (auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
        for (uint32_t i = 0; i < m_maxSize; i++) {
            m_components[i] = {
                .red = uint16_t(std::round(std::clamp(mult->factors.x() * outputScale / inputScale * i / double(m_maxSize - 1), 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
                .green = uint16_t(std::round(std::clamp(mult->factors.y() * outputScale / inputScale * i / double(m_maxSize - 1), 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
                .blue = uint16_t(std::round(std::clamp(mult->factors.z() * outputScale / inputScale * i / double(m_maxSize - 1), 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
                .reserved = 0,
            };
        }
        commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), m_components.data(), sizeof(drm_color_lut) * m_maxSize));
    } else {
        Q_ASSERT(false);
    }
}

void LegacyLutColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addBlob(*m_prop, nullptr);
}

LegacyMatrixColorOp::LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop)
    : DrmAbstractColorOp(next)
    , m_prop(prop)
{
}

bool LegacyMatrixColorOp::canBeUsedFor(const ColorOp &op)
{
    // this isn't necessarily true, but let's keep things simple for now
    if (auto matrix = std::get_if<ColorMatrix>(&op.operation)) {
        return matrix->mat(3, 0) == 0
            && matrix->mat(3, 1) == 0
            && matrix->mat(3, 2) == 0
            && matrix->mat(3, 3) == 1
            && matrix->mat(0, 3) == 0
            && matrix->mat(1, 3) == 0
            && matrix->mat(2, 3) == 0;
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

void LegacyMatrixColorOp::program(DrmAtomicCommit *commit, const ColorOp &op, double inputScale, double outputScale)
{
    if (auto matrix = std::get_if<ColorMatrix>(&op.operation)) {
        QMatrix4x4 scaled = matrix->mat;
        scaled.scale(outputScale / inputScale);
        drm_color_ctm data = {
            .matrix = {
                doubleToFixed(scaled(0, 0)), doubleToFixed(scaled(0, 1)), doubleToFixed(scaled(0, 2)), //
                doubleToFixed(scaled(1, 0)), doubleToFixed(scaled(1, 1)), doubleToFixed(scaled(1, 2)), //
                doubleToFixed(scaled(2, 0)), doubleToFixed(scaled(2, 1)), doubleToFixed(scaled(2, 2)), //
            },
        };
        commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), &data, sizeof(data)));
    } else if (auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
        QVector3D scaled = mult->factors;
        scaled *= outputScale / inputScale;
        drm_color_ctm data = {
            .matrix = {
                doubleToFixed(scaled.x()), doubleToFixed(0), doubleToFixed(0), //
                doubleToFixed(0), doubleToFixed(scaled.y()), doubleToFixed(0), //
                doubleToFixed(0), doubleToFixed(0), doubleToFixed(scaled.z()), //
            },
        };
        commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), &data, sizeof(data)));
    } else {
        Q_ASSERT(false);
    }
}

void LegacyMatrixColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addBlob(*m_prop, nullptr);
}

}
