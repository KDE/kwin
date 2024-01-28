/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorspace.h"

#include <qassert.h>

namespace KWin
{

static QMatrix4x4 matrixFromColumns(const QVector3D &first, const QVector3D &second, const QVector3D &third)
{
    QMatrix4x4 ret;
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

QVector3D Colorimetry::xyToXYZ(QVector2D xy)
{
    return QVector3D(xy.x() / xy.y(), 1, (1 - xy.x() - xy.y()) / xy.y());
}

QVector2D Colorimetry::xyzToXY(QVector3D xyz)
{
    xyz /= xyz.y();
    return QVector2D(xyz.x() / (xyz.x() + xyz.y() + xyz.z()), xyz.y() / (xyz.x() + xyz.y() + xyz.z()));
}

QMatrix4x4 Colorimetry::chromaticAdaptationMatrix(QVector2D sourceWhitepoint, QVector2D destinationWhitepoint)
{
    static const QMatrix4x4 bradford = []() {
        QMatrix4x4 ret;
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
    static const QMatrix4x4 inverseBradford = []() {
        QMatrix4x4 ret;
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
    if (sourceWhitepoint == destinationWhitepoint) {
        return QMatrix4x4{};
    }
    const QVector3D factors = (bradford * xyToXYZ(destinationWhitepoint)) / (bradford * xyToXYZ(sourceWhitepoint));
    QMatrix4x4 adaptation{};
    adaptation(0, 0) = factors.x();
    adaptation(1, 1) = factors.y();
    adaptation(2, 2) = factors.z();
    return inverseBradford * adaptation * bradford;
}

QMatrix4x4 Colorimetry::calculateToXYZMatrix(QVector3D red, QVector3D green, QVector3D blue, QVector3D white)
{
    const auto component_scale = (matrixFromColumns(red, green, blue)).inverted() * white;
    return matrixFromColumns(red * component_scale.x(), green * component_scale.y(), blue * component_scale.z());
}

Colorimetry::Colorimetry(QVector2D red, QVector2D green, QVector2D blue, QVector2D white)
    : m_red(red)
    , m_green(green)
    , m_blue(blue)
    , m_white(white)
    , m_toXYZ(calculateToXYZMatrix(xyToXYZ(red), xyToXYZ(green), xyToXYZ(blue), xyToXYZ(white)))
    , m_fromXYZ(m_toXYZ.inverted())
{
}

Colorimetry::Colorimetry(QVector3D red, QVector3D green, QVector3D blue, QVector3D white)
    : m_red(xyzToXY(red))
    , m_green(xyzToXY(green))
    , m_blue(xyzToXY(blue))
    , m_white(xyzToXY(white))
    , m_toXYZ(calculateToXYZMatrix(red, green, blue, white))
    , m_fromXYZ(m_toXYZ.inverted())
{
}

const QMatrix4x4 &Colorimetry::toXYZ() const
{
    return m_toXYZ;
}

const QMatrix4x4 &Colorimetry::fromXYZ() const
{
    return m_fromXYZ;
}

QMatrix4x4 Colorimetry::toOther(const Colorimetry &other) const
{
    // rendering intent is relative colorimetric, so adapt to the different whitepoint
    return other.fromXYZ() * chromaticAdaptationMatrix(this->white(), other.white()) * toXYZ();
}

Colorimetry Colorimetry::adaptedTo(QVector2D newWhitepoint) const
{
    const auto mat = chromaticAdaptationMatrix(this->white(), newWhitepoint);
    return Colorimetry{
        xyzToXY(mat * xyToXYZ(red())),
        xyzToXY(mat * xyToXYZ(green())),
        xyzToXY(mat * xyToXYZ(blue())),
        newWhitepoint,
    };
}

bool Colorimetry::operator==(const Colorimetry &other) const
{
    return red() == other.red() && green() == other.green() && blue() == other.blue() && white() == other.white();
}

bool Colorimetry::operator==(NamedColorimetry name) const
{
    return *this == fromName(name);
}

const QVector2D &Colorimetry::red() const
{
    return m_red;
}

const QVector2D &Colorimetry::green() const
{
    return m_green;
}

const QVector2D &Colorimetry::blue() const
{
    return m_blue;
}

const QVector2D &Colorimetry::white() const
{
    return m_white;
}

static const Colorimetry BT709 = Colorimetry{
    QVector2D{0.64, 0.33},
    QVector2D{0.30, 0.60},
    QVector2D{0.15, 0.06},
    QVector2D{0.3127, 0.3290},
};

static const Colorimetry BT2020 = Colorimetry{
    QVector2D{0.708, 0.292},
    QVector2D{0.170, 0.797},
    QVector2D{0.131, 0.046},
    QVector2D{0.3127, 0.3290},
};

const Colorimetry &Colorimetry::fromName(NamedColorimetry name)
{
    switch (name) {
    case NamedColorimetry::BT709:
        return BT709;
    case NamedColorimetry::BT2020:
        return BT2020;
    }
    Q_UNREACHABLE();
}

const ColorDescription ColorDescription::sRGB = ColorDescription(NamedColorimetry::BT709, NamedTransferFunction::gamma22, 100, 0, 100, 100, 0);

static Colorimetry sRGBColorimetry(double factor)
{
    return Colorimetry{
        BT709.red() * (1 - factor) + BT2020.red() * factor,
        BT709.green() * (1 - factor) + BT2020.green() * factor,
        BT709.blue() * (1 - factor) + BT2020.blue() * factor,
        BT709.white(), // whitepoint is the same
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

static float nitsToPQ(float nits)
{
    const float normalized = std::clamp(nits / 10000.0f, 0.0f, 1.0f);
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float powed = std::pow(normalized, m1);
    const float num = c1 + c2 * powed;
    const float denum = 1 + c3 * powed;
    return std::pow(num / denum, m2);
}

static float pqToNits(float pq)
{
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1_inv = 1.0 / 0.1593017578125;
    const float m2_inv = 1.0 / 78.84375;
    const float powed = std::pow(pq, m2_inv);
    const float num = std::max(powed - c1, 0.0f);
    const float den = c2 - c3 * powed;
    return 10000.0f * std::pow(num / den, m1_inv);
}

static QVector3D clamp(const QVector3D &vect, float min = 0, float max = 1)
{
    return QVector3D(std::clamp(vect.x(), min, max), std::clamp(vect.y(), min, max), std::clamp(vect.z(), min, max));
}

QVector3D ColorDescription::encodedToNits(const QVector3D &nits, NamedTransferFunction tf, double sdrBrightness)
{
    switch (tf) {
    case NamedTransferFunction::sRGB:
        return sdrBrightness * QVector3D(srgbToLinear(nits.x()), srgbToLinear(nits.y()), srgbToLinear(nits.z()));
    case NamedTransferFunction::gamma22:
        return sdrBrightness * QVector3D(std::pow(nits.x(), 2.2), std::pow(nits.y(), 2.2), std::pow(nits.z(), 2.2));
    case NamedTransferFunction::linear:
        return nits;
    case NamedTransferFunction::scRGB:
        return nits * 80.0f;
    case NamedTransferFunction::PerceptualQuantizer:
        return QVector3D(pqToNits(nits.x()), pqToNits(nits.y()), pqToNits(nits.z()));
    }
    Q_UNREACHABLE();
}

QVector3D ColorDescription::nitsToEncoded(const QVector3D &rgb, NamedTransferFunction tf, double sdrBrightness)
{
    switch (tf) {
    case NamedTransferFunction::sRGB: {
        const auto clamped = clamp(rgb / sdrBrightness);
        return QVector3D(linearToSRGB(clamped.x()), linearToSRGB(clamped.y()), linearToSRGB(clamped.z()));
    }
    case NamedTransferFunction::gamma22: {
        const auto clamped = clamp(rgb / sdrBrightness);
        return QVector3D(std::pow(clamped.x(), 1 / 2.2), std::pow(clamped.y(), 1 / 2.2), std::pow(clamped.z(), 1 / 2.2));
    }
    case NamedTransferFunction::linear:
        return rgb;
    case NamedTransferFunction::scRGB:
        return rgb / 80.0f;
    case NamedTransferFunction::PerceptualQuantizer:
        return QVector3D(nitsToPQ(rgb.x()), nitsToPQ(rgb.y()), nitsToPQ(rgb.z()));
    }
    Q_UNREACHABLE();
}

QVector3D ColorDescription::mapTo(QVector3D rgb, const ColorDescription &dst) const
{
    rgb = encodedToNits(rgb, m_transferFunction, m_sdrBrightness);
    rgb = m_colorimetry.toOther(dst.colorimetry()) * rgb;
    return nitsToEncoded(rgb, dst.transferFunction(), dst.sdrBrightness());
}
}
