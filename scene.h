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

#ifndef KWIN_SCENE_H
#define KWIN_SCENE_H

#include "toplevel.h"
#include "utils.h"
#include "kwineffects.h"

namespace KWin
{

class Workspace;
class Deleted;
class EffectFrameImpl;
class EffectWindowImpl;
class LanczosFilter;
class OverlayWindow;
class Shadow;

// The base class for compositing backends.
class Scene : public QObject
{
    Q_OBJECT
public:
    Scene(Workspace* ws);
    virtual ~Scene() = 0;
    class EffectFrame;
    class Window;

    // Returns true if the ctor failed to properly initialize.
    virtual bool initFailed() const = 0;
    virtual CompositingType compositingType() const = 0;
    // Repaints the given screen areas, windows provides the stacking order.
    // The entry point for the main part of the painting pass.
    virtual void paint(QRegion damage, ToplevelList windows) = 0;

    // Notification function - KWin core informs about changes.
    // Used to mainly discard cached data.

    // a new window has been created
    virtual void windowAdded(Toplevel*) = 0;
    // a window has been destroyed
    virtual void windowDeleted(Deleted*) = 0;
    /**
     * Method invoked when the screen geometry is changed.
     * Reimplementing classes should also invoke the parent method
     * as it takes care of resizing the overlay window.
     * @param size The new screen geometry size
     **/
    virtual void screenGeometryChanged(const QSize &size);
    // Flags controlling how painting is done.
    enum {
        // Window (or at least part of it) will be painted opaque.
        PAINT_WINDOW_OPAQUE         = 1 << 0,
        // Window (or at least part of it) will be painted translucent.
        PAINT_WINDOW_TRANSLUCENT    = 1 << 1,
        // Window will be painted with transformed geometry.
        PAINT_WINDOW_TRANSFORMED    = 1 << 2,
        // Paint only a region of the screen (can be optimized, cannot
        // be used together with TRANSFORMED flags).
        PAINT_SCREEN_REGION         = 1 << 3,
        // Whole screen will be painted with transformed geometry.
        PAINT_SCREEN_TRANSFORMED    = 1 << 4,
        // At least one window will be painted with transformed geometry.
        PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
        // Clear whole background as the very first step, without optimizing it
        PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6,
        // PAINT_DECORATION_ONLY = 1 << 7 has been removed
        // Window will be painted with a lanczos filter.
        PAINT_WINDOW_LANCZOS = 1 << 8
        // PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS = 1 << 9 has been removed
    };
    // types of filtering available
    enum ImageFilterType { ImageFilterFast, ImageFilterGood };
    inline uint estimatedRenderTime() {
        return lastRenderTime;
    }
    // there's nothing to paint (adjust time_diff later)
    void idle();
    bool waitSyncAvailable() {
        return has_waitSync;
    }
    OverlayWindow* overlayWindow();
public Q_SLOTS:
    // opacity of a window changed
    virtual void windowOpacityChanged(KWin::Toplevel* c) = 0;
    // shape/size of a window changed
    virtual void windowGeometryShapeChanged(KWin::Toplevel* c) = 0;
    // a window has been closed
    virtual void windowClosed(KWin::Toplevel* c, KWin::Deleted* deleted) = 0;
protected:
    // shared implementation, starts painting the screen
    void paintScreen(int* mask, QRegion* region);
    friend class EffectsHandlerImpl;
    // called after all effects had their paintScreen() called
    void finalPaintScreen(int mask, QRegion region, ScreenPaintData& data);
    // shared implementation of painting the screen in the generic
    // (unoptimized) way
    virtual void paintGenericScreen(int mask, ScreenPaintData data);
    // shared implementation of painting the screen in an optimized way
    virtual void paintSimpleScreen(int mask, QRegion region);
    // paint the background (not the desktop background - the whole background)
    virtual void paintBackground(QRegion region) = 0;
    // called after all effects had their paintWindow() called
    void finalPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);
    // shared implementation, starts painting the window
    virtual void paintWindow(Window* w, int mask, QRegion region, WindowQuadList quads);
    // called after all effects had their drawWindow() called
    void finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);
    // compute time since the last repaint
    void updateTimeDiff();
    // saved data for 2nd pass of optimized screen painting
    struct Phase2Data {
        Phase2Data(Window* w, QRegion r, QRegion c, int m, const WindowQuadList& q)
            : window(w), region(r), clip(c), mask(m), quads(q) {}
        Phase2Data()  {
            window = 0;
            mask = 0;
        }
        Window* window;
        QRegion region;
        QRegion clip;
        int mask;
        WindowQuadList quads;
    };
    // windows in their stacking order
    QVector< Window* > stacking_order;
    // The region which actually has been painted by paintScreen() and should be
    // copied from the buffer to the screen. I.e. the region returned from Scene::paintScreen().
    // Since prePaintWindow() can extend areas to paint, these changes would have to propagate
    // up all the way from paintSimpleScreen() up to paintScreen(), so save them here rather
    // than propagate them up in arguments.
    QRegion painted_region;
    // time since last repaint
    int time_diff;
    uint lastRenderTime;
    QElapsedTimer last_time;
    Workspace* wspace;
    bool has_waitSync;
    LanczosFilter* lanczos_filter;
    OverlayWindow* m_overlayWindow;
};

// The base class for windows representations in composite backends
class Scene::Window
{
public:
    Window(Toplevel* c);
    virtual ~Window();
    // perform the actual painting of the window
    virtual void performPaint(int mask, QRegion region, WindowPaintData data) = 0;
    // do any cleanup needed when the window's composite pixmap is discarded
    virtual void pixmapDiscarded()  {}
    int x() const;
    int y() const;
    int width() const;
    int height() const;
    QRect geometry() const;
    QPoint pos() const;
    QSize size() const;
    QRect rect() const;
    // access to the internal window class
    // TODO eventually get rid of this
    Toplevel* window();
    // should the window be painted
    bool isPaintingEnabled() const;
    void resetPaintingEnabled();
    // Flags explaining why painting should be disabled
    enum {
        // Window will not be painted
        PAINT_DISABLED                 = 1 << 0,
        // Window will not be painted because it is deleted
        PAINT_DISABLED_BY_DELETE       = 1 << 1,
        // Window will not be painted because of which desktop it's on
        PAINT_DISABLED_BY_DESKTOP      = 1 << 2,
        // Window will not be painted because it is minimized
        PAINT_DISABLED_BY_MINIMIZE     = 1 << 3,
        // Window will not be painted because it is not the active window in a client group
        PAINT_DISABLED_BY_TAB_GROUP = 1 << 4,
        // Window will not be painted because it's not on the current activity
        PAINT_DISABLED_BY_ACTIVITY     = 1 << 5
    };
    void enablePainting(int reason);
    void disablePainting(int reason);
    // is the window visible at all
    bool isVisible() const;
    // is the window fully opaque
    bool isOpaque() const;
    // shape of the window
    QRegion shape() const;
    QRegion clientShape() const;
    void discardShape();
    void updateToplevel(Toplevel* c);
    // creates initial quad list for the window
    virtual WindowQuadList buildQuads(bool force = false) const;
    void suspendUnredirect(bool suspend);
    void updateShadow(Shadow* shadow);
    const Shadow* shadow() const;
    Shadow* shadow();
protected:
    WindowQuadList makeQuads(WindowQuadType type, const QRegion& reg) const;
    Toplevel* toplevel;
    ImageFilterType filter;
    Shadow *m_shadow;
private:
    int disable_painting;
    mutable QRegion shape_region;
    mutable bool shape_valid;
    mutable WindowQuadList* cached_quad_list;
    Q_DISABLE_COPY(Window)
};

class Scene::EffectFrame
{
public:
    EffectFrame(EffectFrameImpl* frame);
    virtual ~EffectFrame();
    virtual void render(QRegion region, double opacity, double frameOpacity) = 0;
    virtual void free() = 0;
    virtual void freeIconFrame() = 0;
    virtual void freeTextFrame() = 0;
    virtual void freeSelection() = 0;
    virtual void crossFadeIcon() = 0;
    virtual void crossFadeText() = 0;

protected:
    EffectFrameImpl* m_effectFrame;
};

extern Scene* scene;

inline
int Scene::Window::x() const
{
    return toplevel->x();
}

inline
int Scene::Window::y() const
{
    return toplevel->y();
}

inline
int Scene::Window::width() const
{
    return toplevel->width();
}

inline
int Scene::Window::height() const
{
    return toplevel->height();
}

inline
QRect Scene::Window::geometry() const
{
    return toplevel->geometry();
}

inline
QSize Scene::Window::size() const
{
    return toplevel->size();
}

inline
QPoint Scene::Window::pos() const
{
    return toplevel->pos();
}

inline
QRect Scene::Window::rect() const
{
    return toplevel->rect();
}

inline
Toplevel* Scene::Window::window()
{
    return toplevel;
}

inline
void Scene::Window::updateToplevel(Toplevel* c)
{
    toplevel = c;
}

inline
void Scene::Window::suspendUnredirect(bool suspend)
{
    toplevel->suspendUnredirect(suspend);
}

inline
void Scene::Window::updateShadow(Shadow* shadow)
{
    m_shadow = shadow;
}

inline
const Shadow* Scene::Window::shadow() const
{
    return m_shadow;
}

inline
Shadow* Scene::Window::shadow()
{
    return m_shadow;
}

} // namespace

#endif
