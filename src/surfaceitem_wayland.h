/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "surfaceitem.h"

namespace KWin
{

/**
 * The SurfaceItemWayland class represents a Wayland surface in the scene.
 */
class KWIN_EXPORT SurfaceItemWayland : public SurfaceItem
{
    Q_OBJECT

public:
    explicit SurfaceItemWayland(KWaylandServer::SurfaceInterface *surface,
                                Scene::Window *window, Item *parent = nullptr);

    QPointF mapToBuffer(const QPointF &point) const override;
    QRegion shape() const override;
    QRegion opaque() const override;

    KWaylandServer::SurfaceInterface *surface() const;

private Q_SLOTS:
    void handleSurfaceCommitted();
    void handleSurfaceSizeChanged();

    void handleChildSubSurfaceAdded(KWaylandServer::SubSurfaceInterface *child);
    void handleChildSubSurfaceRemoved(KWaylandServer::SubSurfaceInterface *child);
    void handleChildSubSurfacesChanged();
    void handleSubSurfacePositionChanged();

protected:
    WindowPixmap *createPixmap() override;

private:
    QPointer<KWaylandServer::SurfaceInterface> m_surface;
    QHash<KWaylandServer::SubSurfaceInterface *, SurfaceItemWayland *> m_subsurfaces;
};

/**
 * The SurfaceItemXwayland class represents an Xwayland surface in the scene.
 */
class KWIN_EXPORT SurfaceItemXwayland : public SurfaceItemWayland
{
    Q_OBJECT

public:
    explicit SurfaceItemXwayland(Scene::Window *window, Item *parent = nullptr);

    QRegion shape() const override;
};

} // namespace KWin
