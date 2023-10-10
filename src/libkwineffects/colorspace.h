/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <optional>

#include <QMatrix3x3>
#include <QVector2D>

#include "libkwineffects/kwineffects_export.h"

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
class KWINEFFECTS_EXPORT Colorimetry
{
public:
    static constexpr Colorimetry createFromName(NamedColorimetry name);

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
class KWINEFFECTS_EXPORT ColorDescription
{
public:
    /**
     * @param colorimety the colorimety of this description
     * @param tf the transfer function of this description
     * @param sdrBrightness the brightness of SDR content
     * @param minHdrBrightness the minimum brightness of HDR content
     * @param maxHdrBrightness the maximum brightness of HDR content, if the whole screen is white
     * @param maxHdrHighlightBrightness the maximum brightness of HDR content, for a small part of the screen only
     */
    explicit ColorDescription(const Colorimetry &colorimety, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxHdrBrightness, double maxHdrHighlightBrightness);
    explicit ColorDescription(NamedColorimetry colorimetry, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxHdrBrightness, double maxHdrHighlightBrightness);

    const Colorimetry &colorimetry() const;
    NamedTransferFunction transferFunction() const;
    double sdrBrightness() const;
    double minHdrBrightness() const;
    double maxHdrBrightness() const;
    double maxHdrHighlightBrightness() const;

    bool operator==(const ColorDescription &other) const;

    QVector3D mapTo(QVector3D rgb, const ColorDescription &other) const;

    static const ColorDescription sRGB;

private:
    Colorimetry m_colorimetry;
    NamedTransferFunction m_transferFunction;
    double m_sdrBrightness;
    double m_minHdrBrightness;
    double m_maxHdrBrightness;
    double m_maxHdrHighlightBrightness;
};
}
