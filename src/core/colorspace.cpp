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
    const auto r_xyz = xyToXYZ(m_red);
    const auto g_xyz = xyToXYZ(m_green);
    const auto b_xyz = xyToXYZ(m_blue);
    const auto w_xyz = xyToXYZ(white());
    const auto component_scale = inverse(matrixFromColumns(r_xyz, g_xyz, b_xyz)) * w_xyz;
    return matrixFromColumns(r_xyz * component_scale.x(), g_xyz * component_scale.y(), b_xyz * component_scale.z());
}

QMatrix3x3 Colorimetry::chromaticAdaptationMatrix(QVector2D sourceWhitepoint, QVector2D destinationWhitepoint)
{
    Q_ASSERT(sourceWhitepoint.y() != 0);
    Q_ASSERT(destinationWhitepoint.y() != 0);
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
    Q_ASSERT(m_white);
    Q_ASSERT(other.m_white);
    if (m_white == other.m_white) {
        return inverse(other.toXYZ()) * toXYZ();
    } else {
        return inverse(other.toXYZ()) * chromaticAdaptationMatrix(white(), other.white()) * toXYZ();
    }
}

Colorimetry Colorimetry::adaptedTo(QVector2D newWhitepoint) const
{
    const auto mat = chromaticAdaptationMatrix(white(), newWhitepoint);
    return Colorimetry{
        xyzToXY(mat * xyToXYZ(m_red)),
        xyzToXY(mat * xyToXYZ(m_green)),
        xyzToXY(mat * xyToXYZ(m_blue)),
        newWhitepoint,
        std::nullopt,
    };
}

bool Colorimetry::operator==(const Colorimetry &other) const
{
    return (m_name || other.m_name) ? (m_name == other.m_name)
                                    : (m_red == other.m_red && m_green == other.m_green && m_blue == other.m_blue && m_white == other.m_white);
}

static const Colorimetry BT709 = Colorimetry{
    {0.64, 0.33},
    {0.30, 0.60},
    {0.15, 0.06},
    {0.3127, 0.3290},
    NamedColorimetry::BT709,
};

static const Colorimetry BT2020 = Colorimetry{
    {0.708, 0.292},
    {0.170, 0.797},
    {0.131, 0.046},
    {0.3127, 0.3290},
    NamedColorimetry::BT2020,
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
        xyzToXY(red),
        xyzToXY(green),
        xyzToXY(blue),
        xyzToXY(white),
        std::nullopt,
    };
}

ColorDescription create(ColorDescription ret)
{
    Q_ASSERT(ret.isValid());
    return ret;
}

// const ColorDescription ColorDescription::sRGB = create(ColorDescription(NamedColorimetry::BT709, NamedTransferFunction::gamma22, 100, 0, 100, 100, 0));

ColorDescription ColorDescription::sRGBf()
{
    static const auto sRGB = ColorDescription(NamedColorimetry::BT709, NamedTransferFunction::gamma22, 100, 0, 100, 100, 0);
    Q_ASSERT(ColorDescription(NamedColorimetry::BT709, NamedTransferFunction::gamma22, 100, 0, 100, 100, 0) == sRGB);
    Q_ASSERT(sRGB.isValid());
    return sRGB;
}

static Colorimetry sRGBColorimetry(double factor)
{
    Q_ASSERT(BT709.white().y() != 0);
    Colorimetry ret{
        BT709.red() * (1 - factor) + BT2020.red() * factor,
        BT709.green() * (1 - factor) + BT2020.green() * factor,
        BT709.blue() * (1 - factor) + BT2020.blue() * factor,
        BT709.white(), // whitepoint is the same
        std::nullopt};
    Q_ASSERT(ret.isValid());
    return ret;
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
    Q_ASSERT(isValid());
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
    Q_ASSERT(isValid());
}

ColorDescription::ColorDescription(const ColorDescription &ret)
    : ColorDescription(ret.m_colorimetry, ret.m_transferFunction, ret.m_sdrGamutWideness, ret.m_sdrBrightness, ret.m_minHdrBrightness, ret.m_maxFrameAverageBrightness, ret.m_maxHdrHighlightBrightness)
{
}

ColorDescription ColorDescription::operator=(const ColorDescription &other)
{
    m_colorimetry = other.m_colorimetry;
    m_transferFunction = other.m_transferFunction;
    m_sdrColorimetry = other.m_sdrColorimetry;
    m_sdrGamutWideness = other.m_sdrGamutWideness;
    m_sdrBrightness = other.m_sdrBrightness;
    m_minHdrBrightness = other.m_minHdrBrightness;
    m_maxFrameAverageBrightness = other.m_maxFrameAverageBrightness;
    m_maxHdrHighlightBrightness = other.m_maxHdrHighlightBrightness;
    Q_ASSERT(isValid() == other.isValid());
    return *this;
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

QVector3D ColorDescription::mapTo(QVector3D rgb, const ColorDescription &dst) const
{
    // transfer function -> nits
    switch (m_transferFunction) {
    case NamedTransferFunction::sRGB:
        rgb = m_sdrBrightness * QVector3D(srgbToLinear(rgb.x()), srgbToLinear(rgb.y()), srgbToLinear(rgb.z()));
        break;
    case NamedTransferFunction::gamma22:
        rgb = m_sdrBrightness * QVector3D(std::pow(rgb.x(), 2.2), std::pow(rgb.y(), 2.2), std::pow(rgb.z(), 2.2));
        break;
    case NamedTransferFunction::linear:
        break;
    case NamedTransferFunction::scRGB:
        rgb *= 80.0f;
        break;
    case NamedTransferFunction::PerceptualQuantizer:
        rgb = QVector3D(pqToNits(rgb.x()), pqToNits(rgb.y()), pqToNits(rgb.z()));
        break;
    }
    rgb = m_colorimetry.toOther(dst.colorimetry()) * rgb;
    // nits -> transfer function
    switch (dst.transferFunction()) {
    case NamedTransferFunction::sRGB:
        rgb = clamp(rgb / dst.sdrBrightness());
        return QVector3D(linearToSRGB(rgb.x()), linearToSRGB(rgb.y()), linearToSRGB(rgb.z()));
    case NamedTransferFunction::gamma22:
        rgb = clamp(rgb / dst.sdrBrightness());
        return QVector3D(std::pow(rgb.x(), 1 / 2.2), std::pow(rgb.y(), 1 / 2.2), std::pow(rgb.z(), 1 / 2.2));
    case NamedTransferFunction::linear:
        return rgb;
    case NamedTransferFunction::scRGB:
        return rgb / 80.0f;
    case NamedTransferFunction::PerceptualQuantizer:
        return QVector3D(nitsToPQ(rgb.x()), nitsToPQ(rgb.y()), nitsToPQ(rgb.z()));
    }
    return QVector3D();
}
}
