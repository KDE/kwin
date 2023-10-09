/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorlut3d.h"
#include "colortransformation.h"

#include <QVector3D>

namespace KWin
{

ColorLUT3D::ColorLUT3D(std::unique_ptr<ColorTransformation> &&transformation, size_t xSize, size_t ySize, size_t zSize)
    : m_transformation(std::move(transformation))
    , m_xSize(xSize)
    , m_ySize(ySize)
    , m_zSize(zSize)
{
}

size_t ColorLUT3D::xSize() const
{
    return m_xSize;
}

size_t ColorLUT3D::ySize() const
{
    return m_ySize;
}

size_t ColorLUT3D::zSize() const
{
    return m_zSize;
}

QVector3D ColorLUT3D::sample(size_t x, size_t y, size_t z)
{
    return m_transformation->transform(QVector3D(x / double(m_xSize - 1), y / double(m_ySize - 1), z / double(m_zSize - 1)));
}

}
