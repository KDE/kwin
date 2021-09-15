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

ShadowBuffer::ShadowBuffer(const QSize &size)
    : m_size(size)
{
    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    GLRenderTarget::setKWinFramebuffer(m_framebuffer);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        qCCritical(KWIN_DRM) << "Error: framebuffer not complete!";
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLRenderTarget::setKWinFramebuffer(0);

    m_vbo.reset(new GLVertexBuffer(KWin::GLVertexBuffer::Static));
    m_vbo->setData(6, 2, vertices, texCoords);
}

ShadowBuffer::~ShadowBuffer()
{
    glDeleteTextures(1, &m_texture);
    glDeleteFramebuffers(1, &m_framebuffer);
}

void ShadowBuffer::render(DrmAbstractOutput *output, RenderMode mode)
{
    const auto size = output->modeSize();
    glViewport(0, 0, size.width(), size.height());
    QMatrix4x4 mvpMatrix;
    ShaderTraits traits = ShaderTrait::MapTexture;
    if (mode == RenderMode::OpenGlToDrm) {
        mvpMatrix.scale(1, -1);
        traits |= ShaderTrait::RgbaToXrgb;
    }
    auto shader = ShaderManager::instance()->pushShader(traits);

    switch (output->transform()) {
    case DrmOutput::Transform::Normal:
    case DrmOutput::Transform::Flipped:
        break;
    case DrmOutput::Transform::Rotated90:
    case DrmOutput::Transform::Flipped90:
        mvpMatrix.rotate(90, 0, 0, 1);
        break;
    case DrmOutput::Transform::Rotated180:
    case DrmOutput::Transform::Flipped180:
        mvpMatrix.rotate(180, 0, 0, 1);
        break;
    case DrmOutput::Transform::Rotated270:
    case DrmOutput::Transform::Flipped270:
        mvpMatrix.rotate(270, 0, 0, 1);
        break;
    }
    switch (output->transform()) {
    case DrmOutput::Transform::Flipped:
    case DrmOutput::Transform::Flipped90:
    case DrmOutput::Transform::Flipped180:
    case DrmOutput::Transform::Flipped270:
        mvpMatrix.scale(-1, 1);
        break;
    default:
        break;
    }

    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvpMatrix);

    glBindTexture(GL_TEXTURE_2D, m_texture);
    m_vbo->render(GL_TRIANGLES);
    ShaderManager::instance()->popShader();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ShadowBuffer::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    GLRenderTarget::setKWinFramebuffer(m_framebuffer);
}

void ShadowBuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLRenderTarget::setKWinFramebuffer(0);
}

bool ShadowBuffer::isComplete() const
{
    return m_texture && m_framebuffer && m_vbo;
}

int ShadowBuffer::texture() const
{
    return m_texture;
}

QSize ShadowBuffer::textureSize() const
{
    return m_size;
}

}
