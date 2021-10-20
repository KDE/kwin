/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "surface.h"

namespace KWaylandServer
{
class ClientBuffer;
class SubSurfaceInterface;
class SurfaceInterface;
}

namespace KWin
{

class Toplevel;

/**
 * The SurfaceWayland class represents a Wayland surface.
 */
class KWIN_EXPORT SurfaceWayland : public Surface
{
    Q_OBJECT

public:
    explicit SurfaceWayland(KWaylandServer::SurfaceInterface *surface,
                            Toplevel *window, QObject *parent = nullptr);

    SurfacePixmap *createPixmap() override;
    QRegion shape() const override;
    QRegion opaque() const override;

    KWaylandServer::SurfaceInterface *surface() const;

private Q_SLOTS:
    void handleSurfaceToBufferMatrixChanged();
    void handleSurfaceCommitted();
    void handleSurfaceSizeChanged();

    void handleChildSubSurfaceRemoved(KWaylandServer::SubSurfaceInterface *child);
    void handleChildSubSurfacesChanged();
    void handleSubSurfacePositionChanged();
    void handleSubSurfaceMappedChanged();

private:
    SurfaceWayland *getOrCreateChild(KWaylandServer::SubSurfaceInterface *subsurface);

    QPointer<KWaylandServer::SurfaceInterface> m_surface;
    QHash<KWaylandServer::SubSurfaceInterface *, SurfaceWayland *> m_subsurfaces;
};

/**
 * The SurfacePixmapWayland class represents a client buffer attached to a wayland surface.
 */
class KWIN_EXPORT SurfacePixmapWayland final : public SurfacePixmap
{
    Q_OBJECT

public:
    explicit SurfacePixmapWayland(SurfaceWayland *surface, QObject *parent = nullptr);
    ~SurfacePixmapWayland() override;

    SurfaceWayland *item() const;
    KWaylandServer::SurfaceInterface *surface() const;
    KWaylandServer::ClientBuffer *buffer() const;

    void create() override;
    void update() override;
    bool isValid() const override;

private:
    void setBuffer(KWaylandServer::ClientBuffer *buffer);

    SurfaceWayland *m_item;
    KWaylandServer::ClientBuffer *m_buffer = nullptr;
};

/**
 * The SurfaceXwayland class represents an Xwayland surface in the scene.
 */
class KWIN_EXPORT SurfaceXwayland : public SurfaceWayland
{
    Q_OBJECT

public:
    explicit SurfaceXwayland(Toplevel *window, QObject *parent = nullptr);

    QRegion shape() const override;
};

} // namespace KWin
