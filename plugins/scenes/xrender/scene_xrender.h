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

namespace Xcb
{
    class Shm;
}

/**
 * @brief Backend for the SceneXRender to hold the compositing buffer and take care of buffer
 * swapping.
 *
 * This class is intended as a small abstraction to support multiple compositing backends in the
 * SceneXRender.
 */
class XRenderBackend
{
public:
    virtual ~XRenderBackend();
    virtual void present(int mask, const QRegion &damage) = 0;

    /**
     * @brief Returns the OverlayWindow used by the backend.
     *
     * A backend does not have to use an OverlayWindow, this is mostly for the X world.
     * In case the backend does not use an OverlayWindow it is allowed to return @c null.
     * It's the task of the caller to check whether it is @c null.
     *
     * @return :OverlayWindow*
     */
    virtual OverlayWindow *overlayWindow();
    virtual bool usesOverlayWindow() const = 0;
    /**
     * @brief Shows the Overlay Window
     *
     * Default implementation does nothing.
     */
    virtual void showOverlay();
    /**
     * @brief React on screen geometry changes.
     *
     * Default implementation does nothing. Override if specific functionality is required.
     *
     * @param size The new screen size
     */
    virtual void screenGeometryChanged(const QSize &size);
    /**
     * @brief The compositing buffer hold by this backend.
     *
     * The Scene composites the new frame into this buffer.
     *
     * @return xcb_render_picture_t
     */
    xcb_render_picture_t buffer() const {
        return m_buffer;
    }
    /**
     * @brief Whether the creation of the Backend failed.
     *
     * The SceneXRender should test whether the Backend got constructed correctly. If this method
     * returns @c true, the SceneXRender should not try to start the rendering.
     *
     * @return bool @c true if the creation of the Backend failed, @c false otherwise.
     */
    bool isFailed() const {
        return m_failed;
    }

protected:
    XRenderBackend();
    /**
     * @brief A subclass needs to call this method once it created the compositing back buffer.
     *
     * @param buffer The buffer to use for compositing
     * @return void
     */
    void setBuffer(xcb_render_picture_t buffer);
    /**
     * @brief Sets the backend initialization to failed.
     *
     * This method should be called by the concrete subclass in case the initialization failed.
     * The given @p reason is logged as a warning.
     *
     * @param reason The reason why the initialization failed.
     */
    void setFailed(const QString &reason);

private:
    // Create the compositing buffer. The root window is not double-buffered,
    // so it is done manually using this buffer,
    xcb_render_picture_t m_buffer;
    bool m_failed;
};

/**
 * @brief XRenderBackend using an X11 Overlay Window as compositing target.
 */
class X11XRenderBackend : public XRenderBackend
{
public:
    X11XRenderBackend();
    ~X11XRenderBackend() override;

    void present(int mask, const QRegion &damage) override;
    OverlayWindow* overlayWindow() override;
    void showOverlay() override;
    void screenGeometryChanged(const QSize &size) override;
    bool usesOverlayWindow() const override;
private:
    void init(bool createOverlay);
    void createBuffer();
    QScopedPointer<OverlayWindow> m_overlayWindow;
    xcb_render_picture_t m_front;
    xcb_render_pictformat_t m_format;
};

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
    qint64 paint(const QRegion &damage, const QList<Toplevel *> &windows) override;
    Scene::EffectFrame *createEffectFrame(EffectFrameImpl *frame) override;
    Shadow *createShadow(Toplevel *toplevel) override;
    void screenGeometryChanged(const QSize &size) override;
    xcb_render_picture_t xrenderBufferPicture() const override;
    OverlayWindow *overlayWindow() const override {
        return m_backend->overlayWindow();
    }
    bool usesOverlayWindow() const override {
        return m_backend->usesOverlayWindow();
    }
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
xcb_render_picture_t SceneXrender::xrenderBufferPicture() const
{
    return m_backend->buffer();
}

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
