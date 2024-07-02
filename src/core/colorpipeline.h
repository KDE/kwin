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

class KWIN_EXPORT ValueRange
{
public:
    double min = 0;
    double max = 1;

    bool operator==(const ValueRange &) const = default;
};

class KWIN_EXPORT ColorTransferFunction
{
public:
    explicit ColorTransferFunction(TransferFunction tf, double referenceLuminance);

    bool operator==(const ColorTransferFunction &) const = default;

    TransferFunction tf;
    double referenceLuminance;
};

class KWIN_EXPORT InverseColorTransferFunction
{
public:
    explicit InverseColorTransferFunction(TransferFunction tf, double referenceLuminance);

    bool operator==(const InverseColorTransferFunction &) const = default;

    TransferFunction tf;
    double referenceLuminance;
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
    explicit ColorMultiplier(double factor);
    explicit ColorMultiplier(const QVector3D &factors);

    bool operator==(const ColorMultiplier &) const = default;

    QVector3D factors;
};

class KWIN_EXPORT ColorOp
{
public:
    ValueRange input;
    std::variant<ColorTransferFunction, InverseColorTransferFunction, ColorMatrix, ColorMultiplier> operation;
    ValueRange output;

    bool operator==(const ColorOp &) const = default;
};

class KWIN_EXPORT ColorPipeline
{
public:
    explicit ColorPipeline();
    explicit ColorPipeline(const ValueRange &inputRange);

    static ColorPipeline create(const ColorDescription &from, const ColorDescription &to);

    ColorPipeline merge(const ColorPipeline &onTop);

    bool isIdentity() const;
    bool operator==(const ColorPipeline &other) const = default;
    const ValueRange &currentOutputRange() const;

    void addMultiplier(double factor);
    void addMultiplier(const QVector3D &factors);
    void addTransferFunction(TransferFunction tf, double referenceLuminance);
    void addInverseTransferFunction(TransferFunction tf, double referenceLuminance);
    void addMatrix(const QMatrix4x4 &mat, const ValueRange &output);
    void add(const ColorOp &op);

    ValueRange inputRange;
    std::vector<ColorOp> ops;
};
}
