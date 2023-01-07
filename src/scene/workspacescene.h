/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/scene.h"

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

class DecorationRenderer;
class Deleted;
class DragAndDropIconItem;
class EffectWindowImpl;
class GLTexture;
class Item;
class RenderLoop;
class WorkspaceScene;
class Shadow;
class ShadowItem;
class SurfaceItem;
class WindowItem;

class KWIN_EXPORT WorkspaceScene : public Scene
{
    Q_OBJECT

public:
    explicit WorkspaceScene(std::unique_ptr<ItemRenderer> renderer);
    ~WorkspaceScene() override;

    void initialize();

    QRegion damage() const override;
    SurfaceItem *scanoutCandidate() const override;
    void prePaint(SceneDelegate *delegate) override;
    void postPaint() override;
    void paint(RenderTarget *renderTarget, const QRegion &region) override;

    /**
     * @brief Creates the Scene specific Shadow subclass.
     *
     * An implementing class has to create a proper instance. It is not allowed to
     * return @c null.
     *
     * @param window The Window for which the Shadow needs to be created.
     */
    virtual std::unique_ptr<Shadow> createShadow(Window *window) = 0;

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

Q_SIGNALS:
    void preFrameRender();
    void frameRendered();

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

    std::chrono::milliseconds m_expectedPresentTimestamp = std::chrono::milliseconds::zero();
    // how many times finalPaintScreen() has been called
    int m_paintScreenCount = 0;
    PaintContext m_paintContext;
    std::unique_ptr<DragAndDropIconItem> m_dndIcon;
};

} // namespace
