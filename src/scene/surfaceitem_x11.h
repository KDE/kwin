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

class Deleted;

/**
 * The SurfaceItemX11 class represents an X11 surface in the scene.
 */
class KWIN_EXPORT SurfaceItemX11 : public SurfaceItem
{
    Q_OBJECT

public:
    explicit SurfaceItemX11(Window *window, Scene *scene, Item *parent = nullptr);
    ~SurfaceItemX11() override;

    Window *window() const;

    void preprocess() override;

    void processDamage();
    bool fetchDamage();
    void waitForDamage();
    void destroyDamage();

    QVector<QRectF> shape() const override;
    QRegion opaque() const override;

private Q_SLOTS:
    void handleBufferGeometryChanged(Window *window, const QRectF &old);
    void handleGeometryShapeChanged();
    void handleWindowClosed(Window *original, Deleted *deleted);

protected:
    std::unique_ptr<SurfacePixmap> createPixmap() override;

private:
    Window *m_window;
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
