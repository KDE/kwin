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

struct Program
{
    double inputScale;
    std::vector<ColorOp> ops;
    double outputScale;
};

static bool findColorPipelineAssignments(std::unordered_map<DrmAbstractColorOp *, Program> &map, DrmAbstractColorOp *currentOp, std::span<const ColorOp> ops, double inputScale)
{
    if (ops.empty()) {
        return true;
    }
    if (!currentOp) {
        return false;
    }
    auto preference = currentOp->colorOpPreference(ops.front());
    while (!preference) {
        currentOp = currentOp->next();
        if (!currentOp) {
            return false;
        }
        preference = currentOp->colorOpPreference(ops.front());
    }
    // if there's a different hw op with a higher preference, try that first
    DrmAbstractColorOp *otherOp = currentOp->next();
    while (otherOp) {
        const auto otherPref = otherOp->colorOpPreference(ops.front());
        if (otherPref && *otherPref > *preference) {
            if (findColorPipelineAssignments(map, otherOp, ops, inputScale)) {
                return true;
            }
        }
        otherOp = otherOp->next();
    }
    // this is the best assignment for this op, ensure the rest of the pipeline can be assigned too
    double outputScale = 1.0 / ops.front().output.max;
    if (ops.size() == 1) {
        // last color op, our internal scaling needs to be undone
        outputScale = 1.0;
    }
    if (!findColorPipelineAssignments(map, currentOp, ops.subspan(1), outputScale)) {
        return false;
    }
    const auto it = map.find(currentOp);
    if (it == map.end()) {
        map.emplace(currentOp, Program{
                                   .inputScale = inputScale,
                                   .ops = {ops.front()},
                                   .outputScale = outputScale,
                               });
    } else {
        // note that because of the recursion above, operations must be added in the front of the vector
        auto &program = it->second;
        program.ops.insert(program.ops.begin(), ops.front());
        program.inputScale = inputScale;
    }
    return true;
}

bool DrmAbstractColorOp::matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline)
{
    if (m_cachedPipeline && *m_cachedPipeline == pipeline) {
        commit->merge(m_cache.get());
        return true;
    }

    std::unordered_map<DrmAbstractColorOp *, Program> assignments;
    // first, search for the most optimal pipeline
    // don't calculate LUTs just yet

    DrmAbstractColorOp *currentOp = this;
    double inputScale = 1.0;
    if (!pipeline.ops.empty() && pipeline.ops.front().input.max > 1) {
        ColorOp initial{
            .input = pipeline.ops.front().input,
            .operation = ColorMultiplier{inputScale},
            .output = ValueRange{
                .min = pipeline.ops.front().input.min * inputScale,
                .max = 1.0,
            },
        };
        while (currentOp && !currentOp->colorOpPreference(initial)) {
            currentOp = currentOp->next();
        }
        if (!currentOp) {
            return false;
        }
        assignments[currentOp] = Program{
            .inputScale = 1,
            .ops = {initial},
            .outputScale = inputScale,
        };
    }

    if (!findColorPipelineAssignments(assignments, currentOp, pipeline.ops, inputScale)) {
        // TODO now that searching for the pipeline is a bit more expensive, maybe cache this result too?
        return false;
    }

    // now actually program the properties
    m_cache = std::make_unique<DrmAtomicCommit>(commit->gpu());
    currentOp = this;
    while (currentOp) {
        const auto it = assignments.find(currentOp);
        if (it != assignments.end()) {
            const auto &[op, program] = *it;
            currentOp->program(m_cache.get(), program.ops, program.inputScale, program.outputScale);
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
    : DrmAbstractColorOp(next)
    , m_prop(prop)
    , m_bypass(bypass)
    , m_maxSize(maxSize)
    , m_components(m_maxSize)
{
}

std::optional<uint32_t> DrmLutColorOp::colorOpPreference(const ColorOp &op)
{
    if (std::holds_alternative<ColorTransferFunction>(op.operation) || std::holds_alternative<InverseColorTransferFunction>(op.operation)
        || std::holds_alternative<ColorTonemapper>(op.operation)) {
        // the required resolution depends heavily on the function and on the input and output ranges / multipliers
        // but this is good enough for now
        if (m_maxSize >= 1024) {
            return 1;
        } else {
            return 0;
        }
    } else if (std::holds_alternative<ColorMultiplier>(op.operation)) {
        return 0;
    }
    return std::nullopt;
}

void DrmLutColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale)
{
    for (uint32_t i = 0; i < m_maxSize; i++) {
        const double input = i / double(m_maxSize - 1);
        const double scaledInput = input / inputScale;
        QVector3D output(scaledInput, scaledInput, scaledInput);
        for (const auto &op : operations) {
            output = op.apply(output);
        }
        m_components[i] = {
            .red = uint16_t(std::round(std::clamp(output.x() * outputScale, 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
            .green = uint16_t(std::round(std::clamp(output.y() * outputScale, 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
            .blue = uint16_t(std::round(std::clamp(output.z() * outputScale, 0.0, 1.0) * std::numeric_limits<uint16_t>::max())),
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
    : DrmAbstractColorOp(next)
    , m_prop(prop)
{
}

std::optional<uint32_t> LegacyMatrixColorOp::colorOpPreference(const ColorOp &op)
{
    // TODO check the resolution of the matrix somehow too?
    if (auto matrix = std::get_if<ColorMatrix>(&op.operation)) {
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
    } else if (std::holds_alternative<ColorMultiplier>(op.operation)) {
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

Matrix3x4ColorOp::Matrix3x4ColorOp(DrmAbstractColorOp *next, DrmProperty *prop, DrmProperty *bypass)
    : DrmAbstractColorOp(next)
    , m_prop(prop)
    , m_bypass(bypass)
{
}

std::optional<uint32_t> Matrix3x4ColorOp::colorOpPreference(const ColorOp &op)
{
    // TODO check the resolution of the matrix somehow too?
    if (auto matrix = std::get_if<ColorMatrix>(&op.operation)) {
        const bool canRepresent = matrix->mat(0, 3) == 0
            && matrix->mat(1, 3) == 0
            && matrix->mat(2, 3) == 0
            && matrix->mat(3, 3) == 1;
        if (!canRepresent) {
            return std::nullopt;
        } else {
            return 1;
        }
    } else if (std::holds_alternative<ColorMultiplier>(op.operation)) {
        return 0;
    }
    return std::nullopt;
}

void Matrix3x4ColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale)
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
    commit->addProperty(*m_bypass, 0);
}

void Matrix3x4ColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

UnknownColorOp::UnknownColorOp(DrmAbstractColorOp *next, DrmProperty *bypass)
    : DrmAbstractColorOp(next)
    , m_bypass(bypass)
{
}

std::optional<uint32_t> UnknownColorOp::colorOpPreference(const ColorOp &op)
{
    return std::nullopt;
}

void UnknownColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale)
{
    Q_UNREACHABLE();
}

void UnknownColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

DrmLut3DColorOp::DrmLut3DColorOp(DrmAbstractColorOp *next, DrmProperty *value, DrmProperty *modeIndex, const QList<drm_mode_3dlut_mode> &modes, DrmProperty *bypass)
    : DrmAbstractColorOp(next)
    , m_value(value)
    , m_bypass(bypass)
    , m_3dLutModeIndex(modeIndex)
    , m_modes(modes)
    , m_components(m_modes.front().lut_stride[0] * m_modes.front().lut_stride[1] * m_modes.front().lut_stride[2])
{
}

std::optional<uint32_t> DrmLut3DColorOp::colorOpPreference(const ColorOp &op)
{
    // everything can be represented as a 3D lut,
    // but resolution isn't great, so push it to other ops if possible
    return 0;
}

void DrmLut3DColorOp::program(DrmAtomicCommit *commit, std::span<const ColorOp> operations, double inputScale, double outputScale)
{
    // just pick the first mode; smarter mode selection can be added later
    const auto &mode = m_modes.front();
    for (size_t r = 0; r < mode.lut_size; r++) {
        for (size_t g = 0; g < mode.lut_size; g++) {
            for (size_t b = 0; b < mode.lut_size; b++) {
                QVector3D output = QVector3D(r, g, b) / ((mode.lut_size - 1) * inputScale);
                for (const auto &op : operations) {
                    output = op.apply(output);
                }
                output *= outputScale;
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
    commit->addProperty(*m_bypass, 0);
}

void DrmLut3DColorOp::bypass(DrmAtomicCommit *commit)
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
                           })
    , m_data(this, "DATA")
    , m_1dlutSize(this, "SIZE")
    , m_bypass(this, "BYPASS")
    , m_3dLutModeIndex(this, "3DLUT_MODE_INDEX")
    , m_3dLutModesBlob(this, "3DLUT_MODES")
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

    if (!m_bypass.isValid()) {
        qCWarning(KWIN_DRM, "Bypass property is invalid, skipping color pipeline!");
        // TODO handle this; as long as the operation matches what we want to use, we could still make it work
        return false;
    }
    if (m_op) {
        return true;
    }
    const auto next = m_nextOp ? m_nextOp->colorOp() : nullptr;
    if (!m_type.hasEnumForValue(m_type.value())) {
        // we don't know this color operation type (yet)
        // but we have a bypass, so it's fine
        m_op = std::make_unique<UnknownColorOp>(next, &m_bypass);
        qCDebug(KWIN_DRM, "skipping unknown color op %lu", m_type.value());
        return true;
    }
    switch (m_type.enumValue()) {
    case Type::Lut1D:
        if (!m_1dlutSize.isValid() || !m_1dlutSize.value()) {
            qCWarning(KWIN_DRM, "skipping 1D lut with invalid size property");
            m_op = std::make_unique<UnknownColorOp>(next, &m_bypass);
            return true;
        }
        m_op = std::make_unique<DrmLutColorOp>(next, &m_data, m_1dlutSize.value(), &m_bypass);
        return true;
    case Type::Matrix3x4:
        m_op = std::make_unique<Matrix3x4ColorOp>(next, &m_data, &m_bypass);
        return true;
    case Type::Lut3D:
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
            m_op = std::make_unique<UnknownColorOp>(next, &m_bypass);
            return true;
        }
        m_op = std::make_unique<DrmLut3DColorOp>(next, &m_data, &m_3dLutModeIndex, modes, &m_bypass);
        return true;
    }
    Q_UNREACHABLE();
}
}
