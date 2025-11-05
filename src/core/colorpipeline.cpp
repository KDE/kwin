/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpipeline.h"
#include "iccprofile.h"

#include <numbers>

namespace KWin
{

ValueRange ValueRange::operator*(double mult) const
{
    return ValueRange{
        .min = min * mult,
        .max = max * mult,
    };
}

static bool s_disableTonemapping = qEnvironmentVariableIntValue("KWIN_DISABLE_TONEMAPPING") == 1;

static ValueRange getValueRange(std::initializer_list<QVector3D> &&vectors)
{
    auto it = vectors.begin();
    ValueRange ret{
        .min = std::min({it->x(), it->y(), it->z()}),
        .max = std::max({it->x(), it->y(), it->z()}),
    };
    for (it = it + 1; it != vectors.end(); it++) {
        ret.min = std::min<double>({ret.min, it->x(), it->y(), it->z()});
        ret.max = std::max<double>({ret.max, it->x(), it->y(), it->z()});
    }
    return ret;
}

ColorPipeline ColorPipeline::create(const std::shared_ptr<ColorDescription> &from, const std::shared_ptr<ColorDescription> &to, RenderingIntent intent)
{
    // to figure out the most extreme RGB values that could be used, we check the extreme values
    // of the mastering display colorimetry - black, red, green, blue and white
    // NOTE that the mastering display white point may differ from the container white point!
    const double minLum = from->minLuminance();
    const double maxLum = from->maxHdrLuminance().value_or(from->referenceLuminance());
    const auto mdBlackPoint = from->masteringColorimetry().white() * minLum;
    const auto &fromXYZ = from->containerColorimetry().fromXYZ();
    const QVector3D mdBlack = fromXYZ * mdBlackPoint.asVector();
    const QVector3D mdRed = fromXYZ * (from->masteringColorimetry().red() * (maxLum - minLum) + mdBlackPoint).asVector();
    const QVector3D mdGreen = fromXYZ * (from->masteringColorimetry().green() * (maxLum - minLum) + mdBlackPoint).asVector();
    const QVector3D mdBlue = fromXYZ * (from->masteringColorimetry().blue() * (maxLum - minLum) + mdBlackPoint).asVector();
    const QVector3D mdWhite = fromXYZ * (from->masteringColorimetry().white() * maxLum).asVector();

    const auto range1 = getValueRange({mdBlack, mdRed, mdGreen, mdBlue, mdWhite});
    const double maxOutputLuminance = to->maxHdrLuminance().value_or(to->referenceLuminance());
    const auto rgbInputSpace = from->transferFunction().type == TransferFunction::linear ? ColorspaceType::LinearRGB : ColorspaceType::NonLinearRGB;
    ColorPipeline ret(ValueRange{
                          .min = from->transferFunction().nitsToEncoded(range1.min),
                          .max = from->transferFunction().nitsToEncoded(range1.max),
                      },
                      rgbInputSpace);
    ret.addTransferFunction(from->transferFunction(), ColorspaceType::LinearRGB);

    const QMatrix4x4 toOther = from->toOther(*to, intent);
    const QVector3D black = toOther.map(mdBlack);
    const QVector3D red = toOther.map(mdRed);
    const QVector3D green = toOther.map(mdGreen);
    const QVector3D blue = toOther.map(mdBlue);
    const QVector3D white = toOther.map(mdWhite);

    ret.addMatrix(toOther, getValueRange({black, red, green, blue, white}), ColorspaceType::LinearRGB);
    if (!s_disableTonemapping && ret.currentOutputRange().max > maxOutputLuminance * 1.01 && intent == RenderingIntent::Perceptual) {
        ret.addTonemapper(to->containerColorimetry(), to->referenceLuminance(), ret.currentOutputRange().max, maxOutputLuminance);
    }

    ret.addInverseTransferFunction(to->transferFunction(), to->transferFunction().type == TransferFunction::linear ? ColorspaceType::LinearRGB : ColorspaceType::NonLinearRGB);
    return ret;
}

ColorPipeline::ColorPipeline()
    : inputRange(ValueRange{
          .min = 0,
          .max = 1,
      })
    , inputSpace(ColorspaceType::AnyNonRGB)
{
}

ColorPipeline::ColorPipeline(const ValueRange &inputRange, ColorspaceType inputSpace)
    : inputRange(inputRange)
    , inputSpace(inputSpace)
{
}

const ValueRange &ColorPipeline::currentOutputRange() const
{
    return ops.empty() ? inputRange : ops.back().output;
}

ColorspaceType ColorPipeline::currentOutputSpace() const
{
    return ops.empty() ? inputSpace : ops.back().outputSpace;
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
            QMatrix4x4 newMat;
            newMat.scale(factors);
            newMat *= mat->mat;
            ops.erase(ops.end() - 1);
            addMatrix(newMat, output, currentOutputSpace());
            return;
        } else if (const auto mult = std::get_if<ColorMultiplier>(lastOp)) {
            mult->factors *= factors;
            if ((mult->factors - QVector3D(1, 1, 1)).lengthSquared() < s_maxResolution * s_maxResolution) {
                ops.erase(ops.end() - 1);
            } else {
                ops.back().output = output;
            }
            return;
        } else if (std::abs(factors.x() - factors.y()) < s_maxResolution && std::abs(factors.x() - factors.z()) < s_maxResolution) {
            if (const auto tf = std::get_if<ColorTransferFunction>(lastOp); tf && tf->tf.hasLinearMinLuminance()) {
                tf->tf.minLuminance *= factors.x();
                tf->tf.maxLuminance *= factors.x();
                ops.back().output = output;
                return;
            } else if (const auto tf = std::get_if<InverseColorTransferFunction>(lastOp); tf && tf->tf.hasLinearMinLuminance()) {
                tf->tf.minLuminance /= factors.x();
                tf->tf.maxLuminance /= factors.x();
                ops.back().output = output;
                return;
            }
        }
    }
    ops.push_back(ColorOp{
        .input = currentOutputRange(),
        .inputSpace = currentOutputSpace(),
        .operation = ColorMultiplier(factors),
        .output = output,
        .outputSpace = currentOutputSpace(),
    });
}

void ColorPipeline::addTransferFunction(TransferFunction tf, ColorspaceType outputType)
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
                       },
                  outputType);
    } else {
        ops.push_back(ColorOp{
            .input = currentOutputRange(),
            .inputSpace = currentOutputSpace(),
            .operation = ColorTransferFunction(tf),
            .output = ValueRange{
                .min = tf.encodedToNits(currentOutputRange().min),
                .max = tf.encodedToNits(currentOutputRange().max),
            },
            .outputSpace = outputType,
        });
    }
}

void ColorPipeline::addInverseTransferFunction(TransferFunction tf, ColorspaceType outputType)
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
                       },
                  outputType);
    } else {
        ops.push_back(ColorOp{
            .input = currentOutputRange(),
            .inputSpace = currentOutputSpace(),
            .operation = InverseColorTransferFunction(tf),
            .output = ValueRange{
                .min = tf.nitsToEncoded(currentOutputRange().min),
                .max = tf.nitsToEncoded(currentOutputRange().max),
            },
            .outputSpace = outputType,
        });
    }
}

bool isFuzzyIdentity(const QMatrix4x4 &mat)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const float targetValue = i == j ? 1 : 0;
            if (std::abs(mat(i, j) - targetValue) > ColorPipeline::s_maxResolution) {
                return false;
            }
        }
    }
    return true;
}

static bool isFuzzyScalingOnly(const QMatrix4x4 &mat)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i == j) {
                continue;
            }
            if (std::abs(mat(i, j)) > ColorPipeline::s_maxResolution) {
                return false;
            }
        }
    }
    return true;
}

void ColorPipeline::addMatrix(const QMatrix4x4 &mat, const ValueRange &output, ColorspaceType outputType)
{
    if (isFuzzyIdentity(mat)) {
        return;
    }
    if (!ops.empty()) {
        auto *lastOp = &ops.back().operation;
        if (const auto otherMat = std::get_if<ColorMatrix>(lastOp)) {
            const auto newMat = mat * otherMat->mat;
            ops.erase(ops.end() - 1);
            addMatrix(newMat, output, outputType);
            return;
        } else if (const auto mult = std::get_if<ColorMultiplier>(lastOp)) {
            QMatrix4x4 scaled = mat;
            scaled.scale(mult->factors);
            ops.erase(ops.end() - 1);
            addMatrix(scaled, output, outputType);
            return;
        } else if (std::abs(mat(0, 3)) >= s_maxResolution
                   && std::abs(mat(0, 3) - mat(1, 3)) < s_maxResolution
                   && std::abs(mat(1, 3) - mat(2, 3)) < s_maxResolution) {
            // there's a color-neutral offset,
            // which can be folded into transfer functions
            bool success = false;
            QMatrix4x4 inverse = mat.inverted(&success);
            if (success) {
                if (const auto tf = std::get_if<ColorTransferFunction>(lastOp); tf && tf->tf.hasLinearMinLuminance()) {
                    tf->tf.minLuminance -= inverse(0, 3);
                    tf->tf.maxLuminance -= inverse(0, 3);
                    ops.back().output.min -= inverse(0, 3);
                    ops.back().output.max -= inverse(0, 3);
                    QMatrix4x4 newMat = mat;
                    newMat(0, 3) = 0;
                    newMat(1, 3) = 0;
                    newMat(2, 3) = 0;
                    addMatrix(newMat, output, outputType);
                    return;
                } else if (const auto invTf = std::get_if<InverseColorTransferFunction>(lastOp); invTf && invTf->tf.hasLinearMinLuminance()) {
                    invTf->tf.minLuminance += inverse(0, 3);
                    invTf->tf.maxLuminance += inverse(0, 3);
                    ops.back().input.min += inverse(0, 3);
                    ops.back().input.max += inverse(0, 3);
                    QMatrix4x4 newMat = mat;
                    newMat(0, 3) = 0;
                    newMat(1, 3) = 0;
                    newMat(2, 3) = 0;
                    addMatrix(newMat, output, outputType);
                    return;
                }
            }
        }
    }
    if (isFuzzyScalingOnly(mat)) {
        // pure scaling, this can be simplified
        addMultiplier(QVector3D(mat(0, 0), mat(1, 1), mat(2, 2)));
        return;
    }
    ops.push_back(ColorOp{
        .input = currentOutputRange(),
        .inputSpace = currentOutputSpace(),
        .operation = ColorMatrix(mat),
        .output = output,
        .outputSpace = outputType,
    });
}

static const QMatrix4x4 s_toICtCp = QMatrix4x4(
    2048.0 / 4096.0, 2048.0 / 4096.0, 0.0, 0.0,
    6610.0 / 4096.0, -13613.0 / 4096.0, 7003.0 / 4096.0, 0.0,
    17933.0 / 4096.0, -17390.0 / 4096.0, -543.0 / 4096.0, 0.0,
    0.0, 0.0, 0.0, 1.0);
static const QMatrix4x4 s_fromICtCp = s_toICtCp.inverted();

void ColorPipeline::addTonemapper(const Colorimetry &containerColorimetry, double referenceLuminance, double maxInputLuminance, double maxOutputLuminance)
{
    // convert from rgb to ICtCp
    Q_ASSERT(currentOutputSpace() == ColorspaceType::LinearRGB);
    addMatrix(containerColorimetry.toLMS(), currentOutputRange(), currentOutputSpace());
    const TransferFunction PQ(TransferFunction::PerceptualQuantizer, 0, 10'000);
    addInverseTransferFunction(PQ, ColorspaceType::AnyNonRGB);
    addMatrix(s_toICtCp, currentOutputRange(), ColorspaceType::ICtCp);
    // apply the tone mapping to the intensity component
    ops.push_back(ColorOp{
        .input = currentOutputRange(),
        .operation = ColorTonemapper(referenceLuminance, maxInputLuminance, maxOutputLuminance),
        .output = ValueRange{
            .min = PQ.nitsToEncoded(currentOutputRange().min),
            .max = PQ.nitsToEncoded(maxOutputLuminance),
        },
    });
    // convert back to rgb
    addMatrix(s_fromICtCp, currentOutputRange(), ColorspaceType::AnyNonRGB);
    addTransferFunction(PQ, ColorspaceType::AnyNonRGB);
    addMatrix(containerColorimetry.fromLMS(), currentOutputRange(), ColorspaceType::LinearRGB);
    // TODO add clipping in RGB here (or gamut mapping in ICtCp), to match the shader
}

void ColorPipeline::add1DLUT(const std::shared_ptr<ColorTransformation> &transform, ColorspaceType outputType)
{
    const auto min = transform->transform(QVector3D(currentOutputRange().min, currentOutputRange().min, currentOutputRange().min));
    const auto max = transform->transform(QVector3D(currentOutputRange().max, currentOutputRange().max, currentOutputRange().max));
    ops.push_back(ColorOp{
        .input = currentOutputRange(),
        .inputSpace = currentOutputSpace(),
        .operation = transform,
        .output = ValueRange{
            .min = std::min({min.x(), min.y(), min.z()}),
            .max = std::max({max.x(), max.y(), max.z()}),
        },
        .outputSpace = outputType,
    });
}

bool ColorPipeline::isIdentity() const
{
    return ops.empty();
}

void ColorPipeline::add(const ColorOp &op)
{
    if (const auto mat = std::get_if<ColorMatrix>(&op.operation)) {
        addMatrix(mat->mat, op.output, op.outputSpace);
    } else if (const auto mult = std::get_if<ColorMultiplier>(&op.operation)) {
        addMultiplier(mult->factors);
    } else if (const auto tf = std::get_if<ColorTransferFunction>(&op.operation)) {
        addTransferFunction(tf->tf, op.outputSpace);
    } else if (const auto tf = std::get_if<InverseColorTransferFunction>(&op.operation)) {
        addInverseTransferFunction(tf->tf, op.outputSpace);
    } else {
        ops.push_back(op);
    }
}

void ColorPipeline::add(const ColorPipeline &pipeline)
{
    for (const auto &op : pipeline.ops) {
        add(op);
    }
}

ColorPipeline ColorPipeline::merged(const ColorPipeline &onTop) const
{
    ColorPipeline ret{inputRange, inputSpace};
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
        ret = op.apply(ret);
    }
    return ret;
}

bool ColorPipeline::operator==(const ColorPipeline &other) const
{
    // NOTE that this can't just use the default compiler-generated
    // comparison, as identity transformations may have different
    // input ranges, which we want to ignore here!
    return ops == other.ops;
}

QVector3D ColorOp::apply(const QVector3D input) const
{
    return applyOperation(operation, input);
}

QVector3D ColorOp::applyOperation(const ColorOp::Operation &operation, const QVector3D &input)
{
    if (const auto mat = std::get_if<ColorMatrix>(&operation)) {
        return mat->mat * input;
    } else if (const auto mult = std::get_if<ColorMultiplier>(&operation)) {
        return mult->factors * input;
    } else if (const auto tf = std::get_if<ColorTransferFunction>(&operation)) {
        return tf->tf.encodedToNits(input);
    } else if (const auto tf = std::get_if<InverseColorTransferFunction>(&operation)) {
        return tf->tf.nitsToEncoded(input);
    } else if (const auto tonemap = std::get_if<ColorTonemapper>(&operation)) {
        return QVector3D(tonemap->map(input.x()), input.y(), input.z());
    } else if (const auto transform1D = std::get_if<std::shared_ptr<ColorTransformation>>(&operation)) {
        return (*transform1D)->transform(input);
    } else if (const auto transform3D = std::get_if<std::shared_ptr<ColorLUT3D>>(&operation)) {
        return (*transform3D)->sample(input);
    } else {
        Q_UNREACHABLE();
    }
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

ColorTonemapper::ColorTonemapper(double referenceLuminance, double maxInputLuminance, double maxOutputLuminance)
    : m_referenceLuminance(referenceLuminance)
    , m_inputRange(maxInputLuminance / referenceLuminance)
    , m_outputRange(maxOutputLuminance / referenceLuminance)
    // derived from
    // outputRange = inputRange * (1 + inputRange * v) / (1 + inputRange)
    // => outputRange * (1 + inputRange) = inputRange + inputRange ^ 2 * v
    // => v = (outputRange * (1 + inputRange) - inputRange) / inputRange ^ 2
    , m_v((m_outputRange * (1 + m_inputRange) - m_inputRange) / std::pow(m_inputRange, 2))
{
}

double ColorTonemapper::map(double pqEncodedLuminance) const
{
    const double luminance = TransferFunction(TransferFunction::PerceptualQuantizer).encodedToNits(pqEncodedLuminance);

    double relativeLuminance = std::max(luminance / m_referenceLuminance, 0.0);
    // This is a modified Reinhart curve. It ensures that
    // f(0) = 0
    // f(x) <= x
    // f(inputRange) = outputRange
    // with inputRange -> infinity, f(1) = 0.5 (=at most reduces reference luminance by half)
    relativeLuminance = relativeLuminance * (1 + relativeLuminance * m_v) / (1.0 + relativeLuminance);
    return TransferFunction(TransferFunction::PerceptualQuantizer).nitsToEncoded(relativeLuminance * m_referenceLuminance);
}
}

QDebug operator<<(QDebug debug, const KWin::ColorOp &op)
{
    if (auto tf = std::get_if<KWin::ColorTransferFunction>(&op.operation)) {
        debug << tf->tf;
    } else if (auto tf = std::get_if<KWin::InverseColorTransferFunction>(&op.operation)) {
        debug << "inverse" << tf->tf;
    } else if (auto mat = std::get_if<KWin::ColorMatrix>(&op.operation)) {
        debug << mat->mat;
    } else if (auto mult = std::get_if<KWin::ColorMultiplier>(&op.operation)) {
        debug << mult->factors;
    } else if (auto tonemap = std::get_if<KWin::ColorTonemapper>(&op.operation)) {
        debug << "tonemapper(" << tonemap->m_inputRange << tonemap->m_outputRange << ")";
    } else if (std::holds_alternative<std::shared_ptr<KWin::ColorTransformation>>(op.operation)) {
        debug << "lut1d";
    } else if (std::holds_alternative<std::shared_ptr<KWin::ColorLUT3D>>(op.operation)) {
        debug << "lut3d";
    }
    return debug;
}

QDebug operator<<(QDebug debug, const KWin::ColorPipeline &pipeline)
{
    debug << "ColorPipeline(";
    for (const auto &op : pipeline.ops) {
        debug << op;
    }
    debug << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const KWin::ValueRange &range)
{
    debug << "[" << range.min << "," << range.max << "]";
    return debug;
}
