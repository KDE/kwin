/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpipeline.h"

namespace KWin
{

ColorPipeline ColorPipeline::create(const ColorDescription &from, const ColorDescription &to)
{
    if (from.colorimetry() == to.colorimetry() && from.transferFunction() == to.transferFunction()) {
        return {};
    }
    ColorPipeline ret;
    double inputScaling = 1;
    if (from.transferFunction() == NamedTransferFunction::scRGB) {
        // attempt to go to 0-1 here
        inputScaling = from.maxHdrHighlightBrightness() == 0 ? to.sdrBrightness() : from.maxHdrHighlightBrightness();
        ret.ops.push_back(ColorMultiplier(80.0f / inputScaling, true));
    } else if (from.transferFunction() != NamedTransferFunction::linear) {
        ret.ops.push_back(ColorTransferFunction(from.transferFunction()));
    }
    if (from.colorimetry() != to.colorimetry()) {
        ret.ops.push_back(ColorMatrix(from.colorimetry().toOther(to.colorimetry())));
    }
    switch (to.transferFunction()) {
    case NamedTransferFunction::scRGB:
        ret.ops.push_back(ColorMultiplier(inputScaling / 80.0f, true));
        break;
    case NamedTransferFunction::linear:
        ret.ops.push_back(ColorMultiplier(inputScaling, true));
        break;
    case NamedTransferFunction::gamma22:
    case NamedTransferFunction::sRGB:
    case NamedTransferFunction::PerceptualQuantizer:
        if (inputScaling != to.sdrBrightness()) {
            ret.ops.push_back(ColorMultiplier(to.sdrBrightness() / inputScaling, true));
        }
        ret.ops.push_back(InverseColorTransferFunction(to.transferFunction()));
        break;
    }
    return ret;
}

ColorTransferFunction::ColorTransferFunction(NamedTransferFunction tf)
    : tf(tf)
{
}

InverseColorTransferFunction::InverseColorTransferFunction(NamedTransferFunction tf)
    : tf(tf)
{
}

ColorMatrix::ColorMatrix(const QMatrix4x4 &mat)
    : mat(mat)
{
}

ColorMultiplier::ColorMultiplier(double factor, bool extendedInputOutputSpace)
    : factor(factor)
    , extendedInputOutputSpace(extendedInputOutputSpace)
{
}
}
