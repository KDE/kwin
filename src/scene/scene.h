/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/renderlayerdelegate.h"
#include "kwineffects.h"
#include "utils/common.h"
#include "window.h"

#include <optional>

#include <QElapsedTimer>
#include <QMatrix4x4>

namespace KWin
{

namespace Decoration
{
class DecoratedClientImpl;
}

class ItemRenderer;
class Output;
class DecorationRenderer;
class Deleted;
class DragAndDropIconItem;
class EffectWindowImpl;
class GLTexture;
class Item;
class RenderLoop;
class Scene;
class Shadow;
class ShadowItem;
class SurfaceItem;
class WindowItem;

class SceneDelegate : public RenderLayerDelegate
{
public:
    explicit SceneDelegate(Scene *scene);
    explicit SceneDelegate(Scene *scene, Output *output);
    ~SceneDelegate() override;

    Output *output() const;
    QRect viewport() const;

    QRegion repaints() const override;
    SurfaceItem *scanoutCandidate() const override;
    void prePaint() override;
    void postPaint() override;
    void paint(RenderTarget *renderTarget, const QRegion &region) override;

private:
    Scene *m_scene;
    Output *m_output = nullptr;
};

class KWIN_EXPORT Scene : public QObject
{
    Q_OBJECT

public:
    explicit Scene(std::unique_ptr<ItemRenderer> renderer);
    ~Scene() override;

    void initialize();

    /**
     * Schedules a repaint for the specified @a region.
     */
    void addRepaint(const QRegion &region);
    void addRepaint(int x, int y, int width, int height);
    void addRepaintFull();
    QRegion damage() const;

    QRect geometry() const;
    void setGeometry(const QRect &rect);

    QList<SceneDelegate *> delegates() const;
    void addDelegate(SceneDelegate *delegate);
    void removeDelegate(SceneDelegate *delegate);

    SurfaceItem *scanoutCandidate() const;
    void prePaint(SceneDelegate *delegate);
    void postPaint();
    void paint(RenderTarget *renderTarget, const QRegion &region);

    /**
     * @brief Creates the Scene specific Shadow subclass.
     *
     * An implementing class has to create a proper instance. It is not allowed to
     * return @c null.
     *
     * @param window The Window for which the Shadow needs to be created.
     */
    virtual Shadow *createShadow(Window *window) = 0;
    // Flags controlling how painting is done.
    enum {
        // WindowItem (or at least part of it) will be painted opaque.
        PAINT_WINDOW_OPAQUE = 1 << 0,
        // WindowItem (or at least part of it) will be painted translucent.
        PAINT_WINDOW_TRANSLUCENT = 1 << 1,
        // WindowItem will be painted with transformed geometry.
        PAINT_WINDOW_TRANSFORMED = 1 << 2,
        // Paint only a region of the screen (can be optimized, cannot
        // be used together with TRANSFORMED flags).
        PAINT_SCREEN_REGION = 1 << 3,
        // Whole screen will be painted with transformed geometry.
        PAINT_SCREEN_TRANSFORMED = 1 << 4,
        // At least one window will be painted with transformed geometry.
        PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
        // Clear whole background as the very first step, without optimizing it
        PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6,
    };

    virtual bool makeOpenGLContextCurrent();
    virtual void doneOpenGLContextCurrent();
    virtual bool supportsNativeFence() const;

    virtual DecorationRenderer *createDecorationRenderer(Decoration::DecoratedClientImpl *) = 0;

    /**
     * Whether the Scene is able to drive animations.
     * This is used as a hint to the effects system which effects can be supported.
     * If the Scene performs software rendering it is supposed to return @c false,
     * if rendering is hardware accelerated it should return @c true.
     */
    virtual bool animationsSupported() const = 0;

    virtual std::shared_ptr<GLTexture> textureForOutput(Output *output) const
    {
        return {};
    }

    ItemRenderer *renderer() const;

Q_SIGNALS:
    void preFrameRender();
    void frameRendered();
    void delegateRemoved(SceneDelegate *delegate);

protected:
    void createStackingOrder();
    void clearStackingOrder();
    friend class EffectsHandlerImpl;
    // called after all effects had their paintScreen() called
    void finalPaintScreen(int mask, const QRegion &region, ScreenPaintData &data);
    // shared implementation of painting the screen in the generic
    // (unoptimized) way
    void preparePaintGenericScreen();
    void paintGenericScreen(int mask, const ScreenPaintData &data);
    // shared implementation of painting the screen in an optimized way
    void preparePaintSimpleScreen();
    void paintSimpleScreen(int mask, const QRegion &region);
    // called after all effects had their paintWindow() called
    void finalPaintWindow(EffectWindowImpl *w, int mask, const QRegion &region, WindowPaintData &data);
    // shared implementation, starts painting the window
    void paintWindow(WindowItem *w, int mask, const QRegion &region);
    // called after all effects had their drawWindow() called
    void finalDrawWindow(EffectWindowImpl *w, int mask, const QRegion &region, WindowPaintData &data);

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
        QVector<Phase2Data> phase2Data;
    };

    // The screen that is being currently painted
    Output *painted_screen = nullptr;
    SceneDelegate *painted_delegate = nullptr;

    // windows in their stacking order
    QVector<WindowItem *> stacking_order;

private:
    void createDndIconItem();
    void destroyDndIconItem();

    std::unique_ptr<ItemRenderer> m_renderer;
    std::chrono::milliseconds m_expectedPresentTimestamp = std::chrono::milliseconds::zero();
    QList<SceneDelegate *> m_delegates;
    QRect m_geometry;
    // how many times finalPaintScreen() has been called
    int m_paintScreenCount = 0;
    PaintContext m_paintContext;
    std::unique_ptr<DragAndDropIconItem> m_dndIcon;
};

} // namespace
