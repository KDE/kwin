/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCENE_XRENDER_H
#define KWIN_SCENE_XRENDER_H

#include "scene.h"
#include "shadow.h"
#include "decorations/decorationrenderer.h"

#ifdef KWIN_HAVE_XRENDER_COMPOSITING

namespace KWin
{

class XRenderBackend;

class SceneXrender
    : public Scene
{
    Q_OBJECT
public:
    class EffectFrame;
    ~SceneXrender() override;
    bool initFailed() const override;
    CompositingType compositingType() const override {
        return XRenderCompositing;
    }
    void paint(int screenId, const QRegion &damage, const QList<Toplevel *> &windows,
               RenderLoop *renderLoop) override;
    Scene::EffectFrame *createEffectFrame(EffectFrameImpl *frame) override;
    Shadow *createShadow(Toplevel *toplevel) override;
    void screenGeometryChanged(const QSize &size) override;
    xcb_render_picture_t xrenderBufferPicture() const override;
    OverlayWindow *overlayWindow() const override;
    Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *client) override;

    bool animationsSupported() const override {
        return true;
    }

    static SceneXrender *createScene(QObject *parent);
protected:
    Scene::Window *createWindow(Toplevel *toplevel) override;
    void paintBackground(const QRegion &region) override;
    void paintGenericScreen(int mask, const ScreenPaintData &data) override;
    void paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data) override;
    void paintCursor(const QRegion &region) override;
    void paintEffectQuickView(EffectQuickView *w) override;
private:
    explicit SceneXrender(XRenderBackend *backend, QObject *parent = nullptr);
    static ScreenPaintData screen_paint;
    class Window;
    QScopedPointer<XRenderBackend> m_backend;
};

class SceneXrender::Window : public Scene::Window
{
    Q_OBJECT

public:
    Window(Toplevel* c, SceneXrender *scene);
    ~Window() override;
    void performPaint(int mask, const QRegion &region, const WindowPaintData &data) override;
    QRegion transformedShape() const;
    void setTransformedShape(const QRegion& shape);
    static void cleanup();
protected:
    WindowPixmap* createWindowPixmap() override;
private:
    QRect mapToScreen(int mask, const WindowPaintData &data, const QRect &rect) const;
    QPoint mapToScreen(int mask, const WindowPaintData &data, const QPoint &point) const;
    QRect bufferToWindowRect(const QRect &rect) const;
    QRegion bufferToWindowRegion(const QRegion &region) const;
    void prepareTempPixmap();
    void setPictureFilter(xcb_render_picture_t pic, ImageFilterType filter);
    SceneXrender *m_scene;
    xcb_render_pictformat_t format;
    QRegion transformed_shape;
    static QRect temp_visibleRect;
    static XRenderPicture *s_tempPicture;
    static XRenderPicture *s_fadeAlphaPicture;
};

class XRenderWindowPixmap : public WindowPixmap
{
public:
    explicit XRenderWindowPixmap(Scene::Window *window, xcb_render_pictformat_t format);
    ~XRenderWindowPixmap() override;
    xcb_render_picture_t picture() const;
    void create() override;
private:
    xcb_render_picture_t m_picture;
    xcb_render_pictformat_t m_format;
};

class SceneXrender::EffectFrame
    : public Scene::EffectFrame
{
public:
    EffectFrame(EffectFrameImpl* frame);
    ~EffectFrame() override;

    void free() override;
    void freeIconFrame() override;
    void freeTextFrame() override;
    void freeSelection() override;
    void crossFadeIcon() override;
    void crossFadeText() override;
    void render(const QRegion &region, double opacity, double frameOpacity) override;
    static void cleanup();

private:
    void updatePicture();
    void updateTextPicture();
    void renderUnstyled(xcb_render_picture_t pict, const QRect &rect, qreal opacity);

    XRenderPicture* m_picture;
    XRenderPicture* m_textPicture;
    XRenderPicture* m_iconPicture;
    XRenderPicture* m_selectionPicture;
    static XRenderPicture* s_effectFrameCircle;
};

inline
QRegion SceneXrender::Window::transformedShape() const
{
    return transformed_shape;
}

inline
void SceneXrender::Window::setTransformedShape(const QRegion& shape)
{
    transformed_shape = shape;
}

inline
xcb_render_picture_t XRenderWindowPixmap::picture() const
{
    return m_picture;
}

/**
 * @short XRender implementation of Shadow.
 *
 * This class extends Shadow by the elements required for XRender rendering.
 * @author Jacopo De Simoi <wilderkde@gmail.org>
 */
class SceneXRenderShadow
    : public Shadow
{
public:
    explicit SceneXRenderShadow(Toplevel *toplevel);
    using Shadow::ShadowElements;
    using Shadow::ShadowElementTop;
    using Shadow::ShadowElementTopRight;
    using Shadow::ShadowElementRight;
    using Shadow::ShadowElementBottomRight;
    using Shadow::ShadowElementBottom;
    using Shadow::ShadowElementBottomLeft;
    using Shadow::ShadowElementLeft;
    using Shadow::ShadowElementTopLeft;
    using Shadow::ShadowElementsCount;
    using Shadow::shadowPixmap;
    ~SceneXRenderShadow() override;

    void layoutShadowRects(QRect& top, QRect& topRight,
                           QRect& right, QRect& bottomRight,
                           QRect& bottom, QRect& bottomLeft,
                           QRect& Left, QRect& topLeft);
    xcb_render_picture_t picture(ShadowElements element) const;

protected:
    void buildQuads() override;
    bool prepareBackend() override;
private:
    XRenderPicture* m_pictures[ShadowElementsCount];
};

class SceneXRenderDecorationRenderer : public Decoration::Renderer
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
    explicit SceneXRenderDecorationRenderer(Decoration::DecoratedClientImpl *client);
    ~SceneXRenderDecorationRenderer() override;

    void render() override;
    void reparent(Deleted *deleted) override;

    xcb_render_picture_t picture(DecorationPart part) const;

private:
    void resizePixmaps();
    QSize m_sizes[int(DecorationPart::Count)];
    xcb_pixmap_t m_pixmaps[int(DecorationPart::Count)];
    xcb_gcontext_t m_gc;
    XRenderPicture* m_pictures[int(DecorationPart::Count)];
};

class KWIN_EXPORT XRenderFactory : public SceneFactory
{
    Q_OBJECT
    Q_INTERFACES(KWin::SceneFactory)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Scene" FILE "xrender.json")

public:
    explicit XRenderFactory(QObject *parent = nullptr);
    ~XRenderFactory() override;

    Scene *create(QObject *parent = nullptr) const override;
};

} // namespace

#endif

#endif
