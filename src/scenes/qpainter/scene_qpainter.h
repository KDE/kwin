/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCENE_QPAINTER_H
#define KWIN_SCENE_QPAINTER_H

#include "qpainterbackend.h"

#include "decorationitem.h"
#include "scene.h"
#include "shadow.h"

namespace KWin {

class KWIN_EXPORT SceneQPainter : public Scene
{
    Q_OBJECT

public:
    ~SceneQPainter() override;
    void paint(AbstractOutput *output, const QRegion &damage, const QList<Toplevel *> &windows,
               RenderLoop *renderLoop) override;
    void paintGenericScreen(int mask, const ScreenPaintData &data) override;
    bool initFailed() const override;
    EffectFrame *createEffectFrame(EffectFrameImpl *frame) override;
    Shadow *createShadow(Toplevel *toplevel) override;
    DecorationRenderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    bool animationsSupported() const override {
        return false;
    }

    QPainter *scenePainter() const override;
    QImage *qpainterRenderBuffer(AbstractOutput *output) const override;

    QPainterBackend *backend() const {
        return m_backend;
    }

    static SceneQPainter *createScene(QPainterBackend *backend, QObject *parent);

protected:
    void paintBackground(const QRegion &region) override;
    Scene::Window *createWindow(Toplevel *toplevel) override;
    void paintCursor(AbstractOutput *output, const QRegion &region) override;
    void paintOffscreenQuickView(OffscreenQuickView *w) override;

private:
    explicit SceneQPainter(QPainterBackend *backend, QObject *parent = nullptr);
    QPainterBackend *m_backend;
    QScopedPointer<QPainter> m_painter;
    class Window;
};

class SceneQPainter::Window : public Scene::Window
{
    Q_OBJECT

public:
    Window(SceneQPainter *scene, Toplevel *c);
    ~Window() override;
    void performPaint(int mask, const QRegion &region, const WindowPaintData &data) override;
private:
    void renderSurfaceItem(QPainter *painter, SurfaceItem *surfaceItem) const;
    void renderDecorationItem(QPainter *painter, DecorationItem *decorationItem) const;
    void renderItem(QPainter *painter, Item *item) const;
    SceneQPainter *m_scene;
};

class QPainterEffectFrame : public Scene::EffectFrame
{
public:
    QPainterEffectFrame(EffectFrameImpl *frame, SceneQPainter *scene);
    ~QPainterEffectFrame() override;
    void crossFadeIcon() override {}
    void crossFadeText() override {}
    void free() override {}
    void freeIconFrame() override {}
    void freeTextFrame() override {}
    void freeSelection() override {}
    void render(const QRegion &region, double opacity, double frameOpacity) override;
private:
    SceneQPainter *m_scene;
};

class SceneQPainterShadow : public Shadow
{
public:
    SceneQPainterShadow(Toplevel* toplevel);
    ~SceneQPainterShadow() override;

protected:
    bool prepareBackend() override;
};

class SceneQPainterDecorationRenderer : public DecorationRenderer
{
    Q_OBJECT
public:
    enum class DecorationPart : int {
        Left,
        Top,
        Right,
        Bottom,
        Count
    };
    explicit SceneQPainterDecorationRenderer(Decoration::DecoratedClientImpl *client);

    void render(const QRegion &region) override;

    QImage image(DecorationPart part) const;

private:
    void resizeImages();
    QImage m_images[int(DecorationPart::Count)];
};

inline
QPainter* SceneQPainter::scenePainter() const
{
    return m_painter.data();
}

} // KWin

#endif // KWIN_SCENEQPAINTER_H
