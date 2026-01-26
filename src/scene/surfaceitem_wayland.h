/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

#include <QTimer>
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
    explicit SurfaceItemWayland(SurfaceInterface *surface, Item *parent = nullptr);

    QList<RectF> shape() const override;
    Region opaque() const override;
    ContentType contentType() const override;
    void setScanoutHint(DrmDevice *device, const FormatModifierMap &drmFormats) override;
    void freeze() override;

    SurfaceInterface *surface() const;

private Q_SLOTS:
    void handleSurfaceCommitted();
    void handleSurfaceSizeChanged();
    void handleBufferChanged();
    void handleBufferSourceBoxChanged();
    void handleBufferTransformChanged();

    void handleChildSubSurfaceRemoved(SubSurfaceInterface *child);
    void handleChildSubSurfacesChanged();
    void handleSubSurfacePositionChanged();
    void handleSurfaceMappedChanged();
    void handleColorDescriptionChanged();
    void handlePresentationModeHintChanged();
    void handleReleasePointChanged();
    void handleAlphaMultiplierChanged();

    void handleFifoFallback();

private:
    SurfaceItemWayland *getOrCreateSubSurfaceItem(SubSurfaceInterface *s);
    void handleFramePainted(LogicalOutput *output, OutputFrame *frame, std::chrono::milliseconds timestamp) override;

    QPointer<SurfaceInterface> m_surface;
    struct ScanoutFeedback
    {
        DrmDevice *device = nullptr;
        FormatModifierMap formats;
    };
    std::optional<ScanoutFeedback> m_scanoutFeedback;
    std::unordered_map<SubSurfaceInterface *, std::unique_ptr<SurfaceItemWayland>> m_subsurfaces;
    QTimer m_fifoFallbackTimer;
};

#if KWIN_BUILD_X11
/**
 * The SurfaceItemXwayland class represents an Xwayland surface in the scene.
 */
class KWIN_EXPORT SurfaceItemXwayland : public SurfaceItemWayland
{
    Q_OBJECT

public:
    explicit SurfaceItemXwayland(X11Window *window, Item *parent = nullptr);

    Region opaque() const override;
    QList<RectF> shape() const override;

private:
    void handleShapeChange();

    X11Window *m_window;
    Region m_previousBufferShape;
};
#endif

} // namespace KWin
