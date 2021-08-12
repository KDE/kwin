/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "surfaceitem.h"

#include <xcb/damage.h>
#include <xcb/xfixes.h>

namespace KWin
{

/**
 * The SurfaceItemX11 class represents an X11 surface in the scene.
 */
class KWIN_EXPORT SurfaceItemX11 : public SurfaceItem
{
    Q_OBJECT

public:
    explicit SurfaceItemX11(Toplevel *window, Item *parent = nullptr);
    ~SurfaceItemX11() override;

    void preprocess() override;

    void processDamage();
    bool fetchDamage();
    void waitForDamage();
    void destroyDamage();

    QRegion shape() const override;
    QRegion opaque() const override;

private Q_SLOTS:
    void handleBufferGeometryChanged(Toplevel *toplevel, const QRect &old);

protected:
    SurfacePixmap *createPixmap() override;

private:
    xcb_damage_damage_t m_damageHandle = XCB_NONE;
    xcb_xfixes_fetch_region_cookie_t m_damageCookie;
    bool m_isDamaged = false;
    bool m_havePendingDamageRegion = false;
};

class KWIN_EXPORT SurfacePixmapX11 final : public SurfacePixmap
{
    Q_OBJECT

public:
    explicit SurfacePixmapX11(SurfaceItemX11 *item, QObject *parent = nullptr);
    ~SurfacePixmapX11() override;

    xcb_pixmap_t pixmap() const;
    xcb_visualid_t visual() const;

    void create() override;
    bool isValid() const override;

private:
    SurfaceItemX11 *m_item;
    xcb_pixmap_t m_pixmap = XCB_PIXMAP_NONE;
};

} // namespace KWaylandServer
