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
 * Describes the meaning of encoded color values
 */
class KWINEFFECTS_EXPORT Colorspace
{
public:
    explicit Colorspace(NamedColorimetry colorimetry, NamedTransferFunction tf);
    explicit Colorspace(const Colorimetry &colorimety, NamedTransferFunction tf);

    bool operator==(const Colorspace &other) const;

    const Colorimetry &colorimetry() const;
    NamedTransferFunction transferFunction() const;

    static const Colorspace sRGB;

private:
    Colorimetry m_colorimetry;
    NamedTransferFunction m_transferFunction;
};

}
