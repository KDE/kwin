/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursordelegate_opengl.h"
#include "composite.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "cursor.h"
#include "kwingltexture.h"
#include "kwinglutils.h"
#include "scene/cursorscene.h"

#include <cmath>

namespace KWin
{

CursorDelegateOpenGL::~CursorDelegateOpenGL()
{
}

void CursorDelegateOpenGL::paint(RenderTarget *renderTarget, const QRegion &region)
{
    if (!region.intersects(layer()->mapToGlobal(layer()->rect()))) {
        return;
    }

    // Render the cursor scene in an offscreen render target.
    const QSize bufferSize = Cursors::self()->currentCursor()->rect().size() * renderTarget->devicePixelRatio();
    if (!m_texture || m_texture->size() != bufferSize) {
        m_texture = std::make_unique<GLTexture>(GL_RGBA8, bufferSize);
        m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
    }

    RenderTarget offscreenRenderTarget(m_framebuffer.get());
    offscreenRenderTarget.setDevicePixelRatio(renderTarget->devicePixelRatio());

    RenderLayer renderLayer(layer()->loop());
    renderLayer.setDelegate(std::make_unique<SceneDelegate>(Compositor::self()->cursorScene()));
    renderLayer.delegate()->prePaint();
    renderLayer.delegate()->paint(&offscreenRenderTarget, infiniteRegion());
    renderLayer.delegate()->postPaint();

    // Show the rendered cursor scene on the screen.
    const QRect cursorRect = layer()->mapToGlobal(layer()->rect());
    const qreal scale = renderTarget->devicePixelRatio();

    QMatrix4x4 mvp;
    mvp.ortho(QRect(QPoint(0, 0), renderTarget->size()));
    mvp.translate(std::round(cursorRect.x() * scale), std::round(cursorRect.y() * scale));

    GLFramebuffer *fbo = std::get<GLFramebuffer *>(renderTarget->nativeHandle());
    GLFramebuffer::pushFramebuffer(fbo);

    // Don't need to call GLVertexBuffer::beginFrame() and GLVertexBuffer::endOfFrame() because
    // the GLVertexBuffer::streamingBuffer() is not being used when painting cursor.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_texture->bind();
    ShaderBinder binder(ShaderTrait::MapTexture);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
    m_texture->render(region, QRect(0, 0, cursorRect.width(), cursorRect.height()), scale);
    m_texture->unbind();
    glDisable(GL_BLEND);

    GLFramebuffer::popFramebuffer();
}

} // namespace KWin
