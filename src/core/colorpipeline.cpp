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
    // attempt to go to 0-1 here
    if (from.transferFunction() == NamedTransferFunction::scRGB) {
        ret.ops.push_back(ColorMultiplier(80.0f / to.sdrBrightness()));
    } else if (from.transferFunction() != NamedTransferFunction::linear) {
        ret.ops.push_back(ColorTransferFunction(from.transferFunction()));
    }
    if (from.colorimetry() != to.colorimetry()) {
        ret.ops.push_back(ColorMatrix(from.colorimetry().toOther(to.colorimetry())));
    }
    if (to.transferFunction() == NamedTransferFunction::scRGB) {
        ret.ops.push_back(ColorMultiplier(to.sdrBrightness() / 80.0f));
    } else if (to.transferFunction() != NamedTransferFunction::linear) {
        ret.ops.push_back(InverseColorTransferFunction(to.transferFunction()));
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
}
