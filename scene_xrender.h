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

#ifdef KWIN_HAVE_XRENDER_COMPOSITING

namespace KWin
{


class SceneXrender
    : public Scene
{
    Q_OBJECT
public:
    class EffectFrame;
    explicit SceneXrender(Workspace* ws);
    virtual ~SceneXrender();
    virtual bool initFailed() const;
    virtual CompositingType compositingType() const {
        return XRenderCompositing;
    }
    virtual qint64 paint(QRegion damage, ToplevelList windows);
    virtual void windowAdded(Toplevel*);
    virtual void windowDeleted(Deleted*);
    virtual void screenGeometryChanged(const QSize &size);
    xcb_render_picture_t bufferPicture();
    virtual OverlayWindow *overlayWindow() {
        return m_overlayWindow;
    }
protected:
    virtual void paintBackground(QRegion region);
    virtual void paintGenericScreen(int mask, ScreenPaintData data);
    virtual void paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data);
public Q_SLOTS:
    virtual void windowOpacityChanged(KWin::Toplevel* c);
    virtual void windowGeometryShapeChanged(KWin::Toplevel* c);
    virtual void windowClosed(KWin::Toplevel* c, KWin::Deleted* deleted);
private:
    void createBuffer();
    void present(int mask, QRegion damage);
    void initXRender(bool createOverlay);
    xcb_render_pictformat_t format;
    xcb_render_picture_t front;
    static xcb_render_picture_t buffer;
    static ScreenPaintData screen_paint;
    class Window;
    QHash< Toplevel*, Window* > windows;
    OverlayWindow* m_overlayWindow;
    bool init_ok;
};

class SceneXrender::Window
    : public Scene::Window
{
public:
    Window(Toplevel* c);
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
    xcb_render_pictformat_t format;
    double alpha_cached_opacity;
    QRegion transformed_shape;
    static QRect temp_visibleRect;
    static XRenderPicture *s_tempPicture;
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
xcb_render_picture_t SceneXrender::bufferPicture()
{
    return buffer;
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

} // namespace

#endif

#endif
