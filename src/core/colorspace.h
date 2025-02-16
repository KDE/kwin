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
    AbsoluteColorimetric,
    /* colorimetric mapping between color spaces, with whitepoint adaptation and black point compensation */
    RelativeColorimetricWithBPC,
};

enum class NamedColorimetry {
    BT709,
    PAL_M,
    PAL,
    NTSC,
    GenericFilm,
    BT2020,
    CIEXYZ,
    DCIP3,
    DisplayP3,
    AdobeRGB
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
    static const Colorimetry &fromName(NamedColorimetry name);
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
    bool operator==(NamedColorimetry name) const;
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

    const XYZ &red() const;
    const XYZ &green() const;
    const XYZ &blue() const;
    const XYZ &white() const;

    std::optional<NamedColorimetry> name() const;

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
    };
    explicit TransferFunction(Type tf);
    explicit TransferFunction(Type tf, double minLuminance, double maxLuminance);

    bool operator==(const TransferFunction &) const;

    bool isRelative() const;
    TransferFunction relativeScaledTo(double referenceLuminance) const;
    double encodedToNits(double encoded) const;
    double nitsToEncoded(double nits) const;
    QVector3D encodedToNits(const QVector3D &encoded) const;
    QVector3D nitsToEncoded(const QVector3D &nits) const;
    QVector4D encodedToNits(const QVector4D &encoded) const;
    QVector4D nitsToEncoded(const QVector4D &nits) const;

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
    explicit ColorDescription(const Colorimetry &containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance);
    explicit ColorDescription(NamedColorimetry containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance);
    explicit ColorDescription(const Colorimetry &containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance, std::optional<Colorimetry> masteringColorimetry, const Colorimetry &sdrColorimetry);
    explicit ColorDescription(NamedColorimetry containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance, std::optional<Colorimetry> masteringColorimetry, const Colorimetry &sdrColorimetry);

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
    const std::optional<Colorimetry> &masteringColorimetry() const;
    const Colorimetry &sdrColorimetry() const;
    TransferFunction transferFunction() const;
    double referenceLuminance() const;
    double minLuminance() const;
    std::optional<double> maxAverageLuminance() const;
    std::optional<double> maxHdrLuminance() const;

    bool operator==(const ColorDescription &other) const = default;

    ColorDescription withTransferFunction(const TransferFunction &func) const;
    /**
     * replaces the current whitepoint with the new one
     * this does not do whitepoint adaptation!
     */
    ColorDescription withWhitepoint(xyY newWhitePoint) const;
    ColorDescription dimmed(double brightnessFactor) const;

    /**
     * @returns a matrix that transforms from linear RGB in this color description to linear RGB in the other one
     */
    QMatrix4x4 toOther(const ColorDescription &other, RenderingIntent intent) const;
    QVector3D mapTo(QVector3D rgb, const ColorDescription &other, RenderingIntent intent) const;

    /**
     * This color description describes display-referred sRGB, with a gamma22 transfer function
     */
    static const ColorDescription sRGB;

private:
    Colorimetry m_containerColorimetry;
    std::optional<Colorimetry> m_masteringColorimetry;
    TransferFunction m_transferFunction;
    Colorimetry m_sdrColorimetry;
    double m_referenceLuminance;
    double m_minLuminance;
    std::optional<double> m_maxAverageLuminance;
    std::optional<double> m_maxHdrLuminance;
};
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::TransferFunction &tf);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::XYZ &xyz);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::xyY &xyY);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::xy &xy);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::Colorimetry &color);
KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::ColorDescription &color);
