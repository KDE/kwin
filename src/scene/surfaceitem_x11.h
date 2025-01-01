/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

#include <xcb/damage.h>
#include <xcb/xfixes.h>

namespace KWin
{

class X11Window;

/**
 * The SurfaceItemX11 class represents an X11 surface in the scene.
 */
class KWIN_EXPORT SurfaceItemX11 : public SurfaceItem
{
    Q_OBJECT

public:
    explicit SurfaceItemX11(X11Window *window, Item *parent = nullptr);
    ~SurfaceItemX11() override;

    X11Window *window() const;

    void preprocess() override;

    void processDamage();
    bool fetchDamage();
    void waitForDamage();
    void forgetDamage();
    void destroyDamage();

    QList<QRectF> shape() const override;
    QRegion opaque() const override;

private Q_SLOTS:
    void handleBufferGeometryChanged();
    void handleShapeChanged();

protected:
    std::unique_ptr<SurfacePixmap> createPixmap() override;

private:
    X11Window *m_window;
    xcb_damage_damage_t m_damageHandle = XCB_NONE;
    xcb_xfixes_fetch_region_cookie_t m_damageCookie;
    bool m_isDamaged = false;
    bool m_havePendingDamageRegion = false;
};

class KWIN_EXPORT SurfacePixmapX11 final : public SurfacePixmap
{
    Q_OBJECT

public:
    explicit SurfacePixmapX11(SurfaceItemX11 *item);
    ~SurfacePixmapX11() override;

    xcb_pixmap_t pixmap() const;
    xcb_visualid_t visual() const;

    void create() override;
    bool isValid() const override;

private:
    xcb_pixmap_t m_pixmap = XCB_PIXMAP_NONE;
};

} // namespace KWin
