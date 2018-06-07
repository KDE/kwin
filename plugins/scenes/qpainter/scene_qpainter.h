/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
    virtual ~SceneQPainter();
    virtual bool usesOverlayWindow() const override;
    virtual OverlayWindow* overlayWindow() override;
    virtual qint64 paint(QRegion damage, ToplevelList windows) override;
    virtual void paintGenericScreen(int mask, ScreenPaintData data) override;
    virtual CompositingType compositingType() const override;
    virtual bool initFailed() const override;
    virtual EffectFrame *createEffectFrame(EffectFrameImpl *frame) override;
    virtual Shadow *createShadow(Toplevel *toplevel) override;
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
    virtual void paintBackground(QRegion region) override;
    virtual Scene::Window *createWindow(Toplevel *toplevel) override;
    void paintCursor() override;

private:
    explicit SceneQPainter(QPainterBackend *backend, QObject *parent = nullptr);
    QScopedPointer<QPainterBackend> m_backend;
    QScopedPointer<QPainter> m_painter;
    class Window;
};

class SceneQPainter::Window : public Scene::Window
{
public:
    Window(SceneQPainter *scene, Toplevel *c);
    virtual ~Window();
    virtual void performPaint(int mask, QRegion region, WindowPaintData data) override;
protected:
    virtual WindowPixmap *createWindowPixmap() override;
private:
    void renderShadow(QPainter *painter);
    void renderWindowDecorations(QPainter *painter);
    SceneQPainter *m_scene;
};

class QPainterWindowPixmap : public WindowPixmap
{
public:
    explicit QPainterWindowPixmap(Scene::Window *window);
    virtual ~QPainterWindowPixmap();
    virtual void create() override;
    bool isValid() const override;

    void updateBuffer() override;
    const QImage &image();

protected:
    WindowPixmap *createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface) override;
private:
    explicit QPainterWindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface, WindowPixmap *parent);
    QImage m_image;
};

class QPainterEffectFrame : public Scene::EffectFrame
{
public:
    QPainterEffectFrame(EffectFrameImpl *frame, SceneQPainter *scene);
    virtual ~QPainterEffectFrame();
    virtual void crossFadeIcon() override {}
    virtual void crossFadeText() override {}
    virtual void free() override {}
    virtual void freeIconFrame() override {}
    virtual void freeTextFrame() override {}
    virtual void freeSelection() override {}
    virtual void render(QRegion region, double opacity, double frameOpacity) override;
private:
    SceneQPainter *m_scene;
};

class SceneQPainterShadow : public Shadow
{
public:
    SceneQPainterShadow(Toplevel* toplevel);
    virtual ~SceneQPainterShadow();

    QImage &shadowTexture() {
        return m_texture;
    }

protected:
    virtual void buildQuads() override;
    virtual bool prepareBackend() override;

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
    virtual ~SceneQPainterDecorationRenderer();

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
OverlayWindow* SceneQPainter::overlayWindow()
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
