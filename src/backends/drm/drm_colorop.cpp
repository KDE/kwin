/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_colorop.h"
#include "core/colorpipeline.h"
#include "drm_blob.h"
#include "drm_commit.h"
#include "drm_object.h"

#include <ranges>

namespace KWin
{

DrmAbstractColorOp::DrmAbstractColorOp(DrmAbstractColorOp *next, Features features)
    : m_next(next)
    , m_features(features)
{
}

DrmAbstractColorOp::~DrmAbstractColorOp()
{
}

DrmAbstractColorOp *DrmAbstractColorOp::next() const
{
    return m_next;
}

bool DrmAbstractColorOp::needsNonlinearity() const
{
    return m_features & Feature::NonLinear;
}

bool DrmAbstractColorOp::canBypass() const
{
    return m_features & Feature::Bypass;
}

bool DrmAbstractColorOp::supportsMultipleOps() const
{
    return m_features & Feature::MultipleOps;
}

static std::pair<bool, std::optional<ColorOp::Operation>> findColorPipelineAssignments(std::unordered_map<DrmAbstractColorOp *, std::vector<ColorOp::Operation>> &map, DrmAbstractColorOp *lastOp, DrmAbstractColorOp *currentOp, std::span<const ColorOp> ops, std::optional<ColorOp::Operation> inputScale)
{
    if (ops.empty()) {
        return std::make_pair(true, std::nullopt);
    }
    if (!currentOp) {
        return std::make_pair(false, std::nullopt);
    }
    if (!lastOp && ops.front().input.max > 1) {
        // ensure that we don't try to input >1 values into a [0; 1] range LUT
        inputScale = ColorMultiplier{1 / ops.front().input.max};
    }
    if (inputScale) {
        while (!currentOp->colorOpPreference(*inputScale)) {
            currentOp = currentOp->next();
            if (!currentOp) {
                return std::make_pair(false, std::nullopt);
            }
        }
    }
    auto preference = currentOp->colorOpPreference(ops.front().operation);
    while (!preference) {
        currentOp = currentOp->next();
        if (!currentOp) {
            return std::make_pair(false, std::nullopt);
        }
        preference = currentOp->colorOpPreference(ops.front().operation);
        if (!preference && !currentOp->canBypass()) {
            return std::make_pair(false, std::nullopt);
        }
    }
    // if there's a different hw op with a higher preference, try that first
    DrmAbstractColorOp *otherOp = currentOp->next();
    while (otherOp) {
        const auto otherPref = otherOp->colorOpPreference(ops.front().operation);
        if (otherPref && (*otherPref > *preference || !otherOp->canBypass())) {
            const auto [success, scaleOp] = findColorPipelineAssignments(map, currentOp, otherOp, ops, inputScale);
            if (success && (!scaleOp || currentOp->colorOpPreference(*scaleOp))) {
                return std::make_pair(true, scaleOp);
            }
        }
        if (!otherOp->canBypass()) {
            break;
        }
        otherOp = otherOp->next();
    }
    // this is the best assignment for this op, ensure the rest of the pipeline can be assigned too
    // and that all the needed scaling is applied
    std::optional<ColorOp::Operation> nonLinearity;
    std::optional<ColorOp::Operation> invNonLinearity;
    std::optional<ColorOp::Operation> outputScale;
    std::optional<ColorOp::Operation> invOutputScale;
    if (currentOp->needsNonlinearity() || (lastOp && lastOp->needsNonlinearity())) {
        if (currentOp != lastOp) {
            const auto tf = TransferFunction(TransferFunction::PerceptualQuantizer, ops.front().input.min, ops.front().input.max);
            nonLinearity = InverseColorTransferFunction{tf};
            invNonLinearity = ColorTransferFunction(tf);
        }
    } else {
        outputScale = ColorMultiplier(1.0 / ops.front().output.max);
        invOutputScale = ColorMultiplier(ops.front().output.max);
        if (ops.size() == 1) {
            // last color op, our internal scaling needs to be undone
            outputScale.reset();
            invOutputScale.reset();
        }
    }
    const auto nextOp = currentOp->supportsMultipleOps() ? currentOp : currentOp->next();
    const auto [success, scaleOp] = findColorPipelineAssignments(map, currentOp, nextOp, ops.subspan(1), invOutputScale);
    if (!success || (scaleOp && !currentOp->colorOpPreference(*scaleOp))) {
        return std::make_pair(false, std::nullopt);
    }
    auto &program = map[currentOp];
    std::vector<ColorOp::Operation> operations;
    if (invNonLinearity) {
        operations.push_back(*invNonLinearity);
    } else if (inputScale) {
        operations.push_back(*inputScale);
    }
    operations.push_back(ops.front().operation);
    if (scaleOp) {
        operations.push_back(*scaleOp);
    } else if (outputScale) {
        operations.push_back(*outputScale);
    }
    // note that because of the recursion above, operations must be added in the front of the vector
    // as other operations might've already been added after it
    program.insert(program.begin(), operations.begin(), operations.end());
    return std::make_pair(true, nonLinearity);
}

bool DrmAbstractColorOp::matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline)
{
    if (m_cachedPipeline && *m_cachedPipeline == pipeline) {
        commit->merge(m_cache.get());
        return true;
    }
    if (m_cachedPipelineFail && *m_cachedPipelineFail == pipeline) {
        return false;
    }

    // first, search for the most optimal pipeline, without calculating LUTs
    std::unordered_map<DrmAbstractColorOp *, std::vector<ColorOp::Operation>> assignments;
    const auto [success, scaleOp] = findColorPipelineAssignments(assignments, nullptr, this, pipeline.ops, std::nullopt);
    if (!success || scaleOp) {
        m_cachedPipelineFail = pipeline;
        return false;
    }

    // now actually program the properties
    m_cache = std::make_unique<DrmAtomicCommit>(commit->gpu());
    DrmAbstractColorOp *currentOp = this;
    while (currentOp) {
        const auto it = assignments.find(currentOp);
        if (it != assignments.end()) {
            const auto &[op, program] = *it;
            ColorPipeline fmt;
            std::ranges::transform(program, std::back_inserter(fmt.ops), [](const auto op) {
                return ColorOp{
                    .input = {1, 1},
                    .operation = op,
                    .output = {1, 1},
                };
            });
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

DrmLutColorOp::DrmLutColorOp(DrmAbstractColorOp *next, DrmProperty *prop, uint32_t maxSize, DrmProperty *bypass)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | (bypass ? Feature::Bypass : Features{}))
    , m_prop(prop)
    , m_bypass(bypass)
    , m_maxSize(maxSize)
    , m_components(m_maxSize)
{
}

std::optional<uint32_t> DrmLutColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    if (std::holds_alternative<ColorTransferFunction>(op) || std::holds_alternative<InverseColorTransferFunction>(op)
        || std::holds_alternative<ColorTonemapper>(op)) {
        // the required resolution depends heavily on the function and on the input and output ranges / multipliers
        // but this is good enough for now
        if (m_maxSize >= 1024) {
            return 1;
        } else {
            return 0;
        }
    } else if (std::holds_alternative<ColorMultiplier>(op)) {
        return 0;
    }
    return std::nullopt;
}

void DrmLutColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
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

void DrmLutColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addBlob(*m_prop, nullptr);
    if (m_bypass) {
        commit->addProperty(*m_bypass, 1);
    }
}

LegacyMatrixColorOp::LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop)
    : DrmAbstractColorOp(next, Features{Feature::Bypass} | Feature::MultipleOps)
    , m_prop(prop)
{
}

std::optional<uint32_t> LegacyMatrixColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    // TODO check the resolution of the matrix somehow too?
    if (auto matrix = std::get_if<ColorMatrix>(&op)) {
        const bool canRepresent = matrix->mat(3, 0) == 0
            && matrix->mat(3, 1) == 0
            && matrix->mat(3, 2) == 0
            && matrix->mat(3, 3) == 1
            && matrix->mat(0, 3) == 0
            && matrix->mat(1, 3) == 0
            && matrix->mat(2, 3) == 0;
        if (!canRepresent) {
            return std::nullopt;
        } else {
            return 1;
        }
    } else if (std::holds_alternative<ColorMultiplier>(op)) {
        return 0;
    }
    return std::nullopt;
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
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | (bypass ? Feature::Bypass : Features{}))
    , m_prop(prop)
    , m_bypass(bypass)
{
}

std::optional<uint32_t> Matrix3x4ColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    // TODO check the resolution of the matrix somehow too?
    if (auto matrix = std::get_if<ColorMatrix>(&op)) {
        const bool canRepresent = matrix->mat(0, 3) == 0
            && matrix->mat(1, 3) == 0
            && matrix->mat(2, 3) == 0
            && matrix->mat(3, 3) == 1;
        if (!canRepresent) {
            return std::nullopt;
        } else {
            return 1;
        }
    } else if (std::holds_alternative<ColorMultiplier>(op)) {
        return 0;
    }
    return std::nullopt;
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
    : DrmAbstractColorOp(next, bypass ? Feature::Bypass : Features{})
    , m_bypass(bypass)
{
}

std::optional<uint32_t> UnknownColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    return std::nullopt;
}

void UnknownColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    Q_UNREACHABLE();
}

void UnknownColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

DrmLut3DColorOp::DrmLut3DColorOp(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *modeIndex, const QList<drm_mode_3dlut_mode> &modes, DrmProperty *bypass)
    : DrmAbstractColorOp(next, Features{Feature::NonLinear} | Feature::MultipleOps | (bypass ? Feature::Bypass : Features{}))
    , m_value(value)
    , m_bypass(bypass)
    , m_3dLutModeIndex(modeIndex)
    , m_modes(modes)
    , m_components(m_modes.front().lut_stride[0] * m_modes.front().lut_stride[1] * m_modes.front().lut_stride[2])
{
}

std::optional<uint32_t> DrmLut3DColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    // everything can be represented as a 3D lut,
    // but resolution isn't great, so push it to other ops if possible
    return 0;
}

void DrmLut3DColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    // just pick the first mode; smarter mode selection can be added later
    const auto &mode = m_modes.front();
    for (size_t r = 0; r < mode.lut_size; r++) {
        for (size_t g = 0; g < mode.lut_size; g++) {
            for (size_t b = 0; b < mode.lut_size; b++) {
                QVector3D output = QVector3D(r, g, b) / (mode.lut_size - 1);
                for (const auto &op : operations) {
                    output = ColorOp::applyOperation(op, output);
                }
                size_t index;
                if (mode.traversal_order == DRM_COLOROP_LUT3D_TRAVERSAL_RGB) {
                    index = b * mode.lut_stride[0] * mode.lut_stride[1] + g * mode.lut_stride[1] + r;
                } else if (mode.traversal_order == DRM_COLOROP_LUT3D_TRAVERSAL_BGR) {
                    index = r * mode.lut_stride[0] * mode.lut_stride[1] + g * mode.lut_stride[1] + b;
                } else {
                    Q_UNREACHABLE();
                }
                m_components[index] = drm_color_lut{
                    .red = uint16_t(std::round(std::clamp(output.x(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max())),
                    .green = uint16_t(std::round(std::clamp(output.y(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max())),
                    .blue = uint16_t(std::round(std::clamp(output.z(), 0.0f, 1.0f) * std::numeric_limits<uint16_t>::max())),
                    .reserved = 0,
                };
            }
        }
    }
    commit->addBlob(*m_value, DrmBlob::create(m_value->drmObject()->gpu(), m_components.data(), m_components.size() * sizeof(drm_color_lut)));
    commit->addProperty(*m_3dLutModeIndex, 0);
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void DrmLut3DColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

DrmMultiplier::DrmMultiplier(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *bypass)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | (bypass ? Feature::Bypass : Features{}))
    , m_value(value)
    , m_bypass(bypass)
{
}

std::optional<uint32_t> DrmMultiplier::colorOpPreference(const ColorOp::Operation &op)
{
    if (const auto mult = std::get_if<ColorMultiplier>(&op)) {
        const float diff1 = std::abs(mult->factors.x() - mult->factors.y());
        const float diff2 = std::abs(mult->factors.y() - mult->factors.z());
        const float diff3 = std::abs(mult->factors.z() - mult->factors.x());
        const float maxDiff = std::max({diff1, diff2, diff3});
        if (maxDiff < 0.00001) {
            return 1;
        }
    }
    return std::nullopt;
}

void DrmMultiplier::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    double factor = 1;
    for (const auto &op : operations) {
        factor *= std::get<ColorMultiplier>(op).factors.x();
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

DrmNamed1DLut::DrmNamed1DLut(DrmAbstractColorOp *next, DrmEnumProperty<Named1DLutType> *value, DrmProperty *bypass)
    : DrmAbstractColorOp(next, bypass ? Feature::Bypass : Features{})
    , m_value(value)
    , m_bypass(bypass)
{
}

std::optional<Named1DLutType> DrmNamed1DLut::getType(const ColorOp::Operation &op) const
{
    // TODO support PQ 125 EOTF and PQ 125 Inverse EOTF -> need to support >1 scaling
    if (const auto tf = std::get_if<ColorTransferFunction>(&op)) {
        switch (tf->tf.type) {
        case TransferFunction::sRGB:
            return Named1DLutType::sRGB_EOTF;
        default:
            return std::nullopt;
        }
    } else if (const auto invTf = std::get_if<InverseColorTransferFunction>(&op)) {
        switch (invTf->tf.type) {
        case TransferFunction::sRGB:
            return Named1DLutType::sRGB_Inverse_EOTF;
        default:
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}

std::optional<uint32_t> DrmNamed1DLut::colorOpPreference(const ColorOp::Operation &op)
{
    const auto namedLut = getType(op);
    if (!namedLut) {
        return std::nullopt;
    }
    if (m_value->hasEnum(*namedLut)) {
        return 2;
    } else {
        return std::nullopt;
    }
}

void DrmNamed1DLut::program(DrmAtomicCommit *commit, std::span<const ColorOp::Operation> operations)
{
    commit->addEnum(*m_value, *getType(operations.front()));
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void DrmNamed1DLut::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

DrmColorOp::DrmColorOp(DrmGpu *gpu, uint32_t objectId)
    : DrmObject(gpu, objectId, DRM_MODE_OBJECT_ANY)
    , m_next(this, "NEXT")
    , m_type(this, "TYPE", {
                               "1D Curve Custom LUT",
                               "3x4 Matrix",
                               "3D LUT",
                               "Multiplier",
                               "1D Curve",
                           })
    , m_data(this, "DATA")
    , m_1dlutSize(this, "SIZE")
    , m_bypass(this, "BYPASS")
    , m_3dLutModeIndex(this, "3DLUT_MODE_INDEX")
    , m_3dLutModesBlob(this, "3DLUT_MODES")
    , m_1dNamedLutType(this, "CURVE_1D_TYPE", {"sRGB EOTF", "sRGB Inverse EOTF"})
{
}

DrmAbstractColorOp *DrmColorOp::colorOp() const
{
    return m_op.get();
}

void DrmColorOp::disable(DrmAtomicCommit *commit)
{
    if (m_bypass.isValid()) {
        commit->addProperty(m_bypass, 1);
    }
}

bool DrmColorOp::updateProperties()
{
    DrmPropertyList props = queryProperties();
    m_next.update(props);
    m_type.update(props);
    m_data.update(props);
    m_1dlutSize.update(props);
    m_bypass.update(props);
    m_3dLutModeIndex.update(props);
    m_3dLutModesBlob.update(props);
    m_1dNamedLutType.update(props);

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
        if (!m_1dlutSize.isValid() || !m_1dlutSize.value()) {
            qCWarning(KWIN_DRM, "skipping 1D lut with invalid size property");
            m_op = std::make_unique<UnknownColorOp>(next, bypassProp);
            return true;
        }
        m_op = std::make_unique<DrmLutColorOp>(next, &m_data, m_1dlutSize.value(), bypassProp);
        return true;
    case Type::Matrix3x4:
        m_op = std::make_unique<Matrix3x4ColorOp>(next, &m_data, bypassProp);
        return true;
    case Type::Lut3D: {
        QList<drm_mode_3dlut_mode> modes;
        if (auto blob = m_3dLutModesBlob.immutableBlob()) {
            std::span blobContents(reinterpret_cast<drm_mode_3dlut_mode *>(blob->data), blob->length / sizeof(drm_mode_3dlut_mode));
            modes.reserve(blobContents.size());
            std::ranges::copy_if(blobContents, std::back_inserter(modes), [](const drm_mode_3dlut_mode &mode) {
                // TODO is the color format relevant in any way?
                return mode.lut_size >= 17
                    && mode.lut_size < 50
                    && (mode.traversal_order == DRM_COLOROP_LUT3D_TRAVERSAL_RGB || mode.traversal_order == DRM_COLOROP_LUT3D_TRAVERSAL_BGR)
                    && mode.color_depth >= 10;
            });
        }
        if (!m_3dLutModeIndex.isValid() || modes.empty()) {
            qCDebug(KWIN_DRM, "skipping 3D lut with invalid mode index, mode blob or exclusively unsupported modes");
            m_op = std::make_unique<UnknownColorOp>(next, bypassProp);
            return true;
        }
        m_op = std::make_unique<DrmLut3DColorOp>(next, &m_data, &m_3dLutModeIndex, modes, bypassProp);
        return true;
    }
    case Type::Multiplier:
        m_op = std::make_unique<DrmMultiplier>(next, &m_data, bypassProp);
        return true;
    case Type::NamedLut1D:
        if (!m_1dNamedLutType.isValid()) {
            return false;
        }
        m_op = std::make_unique<DrmNamed1DLut>(next, &m_1dNamedLutType, bypassProp);
        return true;
    }
    Q_UNREACHABLE();
}
}
