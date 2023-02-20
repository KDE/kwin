/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursorsource.h"
#include "cursor.h"
#include "wayland/clientconnection.h"
#include "wayland/shmclientbuffer.h"
#include "wayland/surface_interface.h"

namespace KWin
{

CursorSource::CursorSource(QObject *parent)
    : QObject(parent)
{
}

QImage CursorSource::image() const
{
    return m_image;
}

QSize CursorSource::size() const
{
    return m_size;
}

QPoint CursorSource::hotspot() const
{
    return m_hotspot;
}

ImageCursorSource::ImageCursorSource(QObject *parent)
    : CursorSource(parent)
{
}

void ImageCursorSource::update(const QImage &image, const QPoint &hotspot)
{
    m_image = image;
    m_size = image.size() / image.devicePixelRatio();
    m_hotspot = hotspot;
    Q_EMIT changed();
}

ShapeCursorSource::ShapeCursorSource(QObject *parent)
    : CursorSource(parent)
{
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &ShapeCursorSource::selectNextSprite);
}

QByteArray ShapeCursorSource::shape() const
{
    return m_shape;
}

void ShapeCursorSource::setShape(const QByteArray &shape)
{
    if (m_shape != shape) {
        m_shape = shape;
        refresh();
    }
}

void ShapeCursorSource::setShape(Qt::CursorShape shape)
{
    setShape(CursorShape(shape).name());
}

KXcursorTheme ShapeCursorSource::theme() const
{
    return m_theme;
}

void ShapeCursorSource::setTheme(const KXcursorTheme &theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        refresh();
    }
}

void ShapeCursorSource::refresh()
{
    m_currentSprite = -1;
    m_delayTimer.stop();

    m_sprites = m_theme.shape(m_shape);
    if (m_sprites.isEmpty()) {
        const auto alternativeNames = Cursor::cursorAlternativeNames(m_shape);
        for (const QByteArray &alternativeName : alternativeNames) {
            m_sprites = m_theme.shape(alternativeName);
            if (!m_sprites.isEmpty()) {
                break;
            }
        }
    }

    if (!m_sprites.isEmpty()) {
        selectSprite(0);
    }
}

void ShapeCursorSource::selectNextSprite()
{
    selectSprite((m_currentSprite + 1) % m_sprites.size());
}

void ShapeCursorSource::selectSprite(int index)
{
    if (m_currentSprite == index) {
        return;
    }
    const KXcursorSprite &sprite = m_sprites[index];
    m_currentSprite = index;
    m_image = sprite.data();
    m_size = m_image.size() / m_image.devicePixelRatio();
    m_hotspot = sprite.hotspot();
    if (sprite.delay().count() && m_sprites.size() > 1) {
        m_delayTimer.start(sprite.delay());
    }
    Q_EMIT changed();
}

SurfaceCursorSource::SurfaceCursorSource(QObject *parent)
    : CursorSource(parent)
{
}

KWaylandServer::SurfaceInterface *SurfaceCursorSource::surface() const
{
    return m_surface;
}

void SurfaceCursorSource::update(KWaylandServer::SurfaceInterface *surface, const QPoint &hotspot)
{
    if (!surface) {
        m_image = QImage();
        m_size = QSize(0, 0);
        m_hotspot = QPoint();
        m_surface = nullptr;
    } else {
        // TODO Plasma 6: once Xwayland cursor scaling can be done correctly, remove this
        // scaling is intentionally applied "wrong" here to make the cursor stay a consistent size even with un-scaled Xwayland:
        // - the device pixel ratio of the image is not multiplied by scaleOverride
        // - the surface size is scaled up with scaleOverride, to un-do the scaling done elsewhere
        auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(surface->buffer());
        if (buffer) {
            m_image = buffer->data().copy();
            m_image.setDevicePixelRatio(surface->bufferScale());
        } else {
            m_image = QImage();
        }
        m_size = (surface->size() * surface->client()->scaleOverride()).toSize();
        m_hotspot = hotspot;
        m_surface = surface;
    }
    Q_EMIT changed();
}

} // namespace KWin
