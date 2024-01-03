/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <optional>

#include <QMatrix3x3>
#include <QVector2D>

#include "kwin_export.h"

namespace KWin
{

enum class NamedColorimetry {
    BT709,
    BT2020,
};

/**
 * Describes the definition of colors in a color space.
 * Red, green and blue define the chromaticities ("absolute colors") of the red, green and blue LEDs on a display in xy coordinates
 * White defines the the chromaticity of the reference white in xy coordinates
 */
class KWIN_EXPORT Colorimetry
{
public:
    static Colorimetry fromName(NamedColorimetry name);
    static Colorimetry fromXYZ(QVector3D red, QVector3D green, QVector3D blue, QVector3D white);
    /**
     * @returns the XYZ representation of the xyY color passed in. Y is assumed to be one
     */
    static QVector3D xyToXYZ(QVector2D xy);
    /**
     * @returns the xyY representation of the XYZ color passed in. Y is normalized to be one
     */
    static QVector2D xyzToXY(QVector3D xyz);
    /**
     * @returns a matrix adapting XYZ values from the source whitepoint to the destination whitepoint with the Bradford transform
     */
    static QMatrix3x3 chromaticAdaptationMatrix(QVector2D sourceWhitepoint, QVector2D destinationWhitepoint);

    /**
     * @returns a matrix that transforms from the linear RGB representation of colors in this colorimetry to the XYZ representation
     */
    QMatrix3x3 toXYZ() const;
    /**
     * @returns a matrix that transforms from linear RGB in this colorimetry to linear RGB in the other colorimetry
     * the rendering intent is relative colorimetric
     */
    QMatrix3x3 toOther(const Colorimetry &colorimetry) const;
    bool operator==(const Colorimetry &other) const;
    /**
     * @returns this colorimetry, adapted to the new whitepoint using the Bradford transform
     */
    Colorimetry adaptedTo(QVector2D newWhitepoint) const;

    Colorimetry()
        : m_red({})
        , m_green({})
        , m_blue({})
        , m_white({})
        , m_name({})
    {
    }
    Colorimetry(QVector2D red,
                QVector2D green,
                QVector2D blue,
                QVector2D white,
                std::optional<NamedColorimetry> name = std::nullopt)
        : m_red(red)
        , m_green(green)
        , m_blue(blue)
        , m_white(white)
        , m_name(name)
    {
        Q_ASSERT(white.y() != 0);
    }

    bool isValid() const
    {
        return m_white.has_value();
    }

    QVector2D red() const
    {
        return m_red;
    }
    QVector2D green() const
    {
        return m_green;
    }
    QVector2D blue() const
    {
        return m_blue;
    }
    QVector2D white() const
    {
        Q_ASSERT(m_white.has_value());
        return m_white.value();
    }
    std::optional<NamedColorimetry> name() const
    {
        return m_name;
    }

private:
    QVector2D m_red;
    QVector2D m_green;
    QVector2D m_blue;
    std::optional<QVector2D> m_white;
    std::optional<NamedColorimetry> m_name;
};

/**
 * Describes an EOTF, that is, how encoded brightness values are converted to light
 */
enum class NamedTransferFunction {
    sRGB = 0,
    linear = 1,
    PerceptualQuantizer = 2,
    scRGB = 3,
    gamma22 = 4,
};

/**
 * Describes the meaning of encoded color values, with additional metadata for how to convert between different encodings
 * Note that not all properties of this description are relevant in all contexts
 */
class KWIN_EXPORT ColorDescription
{
public:
    /**
     * @param colorimety the colorimety of this description
     * @param tf the transfer function of this description
     * @param sdrBrightness the brightness of SDR content
     * @param minHdrBrightness the minimum brightness of HDR content
     * @param maxFrameAverageBrightness the maximum brightness of HDR content, if the whole screen is white
     * @param maxHdrHighlightBrightness the maximum brightness of HDR content, for a small part of the screen only
     * @param sdrGamutWideness the gamut wideness of sRGB content; 0 is rec.709, 1 is rec.2020, everything in between is interpolated
     */
    explicit ColorDescription(const Colorimetry &colorimety, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxFrameAverageBrightness, double maxHdrHighlightBrightness, double sdrGamutWideness);
    explicit ColorDescription(NamedColorimetry colorimetry, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxFrameAverageBrightness, double maxHdrHighlightBrightness, double sdrGamutWideness);

    const Colorimetry &colorimetry() const;
    const Colorimetry &sdrColorimetry() const;
    NamedTransferFunction transferFunction() const;
    double sdrBrightness() const;
    double minHdrBrightness() const;
    double maxFrameAverageBrightness() const;
    double maxHdrHighlightBrightness() const;
    double sdrGamutWideness() const;

    bool operator==(const ColorDescription &other) const;

    QVector3D mapTo(QVector3D rgb, const ColorDescription &other) const;

    /**
     * This color description describes display-referred sRGB, with a gamma22 transfer function
     */
    //     static const ColorDescription sRGB;
    static ColorDescription sRGBf();

    bool isValid() const
    {
        return m_colorimetry.isValid() && m_sdrColorimetry.isValid();
    }

    ColorDescription(const ColorDescription &ret);
    ColorDescription operator=(const ColorDescription &other);

private:
    Colorimetry m_colorimetry;
    NamedTransferFunction m_transferFunction;
    Colorimetry m_sdrColorimetry;
    double m_sdrGamutWideness;
    double m_sdrBrightness;
    double m_minHdrBrightness;
    double m_maxFrameAverageBrightness;
    double m_maxHdrHighlightBrightness;
};
}
