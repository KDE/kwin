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

XYZ xy::toXYZ() const
{
    if (y == 0) {
        return XYZ{0, 0, 0};
    }
    return XYZ{
        .X = x / y,
        .Y = 1.0,
        .Z = (1 - x - y) / y,
    };
}

QVector2D xy::asVector() const
{
    return QVector2D(x, y);
}

bool xy::operator==(const xy &other) const
{
    return qFuzzyCompare(x, other.x)
        && qFuzzyCompare(y, other.y);
}

XYZ xyY::toXYZ() const
{
    if (y == 0) {
        return XYZ{0, 0, 0};
    }
    return XYZ{
        .X = Y * x / y,
        .Y = Y,
        .Z = Y * (1 - x - y) / y,
    };
}

bool xyY::operator==(const xyY &other) const
{
    return qFuzzyCompare(x, other.x)
        && qFuzzyCompare(y, other.y)
        && qFuzzyCompare(Y, other.Y);
}

xyY XYZ::toxyY() const
{
    const double sum = X + Y + Z;
    if (qFuzzyIsNull(sum)) {
        // this is nonsense, but at least won't crash
        return xyY{
            .x = 0,
            .y = 0,
            .Y = 1,
        };
    }
    return xyY{
        .x = X / sum,
        .y = Y / sum,
        .Y = Y,
    };
}

xy XYZ::toxy() const
{
    const double sum = X + Y + Z;
    if (qFuzzyIsNull(sum)) {
        // this is nonsense, but at least won't crash
        return xy{
            .x = 0,
            .y = 0,
        };
    }
    return xy{
        .x = X / sum,
        .y = Y / sum,
    };
}

XYZ XYZ::operator*(double factor) const
{
    return XYZ{
        .X = X * factor,
        .Y = Y * factor,
        .Z = Z * factor,
    };
}

XYZ XYZ::operator/(double divisor) const
{
    return XYZ{
        .X = X / divisor,
        .Y = Y / divisor,
        .Z = Z / divisor,
    };
}

XYZ XYZ::operator+(const XYZ &other) const
{
    return XYZ{
        .X = X + other.X,
        .Y = Y + other.Y,
        .Z = Z + other.Z,
    };
}

QVector3D XYZ::asVector() const
{
    return QVector3D(X, Y, Z);
}

XYZ XYZ::fromVector(const QVector3D &vector)
{
    return XYZ{
        .X = vector.x(),
        .Y = vector.y(),
        .Z = vector.z(),
    };
}

bool XYZ::operator==(const XYZ &other) const
{
    return qFuzzyCompare(X, other.X)
        && qFuzzyCompare(Y, other.Y)
        && qFuzzyCompare(Z, other.Z);
}

QMatrix4x4 Colorimetry::chromaticAdaptationMatrix(XYZ sourceWhitepoint, XYZ destinationWhitepoint)
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
    const QVector3D factors = (bradford * destinationWhitepoint.asVector()) / (bradford * sourceWhitepoint.asVector());
    QMatrix4x4 adaptation{};
    adaptation(0, 0) = factors.x();
    adaptation(1, 1) = factors.y();
    adaptation(2, 2) = factors.z();
    return inverseBradford * adaptation * bradford;
}

QMatrix4x4 Colorimetry::calculateToXYZMatrix(XYZ red, XYZ green, XYZ blue, XYZ white)
{
    const QVector3D r = red.asVector();
    const QVector3D g = green.asVector();
    const QVector3D b = blue.asVector();
    const auto component_scale = (matrixFromColumns(r, g, b)).inverted() * white.asVector();
    return matrixFromColumns(r * component_scale.x(), g * component_scale.y(), b * component_scale.z());
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

static double triangleArea(QVector2D p1, QVector2D p2, QVector2D p3)
{
    return std::abs(0.5 * (p1.x() * (p2.y() - p3.y()) + p2.x() * (p3.y() - p1.y()) + p3.x() * (p1.y() - p2.y())));
}

bool Colorimetry::isValid(xy red, xy green, xy blue, xy white)
{
    // this is more of a heuristic than a hard rule
    // but if the gamut is too small, it's not really usable
    const double gamutArea = triangleArea(red.asVector(), green.asVector(), blue.asVector());
    if (gamutArea < 0.02) {
        return false;
    }
    // if the white point is inside the gamut triangle,
    // the three triangles made up between the primaries and the whitepoint
    // must have the same area as the gamut triangle
    const double area1 = triangleArea(white.asVector(), green.asVector(), blue.asVector());
    const double area2 = triangleArea(red.asVector(), white.asVector(), blue.asVector());
    const double area3 = triangleArea(red.asVector(), green.asVector(), white.asVector());
    if (std::abs(area1 + area2 + area3 - gamutArea) > 0.001) {
        // this would cause terrible glitches
        return false;
    }
    return true;
}

bool Colorimetry::isReal(xy red, xy green, xy blue, xy white)
{
    if (!isValid(red, green, blue, white)) {
        return false;
    }
    // outside of XYZ definitely can't be shown on a display
    // TODO maybe calculate if all values are within the human-visible gamut too?
    if (red.x < 0 || red.x > 1
        || red.y < 0 || red.y > 1
        || green.x < 0 || green.x > 1
        || green.y < 0 || green.y > 1
        || blue.x < 0 || blue.x > 1
        || blue.y < 0 || blue.y > 1
        || white.x < 0 || white.x > 1
        || white.y < 0 || white.y > 1) {
        return false;
    }
    return true;
}

Colorimetry::Colorimetry(XYZ red, XYZ green, XYZ blue, XYZ white)
    : m_red(red)
    , m_green(green)
    , m_blue(blue)
    , m_white(white)
    , m_toXYZ(calculateToXYZMatrix(red, green, blue, white))
    , m_fromXYZ(m_toXYZ.inverted())
{
}

Colorimetry::Colorimetry(xyY red, xyY green, xyY blue, xyY white)
    : Colorimetry(red.toXYZ(), green.toXYZ(), blue.toXYZ(), white.toXYZ())
{
}

Colorimetry::Colorimetry(xy red, xy green, xy blue, xy white)
    : m_white(xyY(white.x, white.y, 1.0).toXYZ())
{
    const auto brightness = (matrixFromColumns(
                                 xyY(red.x, red.y, 1.0).toXYZ().asVector(),
                                 xyY(green.x, green.y, 1.0).toXYZ().asVector(),
                                 xyY(blue.x, blue.y, 1.0).toXYZ().asVector()))
                                .inverted()
        * xyY(white.x, white.y, 1.0).toXYZ().asVector();
    m_red = xyY(red.x, red.y, brightness.x()).toXYZ();
    m_green = xyY(green.x, green.y, brightness.y()).toXYZ();
    m_blue = xyY(blue.x, blue.y, brightness.z()).toXYZ();
    m_toXYZ = calculateToXYZMatrix(m_red, m_green, m_blue, m_white);
    m_fromXYZ = m_toXYZ.inverted();
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

Colorimetry Colorimetry::adaptedTo(xyY newWhitepoint) const
{
    const auto mat = chromaticAdaptationMatrix(this->white(), newWhitepoint.toXYZ());
    return Colorimetry{
        XYZ::fromVector(mat * red().asVector()),
        XYZ::fromVector(mat * green().asVector()),
        XYZ::fromVector(mat * blue().asVector()),
        newWhitepoint.toXYZ(),
    };
}

Colorimetry Colorimetry::withWhitepoint(xyY newWhitePoint) const
{
    newWhitePoint.Y = 1;
    return Colorimetry{
        m_red,
        m_green,
        m_blue,
        newWhitePoint.toXYZ(),
    };
}

QMatrix4x4 Colorimetry::relativeColorimetricTo(const Colorimetry &other) const
{
    return other.fromXYZ() * chromaticAdaptationMatrix(white(), other.white()) * toXYZ();
}

bool Colorimetry::operator==(const Colorimetry &other) const
{
    return red() == other.red() && green() == other.green() && blue() == other.blue() && white() == other.white();
}

bool Colorimetry::operator==(NamedColorimetry name) const
{
    return *this == fromName(name);
}

const XYZ &Colorimetry::red() const
{
    return m_red;
}

const XYZ &Colorimetry::green() const
{
    return m_green;
}

const XYZ &Colorimetry::blue() const
{
    return m_blue;
}

const XYZ &Colorimetry::white() const
{
    return m_white;
}

static const Colorimetry BT709 = Colorimetry{
    xy{0.64, 0.33},
    xy{0.30, 0.60},
    xy{0.15, 0.06},
    xy{0.3127, 0.3290},
};
static const Colorimetry PAL_M = Colorimetry{
    xy{0.67, 0.33},
    xy{0.21, 0.71},
    xy{0.14, 0.08},
    xy{0.310, 0.316},
};
static const Colorimetry PAL = Colorimetry{
    xy{0.640, 0.330},
    xy{0.290, 0.600},
    xy{0.150, 0.060},
    xy{0.3127, 0.3290},
};
static const Colorimetry NTSC = Colorimetry{
    xy{0.630, 0.340},
    xy{0.310, 0.595},
    xy{0.155, 0.070},
    xy{0.3127, 0.3290},
};
static const Colorimetry GenericFilm = Colorimetry{
    xy{0.243, 0.692},
    xy{0.145, 0.049},
    xy{0.681, 0.319},
    xy{0.310, 0.316},
};
static const Colorimetry BT2020 = Colorimetry{
    xy{0.708, 0.292},
    xy{0.170, 0.797},
    xy{0.131, 0.046},
    xy{0.3127, 0.3290},
};
static const Colorimetry CIEXYZ = Colorimetry{
    XYZ{1.0, 0.0, 0.0},
    XYZ{0.0, 1.0, 0.0},
    XYZ{0.0, 0.0, 1.0},
    xy{1.0 / 3.0, 1.0 / 3.0}.toXYZ(),
};
static const Colorimetry DCIP3 = Colorimetry{
    xy{0.680, 0.320},
    xy{0.265, 0.690},
    xy{0.150, 0.060},
    xy{0.314, 0.351},
};
static const Colorimetry DisplayP3 = Colorimetry{
    xy{0.680, 0.320},
    xy{0.265, 0.690},
    xy{0.150, 0.060},
    xy{0.3127, 0.3290},
};
static const Colorimetry AdobeRGB = Colorimetry{
    xy{0.6400, 0.3300},
    xy{0.2100, 0.7100},
    xy{0.1500, 0.0600},
    xy{0.3127, 0.3290},
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

        const double effectiveMin = std::max(minLuminance(), m_transferFunction.minLuminance);
        const double otherEffectiveMin = std::max(other.minLuminance(), other.m_transferFunction.minLuminance);

        // before color conversions, map [src min, src ref] to [0, 1]
        luminanceBefore.scale(1.0 / (referenceLuminance() - minLuminance()));
        luminanceBefore.translate(-effectiveMin, -effectiveMin, -effectiveMin);
        // afterwards, map [0, 1] again to [dst min, dst ref]
        luminanceAfter.translate(otherEffectiveMin, otherEffectiveMin, otherEffectiveMin);
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

ColorDescription ColorDescription::withWhitepoint(xyY newWhitePoint) const
{
    return ColorDescription{
        m_containerColorimetry.withWhitepoint(newWhitePoint),
        m_transferFunction,
        m_referenceLuminance,
        m_minLuminance,
        m_maxAverageLuminance,
        m_maxHdrLuminance,
        m_masteringColorimetry ? std::optional(m_masteringColorimetry->withWhitepoint(newWhitePoint)) : std::nullopt,
        m_sdrColorimetry,
    };
}

ColorDescription ColorDescription::dimmed(double brightnessFactor) const
{
    return ColorDescription{
        m_containerColorimetry,
        m_transferFunction,
        m_referenceLuminance * brightnessFactor,
        m_minLuminance * brightnessFactor,
        m_maxAverageLuminance.transform([&](double value) {
        return value * brightnessFactor;
    }),
        m_maxHdrLuminance.transform([&](double value) {
        return value * brightnessFactor;
    }),
        m_masteringColorimetry,
        m_sdrColorimetry,
    };
}

double TransferFunction::defaultMinLuminanceFor(Type type)
{
    switch (type) {
    case Type::sRGB:
    case Type::gamma22:
    case Type::linear:
        return 0.2;
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
    case Type::linear:
        return 80;
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

QVector4D TransferFunction::encodedToNits(const QVector4D &encoded) const
{
    return QVector4D(encodedToNits(encoded.x()), encodedToNits(encoded.y()), encodedToNits(encoded.z()), encoded.w());
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

QVector4D TransferFunction::nitsToEncoded(const QVector4D &nits) const
{
    return QVector4D(nitsToEncoded(nits.x()), nitsToEncoded(nits.y()), nitsToEncoded(nits.z()), nits.w());
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
    QDebugStateSaver state(debug);
    debug.nospace() << "TransferFunction(" << tf.type << ", [" << tf.minLuminance << "," << tf.maxLuminance << "] )";
    return debug;
}

QDebug operator<<(QDebug debug, const KWin::XYZ &xyz)
{
    QDebugStateSaver state(debug);
    debug.nospace() << "XYZ(" << xyz.X << ", " << xyz.Y << ", " << xyz.Z << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const KWin::xyY &xyY)
{
    QDebugStateSaver state(debug);
    debug.nospace() << "xyY(" << xyY.x << ", " << xyY.y << ", " << xyY.Y << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const KWin::xy &xy)
{
    QDebugStateSaver state(debug);
    debug.nospace() << "xy(" << xy.x << ", " << xy.y << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const KWin::Colorimetry &color)
{
    QDebugStateSaver state(debug);
    debug.nospace() << "Colorimetry(" << color.red() << ", " << color.green() << ", " << color.blue() << ", " << color.white() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const KWin::ColorDescription &color)
{
    QDebugStateSaver state(debug);
    debug << "ColorDescription(" << color.containerColorimetry() << ", " << color.transferFunction() << ", ref" << color.referenceLuminance() << ", min" << color.minLuminance() << ", max. avg" << color.maxAverageLuminance() << ", max" << color.maxHdrLuminance() << ")";
    return debug;
}
