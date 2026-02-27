/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_colorop.h"
#include "drm_blob.h"
#include "drm_commit.h"
#include "drm_gpu.h"
#include "drm_object.h"
#include "utils/envvar.h"

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

static const auto s_disableAmdgpuWorkaround = environmentVariableBoolValue("KWIN_DRM_DISABLE_AMD_GAMMA_WORKAROUND");

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
    if (pipeline.isIdentity() && s_disableAmdgpuWorkaround.value_or(!commit->gpu()->isAmdgpu())) {
        // Applying this config is very simple and cheap, so do it directly
        // and avoid invalidating the cache
        DrmAbstractColorOp *currentOp = this;
        while (currentOp) {
            currentOp->bypass(commit);
            currentOp = currentOp->next();
        }
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

    std::unordered_map<DrmAbstractColorOp *, std::vector<ColorOp::Operation>> assignments;

    double valueScaling = 1;
    if (!pipeline.ops.empty() && needsLimitedRange(pipeline.ops.front()) && pipeline.ops.front().input.max > 1) {
        valueScaling = 1.0 / pipeline.ops.front().input.max;
        ColorOp initialOp{
            .input = pipeline.ops.front().input,
            .operation = ColorMultiplier{valueScaling},
            .output = ValueRange{
                .min = pipeline.ops.front().input.min * valueScaling,
                .max = 1.0,
            },
        };
        while (currentOp && !currentOp->canBeUsedFor(initialOp, false)) {
            currentOp = currentOp->next();
        }
        if (!currentOp) {
            return false;
        }
        assignments[currentOp].push_back(initialOp.operation);
    }
    auto ops = std::span(pipeline.ops);
    while (!ops.empty()) {
        while (currentOp && !currentOp->canBeUsedFor(ops.front(), true)) {
            currentOp = currentOp->next();
        }
        if (!currentOp) {
            return false;
        }
        auto &hwOps = assignments[currentOp];
        if (valueScaling != 1) {
            // un-do the previous scaling, so we get the original values out again
            hwOps.push_back(ColorMultiplier{1.0 / valueScaling});
        }
        hwOps.push_back(ops.front().operation);
        if (ops.size() > 1 && ops.front().output.max > 1 && (needsLimitedRange(ops[0]) || needsLimitedRange(ops[1]))) {
            // scale values so that LUTs with their [0; 1] range can represent the values
            valueScaling = 1.0 / ops.front().output.max;
            hwOps.push_back(ColorMultiplier{valueScaling});
        } else {
            valueScaling = 1;
        }
        ops = ops.subspan(1);
    }

    // now actually program the properties
    currentOp = this;
    m_cache = std::make_unique<DrmAtomicCommit>(commit->gpu());
    currentOp = this;
    while (currentOp) {
        const auto it = assignments.find(currentOp);
        if (it != assignments.end()) {
            const auto &[op, program] = *it;
            currentOp->program(m_cache.get(), program);
        } else {
            currentOp->bypass(m_cache.get());
        }
        currentOp = currentOp->next();
    }
    commit->merge(m_cache.get());
    m_cachedPipeline = pipeline;
    return true;
}

DrmLutColorOp16::DrmLutColorOp16(DrmAbstractColorOp *next, DrmProperty *prop, DrmEnumProperty<Lut1DInterpolation> *interpolation, uint32_t maxSize, DrmProperty *bypass)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | Feature::Bypass, QStringLiteral("1D LUT"))
    , m_prop(prop)
    , m_bypass(bypass)
    , m_interpolationMode(interpolation)
    , m_maxSize(maxSize)
    , m_components(m_maxSize)
{
}

bool DrmLutColorOp16::canBeUsedFor(const ColorOp &op, bool normalizedInput)
{
    constexpr double eta = 0.0001;
    // if normalizedInput is true, we can assume the input to be bounded to [0; 1] already
    if ((!normalizedInput && op.input.max > 1 + eta) || op.input.min < -eta) {
        return false;
    }
    if (std::holds_alternative<ColorTransferFunction>(op.operation) || std::holds_alternative<InverseColorTransferFunction>(op.operation)
        || std::holds_alternative<ColorTonemapper>(op.operation) || std::holds_alternative<std::shared_ptr<ColorTransformation>>(op.operation)) {
        // the required resolution depends heavily on the function and on the input and output ranges / multipliers
        // but this is good enough for now
        return m_maxSize >= 256;
    }
    return std::holds_alternative<ColorMultiplier>(op.operation)
        || std::holds_alternative<ColorClamp>(op.operation);
}

void DrmLutColorOp16::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    for (uint32_t i = 0; i < m_maxSize; i++) {
        const double input = i / double(m_maxSize - 1);
        QVector3D output(input, input, input);
        for (const auto &op : operations) {
            output = ColorOp::applyOperation(op, output);
        }
        m_components[i] = {
            .red = uint16_t(std::round(std::clamp(output.x(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max())),
            .green = uint16_t(std::round(std::clamp(output.y(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max())),
            .blue = uint16_t(std::round(std::clamp(output.z(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max())),
            .reserved = 0,
        };
    }
    commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), m_components.data(), sizeof(drm_color_lut) * m_maxSize));
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void DrmLutColorOp16::bypass(DrmAtomicCommit *commit)
{
    commit->addBlob(*m_prop, nullptr);
    if (m_bypass) {
        commit->addProperty(*m_bypass, 1);
    }
}

DrmLutColorOp32::DrmLutColorOp32(DrmAbstractColorOp *next, DrmProperty *prop, DrmEnumProperty<Lut1DInterpolation> *interpolation, uint32_t maxSize, DrmProperty *bypass)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | Feature::Bypass, QStringLiteral("1D LUT"))
    , m_prop(prop)
    , m_bypass(bypass)
    , m_interpolationMode(interpolation)
    , m_maxSize(maxSize)
    , m_components(m_maxSize)
{
}

bool DrmLutColorOp32::canBeUsedFor(const ColorOp &op, bool normalizedInput)
{
    constexpr double eta = 0.0001;
    // if normalizedInput is true, we can assume the input to be bounded to [0; 1] already
    if ((!normalizedInput && op.input.max > 1 + eta) || op.input.min < -eta) {
        return false;
    }
    if (std::holds_alternative<ColorTransferFunction>(op.operation) || std::holds_alternative<InverseColorTransferFunction>(op.operation)
        || std::holds_alternative<ColorTonemapper>(op.operation) || std::holds_alternative<std::shared_ptr<ColorTransformation>>(op.operation)) {
        // the required resolution depends heavily on the function and on the input and output ranges / multipliers
        // but this is good enough for now
        return m_maxSize >= 256;
    }
    return std::holds_alternative<ColorMultiplier>(op.operation)
        || std::holds_alternative<ColorClamp>(op.operation);
}

void DrmLutColorOp32::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    for (uint32_t i = 0; i < m_maxSize; i++) {
        const double input = i / double(m_maxSize - 1);
        QVector3D output(input, input, input);
        for (const auto &op : operations) {
            output = ColorOp::applyOperation(op, output);
        }
        m_components[i] = {
            .red = uint32_t(std::round(std::clamp<double>(output.x(), 0.0, 1.0) * std::numeric_limits<uint32_t>::max())),
            .green = uint32_t(std::round(std::clamp<double>(output.y(), 0.0, 1.0) * std::numeric_limits<uint32_t>::max())),
            .blue = uint32_t(std::round(std::clamp<double>(output.z(), 0.0, 1.0) * std::numeric_limits<uint32_t>::max())),
            .reserved = 0,
        };
    }
    commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), m_components.data(), sizeof(LutComponent32) * m_maxSize));
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void DrmLutColorOp32::bypass(DrmAtomicCommit *commit)
{
    commit->addBlob(*m_prop, nullptr);
    if (m_bypass) {
        commit->addProperty(*m_bypass, 1);
    }
}

LegacyMatrixColorOp::LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop)
    : DrmAbstractColorOp(next, Features{Feature::Bypass} | Feature::MultipleOps, QStringLiteral("legacy matrix"))
    , m_prop(prop)
{
}

bool LegacyMatrixColorOp::canBeUsedFor(const ColorOp &op, bool normalizedInput)
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

void LegacyMatrixColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    // NOTE that matrix operations have to be added in reverse order to get the correct result!
    QMatrix4x4 result;
    for (const auto &op : operations | std::views::reverse) {
        if (auto matrix = std::get_if<ColorMatrix>(&op)) {
            result *= matrix->mat;
        } else if (auto mult = std::get_if<ColorMultiplier>(&op)) {
            result.scale(mult->factors.x(), mult->factors.y(), mult->factors.z());
        } else {
            Q_UNREACHABLE();
        }
    }
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

Matrix3x4ColorOp::Matrix3x4ColorOp(DrmAbstractColorOp *next, DrmProperty *prop, DrmProperty *bypass)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | (bypass ? Feature::Bypass : Features{}), QStringLiteral("3x4 matrix"))
    , m_prop(prop)
    , m_bypass(bypass)
{
}

bool Matrix3x4ColorOp::canBeUsedFor(const ColorOp &op, bool normalizedInput)
{
    // TODO check the resolution of the matrix too?
    // -> values above abs(5) should be re-scaled with a different (previous?) operation
    if (auto matrix = std::get_if<ColorMatrix>(&op.operation)) {
        return matrix->mat(3, 0) == 0
            && matrix->mat(3, 1) == 0
            && matrix->mat(3, 2) == 0
            && matrix->mat(3, 3) == 1;
    } else if (std::holds_alternative<ColorMultiplier>(op.operation)) {
        return true;
    }
    return false;
}

void Matrix3x4ColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    // NOTE that matrix operations have to be added in reverse order to get the correct result!
    QMatrix4x4 result;
    for (const auto &op : operations | std::views::reverse) {
        if (auto matrix = std::get_if<ColorMatrix>(&op)) {
            result *= matrix->mat;
        } else if (auto mult = std::get_if<ColorMultiplier>(&op)) {
            result.scale(mult->factors.x(), mult->factors.y(), mult->factors.z());
        } else {
            Q_UNREACHABLE();
        }
    }
    std::array<uint64_t, 12> data = {
        doubleToFixed(result(0, 0)), doubleToFixed(result(0, 1)), doubleToFixed(result(0, 2)), doubleToFixed(result(0, 3)), //
        doubleToFixed(result(1, 0)), doubleToFixed(result(1, 1)), doubleToFixed(result(1, 2)), doubleToFixed(result(1, 3)), //
        doubleToFixed(result(2, 0)), doubleToFixed(result(2, 1)), doubleToFixed(result(2, 2)), doubleToFixed(result(2, 3)), //
    };
    commit->addBlob(*m_prop, DrmBlob::create(m_prop->drmObject()->gpu(), data.data(), sizeof(uint64_t) * data.size()));
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void Matrix3x4ColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

UnknownColorOp::UnknownColorOp(DrmAbstractColorOp *next, DrmProperty *bypass)
    : DrmAbstractColorOp(next, bypass ? Feature::Bypass : Features{}, "unknown")
    , m_bypass(bypass)
{
}

bool UnknownColorOp::canBeUsedFor(const ColorOp &op, bool normalizedInput)
{
    return false;
}

void UnknownColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    Q_UNREACHABLE();
}

void UnknownColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

DrmLut3DColorOp::DrmLut3DColorOp(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *bypass, size_t size, DrmEnumProperty<Lut3DInterpolation> *interpolation)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | (bypass ? Feature::Bypass : Features{}), QStringLiteral("3D LUT"))
    , m_value(value)
    , m_bypass(bypass)
    , m_size(size)
    , m_interpolation(interpolation)
    , m_components(m_size * m_size * m_size)
{
}

void DrmLut3DColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    for (size_t r = 0; r < m_size; r++) {
        for (size_t g = 0; g < m_size; g++) {
            for (size_t b = 0; b < m_size; b++) {
                QVector3D output = QVector3D(r, g, b) / float(m_size - 1);
                for (const auto &op : operations) {
                    output = ColorOp::applyOperation(op, output);
                }
                const size_t index = b * m_size * m_size + g * m_size + r;
                m_components[index] = LutComponent32{
                    .red = uint32_t(std::round(std::clamp<double>(output.x(), 0.0, 1.0) * std::numeric_limits<uint32_t>::max())),
                    .green = uint32_t(std::round(std::clamp<double>(output.y(), 0.0, 1.0) * std::numeric_limits<uint32_t>::max())),
                    .blue = uint32_t(std::round(std::clamp<double>(output.z(), 0.0, 1.0) * std::numeric_limits<uint32_t>::max())),
                    .reserved = 0,
                };
            }
        }
    }
    commit->addBlob(*m_value, DrmBlob::create(m_value->drmObject()->gpu(), m_components.data(), m_components.size() * sizeof(LutComponent32)));
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void DrmLut3DColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

bool DrmLut3DColorOp::canBeUsedFor(const ColorOp &op, bool normalizedInput)
{
    constexpr double eta = 0.0001;
    // if normalizedInput is true, we can assume the input to be bounded to [0; 1] already
    if ((!normalizedInput && op.input.max > 1 + eta) || op.input.min < -eta) {
        return false;
    }
    // restricted to simple multipliers and clamps for now,
    // as everything else requires more effort to be yield good results
    return std::holds_alternative<ColorMultiplier>(op.operation)
        || std::holds_alternative<ColorClamp>(op.operation);
}

DrmMultiplier::DrmMultiplier(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *bypass)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | (bypass ? Feature::Bypass : Features{}), QStringLiteral("multiplier"))
    , m_value(value)
    , m_bypass(bypass)
{
}

static float commonScaling(const QMatrix4x4 &mat)
{
    return std::min({mat(0, 0), mat(1, 1), mat(2, 2)});
}

bool DrmMultiplier::canBeUsedFor(const ColorOp &op, bool normalizedInput)
{
    if (const auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
        const float diff1 = std::abs(mult->factors.x() - mult->factors.y());
        const float diff2 = std::abs(mult->factors.y() - mult->factors.z());
        const float diff3 = std::abs(mult->factors.z() - mult->factors.x());
        const float maxDiff = std::max({diff1, diff2, diff3});
        return maxDiff < 0.00001;
    }
    return false;
}

void DrmMultiplier::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    double factor = 1;
    for (const auto &op : operations) {
        if (const auto mult = std::get_if<ColorMultiplier>(&op)) {
            factor *= mult->factors.x();
        } else if (const auto mat = std::get_if<ColorMatrix>(&op)) {
            factor *= commonScaling(mat->mat);
        }
    }
    commit->addProperty(*m_value, doubleToFixed(factor));
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void DrmMultiplier::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

DrmColorOp::DrmColorOp(DrmGpu *gpu, uint32_t objectId)
    : DrmObject(gpu, objectId, DRM_MODE_OBJECT_ANY)
    , m_next(this, QByteArrayLiteral("NEXT"))
    , m_type(this, QByteArrayLiteral("TYPE"), {
                                                  QByteArrayLiteral("1D LUT"),
                                                  QByteArrayLiteral("3x4 Matrix"),
                                                  QByteArrayLiteral("3D LUT"),
                                                  QByteArrayLiteral("Multiplier"),
                                              })
    , m_data(this, QByteArrayLiteral("DATA"))
    , m_size(this, QByteArrayLiteral("SIZE"))
    , m_bypass(this, QByteArrayLiteral("BYPASS"))
    , m_multiplier(this, QByteArrayLiteral("MULTIPLIER"))
    , m_lut1dInterpolation(this, QByteArrayLiteral("LUT1D_INTERPOLATION"), {QByteArrayLiteral("Linear")})
    , m_lut3dInterpolation(this, QByteArrayLiteral("LUT3D_INTERPOLATION"), {QByteArrayLiteral("Tetrahedal")})
{
}

bool DrmColorOp::init()
{
    return updateProperties();
}

DrmAbstractColorOp *DrmColorOp::colorOp() const
{
    return m_op.get();
}

void DrmColorOp::disable(DrmAtomicCommit *commit)
{
    Q_UNREACHABLE();
}

bool DrmColorOp::updateProperties()
{
    DrmPropertyList props = queryProperties();
    m_next.update(props);
    m_type.update(props);
    m_data.update(props);
    m_size.update(props);
    m_bypass.update(props);
    m_multiplier.update(props);
    m_lut1dInterpolation.update(props);
    m_lut3dInterpolation.update(props);

    if (!m_type.isValid()) {
        return false;
    }
    if (m_next.value() != 0) {
        if (!m_nextOp) {
            m_nextOp = std::make_unique<DrmColorOp>(gpu(), m_next.value());
            if (!m_nextOp->init()) {
                return false;
            }
        } else if (!m_nextOp->updateProperties()) {
            return false;
        }
    }

    if (m_op) {
        return true;
    }
    const auto next = m_nextOp ? m_nextOp->colorOp() : nullptr;
    const auto bypassProp = m_bypass.isValid() ? &m_bypass : nullptr;
    if (!m_type.hasEnumForValue(m_type.value())) {
        // we don't know this color operation type (yet)
        // but we have a bypass, so it's fine
        m_op = std::make_unique<UnknownColorOp>(next, bypassProp);
        qCDebug(KWIN_DRM, "skipping unknown color op %lu", m_type.value());
        return true;
    }
    switch (m_type.enumValue()) {
    case Type::Lut1D:
        if (!m_size.isValid() || !m_size.value()) {
            qCWarning(KWIN_DRM, "skipping 1D lut with invalid size property");
            m_op = std::make_unique<UnknownColorOp>(next, bypassProp);
            return true;
        }
        m_op = std::make_unique<DrmLutColorOp32>(next, &m_data, &m_lut1dInterpolation, m_size.value(), bypassProp);
        return true;
    case Type::Matrix3x4:
        m_op = std::make_unique<Matrix3x4ColorOp>(next, &m_data, bypassProp);
        return true;
    case Type::Lut3D: {
        if (!m_size.isValid() || !m_size.value()) {
            qCDebug(KWIN_DRM, "skipping 3D lut with invalid mode index, mode blob or exclusively unsupported modes");
            m_op = std::make_unique<UnknownColorOp>(next, bypassProp);
            return true;
        }
        m_op = std::make_unique<DrmLut3DColorOp>(next, &m_data, bypassProp, m_size.value(), &m_lut3dInterpolation);
        return true;
    }
    case Type::Multiplier:
        if (!m_multiplier.isValid()) {
            qCWarning(KWIN_DRM, "Skipping multiplier with invalid multiplier property");
            m_op = std::make_unique<UnknownColorOp>(next, bypassProp);
            return true;
        }
        m_op = std::make_unique<DrmMultiplier>(next, &m_multiplier, bypassProp);
        return true;
    }
    Q_UNREACHABLE();
}
}
