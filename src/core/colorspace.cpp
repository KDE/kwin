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

const ColorDescription ColorDescription::sRGB = ColorDescription(NamedColorimetry::BT709, TransferFunction(TransferFunction::gamma22), TransferFunction::defaultReferenceLuminanceFor(TransferFunction::gamma22), TransferFunction::defaultMinLuminanceFor(TransferFunction::gamma22), TransferFunction::defaultMaxLuminanceFor(TransferFunction::gamma22), TransferFunction::defaultMaxLuminanceFor(TransferFunction::gamma22));

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

QMatrix4x4 ColorDescription::toOther(const ColorDescription &other, RenderingIntent intent) const
{
    switch (intent) {
    case RenderingIntent::Perceptual: {
        const Colorimetry &srcContainer = containerColorimetry() == NamedColorimetry::BT709 ? other.sdrColorimetry() : containerColorimetry();
        return other.containerColorimetry().fromXYZ() * Colorimetry::chromaticAdaptationMatrix(srcContainer.white(), other.containerColorimetry().white()) * srcContainer.toXYZ();
    }
    case RenderingIntent::RelativeColorimetric:
        return other.containerColorimetry().fromXYZ() * Colorimetry::chromaticAdaptationMatrix(containerColorimetry().white(), other.containerColorimetry().white()) * containerColorimetry().toXYZ();
    case RenderingIntent::AbsoluteColorimetric:
        return other.containerColorimetry().fromXYZ() * containerColorimetry().toXYZ();
    }
    Q_UNREACHABLE();
}

QVector3D ColorDescription::mapTo(QVector3D rgb, const ColorDescription &dst, RenderingIntent intent) const
{
    rgb = m_transferFunction.encodedToNits(rgb);
    rgb *= dst.referenceLuminance() / m_referenceLuminance;
    rgb = toOther(dst, intent) * rgb;
    return dst.transferFunction().nitsToEncoded(rgb);
}

ColorDescription ColorDescription::withTransferFunction(const TransferFunction &func) const
{
    return ColorDescription(m_containerColorimetry, func, m_referenceLuminance, m_minLuminance, m_maxAverageLuminance, m_maxHdrLuminance, m_masteringColorimetry, m_sdrColorimetry);
}

double TransferFunction::defaultMinLuminanceFor(Type type)
{
    switch (type) {
    case Type::sRGB:
    case Type::gamma22:
        return 0.01;
    case Type::linear:
        return 0;
    case Type::PerceptualQuantizer:
        return 0.005;
    }
    Q_UNREACHABLE();
}

double TransferFunction::defaultMaxLuminanceFor(Type type)
{
    switch (type) {
    case Type::sRGB:
    case Type::gamma22:
        return 100;
    case Type::linear:
        return 1;
    case Type::PerceptualQuantizer:
        return 10'000;
    }
    Q_UNREACHABLE();
}

double TransferFunction::defaultReferenceLuminanceFor(Type type)
{
    switch (type) {
    case Type::PerceptualQuantizer:
        return 203;
    case Type::linear:
        return 80;
    case Type::sRGB:
    case Type::gamma22:
        return 100;
    }
    Q_UNREACHABLE();
}

TransferFunction::TransferFunction(Type tf)
    : TransferFunction(tf, defaultMinLuminanceFor(tf), defaultMaxLuminanceFor(tf))
{
}

TransferFunction::TransferFunction(Type tf, double minLuminance, double maxLuminance)
    : type(tf)
    , minLuminance(minLuminance)
    , maxLuminance(maxLuminance)
{
}

double TransferFunction::encodedToNits(double encoded) const
{
    switch (type) {
    case TransferFunction::sRGB: {
        if (encoded < 0.04045) {
            return std::max(encoded / 12.92, 0.0) * (maxLuminance - minLuminance) + minLuminance;
        } else {
            return std::clamp(std::pow((encoded + 0.055) / 1.055, 12.0 / 5.0), 0.0, 1.0) * (maxLuminance - minLuminance) + minLuminance;
        }
    }
    case TransferFunction::gamma22:
        return std::pow(encoded, 2.2) * (maxLuminance - minLuminance) + minLuminance;
    case TransferFunction::linear:
        return encoded * (maxLuminance - minLuminance) + minLuminance;
    case TransferFunction::PerceptualQuantizer: {
        const double c1 = 0.8359375;
        const double c2 = 18.8515625;
        const double c3 = 18.6875;
        const double m1_inv = 1.0 / 0.1593017578125;
        const double m2_inv = 1.0 / 78.84375;
        const double powed = std::pow(encoded, m2_inv);
        const double num = std::max(powed - c1, 0.0);
        const double den = c2 - c3 * powed;
        return std::pow(num / den, m1_inv) * (maxLuminance - minLuminance) + minLuminance;
    }
    }
    Q_UNREACHABLE();
}

QVector3D TransferFunction::encodedToNits(const QVector3D &encoded) const
{
    return QVector3D(encodedToNits(encoded.x()), encodedToNits(encoded.y()), encodedToNits(encoded.z()));
}

double TransferFunction::nitsToEncoded(double nits) const
{
    const double normalized = (nits - minLuminance) / (maxLuminance - minLuminance);
    switch (type) {
    case TransferFunction::sRGB: {
        if (normalized < 0.0031308) {
            return std::max(normalized / 12.92, 0.0);
        } else {
            return std::clamp(std::pow(normalized, 5.0 / 12.0) * 1.055 - 0.055, 0.0, 1.0);
        }
    }
    case TransferFunction::gamma22:
        return std::pow(std::clamp(normalized, 0.0, 1.0), 1.0 / 2.2);
    case TransferFunction::linear:
        return normalized;
    case TransferFunction::PerceptualQuantizer: {
        const double c1 = 0.8359375;
        const double c2 = 18.8515625;
        const double c3 = 18.6875;
        const double m1 = 0.1593017578125;
        const double m2 = 78.84375;
        const double powed = std::pow(std::clamp(normalized, 0.0, 1.0), m1);
        const double num = c1 + c2 * powed;
        const double denum = 1 + c3 * powed;
        return std::pow(num / denum, m2);
    }
    }
    Q_UNREACHABLE();
}

QVector3D TransferFunction::nitsToEncoded(const QVector3D &nits) const
{
    return QVector3D(nitsToEncoded(nits.x()), nitsToEncoded(nits.y()), nitsToEncoded(nits.z()));
}

bool TransferFunction::isRelative() const
{
    switch (type) {
    case TransferFunction::gamma22:
    case TransferFunction::sRGB:
        return true;
    case TransferFunction::linear:
    case TransferFunction::PerceptualQuantizer:
        return false;
    }
    Q_UNREACHABLE();
}

TransferFunction TransferFunction::relativeScaledTo(double referenceLuminance) const
{
    if (isRelative()) {
        return TransferFunction(type, minLuminance * referenceLuminance / maxLuminance, referenceLuminance);
    } else {
        return *this;
    }
}
}

QDebug operator<<(QDebug debug, const KWin::TransferFunction &tf)
{
    debug << "TransferFunction(" << tf.type << ", [" << tf.minLuminance << "," << tf.maxLuminance << "] )";
    return debug;
}
