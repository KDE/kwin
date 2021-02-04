/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_x11.h"

namespace KWin
{

SurfaceItemX11::SurfaceItemX11(Scene::Window *window, Item *parent)
    : SurfaceItem(window, parent)
{
    Toplevel *toplevel = window->window();

    connect(toplevel, &Toplevel::bufferGeometryChanged,
            this, &SurfaceItemX11::handleBufferGeometryChanged);
    connect(toplevel, &Toplevel::markedAsZombie,
            this, &SurfaceItemX11::destroyDamage);
    connect(toplevel, &Toplevel::geometryShapeChanged,
            this, &SurfaceItemX11::discardQuads);

    m_damageHandle = xcb_generate_id(kwinApp()->x11Connection());
    xcb_damage_create(kwinApp()->x11Connection(), m_damageHandle, toplevel->frameId(),
                      XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);

    setSize(toplevel->bufferGeometry().size());
}

SurfaceItemX11::~SurfaceItemX11()
{
}

void SurfaceItemX11::processDamage()
{
    m_isDamaged = true;
    scheduleRepaint();
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

QPointF SurfaceItemX11::mapToBuffer(const QPointF &point) const
{
    return point;
}

QRegion SurfaceItemX11::shape() const
{
    const Toplevel *toplevel = window()->window();
    if (window()->isShaded()) {
        return QRegion();
    }

    const QRect clipRect = toplevel->clientGeometry().translated(-toplevel->bufferGeometry().topLeft());
    const QRegion shape = toplevel->shapeRegion();

    return shape & clipRect;
}

QRegion SurfaceItemX11::opaque() const
{
    return window()->window()->opaqueRegion();
}

WindowPixmap *SurfaceItemX11::createPixmap()
{
    return window()->createWindowPixmap();
}

} // namespace KWin
