/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QVector>
#include <memory>

#include "kwin_export.h"

class QVector3D;

namespace KWin
{

class ColorTransformation;

class KWIN_EXPORT ColorLUT3D
{
public:
    ColorLUT3D(std::unique_ptr<ColorTransformation> &&transformation, size_t xSize, size_t ySize, size_t zSize);

    size_t xSize() const;
    size_t ySize() const;
    size_t zSize() const;

    QVector3D sample(size_t x, size_t y, size_t z);

private:
    const std::unique_ptr<ColorTransformation> m_transformation;
    const size_t m_xSize;
    const size_t m_ySize;
    const size_t m_zSize;
};

}
