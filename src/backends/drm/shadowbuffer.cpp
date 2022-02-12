/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "shadowbuffer.h"

#include "logging.h"
#include "drm_output.h"

namespace KWin
{

static const float vertices[] = {
   -1.0f,  1.0f,
   -1.0f, -1.0f,
    1.0f, -1.0f,

   -1.0f,  1.0f,
    1.0f, -1.0f,
    1.0f,  1.0f,
};

static const float texCoords[] = {
    0.0f,  1.0f,
    0.0f,  0.0f,
    1.0f,  0.0f,

    0.0f,  1.0f,
    1.0f,  0.0f,
    1.0f,  1.0f
};

ShadowBuffer::ShadowBuffer(const QSize &size, const GbmFormat &format)
    : m_size(size)
    , m_drmFormat(format.drmFormat)
{
    m_texture.reset(new GLTexture(internalFormat(format), size));
    m_texture->setFilter(GL_NEAREST);

    m_renderTarget.reset(new GLRenderTarget(m_texture.data()));
    if (!m_renderTarget->valid()) {
        qCCritical(KWIN_DRM) << "Error: framebuffer not complete!";
        return;
    }

    m_vbo.reset(new GLVertexBuffer(KWin::GLVertexBuffer::Static));
    m_vbo->setData(6, 2, vertices, texCoords);
}

ShadowBuffer::~ShadowBuffer()
{
}

void ShadowBuffer::render(DrmDisplayDevice *displayDevice)
{
    QMatrix4x4 mvpMatrix;
    const auto transform = displayDevice->softwareTransforms();
    if (transform & DrmPlane::Transformation::Rotate90) {
        mvpMatrix.rotate(90, 0, 0, 1);
    } else if (transform & DrmPlane::Transformation::Rotate180) {
        mvpMatrix.rotate(180, 0, 0, 1);
    } else if (transform & DrmPlane::Transformation::Rotate270) {
        mvpMatrix.rotate(270, 0, 0, 1);
    }
    if (transform & DrmPlane::Transformation::ReflectX) {
        mvpMatrix.scale(-1, 1);
    }
    if (transform & DrmPlane::Transformation::ReflectY) {
        mvpMatrix.scale(1, -1);
    }

    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvpMatrix);

    m_texture->bind();
    m_vbo->render(GL_TRIANGLES);
    ShaderManager::instance()->popShader();
}

GLRenderTarget *ShadowBuffer::renderTarget() const
{
    return m_renderTarget.data();
}

QSharedPointer<GLTexture> ShadowBuffer::texture() const
{
    return m_texture;
}

bool ShadowBuffer::isComplete() const
{
    return m_renderTarget->valid() && m_vbo;
}

uint32_t ShadowBuffer::drmFormat() const
{
    return m_drmFormat;
}

GLint ShadowBuffer::internalFormat(const GbmFormat &format) const
{
    if (format.redSize <= 8 && format.greenSize <= 8 && format.blueSize <= 8) {
        return GL_RGBA8;
    } else if (format.redSize <= 10 && format.greenSize <= 10 && format.blueSize <= 10) {
        return GL_RGB10_A2;
    } else if (format.redSize <= 12 && format.greenSize <= 12 && format.blueSize <= 12) {
        return GL_RGBA12;
    } else {
        return GL_RGBA16;
    }
}

}
