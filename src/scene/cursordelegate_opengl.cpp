/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/cursordelegate_opengl.h"
#include "compositor.h"
#include "core/output.h"
#include "core/pixelgrid.h"
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

CursorDelegateOpenGL::CursorDelegateOpenGL(Scene *scene, Output *output)
    : SceneDelegate(scene, nullptr)
    , m_output(output)
{
}

CursorDelegateOpenGL::~CursorDelegateOpenGL()
{
}

void CursorDelegateOpenGL::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    const QRegion dirty = region.intersected(layer()->mapToGlobal(layer()->rect()).toAlignedRect());
    if (dirty.isEmpty()) {
        return;
    }

    // Show the rendered cursor scene on the screen.
    const QRect cursorRect = snapToPixelGrid(scaledRect(layer()->mapToGlobal(layer()->rect()), m_output->scale()));

    // Render the cursor scene in an offscreen render target.
    const QSize bufferSize = cursorRect.size();
    const GLenum bufferFormat = renderTarget.colorDescription() == ColorDescription::sRGB ? GL_RGBA8 : GL_RGBA16F;
    if (!m_texture || m_texture->size() != bufferSize || m_texture->internalFormat() != bufferFormat) {
        m_texture = GLTexture::allocate(bufferFormat, bufferSize);
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

    QMatrix4x4 mvp;
    mvp.scale(1, -1); // flip the y axis back
    mvp *= renderTarget.transform().toMatrix();
    mvp.scale(1, -1); // undo ortho() flipping the y axis
    mvp.ortho(QRectF(QPointF(0, 0), m_output->transform().map(renderTarget.size())));
    mvp.translate(cursorRect.x(), cursorRect.y());

    GLFramebuffer *fbo = renderTarget.framebuffer();
    GLFramebuffer::pushFramebuffer(fbo);

    const bool clipping = region != infiniteRegion();
    const QRegion clipRegion = clipping ? RenderViewport(m_output->rectF(), m_output->scale(), renderTarget).mapToRenderTarget(dirty) : infiniteRegion();

    if (clipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    // Don't need to call GLVertexBuffer::beginFrame() and GLVertexBuffer::endOfFrame() because
    // the GLVertexBuffer::streamingBuffer() is not being used when painting cursor.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ShaderBinder binder(ShaderTrait::MapTexture);
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
    m_texture->render(clipRegion, cursorRect.size(), clipping);
    glDisable(GL_BLEND);

    if (clipping) {
        glDisable(GL_SCISSOR_TEST);
    }

    GLFramebuffer::popFramebuffer();
}

} // namespace KWin

#include "moc_cursordelegate_opengl.cpp"
