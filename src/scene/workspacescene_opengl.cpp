/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    Based on glcompmgr code by Felix Bellaby.
    Using code from Compiz and Beryl.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "workspacescene_opengl.h"
#include "compositor.h"
#include "core/output.h"
#include "decorations/decoratedwindow.h"
#include "platformsupport/scenes/opengl/eglbackend.h"
#include "scene/itemrenderer_opengl.h"
#include "shadow.h"
#include "window.h"

#include <cmath>
#include <cstddef>

#include <QMatrix4x4>
#include <QPainter>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>
#include <QtMath>

namespace KWin
{

/************************************************
 * SceneOpenGL
 ***********************************************/

WorkspaceSceneOpenGL::WorkspaceSceneOpenGL(EglBackend *backend)
    : WorkspaceScene(std::make_unique<ItemRendererOpenGL>(backend->eglDisplayObject()))
    , m_backend(backend)
{
}

WorkspaceSceneOpenGL::~WorkspaceSceneOpenGL()
{
    openglContext()->makeCurrent();
}

EglContext *WorkspaceSceneOpenGL::openglContext() const
{
    return m_backend->openglContext();
}

std::unique_ptr<DecorationRenderer> WorkspaceSceneOpenGL::createDecorationRenderer(Decoration::DecoratedWindowImpl *impl)
{
    return std::make_unique<SceneOpenGLDecorationRenderer>(impl);
}

bool WorkspaceSceneOpenGL::animationsSupported() const
{
    const auto context = openglContext();
    return context && !context->isSoftwareRenderer();
}

std::pair<std::shared_ptr<GLTexture>, ColorDescription> WorkspaceSceneOpenGL::textureForOutput(Output *output) const
{
    return m_backend->textureForOutput(output);
}

//****************************************
// SceneOpenGL::Shadow
//****************************************

SceneOpenGLDecorationRenderer::SceneOpenGLDecorationRenderer(Decoration::DecoratedWindowImpl *client)
    : DecorationRenderer(client)
    , m_texture()
{
}

SceneOpenGLDecorationRenderer::~SceneOpenGLDecorationRenderer()
{
    if (WorkspaceScene *scene = Compositor::self()->scene()) {
        scene->openglContext()->makeCurrent();
    }
}

static void clamp_row(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::copy(src, src + width, dest + left);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp_sides(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp(QImage &image, const QRect &viewport)
{
    Q_ASSERT(image.depth() == 32);
    if (viewport.isEmpty()) {
        image = {};
        return;
    }

    const QRect rect = image.rect();

    const int left = viewport.left() - rect.left();
    const int top = viewport.top() - rect.top();
    const int right = rect.right() - viewport.right();
    const int bottom = rect.bottom() - viewport.bottom();

    const int width = rect.width() - left - right;
    const int height = rect.height() - top - bottom;

    const uint32_t *firstRow = reinterpret_cast<uint32_t *>(image.scanLine(top));
    const uint32_t *lastRow = reinterpret_cast<uint32_t *>(image.scanLine(top + height - 1));

    for (int i = 0; i < top; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(i));
        clamp_row(left, width, right, firstRow + left, dest);
    }

    for (int i = 0; i < height; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + i));
        clamp_sides(left, width, right, dest + left, dest);
    }

    for (int i = 0; i < bottom; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + height + i));
        clamp_row(left, width, right, lastRow + left, dest);
    }
}

void SceneOpenGLDecorationRenderer::render(const QRegion &region)
{
    if (areImageSizesDirty()) {
        resizeTexture();
        resetImageSizesDirty();
    }

    if (!m_texture) {
        // for invalid sizes we get no texture, see BUG 361551
        return;
    }

    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);

    const qreal devicePixelRatio = effectiveDevicePixelRatio();
    const int topHeight = std::round(top.height() * devicePixelRatio);
    const int bottomHeight = std::round(bottom.height() * devicePixelRatio);
    const int leftWidth = std::round(left.width() * devicePixelRatio);

    const QPoint topPosition(0, 0);
    const QPoint bottomPosition(0, topPosition.y() + topHeight + (2 * TexturePad));
    const QPoint leftPosition(0, bottomPosition.y() + bottomHeight + (2 * TexturePad));
    const QPoint rightPosition(0, leftPosition.y() + leftWidth + (2 * TexturePad));

    const QRect dirtyRect = region.boundingRect();

    renderPart(top.intersected(dirtyRect), top, topPosition, devicePixelRatio);
    renderPart(bottom.intersected(dirtyRect), bottom, bottomPosition, devicePixelRatio);
    renderPart(left.intersected(dirtyRect), left, leftPosition, devicePixelRatio, true);
    renderPart(right.intersected(dirtyRect), right, rightPosition, devicePixelRatio, true);
}

void SceneOpenGLDecorationRenderer::renderPart(const QRectF &rect, const QRectF &partRect,
                                               const QPoint &textureOffset,
                                               qreal devicePixelRatio, bool rotated)
{
    if (!rect.isValid() || !m_texture) {
        return;
    }
    // We allow partial decoration updates and it might just so happen that the
    // dirty region is completely contained inside the decoration part, i.e.
    // the dirty region doesn't touch any of the decoration's edges. In that
    // case, we should **not** pad the dirty region.
    const QMargins padding = texturePadForPart(rect, partRect);
    int verticalPadding = padding.top() + padding.bottom();
    int horizontalPadding = padding.left() + padding.right();

    QSize imageSize(toNativeSize(rect.width()), toNativeSize(rect.height()));
    if (rotated) {
        imageSize = QSize(imageSize.height(), imageSize.width());
    }
    QSize paddedImageSize = imageSize;
    paddedImageSize.rheight() += verticalPadding;
    paddedImageSize.rwidth() += horizontalPadding;
    QImage image(paddedImageSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(devicePixelRatio);
    image.fill(Qt::transparent);

    QRect padClip = QRect(padding.left(), padding.top(), imageSize.width(), imageSize.height());
    QPainter painter(&image);
    const qreal inverseScale = 1.0 / devicePixelRatio;
    painter.scale(inverseScale, inverseScale);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(padClip);
    painter.translate(padding.left(), padding.top());
    if (rotated) {
        painter.translate(0, imageSize.height());
        painter.rotate(-90);
    }
    painter.scale(devicePixelRatio, devicePixelRatio);
    painter.translate(-rect.topLeft());
    renderToPainter(&painter, rect);
    painter.end();

    // fill padding pixels by copying from the neighbour row
    clamp(image, padClip);

    QPoint dirtyOffset = ((rect.topLeft() - partRect.topLeft()) * devicePixelRatio).toPoint();
    if (padding.top() == 0) {
        dirtyOffset.ry() += TexturePad;
    }
    if (padding.left() == 0) {
        dirtyOffset.rx() += TexturePad;
    }
    m_texture->update(image, image.rect(), textureOffset + dirtyOffset);
}

const QMargins SceneOpenGLDecorationRenderer::texturePadForPart(
    const QRectF &rect, const QRectF &partRect)
{
    QMargins result = QMargins(0, 0, 0, 0);
    if (rect.top() == partRect.top()) {
        result.setTop(TexturePad);
    }
    if (rect.bottom() == partRect.bottom()) {
        result.setBottom(TexturePad);
    }
    if (rect.left() == partRect.left()) {
        result.setLeft(TexturePad);
    }
    if (rect.right() == partRect.right()) {
        result.setRight(TexturePad);
    }
    return result;
}

static int align(int value, int align)
{
    return (value + align - 1) & ~(align - 1);
}

void SceneOpenGLDecorationRenderer::resizeTexture()
{
    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);
    QSize size;

    size.rwidth() = toNativeSize(std::max({top.width(), bottom.width(), left.height(), right.height()}));
    size.rheight() = toNativeSize(top.height()) + toNativeSize(bottom.height()) + toNativeSize(left.width()) + toNativeSize(right.width());

    size.rheight() += 4 * (2 * TexturePad);
    size.rwidth() += 2 * TexturePad;
    size.rwidth() = align(size.width(), 128);

    if (m_texture && m_texture->size() == size) {
        return;
    }

    if (!size.isEmpty()) {
        m_texture = GLTexture::allocate(GL_RGBA8, size);
        if (!m_texture) {
            return;
        }
        m_texture->setContentTransform(OutputTransform::FlipY);
        m_texture->setFilter(GL_LINEAR);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    } else {
        m_texture.reset();
    }
}

int SceneOpenGLDecorationRenderer::toNativeSize(double size) const
{
    return std::round(size * effectiveDevicePixelRatio());
}

} // namespace

#include "moc_workspacescene_opengl.cpp"
