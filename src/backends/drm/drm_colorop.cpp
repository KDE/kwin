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

struct Assignments
{
    // a color op that needs to be applied by the previous operation
    std::optional<ColorOp> outputScaling;
    DrmAbstractColorOp *nextHardwareOp = nullptr;
    std::unordered_map<DrmAbstractColorOp *, std::deque<ColorOp::Operation>> assignments;
};

static bool maybeApplyOutputScaling(DrmAbstractColorOp *hardwareOp, DrmAbstractColorOp *previousHardwareOp, Assignments &assignments)
{
    if (!assignments.outputScaling) {
        return true;
    }
    if (!hardwareOp->colorOpPreference(assignments.outputScaling->operation)) {
        return false;
    }
    if (previousHardwareOp != hardwareOp && hardwareOp->inputScaling(*assignments.outputScaling)) {
        return false;
    }
    if (hardwareOp->outputScaling(*assignments.outputScaling)) {
        return false;
    }
    assignments.assignments[hardwareOp].push_front(assignments.outputScaling->operation);
    assignments.outputScaling.reset();
    return true;
}

static std::optional<Assignments> findColorPipelineAssignments(DrmAbstractColorOp *hardwareOp, DrmAbstractColorOp *previousHardwareOp, std::span<const ColorOp> ops, const std::optional<ColorOp> &inputScaling)
{
    const ColorOp *currentColorOp = nullptr;
    std::span<const ColorOp> nextOps;
    if (previousHardwareOp != hardwareOp && inputScaling.has_value()) {
        // we need to apply input scaling
        currentColorOp = &*inputScaling;
        nextOps = ops;
    } else if (ops.empty()) {
        // we're done here, the whole pipeline is assigned!
        return Assignments{};
    } else {
        currentColorOp = &ops.front();
        nextOps = ops.subspan(1);
    }
    if (!hardwareOp) {
        return std::nullopt;
    }

    const bool isInput = hardwareOp != previousHardwareOp;
    auto preference = hardwareOp->colorOpPreference(currentColorOp->operation);
    if (isInput && !hardwareOp->canBeInputOrOutput(currentColorOp->inputSpace)) {
        preference.reset();
    }
    if (!preference) {
        if (!hardwareOp->canBypass()) {
            return std::nullopt;
        }
        auto ret = findColorPipelineAssignments(hardwareOp->next(), hardwareOp, ops, inputScaling);
        if (ret) {
            maybeApplyOutputScaling(hardwareOp, previousHardwareOp, *ret);
        }
        return ret;
    }
    if (hardwareOp->canBypass()) {
        // attempt to find a higher priority hardware op type
        DrmAbstractColorOp *higherPreferenceOp = hardwareOp->next();
        while (higherPreferenceOp) {
            const auto higherPreference = higherPreferenceOp->colorOpPreference(currentColorOp->operation);
            if (higherPreference.has_value() && *higherPreference > preference) {
                if (auto ret = findColorPipelineAssignments(higherPreferenceOp, hardwareOp, ops, inputScaling)) {
                    maybeApplyOutputScaling(hardwareOp, previousHardwareOp, *ret);
                    return ret;
                } else {
                    break;
                }
            }
            higherPreferenceOp = higherPreferenceOp->next();
        }
    }

    // this hardware op is the best match for currentColorOp
    // before doing anything with it though, figure out if the other operations can be applied as well
    const auto postScaling = hardwareOp->outputScaling(*currentColorOp);
    auto nextHardwareOp = hardwareOp->supportsMultipleOps() ? hardwareOp : hardwareOp->next();
    auto ret = findColorPipelineAssignments(nextHardwareOp, hardwareOp, nextOps, postScaling ? std::optional(postScaling->inverse) : std::nullopt);
    if (!ret) {
        if (!nextHardwareOp) {
            return std::nullopt;
        }
        // in some cases, skipping hardware ops can find working configurations
        // as some hardware ops can only operate on specific color spaces
        return findColorPipelineAssignments(nextHardwareOp->next(), previousHardwareOp, ops, inputScaling);
    }

    const bool isOutput = ret->nextHardwareOp != hardwareOp;
    if (isOutput) {
        auto lastColorSpace = currentColorOp->outputSpace;
        if (ret->outputScaling) {
            lastColorSpace = ret->outputScaling->outputSpace;
        } else if (postScaling) {
            lastColorSpace = postScaling->inverse.inputSpace;
        }
        if (!hardwareOp->canBeInputOrOutput(lastColorSpace)) {
            return std::nullopt;
        }
    }

    // first, the pre-op scaling from the next hardware op
    if (ret->outputScaling && !hardwareOp->supportsMultipleOps()) {
        return std::nullopt;
    }
    if (!maybeApplyOutputScaling(hardwareOp, hardwareOp, *ret)) {
        return std::nullopt;
    }

    // next, our own post-op scaling
    if (postScaling && ret->nextHardwareOp != hardwareOp) {
        // need to prepend, because we recursively walk back
        // from the last color opt to the front
        ret->assignments[hardwareOp].push_front(postScaling->scaling);
    }

    // next, the actual operation
    ret->assignments[hardwareOp].push_front(currentColorOp->operation);

    // last, our own pre-op scaling
    if (isInput) {
        const auto preScaling = hardwareOp->inputScaling(*currentColorOp);
        if (preScaling) {
            ret->assignments[hardwareOp].push_front(preScaling->scaling);
            ret->outputScaling = preScaling->inverse;
        }
    }
    ret->nextHardwareOp = hardwareOp;
    return ret;
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
    const auto ret = findColorPipelineAssignments(this, nullptr, pipeline.ops, std::nullopt);
    if (!ret || ret->outputScaling) {
        m_cachedPipelineFail = pipeline;
        return false;
    }

    // now actually program the properties
    m_cache = std::make_unique<DrmAtomicCommit>(commit->gpu());
    DrmAbstractColorOp *currentOp = this;
    while (currentOp) {
        const auto it = ret->assignments.find(currentOp);
        if (it != ret->assignments.end()) {
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

std::optional<DrmAbstractColorOp::Scaling> DrmAbstractColorOp::inputScaling(const ColorOp &op) const
{
    return std::nullopt;
}

std::optional<DrmAbstractColorOp::Scaling> DrmAbstractColorOp::outputScaling(const ColorOp &op) const
{
    return std::nullopt;
}

bool DrmAbstractColorOp::canBeInputOrOutput(ColorspaceType type) const
{
    return true;
}

DrmLutColorOp::DrmLutColorOp(DrmAbstractColorOp *next, DrmProperty *prop, DrmEnumProperty<Lut1DInterpolation> *interpolation, uint32_t maxSize, DrmProperty *bypass)
    : DrmAbstractColorOp(next, Features{Feature::MultipleOps} | Feature::Bypass, QStringLiteral("1D LUT"))
    , m_prop(prop)
    , m_bypass(bypass)
    , m_interpolationMode(interpolation)
    , m_maxSize(maxSize)
    , m_components(m_maxSize)
{
}

std::optional<uint32_t> DrmLutColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    if (std::holds_alternative<ColorTransferFunction>(op) || std::holds_alternative<InverseColorTransferFunction>(op)
        || std::holds_alternative<ColorTonemapper>(op) || std::holds_alternative<std::shared_ptr<ColorTransformation>>(op)) {
        // the required resolution depends heavily on the function and on the input and output ranges / multipliers
        // but this is good enough for now
        if (m_maxSize >= 1024) {
            return 1;
        } else {
            return 0;
        }
        // TODO also check matrices for scaling + offset only, which can be represented just fine!
    } else if (std::holds_alternative<ColorMultiplier>(op)) {
        return 0;
    }
    return std::nullopt;
}

void DrmLutColorOp::program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations)
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
    // uncomment once the property isn't immutable anymore
    // commit->addEnum(*m_interpolationMode, Lut1DInterpolation::Linear);
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

std::optional<DrmAbstractColorOp::Scaling> DrmLutColorOp::inputScaling(const ColorOp &op) const
{
    if (op.input.max <= 1.001) {
        return std::nullopt;
    }
    return Scaling{
        .scaling = ColorMultiplier(op.input.max),
        .inverse = ColorOp{
            .input = op.input,
            .inputSpace = op.inputSpace,
            .operation = ColorMultiplier(1.0 / op.input.max),
            .output = ValueRange{
                .min = op.input.min / op.input.max,
                .max = 1.0,
            },
            .outputSpace = op.inputSpace,
        },
    };
}

std::optional<DrmAbstractColorOp::Scaling> DrmLutColorOp::outputScaling(const ColorOp &op) const
{
    if (op.output.max <= 1.001) {
        return std::nullopt;
    }
    return Scaling{
        .scaling = ColorMultiplier(1.0 / op.output.max),
        .inverse = ColorOp{
            .input = ValueRange{
                .min = op.output.min / op.output.max,
                .max = 1.0,
            },
            .inputSpace = op.outputSpace,
            .operation = ColorMultiplier(op.output.max),
            .output = op.output,
            .outputSpace = op.outputSpace,
        },
    };
}

LegacyMatrixColorOp::LegacyMatrixColorOp(DrmAbstractColorOp *next, DrmProperty *prop)
    : DrmAbstractColorOp(next, Features{Feature::Bypass} | Feature::MultipleOps, QStringLiteral("legacy matrix"))
    , m_prop(prop)
{
}

std::optional<uint32_t> LegacyMatrixColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    // TODO check the resolution of the matrix too?
    // -> values above abs(5) should be re-scaled with a different (previous?) operation
    if (auto matrix = std::get_if<ColorMatrix>(&op)) {
        const bool canRepresent = std::abs(matrix->mat(3, 0) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(3, 1) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(3, 2) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(3, 3) - 1) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(0, 3) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(1, 3) - 0) < ColorPipeline::s_maxResolution
            && std::abs(matrix->mat(2, 3) - 0) < ColorPipeline::s_maxResolution;
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

void LegacyMatrixColorOp::program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations)
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

std::optional<uint32_t> Matrix3x4ColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    // TODO check the resolution of the matrix too?
    // -> values above abs(5) should be re-scaled with a different (previous?) operation
    if (auto matrix = std::get_if<ColorMatrix>(&op)) {
        const bool canRepresent = matrix->mat(3, 0) == 0
            && matrix->mat(3, 1) == 0
            && matrix->mat(3, 2) == 0
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

void Matrix3x4ColorOp::program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations)
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

std::optional<uint32_t> UnknownColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    return std::nullopt;
}

void UnknownColorOp::program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations)
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

std::optional<uint32_t> DrmLut3DColorOp::colorOpPreference(const ColorOp::Operation &op)
{
    // everything can be represented as a 3D lut,
    // but resolution isn't great, so push it to other ops if possible
    return 0;
}

bool DrmLut3DColorOp::canBeInputOrOutput(ColorspaceType type) const
{
    // because resolution is so limited, input or output has to be non-linear RGB
    // linear RGB as I/O is also okay, because we can apply scaling to it
    return type == ColorspaceType::NonLinearRGB
        || type == ColorspaceType::LinearRGB;
}

void DrmLut3DColorOp::program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations)
{
    for (size_t r = 0; r < m_size; r++) {
        for (size_t g = 0; g < m_size; g++) {
            for (size_t b = 0; b < m_size; b++) {
                QVector3D output = QVector3D(r, g, b) / float(m_size - 1);
                for (const auto &op : operations) {
                    output = ColorOp::applyOperation(op, output);
                }
                const size_t index = b * m_size * m_size + g * m_size + r;
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
    // uncomment once the property isn't immutable anymore
    // commit->addEnum(*m_interpolation, Lut3DInterpolation::Tetrahedal);
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void DrmLut3DColorOp::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

std::optional<DrmAbstractColorOp::Scaling> DrmLut3DColorOp::inputScaling(const ColorOp &op) const
{
    if (op.inputSpace == ColorspaceType::NonLinearRGB) {
        // The encoding is already some sort of non-linear RGB, applying PQ
        // on top of that would just make things worse rather than better
        // TODO use a custom LUT to use the limited resolution as much as is possible
        return std::nullopt;
    }
    TransferFunction tf{TransferFunction::PerceptualQuantizer, op.input.min, op.input.max};
    return Scaling{
        .scaling = ColorTransferFunction(tf),
        .inverse = ColorOp{
            .input = op.input,
            .inputSpace = op.inputSpace,
            .operation = InverseColorTransferFunction(tf),
            .output = ValueRange{
                .min = 0,
                .max = 1,
            },
            // NOTE this assumption only works because we enforce it in canBeInputOrOutput
            .outputSpace = ColorspaceType::NonLinearRGB,
        },
    };
}

std::optional<DrmAbstractColorOp::Scaling> DrmLut3DColorOp::outputScaling(const ColorOp &op) const
{
    if (op.inputSpace == ColorspaceType::NonLinearRGB) {
        // The encoding is already some sort of non-linear RGB, applying PQ
        // on top of that would just make things worse rather than better
        // TODO use a custom LUT to use the limited resolution as much as is possible
        return std::nullopt;
    }
    TransferFunction tf{TransferFunction::PerceptualQuantizer, op.output.min, op.output.max};
    return Scaling{
        .scaling = InverseColorTransferFunction(tf),
        .inverse = ColorOp{
            .input = ValueRange{
                .min = 0,
                .max = 1,
            },
            // NOTE this assumption only works because we enforce it in canBeInputOrOutput
            .inputSpace = ColorspaceType::NonLinearRGB,
            .operation = ColorTransferFunction(tf),
            .output = op.output,
            .outputSpace = op.outputSpace,
        },
    };
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

std::optional<uint32_t> DrmMultiplier::colorOpPreference(const ColorOp::Operation &op)
{
    if (const auto mult = std::get_if<ColorMultiplier>(&op)) {
        const float diff1 = std::abs(mult->factors.x() - mult->factors.y());
        const float diff2 = std::abs(mult->factors.y() - mult->factors.z());
        const float diff3 = std::abs(mult->factors.z() - mult->factors.x());
        const float maxDiff = std::max({diff1, diff2, diff3});
        if (maxDiff < 0.00001) {
            return 2;
        }
    } else if (const auto mat = std::get_if<ColorMatrix>(&op)) {
        // we want to partially apply the matrix
        // as this has much higher resolution than matrices in hardware
        const float common = commonScaling(mat->mat);
        if (!qFuzzyIsNull(common) && !qFuzzyCompare(common, 1)) {
            return 2;
        }
    }
    return std::nullopt;
}

void DrmMultiplier::program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations)
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

std::optional<DrmAbstractColorOp::Scaling> DrmMultiplier::inputScaling(const ColorOp &op) const
{
    return std::nullopt;
}

std::optional<DrmAbstractColorOp::Scaling> DrmMultiplier::outputScaling(const ColorOp &op) const
{
    if (const auto mat = std::get_if<ColorMatrix>(&op.operation)) {
        const float scaling = commonScaling(mat->mat);
        QMatrix4x4 remaining = mat->mat;
        remaining.scale(1.0 / scaling);
        if (!isFuzzyIdentity(remaining)) {
            return Scaling{
                .scaling = ColorMatrix(remaining.inverted()),
                .inverse = ColorOp{
                    .input = op.output * (1.0 / scaling),
                    .inputSpace = op.inputSpace,
                    .operation = ColorMatrix(remaining),
                    .output = op.output,
                    .outputSpace = op.outputSpace,
                },
            };
        }
    }
    return std::nullopt;
}

DrmNamed1DLut::DrmNamed1DLut(DrmAbstractColorOp *next, DrmEnumProperty<Named1DLutType> *value, DrmProperty *bypass)
    : DrmAbstractColorOp(next, bypass ? Feature::Bypass : Features{}, QStringLiteral("1D curve"))
    , m_value(value)
    , m_bypass(bypass)
{
}

std::optional<Named1DLutType> DrmNamed1DLut::getType(const ColorOp::Operation &op) const
{
    if (const auto tf = std::get_if<ColorTransferFunction>(&op)) {
        switch (tf->tf.type) {
        case TransferFunction::sRGB:
            return Named1DLutType::sRGB_EOTF;
        case TransferFunction::PerceptualQuantizer:
            return Named1DLutType::PQ_125_EOTF;
        default:
            return std::nullopt;
        }
    } else if (const auto invTf = std::get_if<InverseColorTransferFunction>(&op)) {
        switch (invTf->tf.type) {
        case TransferFunction::sRGB:
            return Named1DLutType::sRGB_Inverse_EOTF;
        case TransferFunction::PerceptualQuantizer:
            return Named1DLutType::PQ_125_Inverse_EOTF;
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

void DrmNamed1DLut::program(DrmAtomicCommit *commit, const std::deque<ColorOp::Operation> &operations)
{
    // there may be the input / output scaling matrix in here, ignore it
    const auto curve = std::ranges::find_if(operations, [](const ColorOp::Operation &op) {
        return std::holds_alternative<ColorTransferFunction>(op)
            || std::holds_alternative<InverseColorTransferFunction>(op);
    });
    Q_ASSERT(curve != operations.end());
    commit->addEnum(*m_value, *getType(*curve));
    if (m_bypass) {
        commit->addProperty(*m_bypass, 0);
    }
}

void DrmNamed1DLut::bypass(DrmAtomicCommit *commit)
{
    commit->addProperty(*m_bypass, 1);
}

std::optional<DrmAbstractColorOp::Scaling> DrmNamed1DLut::inputScaling(const ColorOp &op) const
{
    const auto evaluateMat = [](const QMatrix4x4 &mat, float value) {
        return (mat * QVector3D(value, value, value)).x();
    };
    if (const auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
        // TODO this should ideally use an offset+scaling op, and not a generic matrix
        QMatrix4x4 mat;
        if (tf->tf.type == TransferFunction::PerceptualQuantizer) {
            // PQ 125 is used, where 10k nits is 125
            mat.scale(125.0);
        }
        mat.scale(1.0 / (tf->tf.maxLuminance - tf->tf.minLuminance));
        mat.translate(-tf->tf.minLuminance, -tf->tf.minLuminance, -tf->tf.minLuminance);
        return Scaling{
            .scaling = ColorMatrix(mat.inverted()),
            .inverse = ColorOp{
                .input = op.input,
                .inputSpace = op.inputSpace,
                .operation = ColorMatrix(mat),
                .output = ValueRange{
                    .min = evaluateMat(mat, op.input.min),
                    .max = evaluateMat(mat, op.input.max),
                },
                // not linear, not normal nonlinear either though
                .outputSpace = ColorspaceType::AnyNonRGB,
            },
        };
    } else {
        return std::nullopt;
    }
}

std::optional<DrmAbstractColorOp::Scaling> DrmNamed1DLut::outputScaling(const ColorOp &op) const
{
    const auto evaluateMat = [](const QMatrix4x4 &mat, float value) {
        return (mat * QVector3D(value, value, value)).x();
    };
    if (const auto tf = std::get_if<ColorTransferFunction>(&op.operation)) {
        // TODO this should ideally use an offset+scaling op, and not a generic matrix
        QMatrix4x4 invMat;
        invMat.translate(tf->tf.minLuminance, tf->tf.minLuminance, tf->tf.minLuminance);
        invMat.scale(tf->tf.maxLuminance - tf->tf.minLuminance);
        if (tf->tf.type == TransferFunction::PerceptualQuantizer) {
            // PQ 125 is used, where 10k nits is 125
            invMat.scale(1 / 125.0);
        }
        return Scaling{
            .scaling = ColorMatrix(invMat.inverted()),
            .inverse = ColorOp{
                .input = ValueRange{
                    .min = evaluateMat(invMat, op.output.min),
                    .max = evaluateMat(invMat, op.output.max),
                },
                // not linear, not normal nonlinear either though
                .inputSpace = ColorspaceType::AnyNonRGB,
                .operation = ColorMatrix(invMat),
                .output = op.output,
                .outputSpace = op.outputSpace,
            },
        };
    } else {
        return std::nullopt;
    }
}

DrmColorOp::DrmColorOp(DrmGpu *gpu, uint32_t objectId)
    : DrmObject(gpu, objectId, DRM_MODE_OBJECT_ANY)
    , m_next(this, QByteArrayLiteral("NEXT"))
    , m_type(this, QByteArrayLiteral("TYPE"), {
                                                  QByteArrayLiteral("1D LUT"),
                                                  QByteArrayLiteral("3x4 Matrix"),
                                                  QByteArrayLiteral("3D LUT"),
                                                  QByteArrayLiteral("Multiplier"),
                                                  QByteArrayLiteral("1D Curve"),
                                              })
    , m_data(this, QByteArrayLiteral("DATA"))
    , m_size(this, QByteArrayLiteral("SIZE"))
    , m_bypass(this, QByteArrayLiteral("BYPASS"))
    , m_multiplier(this, QByteArrayLiteral("MULTIPLIER"))
    , m_1dNamedLutType(this, QByteArrayLiteral("CURVE_1D_TYPE"), {
                                                                     QByteArrayLiteral("sRGB EOTF"),
                                                                     QByteArrayLiteral("sRGB Inverse EOTF"),
                                                                     QByteArrayLiteral("PQ 125 EOTF"),
                                                                     QByteArrayLiteral("PQ 125 Inverse EOTF"),
                                                                 })
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
    m_size.update(props);
    m_bypass.update(props);
    m_multiplier.update(props);
    m_1dNamedLutType.update(props);
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
        m_op = std::make_unique<DrmLutColorOp>(next, &m_data, &m_lut1dInterpolation, m_size.value(), bypassProp);
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
    case Type::NamedLut1D:
        if (!m_1dNamedLutType.isValid()) {
            qCWarning(KWIN_DRM, "Skipping 1D lut with invalid type property");
            m_op = std::make_unique<UnknownColorOp>(next, bypassProp);
            return true;
        }
        m_op = std::make_unique<DrmNamed1DLut>(next, &m_1dNamedLutType, bypassProp);
        return true;
    }
    Q_UNREACHABLE();
}
}
