/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "iccprofile.h"
#include "colorlut.h"
#include "colorlut3d.h"
#include "colorpipelinestage.h"
#include "colortransformation.h"
#include "utils/common.h"

#include <lcms2.h>
#include <span>
#include <tuple>

namespace KWin
{

IccProfile::IccProfile(cmsHPROFILE handle, const Colorimetry &colorimetry, BToATagData &&bToATag, const std::shared_ptr<ColorTransformation> &vcgt)
    : m_handle(handle)
    , m_colorimetry(colorimetry)
    , m_bToATag(std::move(bToATag))
    , m_vcgt(vcgt)
{
}

IccProfile::IccProfile(cmsHPROFILE handle, const Colorimetry &colorimetry, const std::shared_ptr<ColorTransformation> &inverseEOTF, const std::shared_ptr<ColorTransformation> &vcgt)
    : m_handle(handle)
    , m_colorimetry(colorimetry)
    , m_inverseEOTF(inverseEOTF)
    , m_vcgt(vcgt)
{
}

IccProfile::~IccProfile()
{
    cmsCloseProfile(m_handle);
}

const Colorimetry &IccProfile::colorimetry() const
{
    return m_colorimetry;
}

std::shared_ptr<ColorTransformation> IccProfile::inverseEOTF() const
{
    return m_inverseEOTF;
}

std::shared_ptr<ColorTransformation> IccProfile::vcgt() const
{
    return m_vcgt;
}

const IccProfile::BToATagData *IccProfile::BtToATag() const
{
    return m_bToATag ? &m_bToATag.value() : nullptr;
}

static std::vector<uint8_t> readTagRaw(cmsHPROFILE profile, cmsTagSignature tag)
{
    const auto numBytes = cmsReadRawTag(profile, tag, nullptr, 0);
    std::vector<uint8_t> data(numBytes);
    cmsReadRawTag(profile, tag, data.data(), numBytes);
    return data;
}

template<typename T>
static T read(std::span<const uint8_t> data, size_t index)
{
    // ICC profile data is big-endian
    T ret;
    for (size_t i = 0; i < sizeof(T); i++) {
        *(reinterpret_cast<uint8_t *>(&ret) + i) = data[index + sizeof(T) - i - 1];
    }
    return ret;
}

static float readS15Fixed16(std::span<const uint8_t> data, size_t index)
{
    return read<int32_t>(data, index) / 65536.0;
}

static std::optional<std::tuple<size_t, size_t, size_t>> parseBToACLUTSize(std::span<const uint8_t> data)
{
    const uint32_t tagType = read<uint32_t>(data, 0);
    const bool isLutTag = tagType == cmsSigLut8Type || tagType == cmsSigLut16Type;
    if (isLutTag) {
        const uint8_t size = data[10];
        return std::make_tuple(size, size, size);
    } else {
        const uint32_t clutOffset = read<uint32_t>(data, 24);
        if (data.size() < clutOffset + 19) {
            qCWarning(KWIN_CORE, "CLut offset points to invalid position %u", clutOffset);
            return std::nullopt;
        }
        return std::make_tuple(data[clutOffset + 0], data[clutOffset + 1], data[clutOffset + 2]);
    }
}

static std::optional<QMatrix4x4> parseMatrix(std::span<const uint8_t> data, bool hasOffset)
{
    const size_t matrixSize = hasOffset ? 12 : 9;
    std::vector<float> floats;
    floats.reserve(matrixSize);
    for (size_t i = 0; i < matrixSize; i++) {
        floats.push_back(readS15Fixed16(data, i * 4));
    }
    constexpr double xyzEncodingFactor = 65536.0 / (2 * 65535.0);
    QMatrix4x4 ret;
    ret(0, 0) = floats[0] * xyzEncodingFactor;
    ret(0, 1) = floats[1] * xyzEncodingFactor;
    ret(0, 2) = floats[2] * xyzEncodingFactor;
    ret(1, 0) = floats[3] * xyzEncodingFactor;
    ret(1, 1) = floats[4] * xyzEncodingFactor;
    ret(1, 2) = floats[5] * xyzEncodingFactor;
    ret(2, 0) = floats[6] * xyzEncodingFactor;
    ret(2, 1) = floats[7] * xyzEncodingFactor;
    ret(2, 2) = floats[8] * xyzEncodingFactor;
    if (hasOffset) {
        ret(0, 3) = floats[9] * xyzEncodingFactor;
        ret(1, 3) = floats[10] * xyzEncodingFactor;
        ret(2, 3) = floats[11] * xyzEncodingFactor;
    }
    return ret;
}

static std::optional<IccProfile::BToATagData> parseBToATag(cmsHPROFILE profile, cmsTagSignature tag)
{
    cmsPipeline *bToAPipeline = static_cast<cmsPipeline *>(cmsReadTag(profile, tag));
    if (!bToAPipeline) {
        return std::nullopt;
    }
    IccProfile::BToATagData ret;
    auto data = readTagRaw(profile, tag);
    const uint32_t tagType = read<uint32_t>(data, 0);
    switch (tagType) {
    case cmsSigLut8Type:
    case cmsSigLut16Type:
        if (data.size() < 48) {
            qCWarning(KWIN_CORE) << "ICC profile tag is too small" << data.size();
            return std::nullopt;
        }
        break;
    case cmsSigLutBtoAType:
        if (data.size() < 32) {
            qCWarning(KWIN_CORE) << "ICC profile tag is too small" << data.size();
            return std::nullopt;
        }
        break;
    default:
        qCWarning(KWIN_CORE).nospace() << "unknown lut type " << (char)data[0] << (char)data[1] << (char)data[2] << (char)data[3];
        return std::nullopt;
    }
    for (auto stage = cmsPipelineGetPtrToFirstStage(bToAPipeline); stage != nullptr; stage = cmsStageNext(stage)) {
        switch (const cmsStageSignature stageType = cmsStageType(stage)) {
        case cmsStageSignature::cmsSigCurveSetElemType: {
            // TODO read the actual functions and apply them in the shader instead
            // of using LUTs for more accuracy
            std::vector<std::unique_ptr<ColorPipelineStage>> stages;
            stages.push_back(std::make_unique<ColorPipelineStage>(cmsStageDup(stage)));
            auto transformation = std::make_unique<ColorTransformation>(std::move(stages));
            // the order of operations is fixed, so just sort the LUTs into the appropriate places
            // depending on the stages that have already been added
            if (!ret.matrix) {
                ret.B = std::move(transformation);
            } else if (!ret.CLut) {
                ret.M = std::move(transformation);
            } else if (!ret.A) {
                ret.A = std::move(transformation);
            } else {
                qCWarning(KWIN_CORE, "unexpected amount of curve elements in BToA tag");
                return std::nullopt;
            }
        } break;
        case cmsStageSignature::cmsSigMatrixElemType: {
            const bool isLutTag = tagType == cmsSigLut8Type || tagType == cmsSigLut16Type;
            const uint32_t matrixOffset = isLutTag ? 12 : read<uint32_t>(data, 16);
            const uint32_t matrixSize = isLutTag ? 9 : 12;
            if (data.size() < matrixOffset + matrixSize * 4) {
                qCWarning(KWIN_CORE, "matrix offset points to invalid position %u", matrixOffset);
                return std::nullopt;
            }
            const auto mat = parseMatrix(std::span(data).subspan(matrixOffset), !isLutTag);
            if (!mat) {
                return std::nullopt;
            }
            ret.matrix = mat;
        }; break;
        case cmsStageSignature::cmsSigCLutElemType: {
            const auto size = parseBToACLUTSize(data);
            if (!size) {
                return std::nullopt;
            }
            const auto [x, y, z] = *size;
            std::vector<std::unique_ptr<ColorPipelineStage>> stages;
            stages.push_back(std::make_unique<ColorPipelineStage>(cmsStageDup(stage)));
            ret.CLut = std::make_unique<ColorLUT3D>(std::make_unique<ColorTransformation>(std::move(stages)), x, y, z);
        } break;
        default:
            qCWarning(KWIN_CORE, "unknown stage type %u", stageType);
            return std::nullopt;
        }
    }
    return ret;
}

std::unique_ptr<IccProfile> IccProfile::load(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }
    cmsHPROFILE handle = cmsOpenProfileFromFile(path.toUtf8(), "r");
    if (!handle) {
        qCWarning(KWIN_CORE) << "Failed to open color profile file:" << path;
        return nullptr;
    }
    if (cmsGetDeviceClass(handle) != cmsSigDisplayClass) {
        qCWarning(KWIN_CORE) << "Only Display ICC profiles are supported";
        return nullptr;
    }
    if (cmsGetPCS(handle) != cmsColorSpaceSignature::cmsSigXYZData) {
        qCWarning(KWIN_CORE) << "Only ICC profiles with a XYZ connection space are supported";
        return nullptr;
    }
    if (cmsGetColorSpace(handle) != cmsColorSpaceSignature::cmsSigRgbData) {
        qCWarning(KWIN_CORE) << "Only ICC profiles with RGB color spaces are supported";
        return nullptr;
    }

    std::shared_ptr<ColorTransformation> vcgt;
    cmsToneCurve **vcgtTag = static_cast<cmsToneCurve **>(cmsReadTag(handle, cmsSigVcgtTag));
    if (!vcgtTag || !vcgtTag[0]) {
        qCDebug(KWIN_CORE) << "Profile" << path << "has no VCGT tag";
    } else {
        // Need to duplicate the VCGT tone curves as they are owned by the profile.
        cmsToneCurve *toneCurves[] = {
            cmsDupToneCurve(vcgtTag[0]),
            cmsDupToneCurve(vcgtTag[1]),
            cmsDupToneCurve(vcgtTag[2]),
        };
        std::vector<std::unique_ptr<ColorPipelineStage>> stages;
        stages.push_back(std::make_unique<ColorPipelineStage>(cmsStageAllocToneCurves(nullptr, 3, toneCurves)));
        vcgt = std::make_shared<ColorTransformation>(std::move(stages));
    }

    const cmsCIEXYZ *whitepoint = static_cast<cmsCIEXYZ *>(cmsReadTag(handle, cmsSigMediaWhitePointTag));
    if (!whitepoint) {
        qCWarning(KWIN_CORE, "profile is missing the wtpt tag");
        return nullptr;
    }

    QVector3D red;
    QVector3D green;
    QVector3D blue;
    QVector3D white(whitepoint->X, whitepoint->Y, whitepoint->Z);
    std::optional<QMatrix4x4> chromaticAdaptationMatrix;
    if (cmsIsTag(handle, cmsSigChromaticAdaptationTag)) {
        // the chromatic adaptation tag is a 3x3 matrix that converts from the actual whitepoint to D50
        const auto data = readTagRaw(handle, cmsSigChromaticAdaptationTag);
        const auto mat = parseMatrix(std::span(data).subspan(8), false);
        if (!mat) {
            qCWarning(KWIN_CORE, "Parsing chromatic adaptation matrix failed");
            return nullptr;
        }
        bool invertable = false;
        chromaticAdaptationMatrix = mat->inverted(&invertable);
        if (!invertable) {
            qCWarning(KWIN_CORE, "Inverting chromatic adaptation matrix failed");
            return nullptr;
        }
        const QVector3D D50(0.9642, 1.0, 0.8249);
        white = *chromaticAdaptationMatrix * D50;
    }
    if (cmsCIExyYTRIPLE *chrmTag = static_cast<cmsCIExyYTRIPLE *>(cmsReadTag(handle, cmsSigChromaticityTag))) {
        red = Colorimetry::xyToXYZ(QVector2D(chrmTag->Red.x, chrmTag->Red.y)) * chrmTag->Red.Y;
        green = Colorimetry::xyToXYZ(QVector2D(chrmTag->Green.x, chrmTag->Green.y)) * chrmTag->Green.Y;
        blue = Colorimetry::xyToXYZ(QVector2D(chrmTag->Blue.x, chrmTag->Blue.y)) * chrmTag->Blue.Y;
    } else {
        const cmsCIEXYZ *r = static_cast<cmsCIEXYZ *>(cmsReadTag(handle, cmsSigRedColorantTag));
        const cmsCIEXYZ *g = static_cast<cmsCIEXYZ *>(cmsReadTag(handle, cmsSigGreenColorantTag));
        const cmsCIEXYZ *b = static_cast<cmsCIEXYZ *>(cmsReadTag(handle, cmsSigBlueColorantTag));
        if (!r || !g || !b) {
            qCWarning(KWIN_CORE, "rXYZ, gXYZ or bXYZ tag is missing");
            return nullptr;
        }
        if (chromaticAdaptationMatrix) {
            red = *chromaticAdaptationMatrix * QVector3D(r->X, r->Y, r->Z);
            green = *chromaticAdaptationMatrix * QVector3D(g->X, g->Y, g->Z);
            blue = *chromaticAdaptationMatrix * QVector3D(b->X, b->Y, b->Z);
        } else {
            // if the chromatic adaptation tag isn't available, fall back to using the media whitepoint instead
            cmsCIEXYZ adaptedR{};
            cmsCIEXYZ adaptedG{};
            cmsCIEXYZ adaptedB{};
            bool success = cmsAdaptToIlluminant(&adaptedR, cmsD50_XYZ(), whitepoint, r);
            success &= cmsAdaptToIlluminant(&adaptedG, cmsD50_XYZ(), whitepoint, g);
            success &= cmsAdaptToIlluminant(&adaptedB, cmsD50_XYZ(), whitepoint, b);
            if (!success) {
                return nullptr;
            }
            red = QVector3D(adaptedR.X, adaptedR.Y, adaptedR.Z);
            green = QVector3D(adaptedG.X, adaptedG.Y, adaptedG.Z);
            blue = QVector3D(adaptedB.X, adaptedB.Y, adaptedB.Z);
        }
    }

    BToATagData lutData;
    if (cmsIsTag(handle, cmsSigBToD1Tag) && !cmsIsTag(handle, cmsSigBToA1Tag) && !cmsIsTag(handle, cmsSigBToA0Tag)) {
        qCWarning(KWIN_CORE, "Profiles with only BToD tags aren't supported yet");
        return nullptr;
    }
    if (cmsIsTag(handle, cmsSigBToA1Tag)) {
        // lut based profile, with relative colorimetric intent supported
        auto data = parseBToATag(handle, cmsSigBToA1Tag);
        if (data) {
            return std::make_unique<IccProfile>(handle, Colorimetry::fromXYZ(red, green, blue, white), std::move(*data), vcgt);
        } else {
            qCWarning(KWIN_CORE, "Parsing BToA1 tag failed");
            return nullptr;
        }
    }
    if (cmsIsTag(handle, cmsSigBToA0Tag)) {
        // lut based profile, with perceptual intent. The ICC docs say to use this as a fallback
        auto data = parseBToATag(handle, cmsSigBToA0Tag);
        if (data) {
            return std::make_unique<IccProfile>(handle, Colorimetry::fromXYZ(red, green, blue, white), std::move(*data), vcgt);
        } else {
            qCWarning(KWIN_CORE, "Parsing BToA0 tag failed");
            return nullptr;
        }
    }
    // matrix based profile. The matrix is already read out for the colorimetry above
    // All that's missing is the EOTF, which is stored in the rTRC, gTRC and bTRC tags
    cmsToneCurve *r = static_cast<cmsToneCurve *>(cmsReadTag(handle, cmsSigRedTRCTag));
    cmsToneCurve *g = static_cast<cmsToneCurve *>(cmsReadTag(handle, cmsSigGreenTRCTag));
    cmsToneCurve *b = static_cast<cmsToneCurve *>(cmsReadTag(handle, cmsSigBlueTRCTag));
    if (!r || !g || !b) {
        qCWarning(KWIN_CORE) << "ICC profile is missing at least one TRC tag";
        return nullptr;
    }
    cmsToneCurve *toneCurves[] = {
        cmsReverseToneCurveEx(4096, r),
        cmsReverseToneCurveEx(4096, g),
        cmsReverseToneCurveEx(4096, b),
    };
    std::vector<std::unique_ptr<ColorPipelineStage>> stages;
    stages.push_back(std::make_unique<ColorPipelineStage>(cmsStageAllocToneCurves(nullptr, 3, toneCurves)));
    const auto inverseEOTF = std::make_shared<ColorTransformation>(std::move(stages));
    return std::make_unique<IccProfile>(handle, Colorimetry::fromXYZ(red, green, blue, white), inverseEOTF, vcgt);
}

}
