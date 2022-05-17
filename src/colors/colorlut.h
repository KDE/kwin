/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QVector>
#include <memory>

#include "kwin_export.h"

namespace KWin
{

class ColorTransformation;

class KWIN_EXPORT ColorLUT
{
public:
    ColorLUT(const std::shared_ptr<ColorTransformation> &transformation, size_t size);

    uint16_t *red() const;
    uint16_t *green() const;
    uint16_t *blue() const;
    size_t size() const;
    std::shared_ptr<ColorTransformation> transformation() const;

private:
    QVector<uint16_t> m_data;
    const std::shared_ptr<ColorTransformation> m_transformation;
};

}
