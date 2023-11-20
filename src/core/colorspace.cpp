/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorspace.h"

#include <qassert.h>

namespace KWin
{

static QMatrix3x3 inverse(const QMatrix3x3 &m)
{
    const double determinant = m(0, 0) * (m(1, 1) * m(2, 2) - m(2, 1) * m(1, 2)) - m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) + m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
    QMatrix3x3 ret;
    ret(0, 0) = (m(1, 1) * m(2, 2) - m(2, 1) * m(1, 2)) / determinant;
    ret(0, 1) = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) / determinant;
    ret(0, 2) = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) / determinant;
    ret(1, 0) = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) / determinant;
    ret(1, 1) = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) / determinant;
    ret(1, 2) = (m(1, 0) * m(0, 2) - m(0, 0) * m(1, 2)) / determinant;
    ret(2, 0) = (m(1, 0) * m(2, 1) - m(2, 0) * m(1, 1)) / determinant;
    ret(2, 1) = (m(2, 0) * m(0, 1) - m(0, 0) * m(2, 1)) / determinant;
    ret(2, 2) = (m(0, 0) * m(1, 1) - m(1, 0) * m(0, 1)) / determinant;
    return ret;
}

static QMatrix3x3 matrixFromColumns(const QVector3D &first, const QVector3D &second, const QVector3D &third)
{
    QMatrix3x3 ret;
    ret(0, 0) = first.x();
    ret(1, 0) = first.y();
    ret(2, 0) = first.z();
    ret(0, 1) = second.x();
    ret(1, 1) = second.y();
    ret(2, 1) = second.z();
    ret(0, 2) = third.x();
    ret(1, 2) = third.y();
    ret(2, 2) = third.z();
    return ret;
}

static QVector3D operator*(const QMatrix3x3 &mat, const QVector3D &v)
{
    return QVector3D(
        mat(0, 0) * v.x() + mat(0, 1) * v.y() + mat(0, 2) * v.z(),
        mat(1, 0) * v.x() + mat(1, 1) * v.y() + mat(1, 2) * v.z(),
        mat(2, 0) * v.x() + mat(2, 1) * v.y() + mat(2, 2) * v.z());
}

QVector3D Colorimetry::xyToXYZ(QVector2D xy)
{
    return QVector3D(xy.x() / xy.y(), 1, (1 - xy.x() - xy.y()) / xy.y());
}

QVector2D Colorimetry::xyzToXY(QVector3D xyz)
{
    xyz /= xyz.y();
    return QVector2D(xyz.x() / (xyz.x() + xyz.y() + xyz.z()), xyz.y() / (xyz.x() + xyz.y() + xyz.z()));
}

QMatrix3x3 Colorimetry::toXYZ() const
{
    const auto r_xyz = xyToXYZ(red);
    const auto g_xyz = xyToXYZ(green);
    const auto b_xyz = xyToXYZ(blue);
    const auto w_xyz = xyToXYZ(white);
    const auto component_scale = inverse(matrixFromColumns(r_xyz, g_xyz, b_xyz)) * w_xyz;
    return matrixFromColumns(r_xyz * component_scale.x(), g_xyz * component_scale.y(), b_xyz * component_scale.z());
}

QMatrix3x3 Colorimetry::chromaticAdaptationMatrix(QVector2D sourceWhitepoint, QVector2D destinationWhitepoint)
{
    static const QMatrix3x3 bradford = []() {
        QMatrix3x3 ret;
        ret(0, 0) = 0.8951;
        ret(0, 1) = 0.2664;
        ret(0, 2) = -0.1614;
        ret(1, 0) = -0.7502;
        ret(1, 1) = 1.7135;
        ret(1, 2) = 0.0367;
        ret(2, 0) = 0.0389;
        ret(2, 1) = -0.0685;
        ret(2, 2) = 1.0296;
        return ret;
    }();
    static const QMatrix3x3 inverseBradford = []() {
        QMatrix3x3 ret;
        ret(0, 0) = 0.9869929;
        ret(0, 1) = -0.1470543;
        ret(0, 2) = 0.1599627;
        ret(1, 0) = 0.4323053;
        ret(1, 1) = 0.5183603;
        ret(1, 2) = 0.0492912;
        ret(2, 0) = -0.0085287;
        ret(2, 1) = 0.0400428;
        ret(2, 2) = 0.9684867;
        return ret;
    }();
    const QVector3D factors = (bradford * xyToXYZ(destinationWhitepoint)) / (bradford * xyToXYZ(sourceWhitepoint));
    QMatrix3x3 adaptation{};
    adaptation(0, 0) = factors.x();
    adaptation(1, 1) = factors.y();
    adaptation(2, 2) = factors.z();
    return inverseBradford * adaptation * bradford;
}

QMatrix3x3 Colorimetry::toOther(const Colorimetry &other) const
{
    // rendering intent is relative colorimetric, so adapt to the different whitepoint
    if (white == other.white) {
        return inverse(other.toXYZ()) * toXYZ();
    } else {
        return inverse(other.toXYZ()) * chromaticAdaptationMatrix(this->white, other.white) * toXYZ();
    }
}

Colorimetry Colorimetry::adaptedTo(QVector2D newWhitepoint) const
{
    const auto mat = chromaticAdaptationMatrix(this->white, newWhitepoint);
    return Colorimetry{
        .red = xyzToXY(mat * xyToXYZ(red)),
        .green = xyzToXY(mat * xyToXYZ(green)),
        .blue = xyzToXY(mat * xyToXYZ(blue)),
        .white = newWhitepoint,
        .name = std::nullopt,
    };
}

bool Colorimetry::operator==(const Colorimetry &other) const
{
    return (name || other.name) ? (name == other.name)
                                : (red == other.red && green == other.green && blue == other.blue && white == other.white);
}

static const Colorimetry BT709 = Colorimetry{
    .red = {0.64, 0.33},
    .green = {0.30, 0.60},
    .blue = {0.15, 0.06},
    .white = {0.3127, 0.3290},
    .name = NamedColorimetry::BT709,
};

static const Colorimetry BT2020 = Colorimetry{
    .red = {0.708, 0.292},
    .green = {0.170, 0.797},
    .blue = {0.131, 0.046},
    .white = {0.3127, 0.3290},
    .name = NamedColorimetry::BT2020,
};

Colorimetry Colorimetry::fromName(NamedColorimetry name)
{
    switch (name) {
    case NamedColorimetry::BT709:
        return BT709;
    case NamedColorimetry::BT2020:
        return BT2020;
    }
    Q_UNREACHABLE();
}

Colorimetry Colorimetry::fromXYZ(QVector3D red, QVector3D green, QVector3D blue, QVector3D white)
{
    return Colorimetry{
        .red = xyzToXY(red),
        .green = xyzToXY(green),
        .blue = xyzToXY(blue),
        .white = xyzToXY(white),
        .name = std::nullopt,
    };
}

const ColorDescription ColorDescription::sRGB = ColorDescription(NamedColorimetry::BT709, NamedTransferFunction::sRGB, 100, 0, 100, 100, 0);

static Colorimetry sRGBColorimetry(double factor)
{
    return Colorimetry{
        .red = BT709.red * (1 - factor) + BT2020.red * factor,
        .green = BT709.green * (1 - factor) + BT2020.green * factor,
        .blue = BT709.blue * (1 - factor) + BT2020.blue * factor,
        .white = BT709.white, // whitepoint is the same
    };
}

ColorDescription::ColorDescription(const Colorimetry &colorimety, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxFrameAverageBrightness, double maxHdrHighlightBrightness, double sdrGamutWideness)
    : m_colorimetry(colorimety)
    , m_transferFunction(tf)
    , m_sdrColorimetry(sRGBColorimetry(sdrGamutWideness))
    , m_sdrGamutWideness(sdrGamutWideness)
    , m_sdrBrightness(sdrBrightness)
    , m_minHdrBrightness(minHdrBrightness)
    , m_maxFrameAverageBrightness(maxFrameAverageBrightness)
    , m_maxHdrHighlightBrightness(maxHdrHighlightBrightness)
{
}

ColorDescription::ColorDescription(NamedColorimetry colorimetry, NamedTransferFunction tf, double sdrBrightness, double minHdrBrightness, double maxFrameAverageBrightness, double maxHdrHighlightBrightness, double sdrGamutWideness)
    : m_colorimetry(Colorimetry::fromName(colorimetry))
    , m_transferFunction(tf)
    , m_sdrColorimetry(sRGBColorimetry(sdrGamutWideness))
    , m_sdrGamutWideness(sdrGamutWideness)
    , m_sdrBrightness(sdrBrightness)
    , m_minHdrBrightness(minHdrBrightness)
    , m_maxFrameAverageBrightness(maxFrameAverageBrightness)
    , m_maxHdrHighlightBrightness(maxHdrHighlightBrightness)
{
}

const Colorimetry &ColorDescription::colorimetry() const
{
    return m_colorimetry;
}

const Colorimetry &ColorDescription::sdrColorimetry() const
{
    return m_sdrColorimetry;
}

NamedTransferFunction ColorDescription::transferFunction() const
{
    return m_transferFunction;
}

double ColorDescription::sdrBrightness() const
{
    return m_sdrBrightness;
}

double ColorDescription::minHdrBrightness() const
{
    return m_minHdrBrightness;
}

double ColorDescription::maxFrameAverageBrightness() const
{
    return m_maxFrameAverageBrightness;
}

double ColorDescription::maxHdrHighlightBrightness() const
{
    return m_maxHdrHighlightBrightness;
}

double ColorDescription::sdrGamutWideness() const
{
    return m_sdrGamutWideness;
}

bool ColorDescription::operator==(const ColorDescription &other) const
{
    return m_colorimetry == other.m_colorimetry
        && m_transferFunction == other.m_transferFunction
        && m_sdrGamutWideness == other.m_sdrGamutWideness
        && m_sdrBrightness == other.m_sdrBrightness
        && m_minHdrBrightness == other.m_minHdrBrightness
        && m_maxFrameAverageBrightness == other.m_maxFrameAverageBrightness
        && m_maxHdrHighlightBrightness == other.m_maxHdrHighlightBrightness;
}

static float srgbToLinear(float sRGB)
{
    if (sRGB < 0.04045) {
        return std::max(sRGB / 12.92, 0.0);
    } else {
        return std::clamp(std::pow((sRGB + 0.055) / 1.055, 12.0 / 5.0), 0.0, 1.0);
    }
}

static float linearToSRGB(float linear)
{
    if (linear < 0.0031308) {
        return std::max(linear / 12.92, 0.0);
    } else {
        return std::clamp(std::pow(linear, 5.0 / 12.0) * 1.055 - 0.055, 0.0, 1.0);
    }
}

QVector3D ColorDescription::mapTo(QVector3D rgb, const ColorDescription &dst) const
{
    Q_ASSERT_X(m_transferFunction != NamedTransferFunction::PerceptualQuantizer && dst.transferFunction() != NamedTransferFunction::PerceptualQuantizer,
               "ColorDescription::mapTo", "PQ isn't supported yet");
    switch (m_transferFunction) {
    case NamedTransferFunction::sRGB:
        rgb = QVector3D(srgbToLinear(rgb.x()), srgbToLinear(rgb.y()), srgbToLinear(rgb.z()));
        break;
    case NamedTransferFunction::linear:
        rgb /= m_sdrBrightness;
        break;
    case NamedTransferFunction::PerceptualQuantizer:
        return QVector3D();
    }
    rgb = m_colorimetry.toOther(dst.colorimetry()) * rgb;
    switch (dst.transferFunction()) {
    case NamedTransferFunction::sRGB:
        return QVector3D(linearToSRGB(rgb.x()), linearToSRGB(rgb.y()), linearToSRGB(rgb.z()));
    case NamedTransferFunction::linear:
        return rgb * dst.sdrBrightness();
    case NamedTransferFunction::PerceptualQuantizer:
        return QVector3D();
    }
    return QVector3D();
}
}
