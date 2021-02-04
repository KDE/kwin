/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "surfaceitem.h"

namespace KWin
{

/**
 * The SurfaceItemX11 class represents an X11 surface in the scene.
 */
class KWIN_EXPORT SurfaceItemX11 : public SurfaceItem
{
    Q_OBJECT

public:
    explicit SurfaceItemX11(Scene::Window *window, Item *parent = nullptr);
    ~SurfaceItemX11() override;

    void processDamage();
    bool fetchDamage();
    void waitForDamage();
    void destroyDamage();

    QPointF mapToBuffer(const QPointF &point) const override;
    QRegion shape() const override;
    QRegion opaque() const override;

private Q_SLOTS:
    void handleBufferGeometryChanged(Toplevel *toplevel, const QRect &old);

protected:
    WindowPixmap *createPixmap() override;

private:
    xcb_damage_damage_t m_damageHandle = XCB_NONE;
    xcb_xfixes_fetch_region_cookie_t m_damageCookie;
    bool m_isDamaged = false;
    bool m_havePendingDamageRegion = false;
};

} // namespace KWaylandServer
