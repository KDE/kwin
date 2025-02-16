/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "iccprofile.h"
#include "colorlut3d.h"
#include "colorpipelinestage.h"
#include "colortransformation.h"
#include "utils/common.h"

#include <KLocalizedString>
#include <QFileInfo>
#include <lcms2.h>
#include <span>
#include <tuple>

namespace KWin
{

static const Colorimetry CIEXYZD50 = Colorimetry{
    XYZ{1.0, 0.0, 0.0},
    XYZ{0.0, 1.0, 0.0},
    XYZ{0.0, 0.0, 1.0},
    XYZ(0.9642, 1.0, 0.8249),
};

const ColorDescription IccProfile::s_connectionSpace = ColorDescription(CIEXYZD50, TransferFunction(TransferFunction::linear, 0, 1), 1, 0, 1, 1);

IccProfile::IccProfile(cmsHPROFILE handle, const Colorimetry &colorimetry, std::optional<ColorPipeline> &&bToA0Tag, std::optional<ColorPipeline> &&bToA1Tag, const std::shared_ptr<ColorTransformation> &inverseEOTF, const std::shared_ptr<ColorTransformation> &vcgt, std::optional<double> minBrightness, std::optional<double> maxBrightness)
    : m_handle(handle)
    , m_colorimetry(colorimetry)
    , m_bToA0Tag(std::move(bToA0Tag))
    , m_bToA1Tag(std::move(bToA1Tag))
    , m_inverseEOTF(inverseEOTF)
    , m_vcgt(vcgt)
    , m_minBrightness(minBrightness)
    , m_maxBrightness(maxBrightness)
{
}

IccProfile::~IccProfile()
{
    cmsCloseProfile(m_handle);
}

std::optional<double> IccProfile::minBrightness() const
{
    return m_minBrightness;
}

std::optional<double> IccProfile::maxBrightness() const
{
    return m_maxBrightness;
}

const Colorimetry &IccProfile::colorimetry() const
{
    return m_colorimetry;
}

std::shared_ptr<ColorTransformation> IccProfile::inverseTransferFunction() const
{
    return m_inverseEOTF;
}

std::shared_ptr<ColorTransformation> IccProfile::vcgt() const
{
    return m_vcgt;
}

const ColorPipeline *IccProfile::BToATag(RenderingIntent intent) const
{
    switch (intent) {
    case RenderingIntent::Perceptual:
        return m_bToA0Tag ? &*m_bToA0Tag : nullptr;
    case RenderingIntent::RelativeColorimetric:
        // these two are different from relative colorimetric
        // but that has to be handled before the tag is applied
    case RenderingIntent::RelativeColorimetricWithBPC:
    case RenderingIntent::AbsoluteColorimetric:
        return m_bToA1Tag ? &*m_bToA1Tag : nullptr;
    }
    Q_UNREACHABLE();
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
    QMatrix4x4 ret;
    ret(0, 0) = floats[0];
    ret(0, 1) = floats[1];
    ret(0, 2) = floats[2];
    ret(1, 0) = floats[3];
    ret(1, 1) = floats[4];
    ret(1, 2) = floats[5];
    ret(2, 0) = floats[6];
    ret(2, 1) = floats[7];
    ret(2, 2) = floats[8];
    if (hasOffset) {
        ret(0, 3) = floats[9];
        ret(1, 3) = floats[10];
        ret(2, 3) = floats[11];
    }
    return ret;
}

static std::optional<ColorPipeline> parseBToATag(cmsHPROFILE profile, cmsTagSignature tag)
{
    cmsPipeline *bToAPipeline = static_cast<cmsPipeline *>(cmsReadTag(profile, tag));
    if (!bToAPipeline) {
        return std::nullopt;
    }
    ColorPipeline ret;
    // ICC profiles assume you're working in their encoding of XYZ
    // this multiplier converts from our [0, 1] encoding to the ICC one
    ret.addMultiplier(65536.0 / (2 * 65535.0));
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
            auto transformation = std::make_shared<ColorTransformation>(std::move(stages));
            ret.add(ColorOp{
                .input = ValueRange(),
                .operation = transformation,
                .output = ValueRange(),
            });
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
            ret.add(ColorOp{
                .input = ValueRange{},
                .operation = ColorMatrix(*mat),
                .output = ValueRange{},
            });
        }; break;
        case cmsStageSignature::cmsSigCLutElemType: {
            const auto size = parseBToACLUTSize(data);
            if (!size) {
                return std::nullopt;
            }
            const auto [x, y, z] = *size;
            std::vector<std::unique_ptr<ColorPipelineStage>> stages;
            stages.push_back(std::make_unique<ColorPipelineStage>(cmsStageDup(stage)));
            ret.add(ColorOp{
                .input = ValueRange{},
                .operation = std::make_shared<ColorLUT3D>(std::make_unique<ColorTransformation>(std::move(stages)), x, y, z),
                .output = ValueRange{},
            });
        } break;
        default:
            qCWarning(KWIN_CORE, "unknown stage type %u", stageType);
            return std::nullopt;
        }
    }
    return ret;
}

static constexpr XYZ D50{
    .X = 0.9642,
    .Y = 1.0,
    .Z = 0.8249,
};

std::expected<std::unique_ptr<IccProfile>, QString> IccProfile::load(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }
    cmsHPROFILE handle = cmsOpenProfileFromFile(path.toUtf8(), "r");
    if (!handle) {
        if (QFileInfo::exists(path)) {
            return std::unexpected(i18n("Failed to open ICC profile \"%1\"", path));
        } else {
            return std::unexpected(i18n("ICC profile \"%1\" doesn't exist", path));
        }
    }
    if (cmsGetDeviceClass(handle) != cmsSigDisplayClass) {
        return std::unexpected(i18n("ICC profile \"%1\" is not usable for displays", path));
    }
    if (cmsGetPCS(handle) != cmsColorSpaceSignature::cmsSigXYZData) {
        return std::unexpected(i18n("ICC profile \"%1\" has unsupported connection space, only XYZ is supported", path));
    }
    if (cmsGetColorSpace(handle) != cmsColorSpaceSignature::cmsSigRgbData) {
        return std::unexpected(i18n("ICC profile \"%1\" is broken, input/output color space isn't RGB", path));
    }

    std::shared_ptr<ColorTransformation> vcgt;
    cmsToneCurve **vcgtTag = static_cast<cmsToneCurve **>(cmsReadTag(handle, cmsSigVcgtTag));
    if (vcgtTag && vcgtTag[0]) {
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
        return std::unexpected(i18n("ICC profile \"%1\" is broken, it has no whitepoint", path));
    }
    if (whitepoint->Y == 0) {
        return std::unexpected(i18n("ICC profile \"%1\" is broken, its whitepoint is invalid", path));
    }

    XYZ red;
    XYZ green;
    XYZ blue;
    XYZ white = XYZ{whitepoint->X, whitepoint->Y, whitepoint->Z};
    std::optional<QMatrix4x4> chromaticAdaptationMatrix;
    if (cmsIsTag(handle, cmsSigChromaticAdaptationTag)) {
        // the chromatic adaptation tag is a 3x3 matrix that converts from the actual whitepoint to D50
        const auto data = readTagRaw(handle, cmsSigChromaticAdaptationTag);
        const auto mat = parseMatrix(std::span(data).subspan(8), false);
        if (!mat) {
            return std::unexpected(i18n("ICC profile \"%1\" is broken, parsing chromatic adaptation matrix failed", path));
        }
        bool invertable = false;
        chromaticAdaptationMatrix = mat->inverted(&invertable);
        if (!invertable) {
            return std::unexpected(i18n("ICC profile \"%1\" is broken, inverting chromatic adaptation matrix failed", path));
        }
        white = XYZ::fromVector(*chromaticAdaptationMatrix * D50.asVector());
    }
    if (cmsCIExyYTRIPLE *chrmTag = static_cast<cmsCIExyYTRIPLE *>(cmsReadTag(handle, cmsSigChromaticityTag))) {
        red = xyY{chrmTag->Red.x, chrmTag->Red.y, chrmTag->Red.Y}.toXYZ();
        green = xyY{chrmTag->Green.x, chrmTag->Green.y, chrmTag->Green.Y}.toXYZ();
        blue = xyY{chrmTag->Blue.x, chrmTag->Blue.y, chrmTag->Blue.Y}.toXYZ();
    } else {
        const cmsCIEXYZ *r = static_cast<cmsCIEXYZ *>(cmsReadTag(handle, cmsSigRedColorantTag));
        const cmsCIEXYZ *g = static_cast<cmsCIEXYZ *>(cmsReadTag(handle, cmsSigGreenColorantTag));
        const cmsCIEXYZ *b = static_cast<cmsCIEXYZ *>(cmsReadTag(handle, cmsSigBlueColorantTag));
        if (!r || !g || !b) {
            return std::unexpected(i18n("ICC profile \"%1\" is broken, it has no primaries", path));
        }
        if (chromaticAdaptationMatrix) {
            red = XYZ::fromVector(*chromaticAdaptationMatrix * QVector3D(r->X, r->Y, r->Z));
            green = XYZ::fromVector(*chromaticAdaptationMatrix * QVector3D(g->X, g->Y, g->Z));
            blue = XYZ::fromVector(*chromaticAdaptationMatrix * QVector3D(b->X, b->Y, b->Z));
        } else {
            // if the chromatic adaptation tag isn't available, fall back to using the media whitepoint instead
            cmsCIEXYZ adaptedR{};
            cmsCIEXYZ adaptedG{};
            cmsCIEXYZ adaptedB{};
            bool success = cmsAdaptToIlluminant(&adaptedR, cmsD50_XYZ(), whitepoint, r);
            success &= cmsAdaptToIlluminant(&adaptedG, cmsD50_XYZ(), whitepoint, g);
            success &= cmsAdaptToIlluminant(&adaptedB, cmsD50_XYZ(), whitepoint, b);
            if (!success) {
                return std::unexpected(i18n("ICC profile \"%1\" is broken, couldn't calculate its primaries", path));
            }
            red = XYZ(adaptedR.X, adaptedR.Y, adaptedR.Z);
            green = XYZ(adaptedG.X, adaptedG.Y, adaptedG.Z);
            blue = XYZ(adaptedB.X, adaptedB.Y, adaptedB.Z);
        }
    }

    if (red.Y == 0 || green.Y == 0 || blue.Y == 0 || white.Y == 0) {
        return std::unexpected(i18n("ICC profile \"%1\" is broken, its primaries are invalid", path));
    }

    std::optional<double> minBrightness;
    std::optional<double> maxBrightness;
    if (cmsCIEXYZ *luminance = static_cast<cmsCIEXYZ *>(cmsReadTag(handle, cmsSigLuminanceTag))) {
        // for some reason, lcms exposes the luminance as a XYZ triple...
        // only Y is non-zero, and it's the brightness in nits
        maxBrightness = luminance->Y;
        cmsCIEXYZ blackPoint;
        if (cmsDetectDestinationBlackPoint(&blackPoint, handle, INTENT_RELATIVE_COLORIMETRIC, 0)) {
            minBrightness = blackPoint.Y * luminance->Y;
        }
    }

    if (cmsIsTag(handle, cmsSigBToD1Tag) && !cmsIsTag(handle, cmsSigBToA1Tag) && !cmsIsTag(handle, cmsSigBToA0Tag)) {
        return std::unexpected(i18n("ICC profile \"%1\" with only BToD tags isn't supported", path));
    }
    std::optional<ColorPipeline> bToA0;
    std::optional<ColorPipeline> bToA1;
    if (cmsIsTag(handle, cmsSigBToA0Tag)) {
        bToA0 = parseBToATag(handle, cmsSigBToA0Tag);
    }
    if (cmsIsTag(handle, cmsSigBToA1Tag)) {
        bToA1 = parseBToATag(handle, cmsSigBToA1Tag);
    }
    constexpr size_t trcSize = 4096;
    std::array<cmsToneCurve *, 3> toneCurves;
    if (bToA0 || bToA1) {
        // the TRC tags are often nonsense when the BToA tag exists, so this estimates the
        // inverse transfer function by doing a grayscale transform on the BToA tag instead
        const QMatrix4x4 toXYZD50 = Colorimetry::chromaticAdaptationMatrix(white, D50) * Colorimetry(red, green, blue, white).toXYZ();
        ColorPipeline pipeline;
        pipeline.addMatrix(toXYZD50, ValueRange{});
        pipeline.add(bToA1 ? *bToA1 : *bToA0);
        std::array<float, trcSize> red;
        std::array<float, trcSize> green;
        std::array<float, trcSize> blue;
        for (size_t i = 0; i < trcSize; i++) {
            const float relativeI = i / float(trcSize - 1);
            const QVector3D result = pipeline.evaluate(QVector3D{relativeI, relativeI, relativeI});
            red[i] = result.x();
            green[i] = result.y();
            blue[i] = result.z();
        }
        toneCurves = {
            cmsBuildTabulatedToneCurveFloat(nullptr, trcSize, red.data()),
            cmsBuildTabulatedToneCurveFloat(nullptr, trcSize, green.data()),
            cmsBuildTabulatedToneCurveFloat(nullptr, trcSize, blue.data()),
        };
    } else {
        cmsToneCurve *r = static_cast<cmsToneCurve *>(cmsReadTag(handle, cmsSigRedTRCTag));
        cmsToneCurve *g = static_cast<cmsToneCurve *>(cmsReadTag(handle, cmsSigGreenTRCTag));
        cmsToneCurve *b = static_cast<cmsToneCurve *>(cmsReadTag(handle, cmsSigBlueTRCTag));
        if (!r || !g || !b) {
            return std::unexpected(i18n("Color profile is missing TRC tags"));
        }
        toneCurves = {
            cmsReverseToneCurveEx(trcSize, r),
            cmsReverseToneCurveEx(trcSize, g),
            cmsReverseToneCurveEx(trcSize, b),
        };
    }
    std::vector<std::unique_ptr<ColorPipelineStage>> stages;
    stages.push_back(std::make_unique<ColorPipelineStage>(cmsStageAllocToneCurves(nullptr, toneCurves.size(), toneCurves.data())));
    const auto inverseEOTF = std::make_shared<ColorTransformation>(std::move(stages));
    return std::make_unique<IccProfile>(handle, Colorimetry(red, green, blue, white), std::move(bToA0), std::move(bToA1), inverseEOTF, vcgt, minBrightness, maxBrightness);
}

}
