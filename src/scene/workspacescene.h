/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/colorspace.h"
#include "scene/scene.h"

namespace KWin
{

namespace Decoration
{
class DecoratedWindowImpl;
}

class DecorationRenderer;
class Deleted;
class DragAndDropIconItem;
class EffectWindow;
class GLTexture;
class Item;
class RenderLoop;
class WorkspaceScene;
class Shadow;
class ShadowItem;
class ShadowTextureProvider;
class SurfaceItem;
class WindowItem;
class WindowPaintData;
class OpenGlContext;

class KWIN_EXPORT WorkspaceScene : public Scene
{
    Q_OBJECT

public:
    explicit WorkspaceScene(std::unique_ptr<ItemRenderer> renderer);
    ~WorkspaceScene() override;

    void initialize();

    Item *containerItem() const;
    Item *overlayItem() const;

    QList<SurfaceItem *> scanoutCandidates(ssize_t maxCount) const override;
    QRegion prePaint(SceneDelegate *delegate) override;
    void postPaint() override;
    void paint(const RenderTarget &renderTarget, const QRegion &region) override;
    void frame(SceneDelegate *delegate, OutputFrame *frame) override;
    double desiredHdrHeadroom() const override;

    virtual bool makeOpenGLContextCurrent();
    virtual void doneOpenGLContextCurrent();
    virtual bool supportsNativeFence() const;
    virtual OpenGlContext *openglContext() const;

    virtual std::unique_ptr<DecorationRenderer> createDecorationRenderer(Decoration::DecoratedWindowImpl *) = 0;
    virtual std::unique_ptr<ShadowTextureProvider> createShadowTextureProvider(Shadow *shadow) = 0;

    /**
     * Whether the Scene is able to drive animations.
     * This is used as a hint to the effects system which effects can be supported.
     * If the Scene performs software rendering it is supposed to return @c false,
     * if rendering is hardware accelerated it should return @c true.
     */
    virtual bool animationsSupported() const = 0;

    virtual std::pair<std::shared_ptr<GLTexture>, ColorDescription> textureForOutput(Output *output) const
    {
        return {nullptr, ColorDescription::sRGB};
    }

Q_SIGNALS:
    void preFrameRender();
    void frameRendered();

protected:
    void createStackingOrder();
    void clearStackingOrder();
    friend class EffectsHandler;
    // called after all effects had their paintScreen() called
    void finalPaintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen);
    // shared implementation of painting the screen in the generic
    // (unoptimized) way
    void preparePaintGenericScreen();
    void paintGenericScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, Output *screen);
    // shared implementation of painting the screen in an optimized way
    void preparePaintSimpleScreen();
    void paintSimpleScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region);
    // called after all effects had their paintWindow() called
    void finalPaintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data);
    // shared implementation, starts painting the window
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, WindowItem *w, int mask, const QRegion &region);
    // called after all effects had their drawWindow() called
    void finalDrawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data);

    // saved data for 2nd pass of optimized screen painting
    struct Phase2Data
    {
        WindowItem *item = nullptr;
        QRegion region;
        QRegion opaque;
        int mask = 0;
    };

    struct PaintContext
    {
        QRegion damage;
        int mask = 0;
        QList<Phase2Data> phase2Data;
    };

    // The screen that is being currently painted
    Output *painted_screen = nullptr;
    SceneDelegate *painted_delegate = nullptr;

    // windows in their stacking order
    QList<WindowItem *> stacking_order;

private:
    void createDndIconItem();
    void destroyDndIconItem();

    std::chrono::milliseconds m_expectedPresentTimestamp = std::chrono::milliseconds::zero();
    // how many times finalPaintScreen() has been called
    int m_paintScreenCount = 0;
    PaintContext m_paintContext;
    std::unique_ptr<Item> m_containerItem;
    std::unique_ptr<Item> m_overlayItem;
    std::unique_ptr<DragAndDropIconItem> m_dndIcon;
};

} // namespace
