/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <optional>

#include <QMatrix4x4>
#include <QVector2D>

#include "kwin_export.h"

namespace KWin
{

/**
 * rendering intents describe how colors should be mapped between different color spaces
 */
enum class RenderingIntent {
    /* "vendor specific", preserves the overall color appearance */
    Perceptual,
    /* "vendor specific", maps saturated colors to be saturated in the target color space too */
    // TODO Saturation,
    /* colorimetric mapping between color spaces, with whitepoint adaptation */
    RelativeColorimetric,
    /* colorimetric mapping between color spaces, without whitepoint adaptation */
    AbsoluteColorimetricNoAdaptation,
    /* colorimetric mapping between color spaces, with whitepoint adaptation and black point compensation */
    RelativeColorimetricWithBPC,
};

struct XYZ;
/**
 * xyY, with Y unspecified
 */
struct KWIN_EXPORT xy
{
    double x;
    double y;

    XYZ toXYZ() const;
    QVector2D asVector() const;
    bool operator==(const xy &other) const;
};
struct KWIN_EXPORT xyY
{
    double x;
    double y;
    double Y;

    XYZ toXYZ() const;
    bool operator==(const xyY &other) const;
};
struct KWIN_EXPORT XYZ
{
    double X;
    double Y;
    double Z;

    xyY toxyY() const;
    xy toxy() const;
    QVector3D asVector() const;
    XYZ operator*(double factor) const;
    XYZ operator/(double factor) const;
    XYZ operator+(const XYZ &other) const;
    bool operator==(const XYZ &other) const;

    static XYZ fromVector(const QVector3D &vector);
};

/**
 * Describes the definition of colors in a color space.
 * Red, green and blue define the chromaticities ("absolute colors") of the red, green and blue LEDs on a display in xy coordinates
 * White defines the the chromaticity of the reference white in xy coordinates
 */
class KWIN_EXPORT Colorimetry
{
public:
    static const Colorimetry BT709;
    static const Colorimetry PAL_M;
    static const Colorimetry PAL;
    static const Colorimetry NTSC;
    static const Colorimetry GenericFilm;
    static const Colorimetry BT2020;
    static const Colorimetry CIEXYZ;
    static const Colorimetry DCIP3;
    static const Colorimetry DisplayP3;
    static const Colorimetry AdobeRGB;

    /**
     * @returns a matrix adapting XYZ values from the source whitepoint to the destination whitepoint with the Bradford transform
     */
    static QMatrix4x4 chromaticAdaptationMatrix(XYZ sourceWhitepoint, XYZ destinationWhitepoint);

    static QMatrix4x4 calculateToXYZMatrix(XYZ red, XYZ green, XYZ blue, XYZ white);

    /**
     * checks if the colorimetry is sane and won't cause crashes or glitches
     */
    static bool isValid(xy red, xy green, xy blue, xy white);
    /**
     * checks if the colorimetry could be from a real display
     */
    static bool isReal(xy red, xy green, xy blue, xy white);

    explicit Colorimetry(XYZ red, XYZ green, XYZ blue, XYZ white);
    explicit Colorimetry(xyY red, xyY green, xyY blue, xyY white);
    explicit Colorimetry(xy red, xy green, xy blue, xy white);

    /**
     * @returns a matrix that transforms from the linear RGB representation of colors in this colorimetry to the XYZ representation
     */
    const QMatrix4x4 &toXYZ() const;
    /**
     * @returns a matrix that transforms from the XYZ representation to the linear RGB representation of colors in this colorimetry
     */
    const QMatrix4x4 &fromXYZ() const;
    QMatrix4x4 toLMS() const;
    QMatrix4x4 fromLMS() const;

    bool operator==(const Colorimetry &other) const;
    /**
     * @returns this colorimetry, adapted to the new whitepoint using the Bradford transform
     */
    Colorimetry adaptedTo(xyY newWhitepoint) const;
    /**
     * replaces the current whitepoint with the new one
     * this does not do whitepoint adaptation!
     */
    Colorimetry withWhitepoint(xyY newWhitePoint) const;
    /**
     * interpolates the primaries depending on the passed factor. The whitepoint stays unchanged
     */
    Colorimetry interpolateGamutTo(const Colorimetry &one, double factor) const;

    QMatrix4x4 relativeColorimetricTo(const Colorimetry &other) const;
    QMatrix4x4 absoluteColorimetricTo(const Colorimetry &other) const;

    const XYZ &red() const;
    const XYZ &green() const;
    const XYZ &blue() const;
    const XYZ &white() const;

private:
    XYZ m_red;
    XYZ m_green;
    XYZ m_blue;
    XYZ m_white;
    QMatrix4x4 m_toXYZ;
    QMatrix4x4 m_fromXYZ;
};

/**
 * Describes an EOTF - how encoded values are converted to light
 */
class KWIN_EXPORT TransferFunction
{
public:
    enum Type {
        sRGB = 0,
        linear = 1,
        PerceptualQuantizer = 2,
        gamma22 = 3,
        BT1886 = 4,
    };
    explicit TransferFunction(Type tf);
    explicit TransferFunction(Type tf, double minLuminance, double maxLuminance);

    bool operator==(const TransferFunction &) const;

    bool hasLinearMinLuminance() const;
    bool isRelative() const;
    TransferFunction relativeScaledTo(double referenceLuminance) const;
    double encodedToNits(double encoded) const;
    double nitsToEncoded(double nits) const;
    QVector3D encodedToNits(const QVector3D &encoded) const;
    QVector3D nitsToEncoded(const QVector3D &nits) const;
    QVector4D encodedToNits(const QVector4D &encoded) const;
    QVector4D nitsToEncoded(const QVector4D &nits) const;

    double bt1886A() const;
    double bt1886B() const;

    Type type;
    /**
     * the luminance at encoded value zero
     */
    double minLuminance;
    /**
     * the luminance at encoded value 1
     */
    double maxLuminance;

    static double defaultMinLuminanceFor(Type type);
    static double defaultMaxLuminanceFor(Type type);
    static double defaultReferenceLuminanceFor(Type type);
};

enum class YUVMatrixCoefficients {
    Identity,
    BT601,
    BT709,
    BT2020,
};

enum class EncodingRange {
    Limited,
    Full,
};

/**
 * Describes the meaning of encoded color values, with additional metadata for how to convert between different encodings
 * Note that not all properties of this description are relevant in all contexts
 */
class KWIN_EXPORT ColorDescription
{
public:
    /**
     * @param containerColorimetry the container colorimety of this description
     * @param tf the transfer function of this description
     * @param referenceLuminance the brightness of SDR content
     * @param minLuminance the minimum brightness of HDR content
     * @param maxAverageLuminance the maximum brightness of HDR content, if the whole screen is white
     * @param maxHdrLuminance the maximum brightness of HDR content, for a small part of the screen only
     * @param sdrColorimetry
     */
    explicit ColorDescription(const Colorimetry &containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance, YUVMatrixCoefficients yuvCoefficients = YUVMatrixCoefficients::Identity, EncodingRange range = EncodingRange::Full);
    explicit ColorDescription(const Colorimetry &containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance, const Colorimetry &masteringColorimetry, const Colorimetry &sdrColorimetry, YUVMatrixCoefficients yuvCoefficients = YUVMatrixCoefficients::Identity, EncodingRange range = EncodingRange::Full);
    explicit ColorDescription(const Colorimetry &containerColorimetry, TransferFunction tf, YUVMatrixCoefficients yuvCoefficients = YUVMatrixCoefficients::Identity, EncodingRange range = EncodingRange::Full);

    /**
     * The primaries and whitepoint that colors are encoded for. This is used to convert between different colorspaces.
     * In most cases this will be the rec.709 primaries for SDR, or rec.2020 for HDR
     */
    const Colorimetry &containerColorimetry() const;
    /**
     * The mastering colorimetry contains all colors that the image actually (may) contain, which can be used to improve the conversion to a different color description.
     * In most cases this will be smaller than the container colorimetry; for example a screen with an HDR mode but only rec.709 colors would have a container colorimetry of rec.2020 and a mastering colorimetry of rec.709.
     * In some cases however it can be bigger than the container colorimetry, like with scRGB. It has the container colorimetry of sRGB, but a mastering colorimetry that can be bigger (like rec.2020 for example)
     */
    const Colorimetry &masteringColorimetry() const;
    const Colorimetry &sdrColorimetry() const;
    TransferFunction transferFunction() const;
    double referenceLuminance() const;
    double minLuminance() const;
    std::optional<double> maxAverageLuminance() const;
    std::optional<double> maxHdrLuminance() const;
    YUVMatrixCoefficients yuvCoefficients() const;
    EncodingRange range() const;

    /**
     * @returns the matrix that converts from this ColorDescription's encoding to full range RGB
     * TODO move this to ColorPipeline, to deal with ICtCp
     */
    QMatrix4x4 yuvMatrix() const;

    bool operator==(const ColorDescription &other) const = default;

    std::shared_ptr<ColorDescription> withTransferFunction(const TransferFunction &func) const;
    /**
     * replaces the current whitepoint with the new one
     * this does not do whitepoint adaptation!
     */
    std::shared_ptr<ColorDescription> withWhitepoint(xyY newWhitePoint) const;
    std::shared_ptr<ColorDescription> dimmed(double brightnessFactor) const;
    std::shared_ptr<ColorDescription> withReference(double referenceLuminance) const;
    std::shared_ptr<ColorDescription> withHdrMetadata(double maxAverageLuminance, double maxLuminance) const;
    std::shared_ptr<ColorDescription> withYuvCoefficients(YUVMatrixCoefficients coefficient, EncodingRange range) const;

    /**
     * @returns a matrix that transforms from linear RGB in this color description to linear RGB in the other one
     */
    QMatrix4x4 toOther(const ColorDescription &other, RenderingIntent intent) const;
    QVector3D mapTo(QVector3D rgb, const ColorDescription &other, RenderingIntent intent) const;

    /**
     * This color description describes display-referred sRGB, with a gamma22 transfer function
     */
    static const std::shared_ptr<ColorDescription> sRGB;
    static const std::shared_ptr<ColorDescription> BT2020PQ;

private:
    Colorimetry m_containerColorimetry;
    Colorimetry m_masteringColorimetry;
    TransferFunction m_transferFunction;
    Colorimetry m_sdrColorimetry;
    double m_referenceLuminance;
    double m_minLuminance;
    std::optional<double> m_maxAverageLuminance;
    std::optional<double> m_maxHdrLuminance;
    YUVMatrixCoefficients m_yuvCoefficients = YUVMatrixCoefficients::Identity;
    EncodingRange m_range = EncodingRange::Full;
};
}

inline bool operator==(const std::shared_ptr<KWin::ColorDescription> &left, const std::shared_ptr<KWin::ColorDescription> &right)
{
    if (!left || !right) {
        return left.get() == right.get();
    }
    return left.get() == right.get() || *left == *right;
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::TransferFunction &tf);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::XYZ &xyz);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::xyY &xyY);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::xy &xy);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::Colorimetry &color);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::ColorDescription &color);
