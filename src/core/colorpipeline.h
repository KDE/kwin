/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "colorspace.h"
#include "kwin_export.h"

namespace KWin
{

class KWIN_EXPORT ColorTransferFunction
{
public:
    explicit ColorTransferFunction(NamedTransferFunction tf);

    const NamedTransferFunction tf;
};

class KWIN_EXPORT InverseColorTransferFunction
{
public:
    explicit InverseColorTransferFunction(NamedTransferFunction tf);

    const NamedTransferFunction tf;
};

class KWIN_EXPORT ColorMatrix
{
public:
    explicit ColorMatrix(const QMatrix4x4 &mat);

    const QMatrix4x4 mat;
};

class KWIN_EXPORT ColorMultiplier
{
public:
    explicit ColorMultiplier(double factor);

    const double factor;
};

using ColorOp = std::variant<ColorTransferFunction, InverseColorTransferFunction, ColorMatrix, ColorMultiplier>;

class KWIN_EXPORT ColorPipeline
{
public:
    static ColorPipeline create(const ColorDescription &from, const ColorDescription &to);

    std::vector<ColorOp> ops;
};
}
