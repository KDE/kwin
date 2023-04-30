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

static QVector3D xyToXYZ(QVector2D xy)
{
    return QVector3D(xy.x() / xy.y(), 1, (1 - xy.x() - xy.y()) / xy.y());
}

QMatrix3x3 Colorimetry::toXYZ() const
{
    const auto r_xyz = xyToXYZ(red);
    const auto g_xyz = xyToXYZ(blue);
    const auto b_xyz = xyToXYZ(green);
    const auto w_xyz = xyToXYZ(white);
    const auto component_scale = inverse(matrixFromColumns(r_xyz, g_xyz, b_xyz)) * w_xyz;
    return matrixFromColumns(r_xyz * component_scale.x(), g_xyz * component_scale.y(), b_xyz * component_scale.z());
}

QMatrix3x3 Colorimetry::toOther(const Colorimetry &other) const
{
    return toXYZ() * inverse(other.toXYZ());
}

bool Colorimetry::operator==(const Colorimetry &other) const
{
    return (name || other.name) ? (name == other.name)
                                : (red == other.red && green == other.green && blue == other.blue && white == other.white);
}

constexpr Colorimetry Colorimetry::createFromName(NamedColorimetry name)
{
    switch (name) {
    case NamedColorimetry::BT709:
        return Colorimetry{
            .red = {0.64, 0.33},
            .green = {0.30, 0.60},
            .blue = {0.15, 0.06},
            .white = {0.3127, 0.3290},
            .name = name,
        };
    case NamedColorimetry::BT2020:
        return Colorimetry{
            .red = {0.708, 0.292},
            .green = {0.170, 0.797},
            .blue = {0.131, 0.046},
            .white = {0.3127, 0.3290},
            .name = name,
        };
    }
    Q_UNREACHABLE();
}

const Colorspace Colorspace::sRGB(Colorimetry::createFromName(NamedColorimetry::BT709), NamedTransferFunction::sRGB);

Colorspace::Colorspace(NamedColorimetry colorimetry, NamedTransferFunction tf)
    : m_colorimetry(Colorimetry::createFromName(colorimetry))
    , m_transferFunction(tf)
{
}

Colorspace::Colorspace(const Colorimetry &colorimetry, NamedTransferFunction tf)
    : m_colorimetry(colorimetry)
    , m_transferFunction(tf)
{
}

const Colorimetry &Colorspace::colorimetry() const
{
    return m_colorimetry;
}

NamedTransferFunction Colorspace::transferFunction() const
{
    return m_transferFunction;
}

bool Colorspace::operator==(const Colorspace &other) const
{
    return m_colorimetry == other.m_colorimetry && m_transferFunction == other.m_transferFunction;
}
}
