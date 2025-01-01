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

SurfaceItemX11::SurfaceItemX11(X11Window *window, Item *parent)
    : SurfaceItem(parent)
    , m_window(window)
{
    connect(window, &Window::bufferGeometryChanged,
            this, &SurfaceItemX11::handleBufferGeometryChanged);
    connect(window, &X11Window::shapeChanged,
            this, &SurfaceItemX11::handleShapeChanged);

    m_damageHandle = xcb_generate_id(kwinApp()->x11Connection());
    xcb_damage_create(kwinApp()->x11Connection(), m_damageHandle, window->frameId(),
                      XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);

    // With unmanaged windows there is a race condition between the client painting the window
    // and us setting up damage tracking.  If the client wins we won't get a damage event even
    // though the window has been painted.  To avoid this we mark the whole window as damaged
    // immediately after creating the damage object.
    if (window->isUnmanaged()) {
        m_isDamaged = true;
    }

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

void SurfaceItemX11::processDamage()
{
    m_isDamaged = true;
    scheduleFrame();
}

bool SurfaceItemX11::fetchDamage()
{
    if (!m_isDamaged) {
        return false;
    }

    if (m_damageHandle == XCB_NONE) {
        return true;
    }

    xcb_xfixes_region_t region = xcb_generate_id(kwinApp()->x11Connection());
    xcb_xfixes_create_region(kwinApp()->x11Connection(), region, 0, nullptr);
    xcb_damage_subtract(kwinApp()->x11Connection(), m_damageHandle, 0, region);

    m_damageCookie = xcb_xfixes_fetch_region_unchecked(kwinApp()->x11Connection(), region);
    xcb_xfixes_destroy_region(kwinApp()->x11Connection(), region);

    m_havePendingDamageRegion = true;

    return true;
}

void SurfaceItemX11::waitForDamage()
{
    if (!m_havePendingDamageRegion) {
        return;
    }
    m_havePendingDamageRegion = false;

    xcb_xfixes_fetch_region_reply_t *reply =
        xcb_xfixes_fetch_region_reply(kwinApp()->x11Connection(), m_damageCookie, nullptr);
    if (!reply) {
        qCDebug(KWIN_CORE) << "Failed to check damage region";
        return;
    }

    const int rectCount = xcb_xfixes_fetch_region_rectangles_length(reply);
    QRegion region;

    if (rectCount > 1 && rectCount < 16) {
        xcb_rectangle_t *rects = xcb_xfixes_fetch_region_rectangles(reply);

        QList<QRect> qtRects;
        qtRects.reserve(rectCount);

        for (int i = 0; i < rectCount; ++i) {
            qtRects << QRect(rects[i].x, rects[i].y, rects[i].width, rects[i].height);
        }
        region.setRects(qtRects.constData(), rectCount);
    } else {
        region = QRect(reply->extents.x, reply->extents.y, reply->extents.width, reply->extents.height);
    }
    free(reply);

    addDamage(region);
    m_isDamaged = false;
}

void SurfaceItemX11::forgetDamage()
{
    // If the window is destroyed, we cannot destroy XDamage handle. :/
    m_isDamaged = false;
    m_damageHandle = XCB_NONE;
}

void SurfaceItemX11::destroyDamage()
{
    if (m_damageHandle != XCB_NONE) {
        m_isDamaged = false;
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
    QList<QRectF> shapeRegion;
    // bounded to clipRect
    for (QRectF &shapePart : shape) {
        shapeRegion += shapePart.intersected(clipRect);
    }
    return shapeRegion;
}

QRegion SurfaceItemX11::opaque() const
{
    QRegion shapeRegion;
    for (const QRectF &shapePart : shape()) {
        shapeRegion += shapePart.toRect();
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

SurfacePixmapX11::SurfacePixmapX11(SurfaceItemX11 *item)
    : SurfacePixmap(Compositor::self()->backend()->createSurfaceTextureX11(this), item)
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
    return static_cast<SurfaceItemX11 *>(m_item)->window()->visual();
}

void SurfacePixmapX11::create()
{
    const X11Window *window = static_cast<SurfaceItemX11 *>(m_item)->window();
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
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x: window size (%dx%d) != buffer size (%fx%f)", window->window(),
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
