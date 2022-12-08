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

CursorDelegateOpenGL::CursorDelegateOpenGL(Output *output, QObject *parent)
    : CursorDelegate(output, parent)
{
}

CursorDelegateOpenGL::~CursorDelegateOpenGL()
{
}

void CursorDelegateOpenGL::paint(RenderTarget *renderTarget, const QRegion &region)
{
    if (region.isEmpty()) {
        return;
    }
    const QImage img = Cursors::self()->currentCursor()->image();
    auto allocateTexture = [this](const QImage &image) {
        if (image.isNull()) {
            m_cursorTexture.reset();
            m_cacheKey = 0;
        } else {
            m_cursorTexture = std::make_unique<GLTexture>(image);
            m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
            m_cacheKey = image.cacheKey();
        }
    };

    if (!m_cursorTexture) {
        allocateTexture(img);
    } else if (m_cacheKey != img.cacheKey()) {
        if (img.size() == m_cursorTexture->size()) {
            m_cursorTexture->update(img);
            m_cacheKey = img.cacheKey();
        } else {
            allocateTexture(img);
        }
    }

    if (!layer()->superlayer()) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if (!region.intersects(layer()->mapToRoot(layer()->rect()))) {
        return;
    }

    if (m_cursorTexture) {
        const QRect cursorRect = layer()->mapToRoot(layer()->rect());
        const qreal scale = renderTarget->devicePixelRatio();

        QMatrix4x4 mvp;
        mvp.ortho(QRect(QPoint(0, 0), renderTarget->size()));
        mvp.translate(cursorRect.x() * scale, cursorRect.y() * scale);

        // Don't need to call GLVertexBuffer::beginFrame() and GLVertexBuffer::endOfFrame() because
        // the GLVertexBuffer::streamingBuffer() is not being used when painting cursor.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_cursorTexture->bind();
        ShaderBinder binder(ShaderTrait::MapTexture);
        binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
        m_cursorTexture->render(region, QRect(0, 0, cursorRect.width(), cursorRect.height()), scale);
        m_cursorTexture->unbind();
        glDisable(GL_BLEND);
    }
}

} // namespace KWin
