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
 The base class for compositing, implementing shared functionality
 between the OpenGL and XRender backends.

 Design:

 When compositing is turned on, XComposite extension is used to redirect
 drawing of windows to pixmaps and XDamage extension is used to get informed
 about damage (changes) to window contents. This code is mostly in composite.cpp .

 Compositor::performCompositing() starts one painting pass. Painting is done
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

#include <QQuickWindow>
#include <QVector2D>

#include "client.h"
#include "deleted.h"
#include "effects.h"
#include "overlaywindow.h"
#include "screens.h"
#include "shadow.h"
#include "wayland_server.h"

#include "thumbnailitem.h"

#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/subcompositor_interface.h>
#include <KWayland/Server/surface_interface.h>

namespace KWin
{

//****************************************
// Scene
//****************************************

Scene::Scene(QObject *parent)
    : QObject(parent)
{
    last_time.invalidate(); // Initialize the timer
}

Scene::~Scene()
{
    foreach (Window *w, m_windows) {
        delete w;
    }
}

// returns mask and possibly modified region
void Scene::paintScreen(int* mask, const QRegion &damage, const QRegion &repaint,
                        QRegion *updateRegion, QRegion *validRegion, const QMatrix4x4 &projection, const QRect &outputGeometry)
{
    const QSize &screenSize = screens()->size();
    const QRegion displayRegion(0, 0, screenSize.width(), screenSize.height());
    *mask = (damage == displayRegion) ? 0 : PAINT_SCREEN_REGION;

    updateTimeDiff();
    // preparation step
    static_cast<EffectsHandlerImpl*>(effects)->startPaint();

    QRegion region = damage;

    ScreenPrePaintData pdata;
    pdata.mask = *mask;
    pdata.paint = region;

    effects->prePaintScreen(pdata, time_diff);
    *mask = pdata.mask;
    region = pdata.paint;

    if (*mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) {
        // Region painting is not possible with transformations,
        // because screen damage doesn't match transformed positions.
        *mask &= ~PAINT_SCREEN_REGION;
        region = infiniteRegion();
    } else if (*mask & PAINT_SCREEN_REGION) {
        // make sure not to go outside visible screen
        region &= displayRegion;
    } else {
        // whole screen, not transformed, force region to be full
        region = displayRegion;
    }

    painted_region = region;
    repaint_region = repaint;

    if (*mask & PAINT_SCREEN_BACKGROUND_FIRST) {
        paintBackground(region);
    }

    ScreenPaintData data(projection, outputGeometry);
    effects->paintScreen(*mask, region, data);

    foreach (Window *w, stacking_order) {
        effects->postPaintWindow(effectWindow(w));
    }

    effects->postPaintScreen();

    // make sure not to go outside of the screen area
    *updateRegion = damaged_region;
    *validRegion = (region | painted_region) & displayRegion;

    repaint_region = QRegion();
    damaged_region = QRegion();

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
    if (mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS))
        paintGenericScreen(mask, data);
    else
        paintSimpleScreen(mask, region);
}

// The generic painting code that can handle even transformations.
// It simply paints bottom-to-top.
void Scene::paintGenericScreen(int orig_mask, ScreenPaintData)
{
    if (!(orig_mask & PAINT_SCREEN_BACKGROUND_FIRST)) {
        paintBackground(infiniteRegion());
    }
    QList< Phase2Data > phase2;
    foreach (Window * w, stacking_order) { // bottom to top
        Toplevel* topw = w->window();

        // Reset the repaint_region.
        // This has to be done here because many effects schedule a repaint for
        // the next frame within Effects::prePaintWindow.
        topw->resetRepaints();

        WindowPrePaintData data;
        data.mask = orig_mask | (w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT);
        w->resetPaintingEnabled();
        data.paint = infiniteRegion(); // no clipping, so doesn't really matter
        data.clip = QRegion();
        data.quads = w->buildQuads();
        // preparation step
        effects->prePaintWindow(effectWindow(w), data, time_diff);
#ifndef NDEBUG
        if (data.quads.isTransformed()) {
            qFatal("Pre-paint calls are not allowed to transform quads!");
        }
#endif
        if (!w->isPaintingEnabled()) {
            continue;
        }
        phase2.append(Phase2Data(w, infiniteRegion(), data.clip, data.mask, data.quads));
    }

    foreach (const Phase2Data & d, phase2) {
        paintWindow(d.window, d.mask, d.region, d.quads);
    }

    const QSize &screenSize = screens()->size();
    damaged_region = QRegion(0, 0, screenSize.width(), screenSize.height());
}

// The optimized case without any transformations at all.
// It can paint only the requested region and can use clipping
// to reduce painting and improve performance.
void Scene::paintSimpleScreen(int orig_mask, QRegion region)
{
    assert((orig_mask & (PAINT_SCREEN_TRANSFORMED
                         | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS)) == 0);
    QList< QPair< Window*, Phase2Data > > phase2data;

    QRegion dirtyArea = region;
    bool opaqueFullscreen(false);
    for (int i = 0;  // do prePaintWindow bottom to top
            i < stacking_order.count();
            ++i) {
        Window* w = stacking_order[ i ];
        Toplevel* topw = w->window();
        WindowPrePaintData data;
        data.mask = orig_mask | (w->isOpaque() ? PAINT_WINDOW_OPAQUE : PAINT_WINDOW_TRANSLUCENT);
        w->resetPaintingEnabled();
        data.paint = region;
        data.paint |= topw->repaints();

        // Reset the repaint_region.
        // This has to be done here because many effects schedule a repaint for
        // the next frame within Effects::prePaintWindow.
        topw->resetRepaints();

        // Clip out the decoration for opaque windows; the decoration is drawn in the second pass
        opaqueFullscreen = false; // TODO: do we care about unmanged windows here (maybe input windows?)
        if (w->isOpaque()) {
            AbstractClient *c = dynamic_cast<AbstractClient*>(topw);
            if (c) {
                opaqueFullscreen = c->isFullScreen();
            }
            Client *cc = dynamic_cast<Client*>(c);
            // the window is fully opaque
            if (cc && cc->decorationHasAlpha()) {
                // decoration uses alpha channel, so we may not exclude it in clipping
                data.clip = w->clientShape().translated(w->x(), w->y());
            } else {
                // decoration is fully opaque
                if (c && c->isShade()) {
                    data.clip = QRegion();
                } else {
                    data.clip = w->shape().translated(w->x(), w->y());
                }
            }
        } else if (topw->hasAlpha() && topw->opacity() == 1.0) {
            // the window is partially opaque
            data.clip = (w->clientShape() & topw->opaqueRegion().translated(topw->clientPos())).translated(w->x(), w->y());
        } else {
            data.clip = QRegion();
        }
        data.quads = w->buildQuads();
        // preparation step
        effects->prePaintWindow(effectWindow(w), data, time_diff);
#ifndef NDEBUG
        if (data.quads.isTransformed()) {
            qFatal("Pre-paint calls are not allowed to transform quads!");
        }
#endif
        if (!w->isPaintingEnabled()) {
            continue;
        }
        dirtyArea |= data.paint;
        // Schedule the window for painting
        phase2data.append(QPair< Window*, Phase2Data >(w,Phase2Data(w, data.paint, data.clip,
                                                                    data.mask, data.quads)));
    }

    // Save the part of the repaint region that's exclusively rendered to
    // bring a reused back buffer up to date. Then union the dirty region
    // with the repaint region.
    const QRegion repaintClip = repaint_region - dirtyArea;
    dirtyArea |= repaint_region;

    const QSize &screenSize = screens()->size();
    const QRegion displayRegion(0, 0, screenSize.width(), screenSize.height());
    bool fullRepaint(dirtyArea == displayRegion); // spare some expensive region operations
    if (!fullRepaint) {
        extendPaintRegion(dirtyArea, opaqueFullscreen);
        fullRepaint = (dirtyArea == displayRegion);
    }

    QRegion allclips, upperTranslucentDamage;
    upperTranslucentDamage = repaint_region;

    // This is the occlusion culling pass
    for (int i = phase2data.count() - 1; i >= 0; --i) {
        QPair< Window*, Phase2Data > *entry = &phase2data[i];
        Phase2Data *data = &entry->second;

        if (fullRepaint)
            data->region = displayRegion;
        else
            data->region |= upperTranslucentDamage;

        // subtract the parts which will possibly been drawn as part of
        // a higher opaque window
        data->region -= allclips;

        // Here we rely on WindowPrePaintData::setTranslucent() to remove
        // the clip if needed.
        if (!data->clip.isEmpty() && !(data->mask & PAINT_WINDOW_TRANSFORMED)) {
            // clip away the opaque regions for all windows below this one
            allclips |= data->clip;
            // extend the translucent damage for windows below this by remaining (translucent) regions
            if (!fullRepaint)
                upperTranslucentDamage |= data->region - data->clip;
        } else if (!fullRepaint) {
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

    if (fullRepaint) {
        painted_region = displayRegion;
        damaged_region = displayRegion;
    } else {
        painted_region |= paintedArea;

        // Clip the repainted region from the damaged region.
        // It's important that we don't add the union of the damaged region
        // and the repainted region to the damage history. Otherwise the
        // repaint region will grow with every frame until it eventually
        // covers the whole back buffer, at which point we're always doing
        // full repaints.
        damaged_region = paintedArea - repaintClip;
    }
}

void Scene::windowAdded(Toplevel *c)
{
    assert(!m_windows.contains(c));
    Scene::Window *w = createWindow(c);
    m_windows[ c ] = w;
    connect(c, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), SLOT(windowGeometryShapeChanged(KWin::Toplevel*)));
    connect(c, SIGNAL(windowClosed(KWin::Toplevel*,KWin::Deleted*)), SLOT(windowClosed(KWin::Toplevel*,KWin::Deleted*)));
    //A change of scale won't affect the geometry in compositor co-ordinates, but will affect the window quads.
    if (c->surface()) {
        connect(c->surface(), &KWayland::Server::SurfaceInterface::scaleChanged, this, std::bind(&Scene::windowGeometryShapeChanged, this, c));
    }
    connect(c, &Toplevel::screenScaleChanged, std::bind(&Scene::windowGeometryShapeChanged, this, c));
    c->effectWindow()->setSceneWindow(w);
    c->getShadow();
    w->updateShadow(c->shadow());
}

void Scene::windowClosed(Toplevel *c, Deleted *deleted)
{
    assert(m_windows.contains(c));
    if (deleted != NULL) {
        // replace c with deleted
        Window* w = m_windows.take(c);
        w->updateToplevel(deleted);
        if (w->shadow()) {
            w->shadow()->setToplevel(deleted);
        }
        m_windows[ deleted ] = w;
    } else {
        delete m_windows.take(c);
        c->effectWindow()->setSceneWindow(NULL);
    }
}

void Scene::windowDeleted(Deleted *c)
{
    assert(m_windows.contains(c));
    delete m_windows.take(c);
    c->effectWindow()->setSceneWindow(NULL);
}

void Scene::windowGeometryShapeChanged(Toplevel *c)
{
    if (!m_windows.contains(c))    // this is ok, shape is not valid by default
        return;
    Window *w = m_windows[ c ];
    w->discardShape();
}

void Scene::createStackingOrder(ToplevelList toplevels)
{
    // TODO: cache the stacking_order in case it has not changed
    foreach (Toplevel *c, toplevels) {
        assert(m_windows.contains(c));
        stacking_order.append(m_windows[ c ]);
    }
}

void Scene::clearStackingOrder()
{
    stacking_order.clear();
}

static Scene::Window *s_recursionCheck = NULL;

void Scene::paintWindow(Window* w, int mask, QRegion region, WindowQuadList quads)
{
    // no painting outside visible screen (and no transformations)
    const QSize &screenSize = screens()->size();
    region &= QRect(0, 0, screenSize.width(), screenSize.height());
    if (region.isEmpty())  // completely clipped
        return;
    if (w->window()->isDeleted() && w->window()->skipsCloseAnimation()) {
        // should not get painted
        return;
    }

    if (s_recursionCheck == w) {
        return;
    }

    WindowPaintData data(w->window()->effectWindow(), screenProjectionMatrix());
    data.quads = quads;
    effects->paintWindow(effectWindow(w), mask, region, data);
    // paint thumbnails on top of window
    paintWindowThumbnails(w, region, data.opacity(), data.brightness(), data.saturation());
    // and desktop thumbnails
    paintDesktopThumbnails(w);
}

static void adjustClipRegion(AbstractThumbnailItem *item, QRegion &clippingRegion)
{
    if (item->clip() && item->clipTo()) {
        // the x/y positions of the parent item are not correct. The margins are added, though the size seems fine
        // that's why we have to get the offset by inspecting the anchors properties
        QQuickItem *parentItem = item->clipTo();
        QPointF offset;
        QVariant anchors = parentItem->property("anchors");
        if (anchors.isValid()) {
            if (QObject *anchorsObject = anchors.value<QObject*>()) {
                offset.setX(anchorsObject->property("leftMargin").toReal());
                offset.setY(anchorsObject->property("topMargin").toReal());
            }
        }
        QRectF rect = QRectF(parentItem->position() - offset, QSizeF(parentItem->width(), parentItem->height()));
        if (QQuickItem *p = parentItem->parentItem()) {
            rect = p->mapRectToScene(rect);
        }
        clippingRegion &= rect.adjusted(0,0,-1,-1).translated(item->window()->position()).toRect();
    }
}

void Scene::paintWindowThumbnails(Scene::Window *w, QRegion region, qreal opacity, qreal brightness, qreal saturation)
{
    EffectWindowImpl *wImpl = static_cast<EffectWindowImpl*>(effectWindow(w));
    for (QHash<WindowThumbnailItem*, QWeakPointer<EffectWindowImpl> >::const_iterator it = wImpl->thumbnails().constBegin();
            it != wImpl->thumbnails().constEnd();
            ++it) {
        if (it.value().isNull()) {
            continue;
        }
        WindowThumbnailItem *item = it.key();
        if (!item->isVisible()) {
            continue;
        }
        EffectWindowImpl *thumb = it.value().data();
        WindowPaintData thumbData(thumb, screenProjectionMatrix());
        thumbData.setOpacity(opacity);
        thumbData.setBrightness(brightness * item->brightness());
        thumbData.setSaturation(saturation * item->saturation());

        const QRect visualThumbRect(thumb->expandedGeometry());

        QSizeF size = QSizeF(visualThumbRect.size());
        size.scale(QSizeF(item->width(), item->height()), Qt::KeepAspectRatio);
        if (size.width() > visualThumbRect.width() || size.height() > visualThumbRect.height()) {
            size = QSizeF(visualThumbRect.size());
        }
        thumbData.setXScale(size.width() / static_cast<qreal>(visualThumbRect.width()));
        thumbData.setYScale(size.height() / static_cast<qreal>(visualThumbRect.height()));

        if (!item->window()) {
            continue;
        }
        const QPointF point = item->mapToScene(item->position());
        qreal x = point.x() + w->x() + (item->width() - size.width())/2;
        qreal y = point.y() + w->y() + (item->height() - size.height()) / 2;
        x -= thumb->x();
        y -= thumb->y();
        // compensate shadow topleft padding
        x += (thumb->x()-visualThumbRect.x())*thumbData.xScale();
        y += (thumb->y()-visualThumbRect.y())*thumbData.yScale();
        thumbData.setXTranslation(x);
        thumbData.setYTranslation(y);
        int thumbMask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_LANCZOS;
        if (thumbData.opacity() == 1.0) {
            thumbMask |= PAINT_WINDOW_OPAQUE;
        } else {
            thumbMask |= PAINT_WINDOW_TRANSLUCENT;
        }
        QRegion clippingRegion = region;
        clippingRegion &= QRegion(wImpl->x(), wImpl->y(), wImpl->width(), wImpl->height());
        adjustClipRegion(item, clippingRegion);
        effects->drawWindow(thumb, thumbMask, clippingRegion, thumbData);
    }
}

void Scene::paintDesktopThumbnails(Scene::Window *w)
{
    EffectWindowImpl *wImpl = static_cast<EffectWindowImpl*>(effectWindow(w));
    for (QList<DesktopThumbnailItem*>::const_iterator it = wImpl->desktopThumbnails().constBegin();
            it != wImpl->desktopThumbnails().constEnd();
            ++it) {
        DesktopThumbnailItem *item = *it;
        if (!item->isVisible()) {
            continue;
        }
        if (!item->window()) {
            continue;
        }
        s_recursionCheck = w;

        ScreenPaintData data;
        const QSize &screenSize = screens()->size();
        QSize size = screenSize;

        size.scale(item->width(), item->height(), Qt::KeepAspectRatio);
        data *= QVector2D(size.width() / double(screenSize.width()),
                          size.height() / double(screenSize.height()));
        const QPointF point = item->mapToScene(item->position());
        const qreal x = point.x() + w->x() + (item->width() - size.width())/2;
        const qreal y = point.y() + w->y() + (item->height() - size.height()) / 2;
        const QRect region = QRect(x, y, item->width(), item->height());
        QRegion clippingRegion = region;
        clippingRegion &= QRegion(wImpl->x(), wImpl->y(), wImpl->width(), wImpl->height());
        adjustClipRegion(item, clippingRegion);
        data += QPointF(x, y);
        const int desktopMask = PAINT_SCREEN_TRANSFORMED | PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        paintDesktop(item->desktop(), desktopMask, clippingRegion, data);
        s_recursionCheck = NULL;
    }
}

void Scene::paintDesktop(int desktop, int mask, const QRegion &region, ScreenPaintData &data)
{
    static_cast<EffectsHandlerImpl*>(effects)->paintDesktop(desktop, mask, region, data);
}

// the function that'll be eventually called by paintWindow() above
void Scene::finalPaintWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    effects->drawWindow(w, mask, region, data);
}

// will be eventually called from drawWindow()
void Scene::finalDrawWindow(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    if (waylandServer() && waylandServer()->isScreenLocked() && !w->window()->isLockScreen() && !w->window()->isInputMethod()) {
        return;
    }
    w->sceneWindow()->performPaint(mask, region, data);
}

void Scene::extendPaintRegion(QRegion &region, bool opaqueFullscreen)
{
    Q_UNUSED(region);
    Q_UNUSED(opaqueFullscreen);
}

bool Scene::blocksForRetrace() const
{
    return false;
}

bool Scene::syncsToVBlank() const
{
    return false;
}

void Scene::screenGeometryChanged(const QSize &size)
{
    if (!overlayWindow()) {
        return;
    }
    overlayWindow()->resize(size);
}

bool Scene::makeOpenGLContextCurrent()
{
    return false;
}

void Scene::doneOpenGLContextCurrent()
{
}

void Scene::triggerFence()
{
}

QMatrix4x4 Scene::screenProjectionMatrix() const
{
    return QMatrix4x4();
}

xcb_render_picture_t Scene::xrenderBufferPicture() const
{
    return XCB_RENDER_PICTURE_NONE;
}

QPainter *Scene::scenePainter() const
{
    return nullptr;
}

QImage *Scene::qpainterRenderBuffer() const
{
    return nullptr;
}

QVector<QByteArray> Scene::openGLPlatformInterfaceExtensions() const
{
    return QVector<QByteArray>{};
}

//****************************************
// Scene::Window
//****************************************

Scene::Window::Window(Toplevel * c)
    : toplevel(c)
    , filter(ImageFilterFast)
    , m_shadow(NULL)
    , m_currentPixmap()
    , m_previousPixmap()
    , m_referencePixmapCounter(0)
    , disable_painting(0)
    , shape_valid(false)
    , cached_quad_list(NULL)
{
}

Scene::Window::~Window()
{
    delete m_shadow;
}

void Scene::Window::referencePreviousPixmap()
{
    if (!m_previousPixmap.isNull() && m_previousPixmap->isDiscarded()) {
        m_referencePixmapCounter++;
    }
}

void Scene::Window::unreferencePreviousPixmap()
{
    if (m_previousPixmap.isNull() || !m_previousPixmap->isDiscarded()) {
        return;
    }
    m_referencePixmapCounter--;
    if (m_referencePixmapCounter == 0) {
        m_previousPixmap.reset();
    }
}

void Scene::Window::pixmapDiscarded()
{
    if (!m_currentPixmap.isNull()) {
        if (m_currentPixmap->isValid()) {
            m_previousPixmap.reset(m_currentPixmap.take());
            m_previousPixmap->markAsDiscarded();
        } else {
            m_currentPixmap.reset();
        }
    }
}

void Scene::Window::discardShape()
{
    // it is created on-demand and cached, simply
    // reset the flag
    shape_valid = false;
    cached_quad_list.reset();
}

// Find out the shape of the window using the XShape extension
// or if shape is not set then simply it's the window geometry.
const QRegion &Scene::Window::shape() const
{
    if (!shape_valid) {
        if (toplevel->shape()) {
            auto cookie = xcb_shape_get_rectangles_unchecked(connection(), toplevel->frameId(), XCB_SHAPE_SK_BOUNDING);
            ScopedCPointer<xcb_shape_get_rectangles_reply_t> reply(xcb_shape_get_rectangles_reply(connection(), cookie, nullptr));
            if (!reply.isNull()) {
                shape_region = QRegion();
                auto *rects = xcb_shape_get_rectangles_rectangles(reply.data());
                for (int i = 0;
                        i < xcb_shape_get_rectangles_rectangles_length(reply.data());
                        ++i)
                    shape_region += QRegion(rects[ i ].x, rects[ i ].y,
                                            rects[ i ].width, rects[ i ].height);
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
    if (AbstractClient *c = dynamic_cast< AbstractClient * > (toplevel)) {
        if (c->isShade())
            return QRegion();
    }

    // TODO: cache
    const QRegion r = shape() & QRect(toplevel->clientPos(), toplevel->clientSize());
    return r.isEmpty() ? QRegion() : r;
}

bool Scene::Window::isVisible() const
{
    if (toplevel->isDeleted())
        return false;
    if (!toplevel->isOnCurrentDesktop())
        return false;
    if (!toplevel->isOnCurrentActivity())
        return false;
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(toplevel))
        return c->isShown(true);
    return true; // Unmanaged is always visible
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
    if (toplevel->isDeleted())
        disable_painting |= PAINT_DISABLED_BY_DELETE;
    if (static_cast<EffectsHandlerImpl*>(effects)->isDesktopRendering()) {
        if (!toplevel->isOnDesktop(static_cast<EffectsHandlerImpl*>(effects)->currentRenderedDesktop())) {
            disable_painting |= PAINT_DISABLED_BY_DESKTOP;
        }
    } else {
        if (!toplevel->isOnCurrentDesktop())
            disable_painting |= PAINT_DISABLED_BY_DESKTOP;
    }
    if (!toplevel->isOnCurrentActivity())
        disable_painting |= PAINT_DISABLED_BY_ACTIVITY;
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(toplevel)) {
        if (c->isMinimized())
            disable_painting |= PAINT_DISABLED_BY_MINIMIZE;
        if (c->tabGroup() && c != c->tabGroup()->current())
            disable_painting |= PAINT_DISABLED_BY_TAB_GROUP;
        if (c->isHiddenInternal()) {
            disable_painting |= PAINT_DISABLED;
        }
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
    qreal scale = 1.0;
    if (toplevel->surface()) {
        scale = toplevel->surface()->scale();
    }

    if (toplevel->clientPos() == QPoint(0, 0) && toplevel->clientSize() == toplevel->decorationRect().size())
        ret = makeQuads(WindowQuadContents, shape(), QPoint(0,0), scale);  // has no decoration
    else {
        AbstractClient *client = dynamic_cast<AbstractClient*>(toplevel);
        QRegion contents = clientShape();
        QRegion center = toplevel->transparentRect();
        QRegion decoration = (client ? QRegion(client->decorationRect()) : shape()) - center;
        qreal decorationScale = 1.0;
        ret = makeQuads(WindowQuadContents, contents, toplevel->clientContentPos(), scale);

        QRect rects[4];
        bool isShadedClient = false;

        if (client) {
            client->layoutDecorationRects(rects[0], rects[1], rects[2], rects[3]);
            decorationScale = client->screenScale();
            isShadedClient = client->isShade() || center.isEmpty();
        }

        if (isShadedClient) {
            const QRect bounding = rects[0] | rects[1] | rects[2] | rects[3];
            ret += makeDecorationQuads(rects, bounding, decorationScale);
        } else {
            ret += makeDecorationQuads(rects, decoration, decorationScale);
        }

    }
    if (m_shadow && toplevel->wantsShadowToBeRendered()) {
        ret << m_shadow->shadowQuads();
    }
    effects->buildQuads(toplevel->effectWindow(), ret);
    cached_quad_list.reset(new WindowQuadList(ret));
    return ret;
}

WindowQuadList Scene::Window::makeDecorationQuads(const QRect *rects, const QRegion &region, qreal textureScale) const
{
    WindowQuadList list;

    const QPoint offsets[4] = {
        QPoint(-rects[0].x() + rects[1].height() + rects[3].height() + 2, -rects[0].y()),                    // Left
        QPoint(-rects[1].x(), -rects[1].y()),                                                                // Top
        QPoint(-rects[2].x() + rects[1].height() + rects[3].height() + rects[0].width() + 3, -rects[2].y()), // Right
        QPoint(-rects[3].x(), -rects[3].y() + rects[1].height() + 1)                                         // Bottom
    };

    const Qt::Orientation orientations[4] = {
        Qt::Vertical,   // Left
        Qt::Horizontal, // Top
        Qt::Vertical,   // Right
        Qt::Horizontal, // Bottom
    };

    for (int i = 0; i < 4; i++) {
        const QRegion intersectedRegion = (region & rects[i]);
        for (const QRect &r : intersectedRegion) {
            if (!r.isValid())
                continue;

            const bool swap = orientations[i] == Qt::Vertical;

            const int x0 = r.x();
            const int y0 = r.y();
            const int x1 = r.x() + r.width();
            const int y1 = r.y() + r.height();

            const int u0 = (x0 + offsets[i].x()) * textureScale;
            const int v0 = (y0 + offsets[i].y()) * textureScale;
            const int u1 = (x1 + offsets[i].x()) * textureScale;
            const int v1 = (y1 + offsets[i].y()) * textureScale;

            WindowQuad quad(WindowQuadDecoration);
            quad.setUVAxisSwapped(swap);

            if (swap) {
                quad[0] = WindowVertex(x0, y0, v0, u0); // Top-left
                quad[1] = WindowVertex(x1, y0, v0, u1); // Top-right
                quad[2] = WindowVertex(x1, y1, v1, u1); // Bottom-right
                quad[3] = WindowVertex(x0, y1, v1, u0); // Bottom-left
            } else {
                quad[0] = WindowVertex(x0, y0, u0, v0); // Top-left
                quad[1] = WindowVertex(x1, y0, u1, v0); // Top-right
                quad[2] = WindowVertex(x1, y1, u1, v1); // Bottom-right
                quad[3] = WindowVertex(x0, y1, u0, v1); // Bottom-left
            }

            list.append(quad);
        }
    }

    return list;
}

WindowQuadList Scene::Window::makeQuads(WindowQuadType type, const QRegion& reg, const QPoint &textureOffset, qreal scale) const
{
    WindowQuadList ret;
    ret.reserve(reg.rectCount());
    for (const QRect &r : reg) {
        WindowQuad quad(type);
        // TODO asi mam spatne pravy dolni roh - bud tady, nebo v jinych castech
        quad[ 0 ] = WindowVertex(QPointF(r.x(), r.y()),
                                 QPointF(r.x() + textureOffset.x(), r.y() + textureOffset.y()) * scale);
        quad[ 1 ] = WindowVertex(QPointF(r.x() + r.width(), r.y()),
                                 QPointF(r.x() + r.width() + textureOffset.x(), r.y() + textureOffset.y()) * scale);
        quad[ 2 ] = WindowVertex(QPointF(r.x() + r.width(), r.y() + r.height()),
                                 QPointF(r.x() + r.width() + textureOffset.x(), r.y() + r.height() + textureOffset.y()) * scale);
        quad[ 3 ] = WindowVertex(QPointF(r.x(), r.y() + r.height()),
                                 QPointF(r.x() + textureOffset.x(), r.y() + r.height() + textureOffset.y()) * scale);

        ret.append(quad);
    }
    return ret;
}

void Scene::Window::updateShadow(Shadow* shadow)
{
    if (m_shadow == shadow) {
        return;
    }
    delete m_shadow;
    m_shadow = shadow;
}

//****************************************
// WindowPixmap
//****************************************
WindowPixmap::WindowPixmap(Scene::Window *window)
    : m_window(window)
    , m_pixmap(XCB_PIXMAP_NONE)
    , m_discarded(false)
{
}

WindowPixmap::WindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface, WindowPixmap *parent)
    : m_window(parent->m_window)
    , m_pixmap(XCB_PIXMAP_NONE)
    , m_discarded(false)
    , m_parent(parent)
    , m_subSurface(subSurface)
{
}

WindowPixmap::~WindowPixmap()
{
    if (m_pixmap != XCB_WINDOW_NONE) {
        xcb_free_pixmap(connection(), m_pixmap);
    }
    if (m_buffer) {
        using namespace KWayland::Server;
        QObject::disconnect(m_buffer.data(), &BufferInterface::aboutToBeDestroyed, m_buffer.data(), &BufferInterface::unref);
        m_buffer->unref();
    }
}

void WindowPixmap::create()
{
    if (isValid() || toplevel()->isDeleted()) {
        return;
    }
    // always update from Buffer on Wayland, don't try using XPixmap
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        // use Buffer
        updateBuffer();
        if ((m_buffer || !m_fbo.isNull()) && m_subSurface.isNull()) {
            m_window->unreferencePreviousPixmap();
        }
        return;
    }
    XServerGrabber grabber;
    xcb_pixmap_t pix = xcb_generate_id(connection());
    xcb_void_cookie_t namePixmapCookie = xcb_composite_name_window_pixmap_checked(connection(), toplevel()->frameId(), pix);
    Xcb::WindowAttributes windowAttributes(toplevel()->frameId());
    Xcb::WindowGeometry windowGeometry(toplevel()->frameId());
    if (xcb_generic_error_t *error = xcb_request_check(connection(), namePixmapCookie)) {
        qCDebug(KWIN_CORE) << "Creating window pixmap failed: " << error->error_code;
        free(error);
        return;
    }
    // check that the received pixmap is valid and actually matches what we
    // know about the window (i.e. size)
    if (!windowAttributes || windowAttributes->map_state != XCB_MAP_STATE_VIEWABLE) {
        qCDebug(KWIN_CORE) << "Creating window pixmap failed: " << this;
        xcb_free_pixmap(connection(), pix);
        return;
    }
    if (!windowGeometry ||
        windowGeometry->width != toplevel()->width() || windowGeometry->height != toplevel()->height()) {
        qCDebug(KWIN_CORE) << "Creating window pixmap failed: " << this;
        xcb_free_pixmap(connection(), pix);
        return;
    }
    m_pixmap = pix;
    m_pixmapSize = QSize(toplevel()->width(), toplevel()->height());
    m_contentsRect = QRect(toplevel()->clientPos(), toplevel()->clientSize());
    m_window->unreferencePreviousPixmap();
}

WindowPixmap *WindowPixmap::createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface)
{
    Q_UNUSED(subSurface)
    return nullptr;
}

bool WindowPixmap::isValid() const
{
    if (!m_buffer.isNull() || !m_fbo.isNull()) {
        return true;
    }
    return m_pixmap != XCB_PIXMAP_NONE;
}

void WindowPixmap::updateBuffer()
{
    using namespace KWayland::Server;
    if (SurfaceInterface *s = surface()) {
        QVector<WindowPixmap*> oldTree = m_children;
        QVector<WindowPixmap*> children;
        using namespace KWayland::Server;
        const auto subSurfaces = s->childSubSurfaces();
        for (const auto &subSurface : subSurfaces) {
            if (subSurface.isNull()) {
                continue;
            }
            auto it = std::find_if(oldTree.begin(), oldTree.end(), [subSurface] (WindowPixmap *p) { return p->m_subSurface == subSurface; });
            if (it != oldTree.end()) {
                children << *it;
                (*it)->updateBuffer();
                oldTree.erase(it);
            } else {
                WindowPixmap *p = createChild(subSurface);
                if (p) {
                    p->create();
                    children << p;
                }
            }
        }
        setChildren(children);
        qDeleteAll(oldTree);
        if (auto b = s->buffer()) {
            if (b == m_buffer) {
                // no change
                return;
            }
            if (m_buffer) {
                QObject::disconnect(m_buffer.data(), &BufferInterface::aboutToBeDestroyed, m_buffer.data(), &BufferInterface::unref);
                m_buffer->unref();
            }
            m_buffer = b;
            m_buffer->ref();
            QObject::connect(m_buffer.data(), &BufferInterface::aboutToBeDestroyed, m_buffer.data(), &BufferInterface::unref);
        } else if (m_subSurface) {
            if (m_buffer) {
                QObject::disconnect(m_buffer.data(), &BufferInterface::aboutToBeDestroyed, m_buffer.data(), &BufferInterface::unref);
                m_buffer->unref();
                m_buffer.clear();
            }
        } else {
            // might be an internal window
            const auto &fbo = toplevel()->internalFramebufferObject();
            if (!fbo.isNull()) {
                m_fbo = fbo;
            }
        }
    } else {
        if (m_buffer) {
            QObject::disconnect(m_buffer.data(), &BufferInterface::aboutToBeDestroyed, m_buffer.data(), &BufferInterface::unref);
            m_buffer->unref();
            m_buffer.clear();
        }
    }
}

KWayland::Server::SurfaceInterface *WindowPixmap::surface() const
{
    if (!m_subSurface.isNull()) {
        return m_subSurface->surface().data();
    } else {
        return toplevel()->surface();
    }
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

SceneFactory::SceneFactory(QObject *parent)
    : QObject(parent)
{
}

SceneFactory::~SceneFactory()
{
}

} // namespace
