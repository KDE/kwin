#include "drm_colorop.h"
#include "drm_blob.h"
#include "drm_colorop.h"
#include "drm_commit.h"

namespace KWin
{

DrmColorOp::DrmColorOp(DrmGpu *gpu, uint32_t pipelineId)
    : DrmObject(gpu, pipelineId, DRM_MODE_OBJECT_ANY)
    , type(this, "TYPE", {
                             QByteArrayLiteral("1D Curve"),
                             QByteArrayLiteral("1D Curve Custom LUT"),
                             QByteArrayLiteral("3x4 Matrix"),
                             QByteArrayLiteral("Multiplier"),
                         })
    , next(this, "NEXT")
    , bypass(this, "BYPASS")
    , curveType(this, "CURVE_1D_TYPE", {
                                           QByteArrayLiteral("sRGB EOTF"),
                                           QByteArrayLiteral("BT.2020 Inverse OETF"),
                                           QByteArrayLiteral("PQ 125 EOTF"),
                                       })
    , multiplier(this, "MULTIPLIER")
    , matrixData(this, "DATA")
    , lutData(this, "DATA")
    , lutSize(this, "SIZE")
{
}

DrmColorOp::~DrmColorOp()
{
}

bool DrmColorOp::updateProperties()
{
    DrmPropertyList props = queryProperties();
    // for (const auto &[prop, value] : props.m_properties) {
    //     qWarning() << "color op prop:" << prop->name << value;
    //     for (int i = 0; i < prop->count_enums; i++) {
    //         qWarning() << "enum value:" << prop->enums[i].name << "->" << prop->enums[i].value;
    //     }
    // }
    type.update(props);
    next.update(props);
    bypass.update(props);
    switch (type.enumValue()) {
    case Type::Named1DLut:
        curveType.update(props);
        break;
    case Type::Custom1DLut:
        lutData.update(props);
        lutSize.update(props);
        break;
    case Type::Matrix3x4:
        matrixData.update(props);
        break;
    case Type::Multiplier:
        multiplier.update(props);
        break;
    }
    if (!type.isValid()) {
        return false;
    }
    qWarning() << "color op type" << type.enumValue();
    // qWarning() << "\ttype:" << type.value() << "next" << next.value() << "bypass" << bypass.value() << "curve type" << curveType.value() << "multiplier" << multiplier.value() << "matrix" << matrixData.value();
    if (next.value() != 0) {
        nextProp = std::make_unique<DrmColorOp>(gpu(), next.value());
        nextProp->updateProperties();
    } else {
        qWarning() << "\tend";
        nextProp.reset();
    }
    return true;
}

void DrmColorOp::disable(DrmAtomicCommit *commit)
{
    commit->addProperty(bypass, 1);
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

bool DrmColorOp::matchPipeline(DrmAtomicCommit *commit, const ColorPipeline &pipeline)
{
    if (m_cachedPipeline == pipeline) {
        commit->merge(m_cache.get());
        return true;
    }
    // TODO this could use more optimizations, like combining multipliers with custom luts and matrices
    // also, if the pipeline doesn't work, we uselessly calculate luts every frame
    // -> build a map of DrmColorPp -> vector<ColorOp> and program it only after validation is successful
    // -> cache the failed color pipeline and reject it as a fast path

    std::unique_ptr<DrmAtomicCommit> cache = std::make_unique<DrmAtomicCommit>(gpu());
    QVector<QString> components;

    DrmColorOp *currentOp = this;
    const auto findType = [&](const auto &check) {
        while (!check(currentOp)) {
            cache->addProperty(currentOp->bypass, 1);
            components.push_back(QStringLiteral("bypass %1").arg(currentOp->type.value()));
            currentOp = currentOp->nextProp.get();
            if (!currentOp) {
                return false;
            }
        }
        return true;
    };

    for (const auto &targetOp : pipeline.ops) {
        bool typeFound = false;
        while (!typeFound) {
            if (auto tf = std::get_if<ColorTransferFunction>(&targetOp)) {
                std::optional<DrmColorOp::Curve> curve;
                switch (tf->tf) {
                case NamedTransferFunction::gamma22:
                    // need a custom lut
                    break;
                case NamedTransferFunction::linear:
                case NamedTransferFunction::scRGB:
                    // shouldn't be used
                    break;
                case NamedTransferFunction::PerceptualQuantizer:
                    // TODO handle the 125 factor
                    // curve = DrmColorOp::Curve::PQ125_EOTF;
                    break;
                case NamedTransferFunction::sRGB:
                    curve = DrmColorOp::Curve::sRGB_EOTF;
                    break;
                }
                const auto lutMatch = [curve](DrmColorOp *op) {
                    if (op->type.enumValue() == DrmColorOp::Type::Named1DLut) {
                        return curve && op->curveType.hasEnum(*curve);
                    } else {
                        return op->type.enumValue() == DrmColorOp::Type::Custom1DLut;
                    }
                };
                if (!findType(lutMatch)) {
                    qWarning() << "couldn't find lut";
                    return false;
                }
                cache->addProperty(currentOp->bypass, 0);
                if (currentOp->type.enumValue() == DrmColorOp::Type::Named1DLut) {
                    components.push_back(QStringLiteral("named curve"));
                    cache->addEnum(currentOp->curveType, *curve);
                } else {
                    components.push_back(QStringLiteral("'custom' curve"));
                    const uint32_t size = currentOp->lutSize.maxValue();
                    QList<drm_color_lut> atomicLut(size);
                    for (uint32_t i = 0; i < size; i++) {
                        const double input = i / double(size - 1);
                        const auto output = ColorDescription::encodedToNits(QVector3D(input, input, input), tf->tf, 200) * std::numeric_limits<uint16_t>::max();
                        atomicLut[i] = {
                            .red = uint16_t(std::round(output.x())),
                            .green = uint16_t(std::round(output.y())),
                            .blue = uint16_t(std::round(output.z())),
                            .reserved = 0,
                        };
                    }
                    cache->addBlob(currentOp->lutData, DrmBlob::create(gpu(), atomicLut.data(), sizeof(drm_color_lut) * size));
                }
                typeFound = true;
            } else if (auto mat = std::get_if<ColorMatrix>(&targetOp)) {
                const bool possible = mat->mat(0, 3) == 0 && mat->mat(1, 3) == 0 && mat->mat(2, 3) == 0 && mat->mat(3, 3) == 1;
                if (!possible) {
                    qWarning() << "mat not supported";
                    return false;
                }
                const auto match = [](DrmColorOp *op) {
                    return op->type.enumValue() == DrmColorOp::Type::Matrix3x4;
                };
                if (!findType(match)) {
                    qWarning() << "couldn't find mat";
                    return false;
                }
                const std::array<uint64_t, 12> data{
                    doubleToFixed(mat->mat(0, 0)),
                    doubleToFixed(mat->mat(1, 0)),
                    doubleToFixed(mat->mat(2, 0)),
                    doubleToFixed(mat->mat(0, 1)),
                    doubleToFixed(mat->mat(1, 1)),
                    doubleToFixed(mat->mat(2, 1)),
                    doubleToFixed(mat->mat(0, 2)),
                    doubleToFixed(mat->mat(1, 2)),
                    doubleToFixed(mat->mat(2, 2)),
                    doubleToFixed(mat->mat(0, 3)),
                    doubleToFixed(mat->mat(1, 3)),
                    doubleToFixed(mat->mat(2, 3)),
                };
                cache->addProperty(currentOp->bypass, 0);
                cache->addBlob(currentOp->matrixData, DrmBlob::create(gpu(), data.data(), data.size() * sizeof(uint64_t)));
                components.push_back(QStringLiteral("matrix"));
                typeFound = true;
            } else if (auto tf = std::get_if<InverseColorTransferFunction>(&targetOp)) {
                std::optional<DrmColorOp::Curve> curve;
                switch (tf->tf) {
                case NamedTransferFunction::gamma22:
                case NamedTransferFunction::sRGB:
                    // currently not supported
                    break;
                case NamedTransferFunction::linear:
                case NamedTransferFunction::scRGB:
                    // shouldn't be used
                    break;
                case NamedTransferFunction::PerceptualQuantizer:
                    curve = DrmColorOp::Curve::BT2020_inverseEOTF;
                    break;
                }
                const auto lutMatch = [curve](DrmColorOp *op) {
                    if (op->type.enumValue() == DrmColorOp::Type::Named1DLut) {
                        return curve && op->curveType.hasEnum(*curve);
                    } else {
                        return op->type.enumValue() == DrmColorOp::Type::Custom1DLut;
                    }
                };
                if (!findType(lutMatch)) {
                    qWarning() << "couldn't find lut";
                    return false;
                }
                cache->addProperty(currentOp->bypass, 0);
                if (currentOp->type.enumValue() == DrmColorOp::Type::Named1DLut) {
                    components.push_back(QStringLiteral("named inv curve"));
                    cache->addEnum(currentOp->curveType, *curve);
                } else {
                    const uint32_t size = currentOp->lutSize.maxValue();
                    components.push_back(QStringLiteral("'custom' inv curve"));
                    QList<drm_color_lut> atomicLut(size);
                    for (uint32_t i = 0; i < size; i++) {
                        const double input = i / double(size - 1);
                        const auto output = ColorDescription::nitsToEncoded(QVector3D(input, input, input), tf->tf, 1) * std::numeric_limits<uint16_t>::max();
                        atomicLut[i] = {
                            .red = uint16_t(std::round(output.x())),
                            .green = uint16_t(std::round(output.y())),
                            .blue = uint16_t(std::round(output.z())),
                            .reserved = 0,
                        };
                    }
                    cache->addBlob(currentOp->lutData, DrmBlob::create(gpu(), atomicLut.data(), sizeof(drm_color_lut) * size));
                }
                typeFound = true;
            } else if (auto mult = std::get_if<ColorMultiplier>(&targetOp)) {
                if (mult->factor == 1.0) {
                    typeFound = true;
                    continue;
                }
                const auto match = [mult](DrmColorOp *op) {
                    return op->type.enumValue() == DrmColorOp::Type::Multiplier
                        || (op->type.enumValue() == DrmColorOp::Type::Custom1DLut && !mult->extendedInputOutputSpace);
                };
                if (!findType(match)) {
                    qWarning() << "couldn't find multiplier for" << mult->factor;
                    return false;
                }
                cache->addProperty(currentOp->bypass, 0);
                if (currentOp->type.enumValue() == DrmColorOp::Type::Multiplier) {
                    cache->addProperty(currentOp->multiplier, std::round(1 * (1UL << 32)));
                    components.push_back(QStringLiteral("multiplier %1").arg(mult->factor));
                } else {
                    components.push_back(QStringLiteral("multiplier %1 lut").arg(mult->factor));
                    const uint32_t size = currentOp->lutSize.value();
                    QList<drm_color_lut> atomicLut(size);
                    for (uint32_t i = 0; i < size; i++) {
                        const double input = i / double(size - 1);
                        const auto output = input * mult->factor * std::numeric_limits<uint16_t>::max();
                        atomicLut[i].red = output;
                        atomicLut[i].green = output;
                        atomicLut[i].blue = output;
                    }
                    cache->addBlob(currentOp->lutData, DrmBlob::create(gpu(), atomicLut.data(), sizeof(drm_color_lut) * size));
                }
                typeFound = true;
            } else {
                return false;
            }
            currentOp = currentOp->nextProp.get();
        }
    }
    while (currentOp) {
        cache->addProperty(currentOp->bypass, 1);
        components.push_back(QStringLiteral("bypass %1").arg(currentOp->type.value()));
        currentOp = currentOp->nextProp.get();
    }
    m_cachedPipeline = pipeline;
    m_cache = std::move(cache);
    commit->merge(m_cache.get());
    qWarning() << "new pipeline" << components;
    return true;
}
}
