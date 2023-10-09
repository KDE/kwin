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
    static constexpr Colorimetry fromName(NamedColorimetry name);
    static Colorimetry fromXYZ(QVector3D red, QVector3D green, QVector3D blue, QVector3D white);
    static QVector3D xyToXYZ(QVector2D xy);
    static QVector2D xyzToXY(QVector3D xyz);

    QMatrix3x3 toXYZ() const;
    QMatrix3x3 toOther(const Colorimetry &colorimetry) const;
    bool operator==(const Colorimetry &other) const;

    QVector2D red;
    QVector2D green;
    QVector2D blue;
    QVector2D white;
    std::optional<NamedColorimetry> name;
};

/**
 * Describes an EOTF, that is, how encoded brightness values are converted to light
 */
enum class NamedTransferFunction {
    sRGB = 0,
    linear = 1,
    PerceptualQuantizer = 2,
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
     */
    explicit ColorDescription(const Colorimetry &colorimety, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxFrameAverageBrightness, double maxHdrHighlightBrightness);
    explicit ColorDescription(NamedColorimetry colorimetry, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxFrameAverageBrightness, double maxHdrHighlightBrightness);

    const Colorimetry &colorimetry() const;
    NamedTransferFunction transferFunction() const;
    double sdrBrightness() const;
    double minHdrBrightness() const;
    double maxFrameAverageBrightness() const;
    double maxHdrHighlightBrightness() const;

    bool operator==(const ColorDescription &other) const;

    QVector3D mapTo(QVector3D rgb, const ColorDescription &other) const;

    static const ColorDescription sRGB;

private:
    Colorimetry m_colorimetry;
    NamedTransferFunction m_transferFunction;
    double m_sdrBrightness;
    double m_minHdrBrightness;
    double m_maxFrameAverageBrightness;
    double m_maxHdrHighlightBrightness;
};
}
