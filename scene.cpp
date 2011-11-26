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

/*
 (NOTE: The compositing code is work in progress. As such this design
 documentation may get outdated in some areas.)

 The base class for compositing, implementing shared functionality
 between the OpenGL and XRender backends.

 Design:

 When compositing is turned on, XComposite extension is used to redirect
 drawing of windows to pixmaps and XDamage extension is used to get informed
 about damage (changes) to window contents. This code is mostly in composite.cpp .

 Workspace::performCompositing() starts one painting pass. Painting is done
 by painting the screen, which in turn paints every window. Painting can be affected
 using effects, which are chained. E.g. painting a screen means that actually
 paintScreen() of the first effect is called, which possibly does modifications
 and calls next effect's paintScreen() and so on, until Scene::finalPaintScreen()
 is called.

 There are 3 phases of every paint (not necessarily done together):
 The pre-paint phase, the paint phase and the post-paint phase.

 The pre-paint phase is used to find out about how the painting will be actually
 done (i.e. what the effects will do). For example when only a part of the screen
 needs to be updated and no effect will do any transformation it is possible to use
 an optimized paint function. How the painting will be done is controlled
 by the mask argument, see PAINT_WINDOW_* and PAINT_SCREEN_* flags in scene.h .
 For example an effect that decides to paint a normal windows as translucent
 will need to modify the mask in its prePaintWindow() to include
 the PAINT_WINDOW_TRANSLUCENT flag. The paintWindow() function will then get
 the mask with this flag turned on and will also paint using transparency.

 The paint pass does the actual painting, based on the information collected
 using the pre-paint pass. After running through the effects' paintScreen()
 either paintGenericScreen() or optimized paintSimpleScreen() are called.
 Those call paintWindow() on windows (not necessarily all), possibly using
 clipping to optimize performance and calling paintWindow() first with only
 PAINT_WINDOW_OPAQUE to paint the opaque parts and then later
 with PAINT_WINDOW_TRANSLUCENT to paint the transparent parts. Function
 paintWindow() again goes through effects' paintWindow() until
 finalPaintWindow() is called, which calls the window's performPaint() to
 do the actual painting.

 The post-paint can be used for cleanups and is also used for scheduling
 repaints during the next painting pass for animations. Effects wanting to
 repaint certain parts can manually damage them during post-paint and repaint
 of these parts will be done during the next paint pass.

*/

#include "scene.h"

#include <X11/extensions/shape.h>

#include "client.h"
#include "deleted.h"
#include "effects.h"
#include "lanczosfilter.h"
#include "overlaywindow.h"
#include "shadow.h"

#include <kephal/screens.h>
#include "thumbnailitem.h"

namespace KWin
{

//****************************************
// Scene
//****************************************

Scene* scene = 0;

Scene::Scene(Workspace* ws)
    : QObject(ws)
    , lastRenderTime(0)
    , wspace(ws)
    , has_waitSync(false)
    , lanczos_filter(new LanczosFilter())
    , m_overlayWindow(new OverlayWindow())
{
    last_time.invalidate(); // Initialize the timer
}

Scene::~Scene()
{
    delete lanczos_filter;
    delete m_overlayWindow;
}

// returns mask and possibly modified region
void Scene::paintScreen(int* mask, QRegion* region)
{
    *mask = (*region == QRegion(0, 0, displayWidth(), displayHeight()))
            ? 0 : PAINT_SCREEN_REGION;
    updateTimeDiff();
    // preparation step
    static_cast<EffectsHandlerImpl*>(effects)->startPaint();

    ScreenPrePaintData pdata;
    pdata.mask = *mask;
    pdata.paint = *region;

    // region only includes all workspace-specific repaints but some effect (e.g. blur)
    // rely on the full damaged area
    QRegion dirtyArea;
    foreach (Window * w, stacking_order) { // bottom to top
        Toplevel* topw = w->window();
        dirtyArea |= topw->repaints().translated(topw->pos());
        dirtyArea |= topw->decorationPendingRegion();
    }
    pdata.paint |= dirtyArea;

    effects->prePaintScreen(pdata, time_diff);
    *mask = pdata.mask;
    // Subtract the dirty region and let finalPaintScreen decide which areas have to be drawn
    *region |= pdata.paint - dirtyArea;

    if (*mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        // Region painting is not possible with transformations,
        // because screen damage doesn't match transformed positions.
        *mask &= ~PAINT_SCREEN_REGION;
        *region = infiniteRegion();
    } else if (*mask & PAINT_SCREEN_REGION) {
        // make sure not to go outside visible screen
        *region &= QRegion(0, 0, displayWidth(), displayHeight());
    } else {
        // whole screen, not transformed, force region to be full
        *region = QRegion(0, 0, displayWidth(), displayHeight());
    }
    painted_region = *region;
    if (*mask & PAINT_SCREEN_BACKGROUND_FIRST)
        paintBackground(*region);
    ScreenPaintData data;
    effects->paintScreen(*mask, *region, data);
    foreach (Window * w, stacking_order)
    effects->postPaintWindow(effectWindow(w));
    effects->postPaintScreen();
    *region |= painted_region;
    // make sure not to go outside of the screen area
    *region &= QRegion(0, 0, displayWidth(), displayHeight());
    // make sure all clipping is restored
    Q_ASSERT(!PaintClipper::clip());
}

// Compute time since the last painting pass.
void Scene::updateTimeDiff()
{
    if (!last_time.isValid()) {
        // Painting has been idle (optimized out) for some time,
        // which means time_diff would be huge and would break animations.
        // Simply set it to one (zero would mean no change at all and could
        // cause problems).
        time_diff = 1;
        last_time.start();
    } else
        time_diff = last_time.restart();

    if (time_diff < 0)   // check time rollback
        time_diff = 1;
}

// Painting pass is optimized away.
void Scene::idle()
{
    // Don't break time since last paint for the next pass.
    last_time.invalidate();
}

// the function that'll be eventually called by paintScreen() above
void Scene::finalPaintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS
               | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS))
        paintGenericScreen(mask, data);
    else
        paintSimpleScreen(mask, region);
}

// The generic painting code that can handle even transformations.
// It simply paints bottom-to-top.
void Scene::paintGenericScreen(int orig_mask, ScreenPaintData)
{
    if (!(orig_mask & PAINT_SCREEN_BACKGROUND_FIRST))
        paintBackground(infiniteRegion());
    QList< Phase2Data > phase2;
    foreach (Window * w, stacking_order) { // bottom to top
        Toplevel* topw = w->window();
        painted_region |= topw->repaints().translated(topw->pos());
        painted_region |= topw->decorationPendingRegion();

        // Reset the repaint_region.
        // This has to be done here because many effects schedule a repaint for
        // the next frame within Effects::prePaintWindow.
        topw->resetRepaints(topw->decorationRect());
        if (topw->hasShadow()) {
            topw->resetRepaints(topw->shadow()->shadowRegion().boundingRect());
        }

        WindowPrePaintData data;
        data.mask = orig_mask | (w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT);
        w->resetPaintingEnabled();
        data.paint = infiniteRegion(); // no clipping, so doesn't really matter
        data.clip = QRegion();
        data.quads = w->buildQuads();
        // preparation step
        effects->prePaintWindow(effectWindow(w), data, time_diff);
#ifndef NDEBUG
        foreach (const WindowQuad & q, data.quads)
        if (q.isTransformed())
            kFatal(1212) << "Pre-paint calls are not allowed to transform quads!" ;
#endif
        if (!w->isPaintingEnabled())
            continue;
        phase2.append(Phase2Data(w, infiniteRegion(), data.clip, data.mask, data.quads));
        // transformations require window pixmap
        w->suspendUnredirect(data.mask
                             & (PAINT_WINDOW_TRANSLUCENT | PAINT_SCREEN_TRANSFORMED | PAINT_WINDOW_TRANSFORMED));
    }

    foreach (const Phase2Data & d, phase2)
    paintWindow(d.window, d.mask, d.region, d.quads);
}

// The optimized case without any transformations at all.
// It can paint only the requested region and can use clipping
// to reduce painting and improve performance.
void Scene::paintSimpleScreen(int orig_mask, QRegion region)
{
    // TODO PAINT_WINDOW_* flags don't belong here, that's why it's in the assert,
    // perhaps the two enums should be separated
    assert((orig_mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED
                         | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS
                         | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS
                         | PAINT_WINDOW_TRANSLUCENT | PAINT_WINDOW_OPAQUE)) == 0);
    QList< QPair< Window*, Phase2Data > > phase2data;

    QRegion dirtyArea = region;
    for (int i = 0;  // do prePaintWindow bottom to top
            i < stacking_order.count();
            ++i) {
        Window* w = stacking_order[ i ];
        Toplevel* topw = w->window();
        WindowPrePaintData data;
        data.mask = orig_mask | (w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT);
        w->resetPaintingEnabled();
        data.paint = region;
        data.paint |= topw->repaints().translated(topw->pos());
        data.paint |= topw->decorationPendingRegion();

        // Reset the repaint_region.
        // This has to be done here because many effects schedule a repaint for
        // the next frame within Effects::prePaintWindow.
        topw->resetRepaints(topw->decorationRect());
        if (topw->hasShadow()) {
            topw->resetRepaints(topw->shadow()->shadowRegion().boundingRect());
        }
        // Clip out the decoration for opaque windows; the decoration is drawn in the second pass
        if (w->isOpaque()) {
            // the window is fully opaque
            data.clip = w->clientShape().translated(w->x(), w->y());
        } else if (topw->hasAlpha() && topw->opacity() == 1.0) {
            // the window is partially opaque
            data.clip = (w->clientShape() & topw->opaqueRegion()).translated(w->x(), w->y());
        } else {
            data.clip = QRegion();
        }
        data.quads = w->buildQuads();
        // preparation step
        effects->prePaintWindow(effectWindow(w), data, time_diff);
#ifndef NDEBUG
        foreach (const WindowQuad & q, data.quads)
        if (q.isTransformed())
            kFatal(1212) << "Pre-paint calls are not allowed to transform quads!" ;
        if (data.mask & PAINT_WINDOW_TRANSFORMED)
            kFatal(1212) << "PAINT_WINDOW_TRANSFORMED without PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS!";
#endif
        if (!w->isPaintingEnabled()) {
            w->suspendUnredirect(true);
            continue;
        }
        dirtyArea |= data.paint;
        // Schedule the window for painting
        phase2data.append(QPair< Window*, Phase2Data >(w,Phase2Data(w, data.paint, data.clip,
                                                                    data.mask, data.quads)));
        // no transformations, but translucency requires window pixmap
        w->suspendUnredirect(data.mask & PAINT_WINDOW_TRANSLUCENT);
    }

    // This is the occlusion culling pass
    QRegion allclips, upperTranslucentDamage;
    for (int i = phase2data.count() - 1; i >= 0; --i) {
        QPair< Window*, Phase2Data > *entry = &phase2data[i];
        Phase2Data *data = &entry->second;

        Toplevel *tlw = entry->first->window();
        // In case there is a window with a higher stackposition which has translucent regions
        // (e.g. decorations) that still have to be drawn, we also have to repaint the current window
        // in these particular regions
        data->region |= (upperTranslucentDamage & tlw->decorationRect().translated(tlw->pos()));

        // subtract the parts which will possibly been drawn as part of
        // a higher opaque window
        data->region -= allclips;

        // Here we rely on WindowPrePaintData::setTranslucent() to remove
        // the clip if needed.
        if (!data->clip.isEmpty()) {
            // clip away the opaque regions for all windows below this one
            allclips |= data->clip;
            // extend the translucent damage for windows below this by remaining (translucent) regions
            upperTranslucentDamage |= data->region - data->clip;
        } else {
            upperTranslucentDamage |= data->region;
        }
    }

    QRegion paintedArea;
    // Fill any areas of the root window not covered by opaque windows
    if (!(orig_mask & PAINT_SCREEN_BACKGROUND_FIRST)) {
        paintedArea = dirtyArea - allclips;
        paintBackground(paintedArea);
    }

    // Now walk the list bottom to top and draw the windows.
    for (int i = 0; i < phase2data.count(); ++i) {
        Phase2Data *data = &phase2data[i].second;

        // add all regions which have been drawn so far
        paintedArea |= data->region;
        data->region = paintedArea;

        paintWindow(data->window, data->mask, data->region, data->quads);
    }
    painted_region |= paintedArea;
}

void Scene::paintWindow(Window* w, int mask, QRegion region, WindowQuadList quads)
{
    // no painting outside visible screen (and no transformations)
    region &= QRect(0, 0, displayWidth(), displayHeight());
    if (region.isEmpty())  // completely clipped
        return;

    WindowPaintData data(w->window()->effectWindow());
    data.quads = quads;
    effects->paintWindow(effectWindow(w), mask, region, data);
    // paint thumbnails on top of window
    EffectWindowImpl *wImpl = static_cast<EffectWindowImpl*>(effectWindow(w));
    for (QHash<ThumbnailItem*, QWeakPointer<EffectWindowImpl> >::const_iterator it = wImpl->thumbnails().constBegin();
            it != wImpl->thumbnails().constEnd();
            ++it) {
        if (it.value().isNull()) {
            continue;
        }
        ThumbnailItem *item = it.key();
        if (!item->isVisible()) {
            continue;
        }
        EffectWindowImpl *thumb = it.value().data();
        WindowPaintData thumbData(thumb);
        thumbData.opacity = data.opacity;

        QSizeF size = QSizeF(thumb->size());
        size.scale(QSizeF(item->sceneBoundingRect().width(), item->sceneBoundingRect().height()), Qt::KeepAspectRatio);
        thumbData.xScale = size.width() / static_cast<qreal>(thumb->width());
        thumbData.yScale = size.height() / static_cast<qreal>(thumb->height());
        const int x = item->scenePos().x() + w->x() + (item->width() - size.width()) / 2;
        const int y = item->scenePos().y() + w->y() + (item->height() - size.height()) / 2;
        thumbData.xTranslate = x - thumb->x();
        thumbData.yTranslate = y - thumb->y();
        int thumbMask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_LANCZOS;
        if (thumbData.opacity == 1.0) {
            thumbMask |= PAINT_WINDOW_OPAQUE;
        } else {
            thumbMask |= PAINT_WINDOW_TRANSLUCENT;
        }
        if (item->isClip() && (x < wImpl->x() || x + size.width() > wImpl->x() + wImpl->width() ||
            y < wImpl->y() || y + size.height() > wImpl->y() + wImpl->height())) {
            // don't render windows outside the containing window.
            // TODO: improve by spliting out the window quads which do not fit
            continue;
        }
        effects->drawWindow(thumb, thumbMask, QRegion(x, y, size.width(), size.height()), thumbData);
    }
}

// the function that'll be eventually called by paintWindow() above
void Scene::finalPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    effects->drawWindow(w, mask, region, data);
}

// will be eventually called from drawWindow()
void Scene::finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    if (mask & PAINT_WINDOW_LANCZOS)
        lanczos_filter->performPaint(w, mask, region, data);
    else
        w->sceneWindow()->performPaint(mask, region, data);
}

OverlayWindow* Scene::overlayWindow()
{
    return m_overlayWindow;
}

void Scene::screenGeometryChanged(const QSize &size)
{
    m_overlayWindow->resize(size);
}

//****************************************
// Scene::Window
//****************************************

Scene::Window::Window(Toplevel * c)
    : toplevel(c)
    , filter(ImageFilterFast)
    , m_shadow(NULL)
    , disable_painting(0)
    , shape_valid(false)
    , cached_quad_list(NULL)
{
}

Scene::Window::~Window()
{
    delete cached_quad_list;
    delete m_shadow;
}

void Scene::Window::discardShape()
{
    // it is created on-demand and cached, simply
    // reset the flag
    shape_valid = false;
    delete cached_quad_list;
    cached_quad_list = NULL;
}

// Find out the shape of the window using the XShape extension
// or if shape is not set then simply it's the window geometry.
QRegion Scene::Window::shape() const
{
    if (!shape_valid) {
        Client* c = dynamic_cast< Client* >(toplevel);
        if (toplevel->shape() || (c != NULL && !c->mask().isEmpty())) {
            int count, order;
            XRectangle* rects = XShapeGetRectangles(display(), toplevel->frameId(),
                                                    ShapeBounding, &count, &order);
            if (rects) {
                shape_region = QRegion();
                for (int i = 0;
                        i < count;
                        ++i)
                    shape_region += QRegion(rects[ i ].x, rects[ i ].y,
                                            rects[ i ].width, rects[ i ].height);
                XFree(rects);
                // make sure the shape is sane (X is async, maybe even XShape is broken)
                shape_region &= QRegion(0, 0, width(), height());
            } else
                shape_region = QRegion();
        } else
            shape_region = QRegion(0, 0, width(), height());
        shape_valid = true;
    }
    return shape_region;
}

QRegion Scene::Window::clientShape() const
{
    Client *c = dynamic_cast< Client* >(toplevel);
    if (c && c->isShade())
        return QRegion();

    const QRegion r = shape() & QRect(toplevel->clientPos(), toplevel->clientSize());
    return r.isEmpty() ? QRegion() : r;
}

bool Scene::Window::isVisible() const
{
    if (dynamic_cast< Deleted* >(toplevel) != NULL)
        return false;
    if (!toplevel->isOnCurrentDesktop())
        return false;
    if (!toplevel->isOnCurrentActivity())
        return false;
    if (Client* c = dynamic_cast< Client* >(toplevel))
        return c->isShown(true);
    return true; // Unmanaged is always visible
    // TODO there may be transformations, so ignore this for now
    return !toplevel->geometry()
           .intersected(QRect(0, 0, displayWidth(), displayHeight()))
           .isEmpty();
}

bool Scene::Window::isOpaque() const
{
    return toplevel->opacity() == 1.0 && !toplevel->hasAlpha();
}

bool Scene::Window::isPaintingEnabled() const
{
    return !disable_painting;
}

void Scene::Window::resetPaintingEnabled()
{
    disable_painting = 0;
    if (dynamic_cast< Deleted* >(toplevel) != NULL)
        disable_painting |= PAINT_DISABLED_BY_DELETE;
    if (!toplevel->isOnCurrentDesktop())
        disable_painting |= PAINT_DISABLED_BY_DESKTOP;
    if (!toplevel->isOnCurrentActivity())
        disable_painting |= PAINT_DISABLED_BY_ACTIVITY;
    if (Client* c = dynamic_cast< Client* >(toplevel)) {
        if (c->isMinimized())
            disable_painting |= PAINT_DISABLED_BY_MINIMIZE;
        if (c->clientGroup() && c != c->clientGroup()->visible())
            disable_painting |= PAINT_DISABLED_BY_CLIENT_GROUP;
        else if (c->isHiddenInternal())
            disable_painting |= PAINT_DISABLED;
    }
}

void Scene::Window::enablePainting(int reason)
{
    disable_painting &= ~reason;
}

void Scene::Window::disablePainting(int reason)
{
    disable_painting |= reason;
}

WindowQuadList Scene::Window::buildQuads(bool force) const
{
    if (cached_quad_list != NULL && !force)
        return *cached_quad_list;
    WindowQuadList ret;
    if (toplevel->clientPos() == QPoint(0, 0) && toplevel->clientSize() == toplevel->visibleRect().size())
        ret = makeQuads(WindowQuadContents, shape());  // has no decoration
    else {
        Client *client = dynamic_cast<Client*>(toplevel);
        QRegion contents = clientShape();
        QRegion center = toplevel->transparentRect();
        QRegion decoration = (client && Workspace::self()->decorationHasAlpha() ?
                              QRegion(client->decorationRect()) : shape()) - center;
        ret = makeQuads(WindowQuadContents, contents);
        if (!client || !(center.isEmpty() || client->isShade()))
            ret += makeQuads(WindowQuadDecoration, decoration);
        else {
            // this is a shaded client, we have to create four decoartion quads
            QRect left, top, right, bottom;
            client->layoutDecorationRects(left, top, right, bottom, Client::WindowRelative);
            ret += makeQuads(WindowQuadDecoration, top);
            ret += makeQuads(WindowQuadDecoration, bottom);
            ret += makeQuads(WindowQuadDecoration, left);
            ret += makeQuads(WindowQuadDecoration, right);
        }
    }
    if (m_shadow) {
        ret << m_shadow->shadowQuads();
    }
    effects->buildQuads(static_cast<Client*>(toplevel)->effectWindow(), ret);
    cached_quad_list = new WindowQuadList(ret);
    return ret;
}

WindowQuadList Scene::Window::makeQuads(WindowQuadType type, const QRegion& reg) const
{
    WindowQuadList ret;
    foreach (const QRect & r, reg.rects()) {
        WindowQuad quad(type);
        // TODO asi mam spatne pravy dolni roh - bud tady, nebo v jinych castech
        quad[ 0 ] = WindowVertex(r.x(), r.y(), r.x(), r.y());
        quad[ 1 ] = WindowVertex(r.x() + r.width(), r.y(), r.x() + r.width(), r.y());
        quad[ 2 ] = WindowVertex(r.x() + r.width(), r.y() + r.height(), r.x() + r.width(), r.y() + r.height());
        quad[ 3 ] = WindowVertex(r.x(), r.y() + r.height(), r.x(), r.y() + r.height());
        ret.append(quad);
    }
    return ret;
}

//****************************************
// Scene::EffectFrame
//****************************************
Scene::EffectFrame::EffectFrame(EffectFrameImpl* frame)
    : m_effectFrame(frame)
{
}

Scene::EffectFrame::~EffectFrame()
{
}

} // namespace
