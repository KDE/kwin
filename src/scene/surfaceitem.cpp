/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem.h"
#include "compositor.h"
#include "core/graphicsbufferview.h"
#include "core/pixelgrid.h"
#include "core/renderbackend.h"
#include "opengl/eglbackend.h"
#include "opengl/gltexture.h"
#include "qpainter/qpainterbackend.h"
#include "scene/scene.h"
#include "utils/common.h"

#include <QPainter>

#include <epoxy/egl.h>
#include <utils/drm_format_helper.h>

using namespace std::chrono_literals;

namespace KWin
{

SurfaceItem::SurfaceItem(Item *parent)
    : Item(parent)
{
}

QSizeF SurfaceItem::destinationSize() const
{
    return m_destinationSize;
}

void SurfaceItem::setDestinationSize(const QSizeF &size)
{
    if (m_destinationSize != size) {
        m_destinationSize = size;
        setSize(size);
        discardQuads();
    }
}

GraphicsBuffer *SurfaceItem::buffer() const
{
    return m_bufferRef.buffer();
}

void SurfaceItem::setBuffer(GraphicsBuffer *buffer)
{
    if (buffer) {
        m_bufferRef = buffer;
        setBufferSize(buffer->size());
    } else {
        m_bufferRef = nullptr;
        setBufferSize(QSize(0, 0));
    }
}

QRectF SurfaceItem::bufferSourceBox() const
{
    return m_bufferSourceBox;
}

void SurfaceItem::setBufferSourceBox(const QRectF &box)
{
    if (m_bufferSourceBox != box) {
        m_bufferSourceBox = box;
        discardQuads();
    }
}

OutputTransform SurfaceItem::bufferTransform() const
{
    return m_surfaceToBufferTransform;
}

void SurfaceItem::setBufferTransform(OutputTransform transform)
{
    if (m_surfaceToBufferTransform != transform) {
        m_surfaceToBufferTransform = transform;
        m_bufferToSurfaceTransform = transform.inverted();
        discardQuads();
    }
}

QSize SurfaceItem::bufferSize() const
{
    return m_bufferSize;
}

void SurfaceItem::setBufferSize(const QSize &size)
{
    if (m_bufferSize != size) {
        m_bufferSize = size;
        discardQuads();
    }
}

QRegion SurfaceItem::mapFromBuffer(const QRegion &region) const
{
    const QRectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = m_destinationSize.width() / sourceBox.width();
    const qreal yScale = m_destinationSize.height() / sourceBox.height();

    QRegion result;
    for (QRectF rect : region) {
        const QRectF r = m_bufferToSurfaceTransform.map(rect, m_bufferSize).translated(-sourceBox.topLeft());
        result += QRectF(r.x() * xScale, r.y() * yScale, r.width() * xScale, r.height() * yScale).toAlignedRect();
    }
    return result;
}

static QRegion expandRegion(const QRegion &region, const QMargins &padding)
{
    if (region.isEmpty()) {
        return QRegion();
    }

    QRegion ret;
    for (const QRect &rect : region) {
        ret += rect.marginsAdded(padding);
    }

    return ret;
}

void SurfaceItem::addDamage(const QRegion &region)
{
    if (m_lastDamage) {
        const auto diff = std::chrono::steady_clock::now() - *m_lastDamage;
        m_lastDamageTimeDiffs.push_back(diff);
        if (m_lastDamageTimeDiffs.size() > 100) {
            m_lastDamageTimeDiffs.pop_front();
        }
        m_frameTimeEstimation = std::accumulate(m_lastDamageTimeDiffs.begin(), m_lastDamageTimeDiffs.end(), 0ns) / m_lastDamageTimeDiffs.size();
    }
    m_lastDamage = std::chrono::steady_clock::now();
    m_damage += region;

    const QRectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = sourceBox.width() / m_destinationSize.width();
    const qreal yScale = sourceBox.height() / m_destinationSize.height();
    const QRegion logicalDamage = mapFromBuffer(region);

    const auto views = scene()->views();
    for (RenderView *view : views) {
        QRegion viewDamage = logicalDamage;
        const qreal viewScale = view->scale();
        if (xScale != viewScale || yScale != viewScale) {
            // Simplified version of ceil(ceil(0.5 * output_scale / surface_scale) / output_scale)
            const int xPadding = std::ceil(0.5 / xScale);
            const int yPadding = std::ceil(0.5 / yScale);
            viewDamage = expandRegion(viewDamage, QMargins(xPadding, yPadding, xPadding, yPadding));
        }
        scheduleRepaint(view, viewDamage);
    }

    Q_EMIT damaged();
}

void SurfaceItem::resetDamage()
{
    m_damage = QRegion();
}

QRegion SurfaceItem::damage() const
{
    return m_damage;
}

SurfacePixmap *SurfaceItem::pixmap() const
{
    if (m_pixmap && m_pixmap->isValid()) {
        return m_pixmap.get();
    }
    return nullptr;
}

void SurfaceItem::destroyPixmap()
{
    m_pixmap.reset();
}

void SurfaceItem::preprocess()
{
    if (!m_pixmap || m_pixmap->size() != m_bufferSize) {
        m_pixmap = std::make_unique<SurfacePixmap>(this);
    }

    if (m_pixmap->isValid()) {
        m_pixmap->update();
    } else {
        m_pixmap->create();
    }

    if (m_pixmap->isValid()) {
        SurfaceTexture *surfaceTexture = m_pixmap->texture();
        if (surfaceTexture->isValid()) {
            const QRegion region = damage();
            if (!region.isEmpty()) {
                surfaceTexture->update(region);
                resetDamage();
            }
        } else {
            if (surfaceTexture->create()) {
                resetDamage();
            }
        }
    }
}

WindowQuadList SurfaceItem::buildQuads() const
{
    const QList<QRectF> region = shape();
    WindowQuadList quads;
    quads.reserve(region.count());

    const QRectF sourceBox = m_bufferToSurfaceTransform.map(m_bufferSourceBox, m_bufferSize);
    const qreal xScale = sourceBox.width() / m_destinationSize.width();
    const qreal yScale = sourceBox.height() / m_destinationSize.height();

    for (const QRectF rect : region) {
        WindowQuad quad;

        const QPointF bufferTopLeft = snapToPixelGridF(m_bufferSourceBox.topLeft() + m_surfaceToBufferTransform.map(QPointF(rect.left() * xScale, rect.top() * yScale), sourceBox.size()));
        const QPointF bufferTopRight = snapToPixelGridF(m_bufferSourceBox.topLeft() + m_surfaceToBufferTransform.map(QPointF(rect.right() * xScale, rect.top() * yScale), sourceBox.size()));
        const QPointF bufferBottomRight = snapToPixelGridF(m_bufferSourceBox.topLeft() + m_surfaceToBufferTransform.map(QPointF(rect.right() * xScale, rect.bottom() * yScale), sourceBox.size()));
        const QPointF bufferBottomLeft = snapToPixelGridF(m_bufferSourceBox.topLeft() + m_surfaceToBufferTransform.map(QPointF(rect.left() * xScale, rect.bottom() * yScale), sourceBox.size()));

        quad[0] = WindowVertex(rect.topLeft(), bufferTopLeft);
        quad[1] = WindowVertex(rect.topRight(), bufferTopRight);
        quad[2] = WindowVertex(rect.bottomRight(), bufferBottomRight);
        quad[3] = WindowVertex(rect.bottomLeft(), bufferBottomLeft);

        quads << quad;
    }

    return quads;
}

ContentType SurfaceItem::contentType() const
{
    return ContentType::None;
}

void SurfaceItem::setScanoutHint(DrmDevice *device, const QHash<uint32_t, QList<uint64_t>> &drmFormats)
{
}

void SurfaceItem::freeze()
{
}

std::chrono::nanoseconds SurfaceItem::recursiveFrameTimeEstimation() const
{
    std::chrono::nanoseconds ret = frameTimeEstimation();
    const auto children = childItems();
    for (Item *child : children) {
        ret = std::max(ret, static_cast<SurfaceItem *>(child)->frameTimeEstimation());
    }
    return ret;
}

std::chrono::nanoseconds SurfaceItem::frameTimeEstimation() const
{
    if (m_lastDamage) {
        const auto diff = std::chrono::steady_clock::now() - *m_lastDamage;
        return std::max(m_frameTimeEstimation, diff);
    } else {
        return m_frameTimeEstimation;
    }
}

std::shared_ptr<SyncReleasePoint> SurfaceItem::bufferReleasePoint() const
{
    return m_bufferReleasePoint;
}

SurfaceTexture::~SurfaceTexture()
{
}

SurfacePixmap::SurfacePixmap(SurfaceItem *item)
    : m_item(item)
{
    if (auto backend = qobject_cast<EglBackend *>(Compositor::self()->backend())) {
        m_texture = std::make_unique<OpenGLSurfaceTexture>(backend, this);
    } else if (auto backend = qobject_cast<QPainterBackend *>(Compositor::self()->backend())) {
        m_texture = std::make_unique<QPainterSurfaceTexture>(backend, this);
    }
}

void SurfacePixmap::create()
{
    update();
}

void SurfacePixmap::update()
{
    if (GraphicsBuffer *buffer = m_item->buffer()) {
        m_size = buffer->size();
        m_hasAlphaChannel = buffer->hasAlphaChannel();
        m_valid = true;
    }
}

bool SurfacePixmap::isValid() const
{
    return m_valid;
}

SurfaceItem *SurfacePixmap::item() const
{
    return m_item;
}

SurfaceTexture *SurfacePixmap::texture() const
{
    return m_texture.get();
}

bool SurfacePixmap::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

QSize SurfacePixmap::size() const
{
    return m_size;
}

OpenGLSurfaceTexture::OpenGLSurfaceTexture(EglBackend *backend, SurfacePixmap *pixmap)
    : m_backend(backend)
    , m_pixmap(pixmap)
{
}

OpenGLSurfaceTexture::~OpenGLSurfaceTexture()
{
    destroy();
}

bool OpenGLSurfaceTexture::isValid() const
{
    return m_texture.isValid();
}

OpenGLSurfaceContents OpenGLSurfaceTexture::texture() const
{
    return m_texture;
}

bool OpenGLSurfaceTexture::create()
{
    GraphicsBuffer *buffer = m_pixmap->item()->buffer();
    if (buffer->dmabufAttributes()) {
        return loadDmabufTexture(buffer);
    } else if (buffer->shmAttributes()) {
        return loadShmTexture(buffer);
    } else if (buffer->singlePixelAttributes()) {
        return loadSinglePixelTexture(buffer);
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to create OpenGLSurfaceTexture for a buffer of unknown type" << buffer;
        return false;
    }
}

void OpenGLSurfaceTexture::destroy()
{
    m_texture.reset();
    m_bufferType = BufferType::None;
}

void OpenGLSurfaceTexture::update(const QRegion &region)
{
    GraphicsBuffer *buffer = m_pixmap->item()->buffer();
    if (buffer->dmabufAttributes()) {
        updateDmabufTexture(buffer);
    } else if (buffer->shmAttributes()) {
        updateShmTexture(buffer, region);
    } else if (buffer->singlePixelAttributes()) {
        updateSinglePixelTexture(buffer);
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to update OpenGLSurfaceTexture for a buffer of unknown type" << buffer;
    }
}

bool OpenGLSurfaceTexture::loadShmTexture(GraphicsBuffer *buffer)
{
    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return false;
    }

    std::shared_ptr<GLTexture> texture = GLTexture::upload(*view.image());
    if (Q_UNLIKELY(!texture)) {
        return false;
    }

    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->setContentTransform(OutputTransform::FlipY);

    m_texture = {{texture}};

    m_bufferType = BufferType::Shm;

    return true;
}

static QRegion simplifyDamage(const QRegion &damage)
{
    if (damage.rectCount() < 3) {
        return damage;
    } else {
        return damage.boundingRect();
    }
}

void OpenGLSurfaceTexture::updateShmTexture(GraphicsBuffer *buffer, const QRegion &region)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::Shm)) {
        destroy();
        create();
        return;
    }

    const GraphicsBufferView view(buffer);
    if (Q_UNLIKELY(view.isNull())) {
        return;
    }

    m_texture.planes[0]->update(*view.image(), simplifyDamage(region));
}

bool OpenGLSurfaceTexture::loadDmabufTexture(GraphicsBuffer *buffer)
{
    auto createTexture = [](EGLImageKHR image, const QSize &size, bool isExternalOnly) -> std::shared_ptr<GLTexture> {
        if (Q_UNLIKELY(image == EGL_NO_IMAGE_KHR)) {
            qCritical(KWIN_OPENGL) << "Invalid dmabuf-based wl_buffer";
            return nullptr;
        }

        GLint target = isExternalOnly ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
        auto texture = std::make_shared<GLTexture>(target);
        texture->setSize(size);
        if (!texture->create()) {
            return nullptr;
        }
        texture->setWrapMode(GL_CLAMP_TO_EDGE);
        texture->setFilter(GL_LINEAR);
        texture->bind();
        glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(image));
        texture->unbind();
        texture->setContentTransform(OutputTransform::FlipY);
        return texture;
    };

    const auto attribs = buffer->dmabufAttributes();
    if (auto itConv = s_drmConversions.find(buffer->dmabufAttributes()->format); itConv != s_drmConversions.end()) {
        QList<std::shared_ptr<GLTexture>> textures;
        Q_ASSERT(itConv->plane.count() == uint(buffer->dmabufAttributes()->planeCount));

        for (uint plane = 0; plane < itConv->plane.count(); ++plane) {
            const auto &currentPlane = itConv->plane[plane];
            QSize size = buffer->size();
            size.rwidth() /= currentPlane.widthDivisor;
            size.rheight() /= currentPlane.heightDivisor;

            const bool isExternal = m_backend->eglDisplayObject()->isExternalOnly(currentPlane.format, attribs->modifier);
            auto t = createTexture(m_backend->importBufferAsImage(buffer, plane, currentPlane.format, size), size, isExternal);
            if (!t) {
                return false;
            }
            textures << t;
        }
        m_texture = {textures};
    } else {
        const bool isExternal = m_backend->eglDisplayObject()->isExternalOnly(attribs->format, attribs->modifier);
        auto texture = createTexture(m_backend->importBufferAsImage(buffer), buffer->size(), isExternal);
        if (!texture) {
            return false;
        }
        m_texture = {{texture}};
    }
    m_bufferType = BufferType::DmaBuf;

    return true;
}

void OpenGLSurfaceTexture::updateDmabufTexture(GraphicsBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::DmaBuf)) {
        destroy();
        create();
        return;
    }

    const GLint target = GL_TEXTURE_2D;
    if (auto itConv = s_drmConversions.find(buffer->dmabufAttributes()->format); itConv != s_drmConversions.end()) {
        Q_ASSERT(itConv->plane.count() == uint(buffer->dmabufAttributes()->planeCount));
        for (uint plane = 0; plane < itConv->plane.count(); ++plane) {
            const auto &currentPlane = itConv->plane[plane];
            QSize size = buffer->size();
            size.rwidth() /= currentPlane.widthDivisor;
            size.rheight() /= currentPlane.heightDivisor;

            m_texture.planes[plane]->bind();
            glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(m_backend->importBufferAsImage(buffer, plane, currentPlane.format, size)));
            m_texture.planes[plane]->unbind();
        }
    } else {
        Q_ASSERT(m_texture.planes.count() == 1);
        m_texture.planes[0]->bind();
        glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(m_backend->importBufferAsImage(buffer)));
        m_texture.planes[0]->unbind();
    }
}

bool OpenGLSurfaceTexture::loadSinglePixelTexture(GraphicsBuffer *buffer)
{
    // TODO this shouldn't allocate a texture,
    // the renderer should just use a color in the shader
    const GraphicsBufferView view(buffer);
    std::shared_ptr<GLTexture> texture = GLTexture::upload(*view.image());
    if (Q_UNLIKELY(!texture)) {
        return false;
    }
    m_texture = {{texture}};
    m_bufferType = BufferType::SinglePixel;
    return true;
}

void OpenGLSurfaceTexture::updateSinglePixelTexture(GraphicsBuffer *buffer)
{
    if (Q_UNLIKELY(m_bufferType != BufferType::SinglePixel)) {
        destroy();
        create();
        return;
    }
    const GraphicsBufferView view(buffer);
    m_texture.planes[0]->update(*view.image(), QRect(0, 0, 1, 1));
}

QPainterSurfaceTexture::QPainterSurfaceTexture(QPainterBackend *backend, SurfacePixmap *pixmap)
    : m_backend(backend)
    , m_pixmap(pixmap)
{
}

bool QPainterSurfaceTexture::create()
{
    const GraphicsBufferView view(m_pixmap->item()->buffer());
    if (Q_LIKELY(!view.isNull())) {
        // The buffer data is copied as the buffer interface returns a QImage
        // which doesn't own the data of the underlying wl_shm_buffer object.
        m_image = view.image()->copy();
    }
    return !m_image.isNull();
}

void QPainterSurfaceTexture::update(const QRegion &region)
{
    const GraphicsBufferView view(m_pixmap->item()->buffer());
    if (Q_UNLIKELY(view.isNull())) {
        return;
    }

    QPainter painter(&m_image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    // The buffer data is copied as the buffer interface returns a QImage
    // which doesn't own the data of the underlying wl_shm_buffer object.
    for (const QRect &rect : region) {
        painter.drawImage(rect, *view.image(), rect);
    }
}

bool QPainterSurfaceTexture::isValid() const
{
    return !m_image.isNull();
}

QPainterBackend *QPainterSurfaceTexture::backend() const
{
    return m_backend;
}

QImage QPainterSurfaceTexture::image() const
{
    return m_image;
}

} // namespace KWin

#include "moc_surfaceitem.cpp"
