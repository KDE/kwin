/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010 Sebastian Sauer <sebsauer@kdab.com>

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

#include "zoom.h"
// KConfigSkeleton
#include "zoomconfig.h"

#include <QApplication>
#include <QStyle>
#include <QtGui/QVector2D>
#include <QtDBus/QDBusConnection>
#include <kaction.h>
#include <kactioncollection.h>
#include <kstandardaction.h>
#include <KDE/KConfigGroup>
#include <KDE/KLocalizedString>
#include <KDE/KDebug>

#include <kwinglutils.h>
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <kwinxrenderutils.h>
#include <xcb/render.h>
#endif
#include <xcb/xfixes.h>

#include <X11/Xcursor/Xcursor.h>

namespace KWin
{

KWIN_EFFECT(zoom, ZoomEffect)

ZoomEffect::ZoomEffect()
    : Effect()
    , zoom(1)
    , target_zoom(1)
    , polling(false)
    , zoomFactor(1.25)
    , mouseTracking(MouseTrackingProportional)
    , enableFocusTracking(false)
    , followFocus(true)
    , mousePointer(MousePointerScale)
    , focusDelay(350)   // in milliseconds
    , imageWidth(0)
    , imageHeight(0)
    , isMouseHidden(false)
    , xMove(0)
    , yMove(0)
    , moveFactor(20.0)
{
    KActionCollection* actionCollection = new KActionCollection(this);
    KAction* a = 0;
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomIn, this, SLOT(zoomIn())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Equal));
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomOut, this, SLOT(zoomOut())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ActualSize, this, SLOT(actualSize())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomLeft"));
    a->setText(i18n("Move Zoomed Area to Left"));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Left));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(moveZoomLeft()));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomRight"));
    a->setText(i18n("Move Zoomed Area to Right"));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Right));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(moveZoomRight()));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomUp"));
    a->setText(i18n("Move Zoomed Area Upwards"));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Up));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(moveZoomUp()));

    a = static_cast< KAction* >(actionCollection->addAction("MoveZoomDown"));
    a->setText(i18n("Move Zoomed Area Downwards"));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Down));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(moveZoomDown()));

    // TODO: these two actions don't belong into the effect. They need to be moved into KWin core
    a = static_cast< KAction* >(actionCollection->addAction("MoveMouseToFocus"));
    a->setText(i18n("Move Mouse to Focus"));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_F5));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(moveMouseToFocus()));

    a = static_cast< KAction* >(actionCollection->addAction("MoveMouseToCenter"));
    a->setText(i18n("Move Mouse to Center"));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_F6));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(moveMouseToCenter()));

    timeline.setDuration(350);
    timeline.setFrameRange(0, 100);
    connect(&timeline, SIGNAL(frameChanged(int)), this, SLOT(timelineFrameChanged(int)));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            this, SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));

    source_zoom = -1; // used to trigger initialZoom reading
    reconfigure(ReconfigureAll);
}

ZoomEffect::~ZoomEffect()
{
    // switch off and free resources
    showCursor();
    // Save the zoom value.
    KConfigGroup conf = EffectsHandler::effectConfig("Zoom");
    conf.writeEntry("InitialZoom", target_zoom);
    conf.sync();
}

void ZoomEffect::showCursor()
{
    if (isMouseHidden) {
        // show the previously hidden mouse-pointer again and free the loaded texture/picture.
        xcb_xfixes_show_cursor(connection(), rootWindow());
        texture.reset();
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        xrenderPicture.reset();
#endif
        isMouseHidden = false;
    }
}

void ZoomEffect::hideCursor()
{
    if (mouseTracking == MouseTrackingProportional && mousePointer == MousePointerKeep)
        return; // don't replace the actual cursor by a static image for no reason.
    if (!isMouseHidden) {
        // try to load the cursor-theme into a OpenGL texture and if successful then hide the mouse-pointer
        recreateTexture();
        bool shouldHide = false;
        if (effects->isOpenGLCompositing()) {
            shouldHide = !texture.isNull();
        } else if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            shouldHide = !xrenderPicture.isNull();
#endif
        }
        if (shouldHide) {
            xcb_xfixes_hide_cursor(connection(), rootWindow());
            isMouseHidden = true;
        }
    }
}

void ZoomEffect::recreateTexture()
{
    // read details about the mouse-cursor theme define per default
    KConfigGroup mousecfg(KSharedConfig::openConfig("kcminputrc"), "Mouse");
    QString theme = mousecfg.readEntry("cursorTheme", QString());
    QString size  = mousecfg.readEntry("cursorSize", QString());

    // fetch a reasonable size for the cursor-theme image
    bool ok;
    int iconSize = size.toInt(&ok);
    if (!ok)
        iconSize = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);

    // load the cursor-theme image from the Xcursor-library
    XcursorImage *ximg = XcursorLibraryLoadImage("left_ptr", theme.toLocal8Bit(), iconSize);
    if (!ximg) // default is better then nothing, so keep it as backup
        ximg = XcursorLibraryLoadImage("left_ptr", "default", iconSize);
    if (ximg) {
        // turn the XcursorImage into a QImage that will be used to create the GLTexture/XRenderPicture.
        imageWidth = ximg->width;
        imageHeight = ximg->height;
        cursorHotSpot = QPoint(ximg->xhot, ximg->yhot);
        QImage img((uchar*)ximg->pixels, imageWidth, imageHeight, QImage::Format_ARGB32_Premultiplied);
        if (effects->isOpenGLCompositing())
            texture.reset(new GLTexture(img));
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if (effects->compositingType() == XRenderCompositing)
            xrenderPicture.reset(new XRenderPicture(QPixmap::fromImage(img)));
#endif
        XcursorImageDestroy(ximg);
    }
    else {
        kDebug(1212) << "Loading cursor image (" << theme << ") FAILED -> falling back to proportional mouse tracking!";
        mouseTracking = MouseTrackingProportional;
    }
}

void ZoomEffect::reconfigure(ReconfigureFlags)
{
    ZoomConfig::self()->readConfig();
    // On zoom-in and zoom-out change the zoom by the defined zoom-factor.
    zoomFactor = qMax(0.1, ZoomConfig::zoomFactor());
    // Visibility of the mouse-pointer.
    mousePointer = MousePointerType(ZoomConfig::mousePointer());
    // Track moving of the mouse.
    mouseTracking = MouseTrackingType(ZoomConfig::mouseTracking());
    // Enable tracking of the focused location.
    bool _enableFocusTracking = ZoomConfig::enableFocusTracking();
    if (enableFocusTracking != _enableFocusTracking) {
        enableFocusTracking = _enableFocusTracking;
        if (QDBusConnection::sessionBus().isConnected()) {
            if (enableFocusTracking)
                QDBusConnection::sessionBus().connect("org.kde.kaccessibleapp", "/Adaptor", "org.kde.kaccessibleapp.Adaptor", "focusChanged", this, SLOT(focusChanged(int,int,int,int,int,int)));
            else
                QDBusConnection::sessionBus().disconnect("org.kde.kaccessibleapp", "/Adaptor", "org.kde.kaccessibleapp.Adaptor", "focusChanged", this, SLOT(focusChanged(int,int,int,int,int,int)));
        }
    }
    // When the focus changes, move the zoom area to the focused location.
    followFocus = ZoomConfig::enableFollowFocus();
    // The time in milliseconds to wait before a focus-event takes away a mouse-move.
    focusDelay = qMax(uint(0), ZoomConfig::focusDelay());
    // The factor the zoom-area will be moved on touching an edge on push-mode or using the navigation KAction's.
    moveFactor = qMax(0.1, ZoomConfig::moveFactor());
    if (source_zoom < 0) {
        // Load the saved zoom value.
        source_zoom = 1.0;
        target_zoom = ZoomConfig::initialZoom();
        if (target_zoom > 1.0)
            zoomIn(target_zoom);
    } else {
        source_zoom = 1.0;
    }
}

void ZoomEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    bool altered = false;
    if (zoom != target_zoom) {
        altered = true;
        const float zoomDist = qAbs(target_zoom - source_zoom);
        if (target_zoom > zoom)
            zoom = qMin(zoom + ((zoomDist * time) / animationTime(150*zoomFactor)), target_zoom);
        else
            zoom = qMax(zoom - ((zoomDist * time) / animationTime(150*zoomFactor)), target_zoom);
    }

    if (zoom == 1.0) {
        showCursor();
        // reset the generic shader to avoid artifacts in plenty other effects
        if (altered && effects->isOpenGLCompositing())
            ShaderBinder binder(ShaderManager::GenericShader, true);
    } else {
        hideCursor();
        data.mask |= PAINT_SCREEN_TRANSFORMED;
    }

    effects->prePaintScreen(data, time);
}

void ZoomEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (zoom != 1.0) {
        data *= QVector2D(zoom, zoom);

        // mouse-tracking allows navigation of the zoom-area using the mouse.
        switch(mouseTracking) {
        case MouseTrackingProportional:
            data.setXTranslation(- int(cursorPoint.x() * (zoom - 1.0)));
            data.setYTranslation(- int(cursorPoint.y() * (zoom - 1.0)));
            prevPoint = cursorPoint;
            break;
        case MouseTrackingCentred:
            prevPoint = cursorPoint;
            // fall through
        case MouseTrackingDisabled:
            data.setXTranslation(qMin(0, qMax(int(displayWidth() - displayWidth() * zoom), int(displayWidth() / 2 - prevPoint.x() * zoom))));
            data.setYTranslation(qMin(0, qMax(int(displayHeight() - displayHeight() * zoom), int(displayHeight() / 2 - prevPoint.y() * zoom))));
            break;
        case MouseTrackingPush: {
                // touching an edge of the screen moves the zoom-area in that direction.
                int x = cursorPoint.x() * zoom - prevPoint.x() * (zoom - 1.0);
                int y = cursorPoint.y() * zoom - prevPoint.y() * (zoom - 1.0);
                int threshold = 4;
                xMove = yMove = 0;
                if (x < threshold)
                    xMove = (x - threshold) / zoom;
                else if (x + threshold > displayWidth())
                    xMove = (x + threshold - displayWidth()) / zoom;
                if (y < threshold)
                    yMove = (y - threshold) / zoom;
                else if (y + threshold > displayHeight())
                    yMove = (y + threshold - displayHeight()) / zoom;
                if (xMove)
                    prevPoint.setX(qMax(0, qMin(displayWidth(), prevPoint.x() + xMove)));
                if (yMove)
                    prevPoint.setY(qMax(0, qMin(displayHeight(), prevPoint.y() + yMove)));
                data.setXTranslation(- int(prevPoint.x() * (zoom - 1.0)));
                data.setYTranslation(- int(prevPoint.y() * (zoom - 1.0)));
                break;
            }
        }

        // use the focusPoint if focus tracking is enabled
        if (enableFocusTracking && followFocus) {
            bool acceptFocus = true;
            if (mouseTracking != MouseTrackingDisabled && focusDelay > 0) {
                // Wait some time for the mouse before doing the switch. This serves as threshold
                // to prevent the focus from jumping around to much while working with the mouse.
                const int msecs = lastMouseEvent.msecsTo(lastFocusEvent);
                acceptFocus = msecs > focusDelay;
            }
            if (acceptFocus) {
                data.setXTranslation(- int(focusPoint.x() * (zoom - 1.0)));
                data.setYTranslation(- int(focusPoint.y() * (zoom - 1.0)));
                prevPoint = focusPoint;
            }
        }
    }

    effects->paintScreen(mask, region, data);

    if (zoom != 1.0 && mousePointer != MousePointerHide) {
        // Draw the mouse-texture at the position matching to zoomed-in image of the desktop. Hiding the
        // previous mouse-cursor and drawing our own fake mouse-cursor is needed to be able to scale the
        // mouse-cursor up and to re-position those mouse-cursor to match to the chosen zoom-level.
        int w = imageWidth;
        int h = imageHeight;
        if (mousePointer == MousePointerScale) {
            w *= zoom;
            h *= zoom;
        }
        const QPoint p = effects->cursorPos() - cursorHotSpot;
        QRect rect(p.x() * zoom + data.xTranslation(), p.y() * zoom + data.yTranslation(), w, h);

        if (texture) {
            texture->bind();
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            texture->render(region, rect);
            texture->unbind();
            glDisable(GL_BLEND);
        }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if (xrenderPicture) {
#define DOUBLE_TO_FIXED(d) ((xcb_render_fixed_t) ((d) * 65536))
            static xcb_render_transform_t xrenderIdentity = {
                DOUBLE_TO_FIXED(1), DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(0),
                DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(1), DOUBLE_TO_FIXED(0),
                DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(1)
            };
            if (mousePointer == MousePointerScale) {
                xcb_render_set_picture_filter(connection(), *xrenderPicture, 4, const_cast<char*>("good"), 0, NULL);
                const xcb_render_transform_t xform = {
                    DOUBLE_TO_FIXED(1.0 / zoom), DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(0),
                    DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(1.0 / zoom), DOUBLE_TO_FIXED(0),
                    DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(0), DOUBLE_TO_FIXED(1)
                };
                xcb_render_set_picture_transform(connection(), *xrenderPicture, xform);
            }
            xcb_render_composite(connection(), XCB_RENDER_PICT_OP_OVER, *xrenderPicture, XCB_RENDER_PICTURE_NONE,
                                 effects->xrenderBufferPicture(), 0, 0, 0, 0, rect.x(), rect.y(), rect.width(), rect.height());
            if (mousePointer == MousePointerScale)
                xcb_render_set_picture_transform(connection(), *xrenderPicture, xrenderIdentity);
#undef DOUBLE_TO_FIXED
        }
#endif
    }
}

void ZoomEffect::postPaintScreen()
{
    if (zoom != target_zoom)
        effects->addRepaintFull();
    effects->postPaintScreen();
}

void ZoomEffect::zoomIn(double to)
{
    source_zoom = zoom;
    if (to < 0.0)
        target_zoom *= zoomFactor;
    else
        target_zoom = to;
    if (!polling) {
        polling = true;
        effects->startMousePolling();
    }
    cursorPoint = effects->cursorPos();
    if (mouseTracking == MouseTrackingDisabled)
        prevPoint = cursorPoint;
    effects->addRepaintFull();
}

void ZoomEffect::zoomOut()
{
    source_zoom = zoom;
    target_zoom /= zoomFactor;
    if ((zoomFactor > 1 && target_zoom < 1.01) || (zoomFactor < 1 && target_zoom > 0.99)) {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
    }
    if (mouseTracking == MouseTrackingDisabled)
        prevPoint = effects->cursorPos();
    effects->addRepaintFull();
}

void ZoomEffect::actualSize()
{
    source_zoom = zoom;
    target_zoom = 1;
    if (polling) {
        polling = false;
        effects->stopMousePolling();
    }
    effects->addRepaintFull();
}

void ZoomEffect::timelineFrameChanged(int /* frame */)
{
    prevPoint.setX(qMax(0, qMin(displayWidth(), prevPoint.x() + xMove)));
    prevPoint.setY(qMax(0, qMin(displayHeight(), prevPoint.y() + yMove)));
    cursorPoint = prevPoint;
    effects->addRepaintFull();
}

void ZoomEffect::moveZoom(int x, int y)
{
    if (timeline.state() == QTimeLine::Running)
        timeline.stop();

    if (x < 0)
        xMove = - qMax(1.0, displayWidth() / zoom / moveFactor);
    else if (x > 0)
        xMove = qMax(1.0, displayWidth() / zoom  / moveFactor);
    else
        xMove = 0;

    if (y < 0)
        yMove = - qMax(1.0, displayHeight() / zoom / moveFactor);
    else if (y > 0)
        yMove = qMax(1.0, displayHeight() / zoom / moveFactor);
    else
        yMove = 0;

    timeline.start();
}

void ZoomEffect::moveZoomLeft()
{
    moveZoom(-1, 0);
}

void ZoomEffect::moveZoomRight()
{
    moveZoom(1, 0);
}

void ZoomEffect::moveZoomUp()
{
    moveZoom(0, -1);
}

void ZoomEffect::moveZoomDown()
{
    moveZoom(0, 1);
}

void ZoomEffect::moveMouseToFocus()
{
    QCursor::setPos(focusPoint.x(), focusPoint.y());
}

void ZoomEffect::moveMouseToCenter()
{
    QRect r(0, 0, displayWidth(), displayHeight());
    QCursor::setPos(r.x() + r.width() / 2, r.y() + r.height() / 2);
}

void ZoomEffect::slotMouseChanged(const QPoint& pos, const QPoint& old, Qt::MouseButtons,
                              Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (zoom == 1.0)
        return;
    cursorPoint = pos;
    if (pos != old) {
        lastMouseEvent = QTime::currentTime();
        effects->addRepaintFull();
    }
}

void ZoomEffect::focusChanged(int px, int py, int rx, int ry, int rwidth, int rheight)
{
    if (zoom == 1.0)
        return;
    focusPoint = (px >= 0 && py >= 0) ? QPoint(px, py) : QPoint(rx + qMax(0, (qMin(displayWidth(), rwidth) / 2) - 60), ry + qMax(0, (qMin(displayHeight(), rheight) / 2) - 60));
    if (enableFocusTracking) {
        lastFocusEvent = QTime::currentTime();
        effects->addRepaintFull();
    }
}

bool ZoomEffect::isActive() const
{
    return zoom != 1.0 || zoom != target_zoom;
}

} // namespace

#include "zoom.moc"
