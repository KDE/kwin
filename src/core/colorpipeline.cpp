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
    const auto range1 = ValueRange(from.minLuminance(), from.maxHdrLuminance().value_or(from.referenceLuminance()));
    ColorPipeline ret(ValueRange{
        .min = from.transferFunction().nitsToEncoded(range1.min, from.referenceLuminance()),
        .max = from.transferFunction().nitsToEncoded(range1.max, from.referenceLuminance()),
    });
    ret.addTransferFunction(from.transferFunction(), from.referenceLuminance());
    ret.addMultiplier(to.referenceLuminance() / from.referenceLuminance());

    // FIXME this assumes that the range stays the same with matrix multiplication
    // that's not necessarily true, and figuring out the actual range could be complicated..
    ret.addMatrix(from.containerColorimetry().toOther(to.containerColorimetry()), ret.currentOutputRange());

    ret.addInverseTransferFunction(to.transferFunction(), to.referenceLuminance());
    return ret;
}

ColorPipeline::ColorPipeline()
    : inputRange(ValueRange{
          .min = 0,
          .max = 1,
      })
{
}

ColorPipeline::ColorPipeline(const ValueRange &inputRange)
    : inputRange(inputRange)
{
}

const ValueRange &ColorPipeline::currentOutputRange() const
{
    return ops.empty() ? inputRange : ops.back().output;
}

void ColorPipeline::addMultiplier(double factor)
{
    addMultiplier(QVector3D(factor, factor, factor));
}

void ColorPipeline::addMultiplier(const QVector3D &factors)
{
    if (factors == QVector3D(1, 1, 1)) {
        return;
    }
    const ValueRange output{
        .min = currentOutputRange().min * std::min(factors.x(), std::min(factors.y(), factors.z())),
        .max = currentOutputRange().max * std::max(factors.x(), std::max(factors.y(), factors.z())),
    };
    if (!ops.empty()) {
        auto *lastOp = &ops.back().operation;
        if (const auto mat = std::get_if<ColorMatrix>(lastOp)) {
            mat->mat.scale(factors);
            ops.back().output = output;
            return;
        } else if (const auto mult = std::get_if<ColorMultiplier>(lastOp)) {
            mult->factors *= factors;
            if (mult->factors == QVector3D(1, 1, 1)) {
                ops.erase(ops.end() - 1);
            } else {
                ops.back().output = output;
            }
            return;
        } else if (factors.x() == factors.y() && factors.y() == factors.z()) {
            if (const auto tf = std::get_if<ColorTransferFunction>(lastOp); tf && tf->tf.isRelative()) {
                tf->referenceLuminance *= factors.x();
                ops.back().output = output;
                return;
            } else if (const auto tf = std::get_if<InverseColorTransferFunction>(lastOp); tf && tf->tf.isRelative()) {
                tf->referenceLuminance /= factors.x();
                ops.back().output = output;
                return;
            }
        }
    }
    ops.push_back(ColorOp{
        .input = currentOutputRange(),
        .operation = ColorMultiplier(factors),
        .output = output,
    });
}

void ColorPipeline::addTransferFunction(TransferFunction tf, double referenceLuminance)
{
    if (tf == TransferFunction::linear) {
        return;
    }
    if (!ops.empty()) {
        if (const auto otherTf = std::get_if<InverseColorTransferFunction>(&ops.back().operation)) {
            if (otherTf->tf == tf) {
                const double reference = otherTf->referenceLuminance;
                ops.erase(ops.end() - 1);
                addMultiplier(referenceLuminance / reference);
                return;
            }
        }
    }
    if (tf == TransferFunction::scRGB) {
        addMultiplier(80.0);
    } else {
        ops.push_back(ColorOp{
            .input = currentOutputRange(),
            .operation = ColorTransferFunction(tf, referenceLuminance),
            .output = ValueRange{
                .min = tf.encodedToNits(currentOutputRange().min, referenceLuminance),
                .max = tf.encodedToNits(currentOutputRange().max, referenceLuminance),
            },
        });
    }
}

void ColorPipeline::addInverseTransferFunction(TransferFunction tf, double referenceLuminance)
{
    if (tf == TransferFunction::linear) {
        return;
    }
    if (!ops.empty()) {
        if (const auto otherTf = std::get_if<ColorTransferFunction>(&ops.back().operation)) {
            if (otherTf->tf == tf) {
                const double reference = otherTf->referenceLuminance;
                ops.erase(ops.end() - 1);
                addMultiplier(reference / referenceLuminance);
                return;
            }
        }
    }
    if (tf == TransferFunction::scRGB) {
        addMultiplier(1.0 / 80.0);
    } else {
        ops.push_back(ColorOp{
            .input = currentOutputRange(),
            .operation = InverseColorTransferFunction(tf, referenceLuminance),
            .output = ValueRange{
                .min = tf.nitsToEncoded(currentOutputRange().min, referenceLuminance),
                .max = tf.nitsToEncoded(currentOutputRange().max, referenceLuminance),
            },
        });
    }
}

void ColorPipeline::addMatrix(const QMatrix4x4 &mat, const ValueRange &output)
{
    if (mat.isIdentity()) {
        return;
    }
    if (!ops.empty()) {
        auto *lastOp = &ops.back().operation;
        if (const auto otherMat = std::get_if<ColorMatrix>(lastOp)) {
            otherMat->mat *= mat;
            ops.back().output = output;
            return;
        } else if (const auto mult = std::get_if<ColorMultiplier>(lastOp)) {
            QMatrix4x4 scaled = mat;
            scaled.scale(mult->factors);
            ops.back() = ColorOp{
                .input = currentOutputRange(),
                .operation = ColorMatrix(scaled),
                .output = output,
            };
            return;
        }
    }
    ops.push_back(ColorOp{
        .input = currentOutputRange(),
        .operation = ColorMatrix(mat),
        .output = output,
    });
}

bool ColorPipeline::isIdentity() const
{
    return ops.empty();
}

void ColorPipeline::add(const ColorOp &op)
{
    if (const auto mat = std::get_if<ColorMatrix>(&op.operation)) {
        addMatrix(mat->mat, op.output);
    } else if (const auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
        addMultiplier(mult->factors);
    } else if (const auto tf = std::get_if<ColorTransferFunction>(&op.operation)) {
        addTransferFunction(tf->tf, tf->referenceLuminance);
    } else if (const auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
        addInverseTransferFunction(tf->tf, tf->referenceLuminance);
    }
}

ColorPipeline ColorPipeline::merge(const ColorPipeline &onTop)
{
    ColorPipeline ret{inputRange};
    ret.ops = ops;
    for (const auto &op : onTop.ops) {
        ret.add(op);
    }
    return ret;
}

ColorTransferFunction::ColorTransferFunction(TransferFunction tf, double referenceLLuminance)
    : tf(tf)
    , referenceLuminance(referenceLLuminance)
{
}

InverseColorTransferFunction::InverseColorTransferFunction(TransferFunction tf, double referenceLLuminance)
    : tf(tf)
    , referenceLuminance(referenceLLuminance)
{
}

ColorMatrix::ColorMatrix(const QMatrix4x4 &mat)
    : mat(mat)
{
}

ColorMultiplier::ColorMultiplier(const QVector3D &factors)
    : factors(factors)
{
}

ColorMultiplier::ColorMultiplier(double factor)
    : factors(factor, factor, factor)
{
}
}
