/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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
 *
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
     **/
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
     **/
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
     **/
    void setFailed(const QString &reason);

private:
    // Create the compositing buffer. The root window is not double-buffered,
    // so it is done manually using this buffer,
    xcb_render_picture_t m_buffer;
    bool m_failed;
};

/**
 * @brief XRenderBackend using an X11 Overlay Window as compositing target.
 *
 */
class X11XRenderBackend : public XRenderBackend
{
public:
    X11XRenderBackend();
    ~X11XRenderBackend();

    virtual void present(int mask, const QRegion &damage);
    virtual OverlayWindow* overlayWindow();
    virtual void showOverlay();
    virtual void screenGeometryChanged(const QSize &size);
    virtual bool usesOverlayWindow() const;
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
    virtual ~SceneXrender();
    virtual bool initFailed() const;
    virtual CompositingType compositingType() const {
        return XRenderCompositing;
    }
    virtual qint64 paint(QRegion damage, ToplevelList windows);
    virtual Scene::EffectFrame *createEffectFrame(EffectFrameImpl *frame);
    virtual Shadow *createShadow(Toplevel *toplevel);
    virtual void screenGeometryChanged(const QSize &size);
    xcb_render_picture_t xrenderBufferPicture() const override;
    virtual OverlayWindow *overlayWindow() {
        return m_backend->overlayWindow();
    }
    virtual bool usesOverlayWindow() const {
        return m_backend->usesOverlayWindow();
    }
    Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *client);

    bool animationsSupported() const override {
        return true;
    }

    static SceneXrender *createScene(QObject *parent);
protected:
    virtual Scene::Window *createWindow(Toplevel *toplevel);
    virtual void paintBackground(QRegion region);
    virtual void paintGenericScreen(int mask, ScreenPaintData data);
    virtual void paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data);
    void paintCursor() override;
private:
    explicit SceneXrender(XRenderBackend *backend, QObject *parent = nullptr);
    static ScreenPaintData screen_paint;
    class Window;
    QScopedPointer<XRenderBackend> m_backend;
};

class SceneXrender::Window
    : public Scene::Window
{
public:
    Window(Toplevel* c, SceneXrender *scene);
    virtual ~Window();
    virtual void performPaint(int mask, QRegion region, WindowPaintData data);
    QRegion transformedShape() const;
    void setTransformedShape(const QRegion& shape);
    static void cleanup();
protected:
    virtual WindowPixmap* createWindowPixmap();
private:
    QRect mapToScreen(int mask, const WindowPaintData &data, const QRect &rect) const;
    QPoint mapToScreen(int mask, const WindowPaintData &data, const QPoint &point) const;
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
    virtual ~XRenderWindowPixmap();
    xcb_render_picture_t picture() const;
    virtual void create();
private:
    xcb_render_picture_t m_picture;
    xcb_render_pictformat_t m_format;
};

class SceneXrender::EffectFrame
    : public Scene::EffectFrame
{
public:
    EffectFrame(EffectFrameImpl* frame);
    virtual ~EffectFrame();

    virtual void free();
    virtual void freeIconFrame();
    virtual void freeTextFrame();
    virtual void freeSelection();
    virtual void crossFadeIcon();
    virtual void crossFadeText();
    virtual void render(QRegion region, double opacity, double frameOpacity);
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
 **/

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
    virtual ~SceneXRenderShadow();

    void layoutShadowRects(QRect& top, QRect& topRight,
                           QRect& right, QRect& bottomRight,
                           QRect& bottom, QRect& bottomLeft,
                           QRect& Left, QRect& topLeft);
    xcb_render_picture_t picture(ShadowElements element) const;

protected:
    virtual void buildQuads();
    virtual bool prepareBackend();
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
    virtual ~SceneXRenderDecorationRenderer();

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
