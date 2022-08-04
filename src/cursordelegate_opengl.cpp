/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursordelegate_opengl.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "cursor.h"
#include "kwingltexture.h"
#include "kwinglutils.h"

namespace KWin
{

CursorDelegateOpenGL::CursorDelegateOpenGL(QObject *parent)
    : RenderLayerDelegate(parent)
{
}

CursorDelegateOpenGL::~CursorDelegateOpenGL()
{
}

void CursorDelegateOpenGL::paint(RenderTarget *renderTarget, const QRegion &region)
{
    if (!region.intersects(layer()->mapToGlobal(layer()->rect()))) {
        return;
    }

    auto allocateTexture = [this]() {
        const QImage img = Cursors::self()->currentCursor()->image();
        if (img.isNull()) {
            m_cursorTextureDirty = false;
            return;
        }
        m_cursorTexture.reset(new GLTexture(img));
        m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_cursorTextureDirty = false;
    };

    // lazy init texture cursor only in case we need software rendering
    if (!m_cursorTexture) {
        allocateTexture();

        // handle shape update on case cursor image changed
        connect(Cursors::self(), &Cursors::currentCursorChanged, this, [this]() {
            m_cursorTextureDirty = true;
        });
    } else if (m_cursorTextureDirty) {
        const QImage image = Cursors::self()->currentCursor()->image();
        if (image.size() == m_cursorTexture->size()) {
            m_cursorTexture->update(image);
            m_cursorTextureDirty = false;
        } else {
            allocateTexture();
        }
    }

    const QRect cursorRect = layer()->mapToGlobal(layer()->rect());

    QMatrix4x4 mvp;
    mvp.ortho(QRect(QPoint(0, 0), renderTarget->size() / renderTarget->devicePixelRatio()));
    mvp.translate(cursorRect.x(), cursorRect.y());

    // Don't need to call GLVertexBuffer::beginFrame() and GLVertexBuffer::endOfFrame() because
    // the GLVertexBuffer::streamingBuffer() is not being used when painting cursor.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_cursorTexture->bind();
    ShaderBinder binder(ShaderTrait::MapTexture);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
    m_cursorTexture->render(region, QRect(0, 0, cursorRect.width(), cursorRect.height()), renderTarget->devicePixelRatio());
    m_cursorTexture->unbind();
    glDisable(GL_BLEND);
}

} // namespace KWin
