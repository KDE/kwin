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
#include "opengl/gltexture.h"

namespace KWin
{

static constexpr size_t lutSize = 1 << 12;

IccShader::IccShader()
    : m_shader(ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QString(), QStringLiteral(":/backends/drm/icc.frag")))
{
    m_locations = {
        .src = m_shader->uniformLocation("src"),
        .sdrBrightness = m_shader->uniformLocation("sdrBrightness"),
        .matrix1 = m_shader->uniformLocation("matrix1"),
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

static const QVector2D D50 = Colorimetry::xyzToXY(QVector3D(0.9642, 1.0, 0.8249));

bool IccShader::setProfile(const std::shared_ptr<IccProfile> &profile)
{
    if (!profile) {
        m_matrix1.setToIdentity();
        m_B.reset();
        m_matrix2.setToIdentity();
        m_M.reset();
        m_C.reset();
        m_A.reset();
        return false;
    }
    if (m_profile != profile) {
        const auto vcgt = profile->vcgt();
        QMatrix3x3 matrix1;
        std::unique_ptr<GlLookUpTable> B;
        QMatrix4x4 matrix2;
        std::unique_ptr<GlLookUpTable> M;
        std::unique_ptr<GlLookUpTable3D> C;
        std::unique_ptr<GlLookUpTable> A;
        if (const IccProfile::BToATagData *tag = profile->BtToATag()) {
            matrix1 = Colorimetry::chromaticAdaptationMatrix(profile->colorimetry().white, D50) * profile->colorimetry().toXYZ();
            if (tag->B) {
                const auto sample = [&tag](size_t x) {
                    const float relativeX = x / double(lutSize - 1);
                    return tag->B->transform(QVector3D(relativeX, relativeX, relativeX));
                };
                B = GlLookUpTable::create(sample, lutSize);
                if (!B) {
                    return false;
                }
            }
            matrix2 = tag->matrix.value_or(QMatrix4x4());
            if (tag->M) {
                const auto sample = [&tag](size_t x) {
                    const float relativeX = x / double(lutSize - 1);
                    return tag->M->transform(QVector3D(relativeX, relativeX, relativeX));
                };
                M = GlLookUpTable::create(sample, lutSize);
                if (!M) {
                    return false;
                }
            }
            if (tag->CLut) {
                const auto sample = [&tag](size_t x, size_t y, size_t z) {
                    return tag->CLut->sample(x, y, z);
                };
                C = GlLookUpTable3D::create(sample, tag->CLut->xSize(), tag->CLut->ySize(), tag->CLut->zSize());
                if (!C) {
                    return false;
                }
            }
            if (tag->A) {
                const auto sample = [&tag, vcgt](size_t x) {
                    const float relativeX = x / double(lutSize - 1);
                    QVector3D ret = tag->A->transform(QVector3D(relativeX, relativeX, relativeX));
                    if (vcgt) {
                        ret = vcgt->transform(ret);
                    }
                    return ret;
                };
                A = GlLookUpTable::create(sample, lutSize);
                if (!A) {
                    return false;
                }
            } else if (vcgt) {
                const auto sample = [&vcgt](size_t x) {
                    const float relativeX = x / double(lutSize - 1);
                    return vcgt->transform(QVector3D(relativeX, relativeX, relativeX));
                };
                A = GlLookUpTable::create(sample, lutSize);
            }
        } else {
            const auto inverseEOTF = profile->inverseEOTF();
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
        m_matrix1 = matrix1;
        m_B = std::move(B);
        m_matrix2 = matrix2;
        m_M = std::move(M);
        m_C = std::move(C);
        m_A = std::move(A);
        m_profile = profile;
    }
    return true;
}

GLShader *IccShader::shader() const
{
    return m_shader.get();
}

void IccShader::setUniforms(const std::shared_ptr<IccProfile> &profile, float sdrBrightness)
{
    // this failing can be silently ignored, it should only happen with GPU resets and gets corrected later
    setProfile(profile);

    m_shader->setUniform(m_locations.sdrBrightness, sdrBrightness);
    m_shader->setUniform(m_locations.matrix1, m_matrix1);

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
