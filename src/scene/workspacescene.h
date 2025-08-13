/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/renderviewport.h"
#include "scene/scene.h"

namespace KWin
{

class DragAndDropIconItem;
class EffectWindow;
class EglContext;
class Item;
class SurfaceItem;
class WindowItem;
class WindowPaintData;
class CursorItem;

class KWIN_EXPORT WorkspaceScene : public Scene
{
    Q_OBJECT

public:
    explicit WorkspaceScene(std::unique_ptr<ItemRenderer> renderer);
    ~WorkspaceScene() override;

    void initialize();

    Item *containerItem() const;
    Item *overlayItem() const;
    Item *cursorItem() const;

    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    OverlayCandidates overlayCandidates(ssize_t maxTotalCount, ssize_t maxOverlayCount, ssize_t maxUnderlayCount) const override;
    void prePaint(SceneView *delegate) override;
    QRegion collectDamage() override;
    void postPaint() override;
    void paint(const RenderTarget &renderTarget, const QPoint &deviceOffset, const QRegion &deviceRegion) override;
    void frame(SceneView *delegate, OutputFrame *frame) override;
    double desiredHdrHeadroom() const override;

    EglContext *openglContext() const;

    /**
     * Whether the Scene is able to drive animations.
     * This is used as a hint to the effects system which effects can be supported.
     * If the Scene performs software rendering it is supposed to return @c false,
     * if rendering is hardware accelerated it should return @c true.
     */
    bool animationsSupported() const;

Q_SIGNALS:
    void preFrameRender();
    void frameRendered();

protected:
    void createStackingOrder();
    void clearStackingOrder();
    friend class EffectsHandler;
    // called after all effects had their paintScreen() called
    void finalPaintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &deviceRegion, LogicalOutput *screen);
    // shared implementation of painting the screen in the generic
    // (unoptimized) way
    void preparePaintGenericScreen();
    void paintGenericScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, LogicalOutput *screen);
    // shared implementation of painting the screen in an optimized way
    void preparePaintSimpleScreen();
    void paintSimpleScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &deviceRegion);
    // called after all effects had their paintWindow() called
    void finalPaintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &deviceRegion, WindowPaintData &data);
    // shared implementation, starts painting the window
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, WindowItem *w, int mask, const QRegion &deviceRegion);
    // called after all effects had their drawWindow() called
    void finalDrawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &deviceRegion, WindowPaintData &data);

    // saved data for 2nd pass of optimized screen painting
    struct Phase2Data
    {
        WindowItem *item = nullptr;
        QRegion deviceRegion;
        QRegion deviceOpaque;
        int mask = 0;
    };

    struct PaintContext
    {
        QRegion deviceDamage;
        int mask = 0;
        QList<Phase2Data> phase2Data;
    };

    // The screen that is being currently painted
    LogicalOutput *painted_screen = nullptr;
    SceneView *painted_delegate = nullptr;

    // windows in their stacking order
    QList<WindowItem *> stacking_order;

private:
    void createDndIconItem();
    void destroyDndIconItem();
    void updateCursor();

    std::chrono::milliseconds m_expectedPresentTimestamp = std::chrono::milliseconds::zero();
    // how many times finalPaintScreen() has been called
    int m_paintScreenCount = 0;
    PaintContext m_paintContext;
    std::unique_ptr<Item> m_containerItem;
    std::unique_ptr<Item> m_overlayItem;
    std::unique_ptr<DragAndDropIconItem> m_dndIcon;
    std::unique_ptr<CursorItem> m_cursorItem;
};

} // namespace
