/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "icc_shader.h"
#include "core/colorlut3d.h"
#include "core/colortransformation.h"
#include "core/iccprofile.h"
#include "opengl/gllut.h"
#include "opengl/gllut3D.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "utils/common.h"

namespace KWin
{

static constexpr size_t lutSize = 1 << 12;

IccShader::IccShader()
    : m_shader(ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QString(), QStringLiteral(":/opengl/icc.frag")))
{
    m_locations = {
        .src = m_shader->uniformLocation("src"),
        .toXYZD50 = m_shader->uniformLocation("toXYZD50"),
        .bsize = m_shader->uniformLocation("Bsize"),
        .bsampler = m_shader->uniformLocation("Bsampler"),
        .matrix2 = m_shader->uniformLocation("matrix2"),
        .msize = m_shader->uniformLocation("Msize"),
        .msampler = m_shader->uniformLocation("Msampler"),
        .csize = m_shader->uniformLocation("Csize"),
        .csampler = m_shader->uniformLocation("Csampler"),
        .asize = m_shader->uniformLocation("Asize"),
        .asampler = m_shader->uniformLocation("Asampler"),
    };
}

IccShader::~IccShader()
{
}

bool IccShader::setProfile(const std::shared_ptr<IccProfile> &profile, const ColorDescription &inputColor, RenderingIntent intent)
{
    if (!profile) {
        m_toXYZD50.setToIdentity();
        m_B.reset();
        m_matrix2.setToIdentity();
        m_M.reset();
        m_C.reset();
        m_A.reset();
        return false;
    }
    if (m_profile != profile || m_inputColor != inputColor || m_intent != intent) {
        const auto vcgt = profile->vcgt();
        QMatrix4x4 toXYZD50;
        std::unique_ptr<GlLookUpTable> B;
        QMatrix4x4 matrix2;
        std::unique_ptr<GlLookUpTable> M;
        std::unique_ptr<GlLookUpTable3D> C;
        std::unique_ptr<GlLookUpTable> A;
        const ColorDescription linearizedInput(inputColor.containerColorimetry(), TransferFunction(TransferFunction::linear, 0, 1), 1, 0, 1, 1);
        const ColorDescription linearizedProfile(profile->colorimetry(), TransferFunction(TransferFunction::linear, 0, 1), 1, 0, 1, 1);
        if (const auto tag = profile->BToATag(intent)) {
            if (intent == RenderingIntent::AbsoluteColorimetric) {
                // There's no BToA tag for absolute colorimetric, we have to piece it together ourselves with
                // input white point -(absolute colorimetric)-> display white point
                // -(relative colorimetric)-> XYZ D50 -(BToA1, also relative colorimetric)-> display white point

                // First, transform from the input color to the display color space in absolute colorimetric mode
                const QMatrix4x4 toLinearDisplay = linearizedInput.toOther(linearizedProfile, RenderingIntent::AbsoluteColorimetric);

                // Now transform that display color space to XYZ D50 in relative colorimetric mode.
                // the BToA1 tag goes from XYZ D50 to the native white point of the display,
                // so this matrix gets reverted by it
                const QMatrix4x4 toXYZ = linearizedProfile.toOther(IccProfile::s_connectionSpace, RenderingIntent::RelativeColorimetric);

                toXYZD50 = toXYZ * toLinearDisplay;
            } else {
                toXYZD50 = linearizedInput.toOther(IccProfile::s_connectionSpace, intent);
            }
            // while the above converts to XYZ D50, the encoding the ICC profile tag
            // wants is CIEXYZ -> add the (absolute colorimetric) transform to that
            toXYZD50 = IccProfile::s_connectionSpace.containerColorimetry().toXYZ() * toXYZD50;
            auto it = tag->ops.begin();
            if (it != tag->ops.end() && std::holds_alternative<std::shared_ptr<ColorTransformation>>(it->operation)) {
                const auto sample = [&op = std::get<std::shared_ptr<ColorTransformation>>(it->operation)](size_t x) {
                    const float relativeX = x / double(lutSize - 1);
                    return op->transform(QVector3D(relativeX, relativeX, relativeX));
                };
                B = GlLookUpTable::create(sample, lutSize);
                if (!B) {
                    return false;
                }
                it++;
            }
            if (it != tag->ops.end() && std::holds_alternative<ColorMatrix>(it->operation)) {
                matrix2 = std::get<ColorMatrix>(it->operation).mat;
                it++;
            }
            if (it != tag->ops.end() && std::holds_alternative<std::shared_ptr<ColorTransformation>>(it->operation)) {
                const auto sample = [&op = std::get<std::shared_ptr<ColorTransformation>>(it->operation)](size_t x) {
                    const float relativeX = x / double(lutSize - 1);
                    return op->transform(QVector3D(relativeX, relativeX, relativeX));
                };
                M = GlLookUpTable::create(sample, lutSize);
                if (!M) {
                    return false;
                }
                it++;
            }
            if (it != tag->ops.end() && std::holds_alternative<std::shared_ptr<ColorLUT3D>>(it->operation)) {
                const auto &op = std::get<std::shared_ptr<ColorLUT3D>>(it->operation);
                const auto sample = [op](size_t x, size_t y, size_t z) {
                    return op->sample(x, y, z);
                };
                C = GlLookUpTable3D::create(sample, op->xSize(), op->ySize(), op->zSize());
                if (!C) {
                    return false;
                }
                it++;
            }
            if (it != tag->ops.end() && std::holds_alternative<std::shared_ptr<ColorTransformation>>(it->operation)) {
                const auto sample = [&op = std::get<std::shared_ptr<ColorTransformation>>(it->operation), vcgt](size_t x) {
                    const float relativeX = x / double(lutSize - 1);
                    QVector3D ret = op->transform(QVector3D(relativeX, relativeX, relativeX));
                    if (vcgt) {
                        ret = vcgt->transform(ret);
                    }
                    return ret;
                };
                A = GlLookUpTable::create(sample, lutSize);
                if (!A) {
                    return false;
                }
                it++;
            } else if (vcgt) {
                const auto sample = [&vcgt](size_t x) {
                    const float relativeX = x / double(lutSize - 1);
                    return vcgt->transform(QVector3D(relativeX, relativeX, relativeX));
                };
                A = GlLookUpTable::create(sample, lutSize);
            }
            if (it != tag->ops.end()) {
                qCCritical(KWIN_OPENGL, "Couldn't represent ICC profile in the ICC shader!");
                return false;
            }
        } else {
            toXYZD50 = linearizedInput.toOther(linearizedProfile, intent);
            const auto inverseEOTF = profile->inverseTransferFunction();
            const auto sample = [inverseEOTF, vcgt](size_t x) {
                const float relativeX = x / double(lutSize - 1);
                QVector3D ret(relativeX, relativeX, relativeX);
                ret = inverseEOTF->transform(ret);
                if (vcgt) {
                    ret = vcgt->transform(ret);
                }
                return ret;
            };
            A = GlLookUpTable::create(sample, lutSize);
            if (!A) {
                return false;
            }
        }
        m_toXYZD50 = toXYZD50;
        m_B = std::move(B);
        m_matrix2 = matrix2;
        m_M = std::move(M);
        m_C = std::move(C);
        m_A = std::move(A);
        m_profile = profile;
        m_inputColor = inputColor;
        m_intent = intent;
    }
    return true;
}

GLShader *IccShader::shader() const
{
    return m_shader.get();
}

void IccShader::setUniforms(const std::shared_ptr<IccProfile> &profile, const ColorDescription &inputColor, RenderingIntent intent)
{
    // this failing can be silently ignored, it should only happen with GPU resets and gets corrected later
    setProfile(profile, inputColor, intent);

    m_shader->setUniform(m_locations.toXYZD50, m_toXYZD50);
    m_shader->setUniform(GLShader::IntUniform::SourceNamedTransferFunction, inputColor.transferFunction().type);
    m_shader->setUniform(GLShader::Vec2Uniform::SourceTransferFunctionParams, QVector2D(inputColor.transferFunction().minLuminance, inputColor.transferFunction().maxLuminance - inputColor.transferFunction().minLuminance));
    m_shader->setUniform(GLShader::FloatUniform::SourceReferenceLuminance, inputColor.referenceLuminance());
    m_shader->setUniform(GLShader::FloatUniform::DestinationReferenceLuminance, inputColor.referenceLuminance());
    m_shader->setUniform(GLShader::FloatUniform::MaxDestinationLuminance, inputColor.referenceLuminance());

    glActiveTexture(GL_TEXTURE1);
    if (m_B) {
        m_shader->setUniform(m_locations.bsize, int(m_B->size()));
        m_shader->setUniform(m_locations.bsampler, 1);
        m_B->bind();
    } else {
        m_shader->setUniform(m_locations.bsize, 0);
        m_shader->setUniform(m_locations.bsampler, 1);
        glBindTexture(GL_TEXTURE_1D, 0);
    }

    m_shader->setUniform(m_locations.matrix2, m_matrix2);

    glActiveTexture(GL_TEXTURE2);
    if (m_M) {
        m_shader->setUniform(m_locations.msize, int(m_M->size()));
        m_shader->setUniform(m_locations.msampler, 2);
        m_M->bind();
    } else {
        m_shader->setUniform(m_locations.msize, 0);
        m_shader->setUniform(m_locations.msampler, 1);
        glBindTexture(GL_TEXTURE_1D, 0);
    }

    glActiveTexture(GL_TEXTURE3);
    if (m_C) {
        m_shader->setUniform(m_locations.csize, m_C->xSize(), m_C->ySize(), m_C->zSize());
        m_shader->setUniform(m_locations.csampler, 3);
        m_C->bind();
    } else {
        m_shader->setUniform(m_locations.csize, 0, 0, 0);
        m_shader->setUniform(m_locations.csampler, 3);
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    glActiveTexture(GL_TEXTURE4);
    if (m_A) {
        m_shader->setUniform(m_locations.asize, int(m_A->size()));
        m_shader->setUniform(m_locations.asampler, 4);
        m_A->bind();
    } else {
        m_shader->setUniform(m_locations.asize, 0);
        m_shader->setUniform(m_locations.asampler, 4);
        glBindTexture(GL_TEXTURE_1D, 0);
    }

    glActiveTexture(GL_TEXTURE0);
    m_shader->setUniform(m_locations.src, 0);
}

}
