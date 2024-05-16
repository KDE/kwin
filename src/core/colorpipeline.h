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

    bool operator==(const ColorTransferFunction &) const = default;

    NamedTransferFunction tf;
};

class KWIN_EXPORT InverseColorTransferFunction
{
public:
    explicit InverseColorTransferFunction(NamedTransferFunction tf);

    bool operator==(const InverseColorTransferFunction &) const = default;

    NamedTransferFunction tf;
};

class KWIN_EXPORT ColorMatrix
{
public:
    explicit ColorMatrix(const QMatrix4x4 &mat);

    bool operator==(const ColorMatrix &) const = default;

    QMatrix4x4 mat;
};

class KWIN_EXPORT ColorMultiplier
{
public:
    explicit ColorMultiplier(double factor, bool extendedInputOutputSpace);

    bool operator==(const ColorMultiplier &) const = default;

    double factor;
    bool extendedInputOutputSpace;
};

using ColorOp = std::variant<ColorTransferFunction, InverseColorTransferFunction, ColorMatrix, ColorMultiplier>;

class KWIN_EXPORT ColorPipeline
{
public:
    static ColorPipeline create(const ColorDescription &from, const ColorDescription &to);

    bool operator==(const ColorPipeline &other) const = default;

    std::vector<ColorOp> ops;
};
}
