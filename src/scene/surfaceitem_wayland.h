/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

#include <unordered_map>

namespace KWin
{

class GraphicsBuffer;
class SubSurfaceInterface;
class SurfaceInterface;
class X11Window;

/**
 * The SurfaceItemWayland class represents a Wayland surface in the scene.
 */
class KWIN_EXPORT SurfaceItemWayland : public SurfaceItem
{
    Q_OBJECT

public:
    explicit SurfaceItemWayland(SurfaceInterface *surface, Scene *scene, Item *parent = nullptr);

    QVector<QRectF> shape() const override;
    QRegion opaque() const override;
    ContentType contentType() const override;

    void sendScannedOut(Output *output) override;
    void sendCopied(Output *output) override;

    SurfaceInterface *surface() const;

private Q_SLOTS:
    void handleSurfaceToBufferMatrixChanged();
    void handleSurfaceCommitted();
    void handleSurfaceSizeChanged();
    void handleBufferSizeChanged();
    void handleBufferSourceBoxChanged();
    void handleBufferTransformChanged();

    void handleChildSubSurfaceRemoved(SubSurfaceInterface *child);
    void handleChildSubSurfacesChanged();
    void handleSubSurfacePositionChanged();
    void handleSubSurfaceMappedChanged();

protected:
    std::unique_ptr<SurfacePixmap> createPixmap() override;

private:
    SurfaceItemWayland *getOrCreateSubSurfaceItem(SubSurfaceInterface *s);

    QPointer<SurfaceInterface> m_surface;
    std::unordered_map<SubSurfaceInterface *, std::unique_ptr<SurfaceItemWayland>> m_subsurfaces;
};

class KWIN_EXPORT SurfacePixmapWayland final : public SurfacePixmap
{
    Q_OBJECT

public:
    explicit SurfacePixmapWayland(SurfaceItemWayland *item, QObject *parent = nullptr);

    void create() override;
    void update() override;
    bool isValid() const override;

private:
    SurfaceItemWayland *m_item;
};

/**
 * The SurfaceItemXwayland class represents an Xwayland surface in the scene.
 */
class KWIN_EXPORT SurfaceItemXwayland : public SurfaceItemWayland
{
    Q_OBJECT

public:
    explicit SurfaceItemXwayland(X11Window *window, Scene *scene, Item *parent = nullptr);

    QVector<QRectF> shape() const override;

private:
    X11Window *m_window;
};

} // namespace KWin
