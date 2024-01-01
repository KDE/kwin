/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/surfaceitem_x11.h"
#include "compositor_x11.h"
#include "core/renderbackend.h"
#include "x11syncmanager.h"
#include "x11window.h"

namespace KWin
{

SurfaceItemX11::SurfaceItemX11(X11Window *window, Scene *scene, Item *parent)
    : SurfaceItem(scene, parent)
    , m_window(window)
{
    connect(window, &Window::bufferGeometryChanged,
            this, &SurfaceItemX11::handleBufferGeometryChanged);
    connect(window, &X11Window::shapeChanged,
            this, &SurfaceItemX11::handleShapeChanged);

    m_damageHandle = xcb_generate_id(kwinApp()->x11Connection());
    xcb_damage_create(kwinApp()->x11Connection(), m_damageHandle, window->frameId(),
                      XCB_DAMAGE_REPORT_LEVEL_BOUNDING_BOX);

    setDestinationSize(window->bufferGeometry().size());
    setBufferSourceBox(QRectF(QPointF(0, 0), window->bufferGeometry().size()));
    setBufferSize(window->bufferGeometry().size().toSize());
}

SurfaceItemX11::~SurfaceItemX11()
{
    destroyDamage();
}

X11Window *SurfaceItemX11::window() const
{
    return m_window;
}

void SurfaceItemX11::preprocess()
{
    if (!damage().isEmpty()) {
        X11Compositor *compositor = X11Compositor::self();
        if (X11SyncManager *syncManager = compositor->syncManager()) {
            syncManager->insertWait();
        }
    }
    SurfaceItem::preprocess();
}

void SurfaceItemX11::destroyDamage()
{
    if (m_damageHandle != XCB_NONE) {
        xcb_damage_destroy(kwinApp()->x11Connection(), m_damageHandle);
        m_damageHandle = XCB_NONE;
    }
}

void SurfaceItemX11::handleBufferGeometryChanged()
{
    setDestinationSize(m_window->bufferGeometry().size());
    setBufferSourceBox(QRectF(QPointF(0, 0), m_window->bufferGeometry().size()));
    setBufferSize(m_window->bufferGeometry().size().toSize());
}

void SurfaceItemX11::handleShapeChanged()
{
    scheduleRepaint(boundingRect());
    discardQuads();
}

QList<QRectF> SurfaceItemX11::shape() const
{
    const QRectF clipRect = m_window->clientGeometry().translated(-m_window->bufferGeometry().topLeft());
    QList<QRectF> shape = m_window->shapeRegion();
    // bounded to clipRect
    for (QRectF &shapePart : shape) {
        shapePart = shapePart.intersected(clipRect);
    }
    return shape;
}

QRegion SurfaceItemX11::opaque() const
{
    QRegion shapeRegion;
    for (const QRectF &shapePart : shape()) {
        shapeRegion |= shapePart.toRect();
    }
    if (!m_window->hasAlpha()) {
        return shapeRegion;
    } else {
        return m_window->opaqueRegion() & shapeRegion;
    }
    return QRegion();
}

std::unique_ptr<SurfacePixmap> SurfaceItemX11::createPixmap()
{
    return std::make_unique<SurfacePixmapX11>(this);
}

SurfacePixmapX11::SurfacePixmapX11(SurfaceItemX11 *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->backend()->createSurfaceTextureX11(this), parent)
    , m_item(item)
{
}

SurfacePixmapX11::~SurfacePixmapX11()
{
    if (m_pixmap != XCB_PIXMAP_NONE) {
        xcb_free_pixmap(kwinApp()->x11Connection(), m_pixmap);
    }
}

bool SurfacePixmapX11::isValid() const
{
    return m_pixmap != XCB_PIXMAP_NONE;
}

xcb_pixmap_t SurfacePixmapX11::pixmap() const
{
    return m_pixmap;
}

xcb_visualid_t SurfacePixmapX11::visual() const
{
    return m_item->window()->visual();
}

void SurfacePixmapX11::create()
{
    const X11Window *window = m_item->window();
    if (window->isDeleted()) {
        return;
    }

    XServerGrabber grabber;
    xcb_connection_t *connection = kwinApp()->x11Connection();
    xcb_window_t frame = window->frameId();
    xcb_pixmap_t pixmap = xcb_generate_id(connection);
    xcb_void_cookie_t namePixmapCookie = xcb_composite_name_window_pixmap_checked(connection,
                                                                                  frame,
                                                                                  pixmap);
    Xcb::WindowAttributes windowAttributes(frame);
    Xcb::WindowGeometry windowGeometry(frame);
    if (xcb_generic_error_t *error = xcb_request_check(connection, namePixmapCookie)) {
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x (error code %d)",
                window->window(), error->error_code);
        free(error);
        return;
    }
    // check that the received pixmap is valid and actually matches what we
    // know about the window (i.e. size)
    if (!windowAttributes || windowAttributes->map_state != XCB_MAP_STATE_VIEWABLE) {
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x (not viewable)",
                window->window());
        xcb_free_pixmap(connection, pixmap);
        return;
    }
    const QRectF bufferGeometry = window->bufferGeometry();
    if (windowGeometry.size() != bufferGeometry.size()) {
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x: window size (%fx%f) != buffer size (%fx%f)", window->window(),
                windowGeometry.size().width(), windowGeometry.size().height(), bufferGeometry.width(), bufferGeometry.height());
        xcb_free_pixmap(connection, pixmap);
        return;
    }

    m_pixmap = pixmap;
    m_hasAlphaChannel = window->hasAlpha();
    // this class is only used on X11 where the logical size and
    // device pixel size is guaranteed to be the same and we can convert safely
    m_size = bufferGeometry.size().toSize();
}

} // namespace KWin

#include "moc_surfaceitem_x11.cpp"
