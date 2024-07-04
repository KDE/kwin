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

static QVector3D normalizeToY1(const QVector3D &vect)
{
    return vect.y() == 0 ? vect : QVector3D(vect.x() / vect.y(), 1, vect.z() / vect.y());
}

QMatrix4x4 Colorimetry::calculateToXYZMatrix(QVector3D red, QVector3D green, QVector3D blue, QVector3D white)
{
    const auto component_scale = (matrixFromColumns(red, green, blue)).inverted() * white;
    return matrixFromColumns(red * component_scale.x(), green * component_scale.y(), blue * component_scale.z());
}

Colorimetry Colorimetry::interpolateGamutTo(const Colorimetry &one, double factor) const
{
    return Colorimetry{
        m_red * (1 - factor) + one.red() * factor,
        m_green * (1 - factor) + one.green() * factor,
        m_blue * (1 - factor) + one.blue() * factor,
        m_white, // whitepoint should stay the same
    };
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
    , m_toXYZ(calculateToXYZMatrix(normalizeToY1(red), normalizeToY1(green), normalizeToY1(blue), normalizeToY1(white)))
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

const ColorDescription ColorDescription::sRGB = ColorDescription(NamedColorimetry::BT709, TransferFunction::gamma22, 100, 0, 100, 100);

ColorDescription::ColorDescription(const Colorimetry &containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance)
    : ColorDescription(containerColorimetry, tf, referenceLuminance, minLuminance, maxAverageLuminance, maxHdrLuminance, std::nullopt, Colorimetry::fromName(NamedColorimetry::BT709))
{
}

ColorDescription::ColorDescription(NamedColorimetry containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance)
    : ColorDescription(Colorimetry::fromName(containerColorimetry), tf, referenceLuminance, minLuminance, maxAverageLuminance, maxHdrLuminance, std::nullopt, Colorimetry::fromName(NamedColorimetry::BT709))
{
}

ColorDescription::ColorDescription(const Colorimetry &containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance, std::optional<Colorimetry> masteringColorimetry, const Colorimetry &sdrColorimetry)
    : m_containerColorimetry(containerColorimetry)
    , m_masteringColorimetry(masteringColorimetry)
    , m_transferFunction(tf)
    , m_sdrColorimetry(sdrColorimetry)
    , m_referenceLuminance(referenceLuminance)
    , m_minLuminance(minLuminance)
    , m_maxAverageLuminance(maxAverageLuminance)
    , m_maxHdrLuminance(maxHdrLuminance)
{
}

ColorDescription::ColorDescription(NamedColorimetry containerColorimetry, TransferFunction tf, double referenceLuminance, double minLuminance, std::optional<double> maxAverageLuminance, std::optional<double> maxHdrLuminance, std::optional<Colorimetry> masteringColorimetry, const Colorimetry &sdrColorimetry)
    : ColorDescription(Colorimetry::fromName(containerColorimetry), tf, referenceLuminance, minLuminance, maxAverageLuminance, maxHdrLuminance, masteringColorimetry, sdrColorimetry)
{
}

const Colorimetry &ColorDescription::containerColorimetry() const
{
    return m_containerColorimetry;
}

const std::optional<Colorimetry> &ColorDescription::masteringColorimetry() const
{
    return m_masteringColorimetry;
}

const Colorimetry &ColorDescription::sdrColorimetry() const
{
    return m_sdrColorimetry;
}

TransferFunction ColorDescription::transferFunction() const
{
    return m_transferFunction;
}

double ColorDescription::referenceLuminance() const
{
    return m_referenceLuminance;
}

double ColorDescription::minLuminance() const
{
    return m_minLuminance;
}

std::optional<double> ColorDescription::maxAverageLuminance() const
{
    return m_maxAverageLuminance;
}

std::optional<double> ColorDescription::maxHdrLuminance() const
{
    return m_maxHdrLuminance;
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

QVector3D ColorDescription::mapTo(QVector3D rgb, const ColorDescription &dst) const
{
    rgb = m_transferFunction.encodedToNits(rgb, m_referenceLuminance);
    rgb = m_containerColorimetry.toOther(dst.containerColorimetry()) * rgb;
    return dst.transferFunction().nitsToEncoded(rgb, dst.referenceLuminance());
}

TransferFunction::TransferFunction(Type tf)
    : type(tf)
{
}

double TransferFunction::encodedToNits(double encoded, double referenceLuminance) const
{
    switch (type) {
    case TransferFunction::sRGB:
        return referenceLuminance * srgbToLinear(encoded);
    case TransferFunction::gamma22:
        return referenceLuminance * std::pow(encoded, 2.2);
    case TransferFunction::linear:
        return encoded;
    case TransferFunction::scRGB:
        return encoded * 80.0f;
    case TransferFunction::PerceptualQuantizer:
        return pqToNits(encoded);
    }
    Q_UNREACHABLE();
}

QVector3D TransferFunction::encodedToNits(const QVector3D &encoded, double referenceLuminance) const
{
    return QVector3D(encodedToNits(encoded.x(), referenceLuminance), encodedToNits(encoded.y(), referenceLuminance), encodedToNits(encoded.z(), referenceLuminance));
}

double TransferFunction::nitsToEncoded(double nits, double referenceLuminance) const
{
    switch (type) {
    case TransferFunction::sRGB:
        return linearToSRGB(std::clamp(nits / referenceLuminance, 0.0, 1.0));
    case TransferFunction::gamma22:
        return std::pow(std::clamp(nits / referenceLuminance, 0.0, 1.0), 1.0 / 2.2);
    case TransferFunction::linear:
        return nits;
    case TransferFunction::scRGB:
        return nits / 80.0f;
    case TransferFunction::PerceptualQuantizer:
        return nitsToPQ(nits);
    }
    Q_UNREACHABLE();
}

QVector3D TransferFunction::nitsToEncoded(const QVector3D &nits, double referenceLuminance) const
{
    return QVector3D(nitsToEncoded(nits.x(), referenceLuminance), nitsToEncoded(nits.y(), referenceLuminance), nitsToEncoded(nits.z(), referenceLuminance));
}
}
