/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpipeline.h"

namespace KWin
{

ColorPipeline ColorPipeline::create(const ColorDescription &from, const ColorDescription &to, RenderingIntent intent)
{
    const auto range1 = ValueRange(from.minLuminance(), from.maxHdrLuminance().value_or(from.referenceLuminance()));
    ColorPipeline ret(ValueRange{
        .min = from.transferFunction().nitsToEncoded(range1.min),
        .max = from.transferFunction().nitsToEncoded(range1.max),
    });
    ret.addTransferFunction(from.transferFunction());
    ret.addMultiplier(to.referenceLuminance() / from.referenceLuminance());

    // FIXME this assumes that the range stays the same with matrix multiplication
    // that's not necessarily true, and figuring out the actual range could be complicated..
    ret.addMatrix(from.toOther(to, intent), ret.currentOutputRange());

    ret.addInverseTransferFunction(to.transferFunction());
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
                tf->tf.minLuminance *= factors.x();
                tf->tf.maxLuminance *= factors.x();
                ops.back().output = output;
                return;
            } else if (const auto tf = std::get_if<InverseColorTransferFunction>(lastOp); tf && tf->tf.isRelative()) {
                tf->tf.minLuminance /= factors.x();
                tf->tf.maxLuminance /= factors.x();
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

void ColorPipeline::addTransferFunction(TransferFunction tf)
{
    if (!ops.empty()) {
        if (const auto invTf = std::get_if<InverseColorTransferFunction>(&ops.back().operation)) {
            if (invTf->tf == tf) {
                ops.erase(ops.end() - 1);
                return;
            }
        }
    }
    if (tf.type == TransferFunction::linear) {
        QMatrix4x4 mat;
        mat.translate(tf.minLuminance, tf.minLuminance, tf.minLuminance);
        mat.scale(tf.maxLuminance - tf.minLuminance);
        addMatrix(mat, ValueRange{
                           .min = (mat * QVector3D(currentOutputRange().min, 0, 0)).x(),
                           .max = (mat * QVector3D(currentOutputRange().max, 0, 0)).x(),
                       });
    } else {
        ops.push_back(ColorOp{
            .input = currentOutputRange(),
            .operation = ColorTransferFunction(tf),
            .output = ValueRange{
                .min = tf.encodedToNits(currentOutputRange().min),
                .max = tf.encodedToNits(currentOutputRange().max),
            },
        });
    }
}

void ColorPipeline::addInverseTransferFunction(TransferFunction tf)
{
    if (!ops.empty()) {
        if (const auto otherTf = std::get_if<ColorTransferFunction>(&ops.back().operation)) {
            if (otherTf->tf == tf) {
                ops.erase(ops.end() - 1);
                return;
            }
        }
    }
    if (tf.type == TransferFunction::linear) {
        QMatrix4x4 mat;
        mat.scale(1.0 / (tf.maxLuminance - tf.minLuminance));
        mat.translate(-tf.minLuminance, -tf.minLuminance, -tf.minLuminance);
        addMatrix(mat, ValueRange{
                           .min = (mat * QVector3D(currentOutputRange().min, 0, 0)).x(),
                           .max = (mat * QVector3D(currentOutputRange().max, 0, 0)).x(),
                       });
    } else {
        ops.push_back(ColorOp{
            .input = currentOutputRange(),
            .operation = InverseColorTransferFunction(tf),
            .output = ValueRange{
                .min = tf.nitsToEncoded(currentOutputRange().min),
                .max = tf.nitsToEncoded(currentOutputRange().max),
            },
        });
    }
}

static bool isFuzzyIdentity(const QMatrix4x4 &mat)
{
    // matrix calculations with floating point numbers can result in very small errors
    // -> ignore them, as that just causes inefficiencies and more rounding errors
    constexpr float maxResolution = 0.0000001;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const float targetValue = i == j ? 1 : 0;
            if (std::abs(mat(i, j) - targetValue) > maxResolution) {
                return false;
            }
        }
    }
    return true;
}

static bool isFuzzyScalingOnly(const QMatrix4x4 &mat)
{
    // matrix calculations with floating point numbers can result in very small errors
    // -> ignore them, as that just causes inefficiencies and more rounding errors
    constexpr float maxResolution = 0.0000001;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i < 3 && i == j) {
                continue;
            }
            if (std::abs(mat(i, j)) > maxResolution) {
                return false;
            }
        }
    }
    return true;
}

void ColorPipeline::addMatrix(const QMatrix4x4 &mat, const ValueRange &output)
{
    if (isFuzzyIdentity(mat)) {
        return;
    }
    if (!ops.empty()) {
        auto *lastOp = &ops.back().operation;
        if (const auto otherMat = std::get_if<ColorMatrix>(lastOp)) {
            otherMat->mat = mat * otherMat->mat;
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
    if (isFuzzyScalingOnly(mat)) {
        // pure scaling, this can be simplified
        addMultiplier(QVector3D(mat(0, 0), mat(1, 1), mat(2, 2)));
        return;
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
        addTransferFunction(tf->tf);
    } else if (const auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
        addInverseTransferFunction(tf->tf);
    }
}

ColorPipeline ColorPipeline::merged(const ColorPipeline &onTop) const
{
    ColorPipeline ret{inputRange};
    ret.ops = ops;
    for (const auto &op : onTop.ops) {
        ret.add(op);
    }
    return ret;
}

QVector3D ColorPipeline::evaluate(const QVector3D &input) const
{
    QVector3D ret = input;
    for (const auto &op : ops) {
        if (const auto mat = std::get_if<ColorMatrix>(&op.operation)) {
            ret = mat->mat * ret;
        } else if (const auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
            ret *= mult->factors;
        } else if (const auto tf = std::get_if<ColorTransferFunction>(&op.operation)) {
            ret = tf->tf.encodedToNits(ret);
        } else if (const auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
            ret = tf->tf.nitsToEncoded(ret);
        }
    }
    return ret;
}

ColorTransferFunction::ColorTransferFunction(TransferFunction tf)
    : tf(tf)
{
}

InverseColorTransferFunction::InverseColorTransferFunction(TransferFunction tf)
    : tf(tf)
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

QDebug operator<<(QDebug debug, const KWin::ColorPipeline &pipeline)
{
    debug << "ColorPipeline(";
    for (const auto &op : pipeline.ops) {
        if (auto tf = std::get_if<KWin::ColorTransferFunction>(&op.operation)) {
            debug << tf->tf;
        } else if (auto tf = std::get_if<KWin::InverseColorTransferFunction>(&op.operation)) {
            debug << "inverse" << tf->tf;
        } else if (auto mat = std::get_if<KWin::ColorMatrix>(&op.operation)) {
            debug << mat->mat;
        } else if (auto mult = std::get_if<KWin::ColorMultiplier>(&op.operation)) {
            debug << mult->factors;
        }
    }
    debug << ")";
    return debug;
}
