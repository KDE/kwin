/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCENE_QPAINTER_H
#define KWIN_SCENE_QPAINTER_H

#include "scene.h"
#include <platformsupport/scenes/qpainter/backend.h>
#include "shadow.h"

#include "decorations/decorationrenderer.h"

namespace KWin {

class KWIN_EXPORT SceneQPainter : public Scene
{
    Q_OBJECT

public:
    ~SceneQPainter() override;
    bool usesOverlayWindow() const override;
    OverlayWindow* overlayWindow() const override;
    qint64 paint(const QRegion &damage, const QList<Toplevel *> &windows) override;
    void paintGenericScreen(int mask, const ScreenPaintData &data) override;
    CompositingType compositingType() const override;
    bool initFailed() const override;
    EffectFrame *createEffectFrame(EffectFrameImpl *frame) override;
    Shadow *createShadow(Toplevel *toplevel) override;
    Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;
    void screenGeometryChanged(const QSize &size) override;

    bool animationsSupported() const override {
        return false;
    }

    QPainter *scenePainter() const override;
    QImage *qpainterRenderBuffer() const override;

    QPainterBackend *backend() const {
        return m_backend.data();
    }

    static SceneQPainter *createScene(QObject *parent);

protected:
    void paintBackground(const QRegion &region) override;
    Scene::Window *createWindow(Toplevel *toplevel) override;
    void paintCursor(const QRegion &region) override;
    void paintEffectQuickView(EffectQuickView *w) override;

private:
    explicit SceneQPainter(QPainterBackend *backend, QObject *parent = nullptr);
    QScopedPointer<QPainterBackend> m_backend;
    QScopedPointer<QPainter> m_painter;
    class Window;
};

class QPainterWindowPixmap : public WindowPixmap
{
public:
    explicit QPainterWindowPixmap(Scene::Window *window);
    ~QPainterWindowPixmap() override;
    void create() override;
    void update() override;
    bool isValid() const override;

    const QImage &image();

protected:
    WindowPixmap *createChild(const QPointer<KWaylandServer::SubSurfaceInterface> &subSurface) override;
private:
    explicit QPainterWindowPixmap(const QPointer<KWaylandServer::SubSurfaceInterface> &subSurface, WindowPixmap *parent);
    QImage m_image;
};

class SceneQPainter::Window : public Scene::Window
{
    Q_OBJECT

public:
    Window(SceneQPainter *scene, Toplevel *c);
    ~Window() override;
    void performPaint(int mask, const QRegion &region, const WindowPaintData &data) override;
protected:
    WindowPixmap *createWindowPixmap() override;
private:
    void renderWindowPixmap(QPainter *painter, QPainterWindowPixmap *windowPixmap);
    void renderShadow(QPainter *painter);
    void renderWindowDecorations(QPainter *painter);
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

    QImage &shadowTexture() {
        return m_texture;
    }

protected:
    void buildQuads() override;
    bool prepareBackend() override;

private:
    QImage m_texture;
};

class SceneQPainterDecorationRenderer : public Decoration::Renderer
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
    ~SceneQPainterDecorationRenderer() override;

    void render() override;
    void reparent(Deleted *deleted) override;

    QImage image(DecorationPart part) const;

private:
    void resizeImages();
    QImage m_images[int(DecorationPart::Count)];
};

class KWIN_EXPORT QPainterFactory : public SceneFactory
{
    Q_OBJECT
    Q_INTERFACES(KWin::SceneFactory)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Scene" FILE "qpainter.json")

public:
    explicit QPainterFactory(QObject *parent = nullptr);
    ~QPainterFactory() override;

    Scene *create(QObject *parent = nullptr) const override;
};

inline
bool SceneQPainter::usesOverlayWindow() const
{
    return m_backend->usesOverlayWindow();
}

inline
OverlayWindow* SceneQPainter::overlayWindow() const
{
    return m_backend->overlayWindow();
}

inline
QPainter* SceneQPainter::scenePainter() const
{
    return m_painter.data();
}

inline
const QImage &QPainterWindowPixmap::image()
{
    return m_image;
}

} // KWin

#endif // KWIN_SCENEQPAINTER_H
