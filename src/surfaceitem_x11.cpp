/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_x11.h"
#include "composite.h"
#include "scene.h"
#include "x11syncmanager.h"

namespace KWin
{

SurfaceItemX11::SurfaceItemX11(Toplevel *window, Item *parent)
    : SurfaceItem(window, parent)
{
    connect(window, &Toplevel::bufferGeometryChanged,
            this, &SurfaceItemX11::handleBufferGeometryChanged);
    connect(window, &Toplevel::geometryShapeChanged,
            this, &SurfaceItemX11::discardQuads);

    m_damageHandle = xcb_generate_id(kwinApp()->x11Connection());
    xcb_damage_create(kwinApp()->x11Connection(), m_damageHandle, window->frameId(),
                      XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);

    setSize(window->bufferGeometry().size());
}

SurfaceItemX11::~SurfaceItemX11()
{
    // destroyDamage() will be called by the associated Toplevel.
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
    m_isDamaged = false;

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

        QVector<QRect> qtRects;
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
}

void SurfaceItemX11::destroyDamage()
{
    if (m_damageHandle != XCB_NONE) {
        xcb_damage_destroy(kwinApp()->x11Connection(), m_damageHandle);
        m_damageHandle = XCB_NONE;
    }
}

void SurfaceItemX11::handleBufferGeometryChanged(Toplevel *toplevel, const QRect &old)
{
    if (toplevel->bufferGeometry().size() != old.size()) {
        discardPixmap();
    }
    setSize(toplevel->bufferGeometry().size());
}

QRegion SurfaceItemX11::shape() const
{
    const QRect clipRect = window()->clientGeometry().translated(-window()->bufferGeometry().topLeft());
    const QRegion shape = window()->shapeRegion();

    return shape & clipRect;
}

QRegion SurfaceItemX11::opaque() const
{
    return window()->opaqueRegion();
}

SurfacePixmap *SurfaceItemX11::createPixmap()
{
    return new SurfacePixmapX11(this);
}

SurfacePixmapX11::SurfacePixmapX11(SurfaceItemX11 *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->scene()->createSurfaceTextureX11(this), parent)
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
    const Toplevel *toplevel = m_item->window();
    if (toplevel->isDeleted()) {
        return;
    }

    XServerGrabber grabber;
    xcb_connection_t *connection = kwinApp()->x11Connection();
    xcb_window_t frame = toplevel->frameId();
    xcb_pixmap_t pixmap = xcb_generate_id(connection);
    xcb_void_cookie_t namePixmapCookie = xcb_composite_name_window_pixmap_checked(connection,
                                                                                  frame,
                                                                                  pixmap);
    Xcb::WindowAttributes windowAttributes(frame);
    Xcb::WindowGeometry windowGeometry(frame);
    if (xcb_generic_error_t *error = xcb_request_check(connection, namePixmapCookie)) {
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x (error code %d)",
                toplevel->window(), error->error_code);
        free(error);
        return;
    }
    // check that the received pixmap is valid and actually matches what we
    // know about the window (i.e. size)
    if (!windowAttributes || windowAttributes->map_state != XCB_MAP_STATE_VIEWABLE) {
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x (not viewable)",
                toplevel->window());
        xcb_free_pixmap(connection, pixmap);
        return;
    }
    const QRect bufferGeometry = toplevel->bufferGeometry();
    if (windowGeometry.size() != bufferGeometry.size()) {
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x (mismatched geometry)",
                toplevel->window());
        xcb_free_pixmap(connection, pixmap);
        return;
    }

    m_pixmap = pixmap;
    m_hasAlphaChannel = toplevel->hasAlpha();
    m_size = bufferGeometry.size();
    m_contentsRect = QRect(toplevel->clientPos(), toplevel->clientSize());
}

} // namespace KWin
