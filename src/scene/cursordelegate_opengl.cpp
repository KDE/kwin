/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/cursordelegate_opengl.h"
#include "compositor.h"
#include "core/output.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "cursor.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "scene/cursorscene.h"

#include <cmath>

namespace KWin
{

CursorDelegateOpenGL::CursorDelegateOpenGL(Output *output)
    : m_output(output)
{
}

CursorDelegateOpenGL::~CursorDelegateOpenGL()
{
}

void CursorDelegateOpenGL::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    if (!region.intersects(layer()->mapToGlobal(layer()->rect()).toAlignedRect())) {
        return;
    }

    // Show the rendered cursor scene on the screen.
    const QRectF cursorRect = layer()->mapToGlobal(layer()->rect());
    const double scale = m_output->scale();

    // Render the cursor scene in an offscreen render target.
    const QSize bufferSize = (Cursors::self()->currentCursor()->rect().size() * scale).toSize();
    if (!m_texture || m_texture->size() != bufferSize || m_texture->internalFormat() != renderTarget.framebuffer()->colorAttachment()->internalFormat()) {
        m_texture = GLTexture::allocate(renderTarget.framebuffer()->colorAttachment()->internalFormat(), bufferSize);
        if (!m_texture) {
            return;
        }
        m_framebuffer = std::make_unique<GLFramebuffer>(m_texture.get());
    }

    RenderTarget offscreenRenderTarget(m_framebuffer.get(), renderTarget.colorDescription());

    RenderLayer renderLayer(layer()->loop());
    renderLayer.setDelegate(std::make_unique<SceneDelegate>(Compositor::self()->cursorScene(), m_output));
    renderLayer.delegate()->prePaint();
    renderLayer.delegate()->paint(offscreenRenderTarget, infiniteRegion());
    renderLayer.delegate()->postPaint();

    QMatrix4x4 mvp = renderTarget.transform().toMatrix();
    mvp.ortho(QRectF(QPointF(0, 0), m_output->transform().map(renderTarget.size())));
    mvp.translate(std::round(cursorRect.x() * scale), std::round(cursorRect.y() * scale));

    GLFramebuffer *fbo = renderTarget.framebuffer();
    GLFramebuffer::pushFramebuffer(fbo);

    // Don't need to call GLVertexBuffer::beginFrame() and GLVertexBuffer::endOfFrame() because
    // the GLVertexBuffer::streamingBuffer() is not being used when painting cursor.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ShaderBinder binder(ShaderTrait::MapTexture);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
    m_texture->render(region, cursorRect.size(), scale);
    glDisable(GL_BLEND);

    GLFramebuffer::popFramebuffer();
}

} // namespace KWin

#include "moc_cursordelegate_opengl.cpp"
