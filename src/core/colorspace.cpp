/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorspace.h"
#include "colorpipeline.h"

#include <QtAssert>

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
    if (xy.y() == 0) {
        // special case for XYZ Colorimetry
        // where xy.y == 0 is valid
        return QVector3D(xy.x(), 1, 0);
    }
    return QVector3D(xy.x() / xy.y(), 1, (1 - xy.x() - xy.y()) / xy.y());
}

QVector2D Colorimetry::xyzToXY(QVector3D xyz)
{
    if (xyz.y() == 0) {
        // this is nonsense, but at least doesn't crash
        return QVector2D(0, 0);
    }
    return QVector2D(xyz.x() / (xyz.x() + xyz.y() + xyz.z()), xyz.y() / (xyz.x() + xyz.y() + xyz.z()));
}

QMatrix4x4 Colorimetry::chromaticAdaptationMatrix(QVector3D sourceWhitepoint, QVector3D destinationWhitepoint)
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
    const QVector3D factors = (bradford * destinationWhitepoint) / (bradford * sourceWhitepoint);
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

Colorimetry Colorimetry::interpolateGamutTo(const Colorimetry &one, double factor) const
{
    return Colorimetry{
        m_red * (1 - factor) + one.red() * factor,
        m_green * (1 - factor) + one.green() * factor,
        m_blue * (1 - factor) + one.blue() * factor,
        m_white, // whitepoint should stay the same
    };
}

static QVector3D normalizeToY1(const QVector3D &xyz)
{
    if (xyz.y() == 0) {
        return QVector3D(0, 0, 0);
    }
    return xyz / xyz.y();
}

Colorimetry::Colorimetry(QVector3D red, QVector3D green, QVector3D blue, QVector3D white)
    : m_red(normalizeToY1(red))
    , m_green(normalizeToY1(green))
    , m_blue(normalizeToY1(blue))
    , m_white(normalizeToY1(white))
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

// converts from XYZ to LMS suitable for ICtCp
static const QMatrix4x4 s_xyzToDolbyLMS = []() {
    QMatrix4x4 ret;
    ret(0, 0) = 0.3593;
    ret(0, 1) = 0.6976;
    ret(0, 2) = -0.0359;
    ret(1, 0) = -0.1921;
    ret(1, 1) = 1.1005;
    ret(1, 2) = 0.0754;
    ret(2, 0) = 0.0071;
    ret(2, 1) = 0.0748;
    ret(2, 2) = 0.8433;
    return ret;
}();
static const QMatrix4x4 s_inverseDolbyLMS = s_xyzToDolbyLMS.inverted();

QMatrix4x4 Colorimetry::toLMS() const
{
    return s_xyzToDolbyLMS * m_toXYZ;
}

QMatrix4x4 Colorimetry::fromLMS() const
{
    return m_fromXYZ * s_inverseDolbyLMS;
}

Colorimetry Colorimetry::adaptedTo(QVector2D newWhitepoint) const
{
    const auto mat = chromaticAdaptationMatrix(this->white(), xyToXYZ(newWhitepoint));
    return Colorimetry{
        mat * red(),
        mat * green(),
        mat * blue(),
        Colorimetry::xyToXYZ(newWhitepoint),
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

const QVector3D &Colorimetry::red() const
{
    return m_red;
}

const QVector3D &Colorimetry::green() const
{
    return m_green;
}

const QVector3D &Colorimetry::blue() const
{
    return m_blue;
}

const QVector3D &Colorimetry::white() const
{
    return m_white;
}

static const Colorimetry BT709 = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.64, 0.33}),
    Colorimetry::xyToXYZ(QVector2D{0.30, 0.60}),
    Colorimetry::xyToXYZ(QVector2D{0.15, 0.06}),
    Colorimetry::xyToXYZ(QVector2D{0.3127, 0.3290}),
};
static const Colorimetry PAL_M = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.67, 0.33}),
    Colorimetry::xyToXYZ(QVector2D{0.21, 0.71}),
    Colorimetry::xyToXYZ(QVector2D{0.14, 0.08}),
    Colorimetry::xyToXYZ(QVector2D{0.310, 0.316}),
};
static const Colorimetry PAL = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.640, 0.330}),
    Colorimetry::xyToXYZ(QVector2D{0.290, 0.600}),
    Colorimetry::xyToXYZ(QVector2D{0.150, 0.060}),
    Colorimetry::xyToXYZ(QVector2D{0.3127, 0.3290}),
};
static const Colorimetry NTSC = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.630, 0.340}),
    Colorimetry::xyToXYZ(QVector2D{0.310, 0.595}),
    Colorimetry::xyToXYZ(QVector2D{0.155, 0.070}),
    Colorimetry::xyToXYZ(QVector2D{0.3127, 0.3290}),
};
static const Colorimetry GenericFilm = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.243, 0.692}),
    Colorimetry::xyToXYZ(QVector2D{0.145, 0.049}),
    Colorimetry::xyToXYZ(QVector2D{0.681, 0.319}),
    Colorimetry::xyToXYZ(QVector2D{0.310, 0.316}),
};
static const Colorimetry BT2020 = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.708, 0.292}),
    Colorimetry::xyToXYZ(QVector2D{0.170, 0.797}),
    Colorimetry::xyToXYZ(QVector2D{0.131, 0.046}),
    Colorimetry::xyToXYZ(QVector2D{0.3127, 0.3290}),
};
static const Colorimetry CIEXYZ = Colorimetry{
    QVector3D(1.0, 0.0, 0.0),
    QVector3D(0.0, 1.0, 0.0),
    QVector3D(0.0, 0.0, 1.0),
    Colorimetry::xyToXYZ(QVector2D{1.0 / 3.0, 1.0 / 3.0}),
};
static const Colorimetry DCIP3 = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.680, 0.320}),
    Colorimetry::xyToXYZ(QVector2D{0.265, 0.690}),
    Colorimetry::xyToXYZ(QVector2D{0.150, 0.060}),
    Colorimetry::xyToXYZ(QVector2D{0.314, 0.351}),
};
static const Colorimetry DisplayP3 = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.680, 0.320}),
    Colorimetry::xyToXYZ(QVector2D{0.265, 0.690}),
    Colorimetry::xyToXYZ(QVector2D{0.150, 0.060}),
    Colorimetry::xyToXYZ(QVector2D{0.3127, 0.3290}),
};
static const Colorimetry AdobeRGB = Colorimetry{
    Colorimetry::xyToXYZ(QVector2D{0.6400, 0.3300}),
    Colorimetry::xyToXYZ(QVector2D{0.2100, 0.7100}),
    Colorimetry::xyToXYZ(QVector2D{0.1500, 0.0600}),
    Colorimetry::xyToXYZ(QVector2D{0.3127, 0.3290}),
};

const Colorimetry &Colorimetry::fromName(NamedColorimetry name)
{
    switch (name) {
    case NamedColorimetry::BT709:
        return BT709;
    case NamedColorimetry::PAL_M:
        return PAL_M;
    case NamedColorimetry::PAL:
        return PAL;
    case NamedColorimetry::NTSC:
        return NTSC;
    case NamedColorimetry::GenericFilm:
        return GenericFilm;
    case NamedColorimetry::BT2020:
        return BT2020;
    case NamedColorimetry::CIEXYZ:
        return CIEXYZ;
    case NamedColorimetry::DCIP3:
        return DCIP3;
    case NamedColorimetry::DisplayP3:
        return DisplayP3;
    case NamedColorimetry::AdobeRGB:
        return AdobeRGB;
    }
    Q_UNREACHABLE();
}

std::optional<NamedColorimetry> Colorimetry::name() const
{
    constexpr std::array names = {
        NamedColorimetry::BT709,
        NamedColorimetry::PAL_M,
        NamedColorimetry::PAL,
        NamedColorimetry::NTSC,
        NamedColorimetry::GenericFilm,
        NamedColorimetry::BT2020,
        NamedColorimetry::CIEXYZ,
        NamedColorimetry::DCIP3,
        NamedColorimetry::DisplayP3,
        NamedColorimetry::AdobeRGB,
    };
    const auto it = std::ranges::find_if(names, [this](NamedColorimetry name) {
        return *this == name;
    });
    return it != names.end() ? std::optional(*it) : std::nullopt;
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
    QMatrix4x4 luminanceBefore;
    QMatrix4x4 luminanceAfter;
    if (intent == RenderingIntent::Perceptual || intent == RenderingIntent::RelativeColorimetricWithBPC) {
        // add black point compensation: black and reference white from the source color space
        // should both be mapped to black and reference white in the destination color space

        // before color conversions, map [src min, src ref] to [0, 1]
        luminanceBefore.scale(1.0 / (referenceLuminance() - minLuminance()));
        luminanceBefore.translate(-minLuminance(), -minLuminance(), -minLuminance());
        // afterwards, map [0, 1] again to [dst min, dst ref]
        luminanceAfter.translate(other.minLuminance(), other.minLuminance(), other.minLuminance());
        luminanceAfter.scale(other.referenceLuminance() - other.minLuminance());
    } else {
        // map only the reference luminance
        luminanceBefore.scale(other.referenceLuminance() / referenceLuminance());
    }
    switch (intent) {
    case RenderingIntent::Perceptual: {
        const Colorimetry &srcContainer = containerColorimetry() == NamedColorimetry::BT709 ? other.sdrColorimetry() : containerColorimetry();
        return luminanceAfter * other.containerColorimetry().fromXYZ() * Colorimetry::chromaticAdaptationMatrix(srcContainer.white(), other.containerColorimetry().white()) * srcContainer.toXYZ() * luminanceBefore;
    }
    case RenderingIntent::RelativeColorimetric: {
        return luminanceAfter * other.containerColorimetry().fromXYZ() * Colorimetry::chromaticAdaptationMatrix(containerColorimetry().white(), other.containerColorimetry().white()) * containerColorimetry().toXYZ() * luminanceBefore;
    }
    case RenderingIntent::RelativeColorimetricWithBPC: {
        return luminanceAfter * other.containerColorimetry().fromXYZ() * Colorimetry::chromaticAdaptationMatrix(containerColorimetry().white(), other.containerColorimetry().white()) * containerColorimetry().toXYZ() * luminanceBefore;
    }
    case RenderingIntent::AbsoluteColorimetric: {
        return luminanceAfter * other.containerColorimetry().fromXYZ() * containerColorimetry().toXYZ() * luminanceBefore;
    }
    }
    Q_UNREACHABLE();
}

QVector3D ColorDescription::mapTo(QVector3D rgb, const ColorDescription &dst, RenderingIntent intent) const
{
    rgb = m_transferFunction.encodedToNits(rgb);
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
        return 0.02;
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
        return 80;
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
        return 80;
    }
    Q_UNREACHABLE();
}

bool TransferFunction::operator==(const TransferFunction &other) const
{
    // allow for a greater error with large max. luminance, as floating point errors get larger there
    // and the effect of errors is smaller too
    return type == other.type
        && std::abs(other.minLuminance - minLuminance) < ColorPipeline::s_maxResolution
        && std::abs(other.maxLuminance - maxLuminance) < ColorPipeline::s_maxResolution * maxLuminance;
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
