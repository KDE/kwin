/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "colorlut3d.h"
#include "colorspace.h"
#include "colortransformation.h"
#include "kwin_export.h"

namespace KWin
{

enum class ColorspaceType {
    LinearRGB = 0,
    NonLinearRGB,
    ICtCp,
    AnyNonRGB,
};

class IccProfile;

class KWIN_EXPORT ValueRange
{
public:
    double min = 0;
    double max = 1;

    bool operator==(const ValueRange &) const = default;
    ValueRange operator*(double mult) const;
};

class KWIN_EXPORT ColorTransferFunction
{
public:
    explicit ColorTransferFunction(TransferFunction tf);

    bool operator==(const ColorTransferFunction &) const = default;

    TransferFunction tf;
};

class KWIN_EXPORT InverseColorTransferFunction
{
public:
    explicit InverseColorTransferFunction(TransferFunction tf);

    bool operator==(const InverseColorTransferFunction &) const = default;

    TransferFunction tf;
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

class KWIN_EXPORT ColorTonemapper
{
public:
    explicit ColorTonemapper(double referenceLuminance, double maxInputLuminance, double maxOutputLuminance);

    double map(double pqEncodedLuminance) const;
    bool operator==(const ColorTonemapper &) const = default;

    double m_inputReferenceLuminance;
    double m_maxInputLuminance;
    double m_maxOutputLuminance;
    double m_inputRange;
    double m_referenceDimming;
    double m_outputReferenceLuminance;
};

class KWIN_EXPORT ColorOp
{
public:
    using Operation = std::variant<ColorTransferFunction, InverseColorTransferFunction, ColorMatrix, ColorMultiplier, ColorTonemapper, std::shared_ptr<ColorTransformation>, std::shared_ptr<ColorLUT3D>>;
    ValueRange input;
    ColorspaceType inputSpace = ColorspaceType::AnyNonRGB;
    Operation operation;
    ValueRange output;
    ColorspaceType outputSpace = ColorspaceType::AnyNonRGB;

    bool operator==(const ColorOp &) const = default;
    QVector3D apply(const QVector3D input) const;
    static QVector3D applyOperation(const ColorOp::Operation &operation, const QVector3D &input);
};

class KWIN_EXPORT ColorPipeline
{
public:
    /**
     * matrix calculations with floating point numbers can result in very small errors
     * this value is the minimum difference we actually care about; everything below
     * can and should be optimized out
     */
    static constexpr float s_maxResolution = 0.00001;

    explicit ColorPipeline();
    explicit ColorPipeline(const ValueRange &inputRange, ColorspaceType inputType);

    static ColorPipeline create(const ColorDescription &from, const ColorDescription &to, RenderingIntent intent);

    ColorPipeline merged(const ColorPipeline &onTop) const;

    bool isIdentity() const;
    bool operator==(const ColorPipeline &other) const = default;
    const ValueRange &currentOutputRange() const;
    ColorspaceType currentOutputSpace() const;
    QVector3D evaluate(const QVector3D &input) const;

    void addMultiplier(double factor);
    void addMultiplier(const QVector3D &factors);
    void addTransferFunction(TransferFunction tf, ColorspaceType outputType);
    void addInverseTransferFunction(TransferFunction tf, ColorspaceType outputType);
    void addMatrix(const QMatrix4x4 &mat, const ValueRange &output, ColorspaceType outputType);
    void addTonemapper(const Colorimetry &containerColorimetry, double referenceLuminance, double maxInputLuminance, double maxOutputLuminance);
    void add(const ColorOp &op);
    void add(const ColorPipeline &pipeline);
    void add1DLUT(const std::shared_ptr<ColorTransformation> &transform, ColorspaceType outputType);

    ValueRange inputRange;
    ColorspaceType inputSpace;
    std::vector<ColorOp> ops;
};

KWIN_EXPORT bool isFuzzyIdentity(const QMatrix4x4 &mat);
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const KWin::ColorPipeline &pipeline);
