/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#include "desktopgrid.h"

#include "../presentwindows/presentwindows_proxy.h"

#include <math.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <klocale.h>
#include <kconfiggroup.h>
#include <netwm_def.h>
#include <QEvent>
#include <QMouseEvent>
#include <kglobalsettings.h>
#include <QtGui/QPainter>
#include <QtGui/QGraphicsLinearLayout>
#include <Plasma/FrameSvg>
#include <Plasma/PushButton>
#include <Plasma/WindowEffects>

namespace KWin
{

// WARNING, TODO: This effect relies on the desktop layout being EWMH-compliant.

KWIN_EFFECT(desktopgrid, DesktopGridEffect)

DesktopGridEffect::DesktopGridEffect()
    : activated(false)
    , timeline()
    , keyboardGrab(false)
    , wasWindowMove(false)
    , wasDesktopMove(false)
    , isValidMove(false)
    , windowMove(NULL)
    , windowMoveDiff()
    , gridSize()
    , orientation(Qt::Horizontal)
    , activeCell(1, 1)
    , scale()
    , unscaledBorder()
    , scaledSize()
    , scaledOffset()
    , m_proxy(0)
{
    // Load shortcuts
    KActionCollection* actionCollection = new KActionCollection(this);
    KAction* a = (KAction*) actionCollection->addAction("ShowDesktopGrid");
    a->setText(i18n("Show Desktop Grid"));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F8));
    shortcut = a->globalShortcut();
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    connect(a, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(globalShortcutChanged(QKeySequence)));
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
    connect(effects, SIGNAL(numberDesktopsChanged(int)), this, SLOT(slotNumberDesktopsChanged(int)));
    connect(effects, SIGNAL(windowGeometryShapeChanged(KWin::EffectWindow*,QRect)), this, SLOT(slotWindowGeometryShapeChanged(KWin::EffectWindow*,QRect)));

    // Load all other configuration details
    reconfigure(ReconfigureAll);
}

DesktopGridEffect::~DesktopGridEffect()
{
    foreach (ElectricBorder border, borderActivate) {
        effects->unreserveElectricBorder(border);
    }
    QHash< DesktopButtonsView*, EffectWindow* >::iterator i = m_desktopButtonsViews.begin();
    while (i != m_desktopButtonsViews.end()) {
        DesktopButtonsView *view = i.key();
        i = m_desktopButtonsViews.erase(i);
        view->deleteLater();
    }
}

void DesktopGridEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = effects->effectConfig("DesktopGrid");

    foreach (ElectricBorder border, borderActivate) {
        effects->unreserveElectricBorder(border);
    }
    borderActivate.clear();
    QList<int> borderList = QList<int>();
    borderList.append(int(ElectricNone));
    borderList = conf.readEntry("BorderActivate", borderList);
    foreach (int i, borderList) {
        borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i));
    }

    zoomDuration = animationTime(conf, "ZoomDuration", 300);
    timeline.setCurveShape(QTimeLine::EaseInOutCurve);
    timeline.setDuration(zoomDuration);

    border = conf.readEntry("BorderWidth", 10);
    desktopNameAlignment = Qt::Alignment(conf.readEntry("DesktopNameAlignment", 0));
    layoutMode = conf.readEntry("LayoutMode", int(LayoutPager));
    customLayoutRows = conf.readEntry("CustomLayoutRows", 2);
    m_usePresentWindows = conf.readEntry("PresentWindows", true);
}

//-----------------------------------------------------------------------------
// Screen painting

void DesktopGridEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (timeline.currentValue() != 0 || activated || (isUsingPresentWindows() && isMotionManagerMovingWindows())) {
        if (activated)
            timeline.setCurrentTime(timeline.currentTime() + time);
        else
            timeline.setCurrentTime(timeline.currentTime() - time);
        for (int i = 0; i < effects->numberOfDesktops(); i++) {
            if (i == highlightedDesktop - 1)
                hoverTimeline[i]->setCurrentTime(hoverTimeline[i]->currentTime() + time);
            else
                hoverTimeline[i]->setCurrentTime(hoverTimeline[i]->currentTime() - time);
        }
        if (isUsingPresentWindows()) {
            QList<WindowMotionManager>::iterator i;
            for (i = m_managers.begin(); i != m_managers.end(); ++i)
                (*i).calculate(time);
        }
        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if (timeline.currentValue() != 0 || (isUsingPresentWindows() && isMotionManagerMovingWindows()))
            data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        if (!activated && timeline.currentValue() == 0 && !(isUsingPresentWindows() && isMotionManagerMovingWindows()))
            finish();
    }
    effects->prePaintScreen(data, time);
}

void DesktopGridEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (timeline.currentValue() == 0 && !isUsingPresentWindows()) {
        effects->paintScreen(mask, region, data);
        return;
    }
    for (int desktop = 1; desktop <= effects->numberOfDesktops(); desktop++) {
        ScreenPaintData d = data;
        paintingDesktop = desktop;
        effects->paintScreen(mask, region, d);
    }

    // paint the add desktop button
    for (QHash< DesktopButtonsView*, EffectWindow*>::iterator it = m_desktopButtonsViews.begin();
            it != m_desktopButtonsViews.end(); ++it) {
        if (!it.value()) {
            EffectWindow *view = effects->findWindow(it.key()->winId());
            if (view) {
                view->setData(WindowForceBlurRole, QVariant(true));
                it.value() = view;
            }
        }
        if (it.value()) {
            WindowPaintData d(it.value());
            d.opacity *= timeline.currentValue();
            effects->drawWindow(it.value(), PAINT_WINDOW_TRANSLUCENT,
                                infiniteRegion(), d);
        }
    }

    if (isUsingPresentWindows() && windowMove && wasWindowMove) {
        // the moving window has to be painted on top of all desktops
        QPoint diff = cursorPos() - m_windowMoveStartPoint;
        QRect geo = m_windowMoveGeometry.translated(diff);
        WindowPaintData d(windowMove);
        d.xScale *= (float)geo.width() / (float)windowMove->width();
        d.yScale *= (float)geo.height() / (float)windowMove->height();
        d.xTranslate += qRound(geo.left() - windowMove->x());
        d.yTranslate += qRound(geo.top() - windowMove->y());
        effects->drawWindow(windowMove, PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_LANCZOS, infiniteRegion(), d);
    }

    if (desktopNameAlignment) {
        for (int screen = 0; screen < effects->numScreens(); screen++) {
            QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
            int desktop = 1;
            foreach (EffectFrame * frame, desktopNames) {
                QPointF posTL(scalePos(screenGeom.topLeft(), desktop, screen));
                QPointF posBR(scalePos(screenGeom.bottomRight(), desktop, screen));
                QRect textArea(posTL.x(), posTL.y(), posBR.x() - posTL.x(), posBR.y() - posTL.y());
                textArea.adjust(textArea.width() / 10, textArea.height() / 10,
                                -textArea.width() / 10, -textArea.height() / 10);
                int x, y;
                if (desktopNameAlignment & Qt::AlignLeft)
                    x = textArea.x();
                else if (desktopNameAlignment & Qt::AlignRight)
                    x = textArea.right();
                else
                    x = textArea.center().x();
                if (desktopNameAlignment & Qt::AlignTop)
                    y = textArea.y();
                else if (desktopNameAlignment & Qt::AlignBottom)
                    y = textArea.bottom();
                else
                    y = textArea.center().y();
                frame->setPosition(QPoint(x, y));
                frame->render(region, timeline.currentValue(), 0.7);
                ++desktop;
            }
        }
    }
}

void DesktopGridEffect::postPaintScreen()
{
    if (activated ? timeline.currentValue() != 1 : timeline.currentValue() != 0)
        effects->addRepaintFull(); // Repaint during zoom
    if (isUsingPresentWindows() && isMotionManagerMovingWindows())
        effects->addRepaintFull();
    if (activated) {
        for (int i = 0; i < effects->numberOfDesktops(); i++) {
            if (hoverTimeline[i]->currentValue() != 0.0 && hoverTimeline[i]->currentValue() != 1.0) {
                // Repaint during soft highlighting
                effects->addRepaintFull();
                break;
            }
        }
    }
    effects->postPaintScreen();
}

//-----------------------------------------------------------------------------
// Window painting

void DesktopGridEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (timeline.currentValue() != 0 || (isUsingPresentWindows() && isMotionManagerMovingWindows())) {
        if (w->isOnDesktop(paintingDesktop)) {
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            if (w->isMinimized() && isUsingPresentWindows())
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
            data.mask |= PAINT_WINDOW_TRANSFORMED;

            // Split windows at screen edges
            for (int screen = 0; screen < effects->numScreens(); screen++) {
                QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
                if (w->x() < screenGeom.x())
                    data.quads = data.quads.splitAtX(screenGeom.x() - w->x());
                if (w->x() + w->width() > screenGeom.x() + screenGeom.width())
                    data.quads = data.quads.splitAtX(screenGeom.x() + screenGeom.width() - w->x());
                if (w->y() < screenGeom.y())
                    data.quads = data.quads.splitAtY(screenGeom.y() - w->y());
                if (w->y() + w->height() > screenGeom.y() + screenGeom.height())
                    data.quads = data.quads.splitAtY(screenGeom.y() + screenGeom.height() - w->y());
            }
            if (windowMove && wasWindowMove && windowMove->findModal() == w)
                w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        } else
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    }
    effects->prePaintWindow(w, data, time);
}

void DesktopGridEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (timeline.currentValue() != 0 || (isUsingPresentWindows() && isMotionManagerMovingWindows())) {
        if (isUsingPresentWindows() && w == windowMove && wasWindowMove) {
            return; // will be painted on top of all other windows
        }
        if (m_desktopButtonsViews.values().contains(w))
            return; // will be painted on top of all other windows

        double xScale = data.xScale;
        double yScale = data.yScale;

        // Don't change brightness of windows on all desktops as this causes flickering
        if (!w->isOnAllDesktops() || w->isDesktop())
            data.brightness *= 1.0 - (0.3 * (1.0 - hoverTimeline[paintingDesktop - 1]->currentValue()));

        for (int screen = 0; screen < effects->numScreens(); screen++) {
            // Assume desktop windows can never be on two screens at once (Plasma makes one window per screen)
            if (w->isDesktop())
                screen = w->screen();
            QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

            QRectF transformedGeo = w->geometry();
            // Display all quads on the same screen on the same pass
            WindowQuadList screenQuads;
            bool quadsAdded = false;
            if (isUsingPresentWindows()) {
                WindowMotionManager& manager = m_managers[(paintingDesktop-1)*(effects->numScreens())+screen ];
                if (manager.isManaging(w)) {
                    foreach (const WindowQuad & quad, data.quads)
                    screenQuads.append(quad);
                    transformedGeo = manager.transformedGeometry(w);
                    quadsAdded = true;
                    if (!manager.areWindowsMoving() && timeline.currentValue() == 1.0)
                        mask |= PAINT_WINDOW_LANCZOS;
                } else if (w->screen() != screen)
                    quadsAdded = true; // we don't want parts of overlapping windows on the other screen
            }
            if (!quadsAdded) {
                foreach (const WindowQuad & quad, data.quads) {
                    QRect quadRect(
                        w->x() + quad.left(), w->y() + quad.top(),
                        quad.right() - quad.left(), quad.bottom() - quad.top()
                    );
                    if (quadRect.intersects(screenGeom))
                        screenQuads.append(quad);
                }
            }
            if (screenQuads.isEmpty())
                continue; // Nothing is being displayed, don't bother
            WindowPaintData d = data;
            d.quads = screenQuads;

            QPointF newPos = scalePos(transformedGeo.topLeft().toPoint(), paintingDesktop, screen);
            double progress = timeline.currentValue();
            d.xScale = interpolate(1, xScale * scale[screen] * (float)transformedGeo.width() / (float)w->geometry().width(), progress);
            d.yScale = interpolate(1, yScale * scale[screen] * (float)transformedGeo.height() / (float)w->geometry().height(), progress);
            d.xTranslate += qRound(newPos.x() - w->x());
            d.yTranslate += qRound(newPos.y() - w->y());

            if (isUsingPresentWindows() && (w->isDock() || w->isSkipSwitcher())) {
                // fade out panels if present windows is used
                d.opacity *= (1.0 - timeline.currentValue());
            }
            if (isUsingPresentWindows() && w->isMinimized()) {
                d.opacity *= timeline.currentValue();
            }

            if (effects->compositingType() == XRenderCompositing) {
                // More exact clipping as XRender displays the entire window instead of just the quad
                QPointF screenPosF = scalePos(screenGeom.topLeft(), paintingDesktop).toPoint();
                QPoint screenPos(
                    qRound(screenPosF.x()),
                    qRound(screenPosF.y())
                );
                QSize screenSize(
                    qRound(interpolate(screenGeom.width(), scaledSize[screen].width(), progress)),
                    qRound(interpolate(screenGeom.height(), scaledSize[screen].height(), progress))
                );
                PaintClipper pc(effects->clientArea(ScreenArea, screen, 0) & QRect(screenPos, screenSize));
                effects->paintWindow(w, mask, region, d);
            } else {
                if (w->isDesktop() && timeline.currentValue() == 1.0) {
                    // desktop windows are not in a motion manager and can always be rendered with
                    // lanczos sampling except for animations
                    mask |= PAINT_WINDOW_LANCZOS;
                }
                effects->paintWindow(w, mask, effects->clientArea(ScreenArea, screen, 0), d);
            }
            // Assume desktop windows can never be on two screens at once (Plasma makes one window per screen)
            if (w->isDesktop())
                break;
        }
    } else
        effects->paintWindow(w, mask, region, data);
}

//-----------------------------------------------------------------------------
// User interaction

void DesktopGridEffect::slotWindowAdded(EffectWindow* w)
{
    if (!activated)
        return;
    if (isUsingPresentWindows()) {
        if (w->isDesktop() || w->isDock() || !w->isCurrentTab())
            return; // don't add
        if (w->isOnAllDesktops()) {
            for (int i = 0; i < effects->numberOfDesktops(); i++) {
                WindowMotionManager& manager = m_managers[ i*effects->numScreens()+w->screen()];
                manager.manage(w);
                m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
            }
        } else {
            WindowMotionManager& manager = m_managers[(w->desktop()-1)*effects->numScreens()+w->screen()];
            manager.manage(w);
            m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
        }
    }
    effects->addRepaintFull();
}

void DesktopGridEffect::slotWindowClosed(EffectWindow* w)
{
    if (!activated && timeline.currentValue() == 0)
        return;
    if (w == windowMove) {
        effects->setElevatedWindow(windowMove, false);
        windowMove = NULL;
    }
    if (isUsingPresentWindows()) {
        if (w->isOnAllDesktops()) {
            for (int i = 0; i < effects->numberOfDesktops(); i++) {
                WindowMotionManager& manager = m_managers[i*effects->numScreens()+w->screen()];
                manager.unmanage(w);
                m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
            }
        } else {
            if (w->desktop() <= effects->numberOfDesktops()) {
                WindowMotionManager& manager = m_managers[(w->desktop()-1)*effects->numScreens()+w->screen()];
                manager.unmanage(w);
                m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
            }
        }
    }
    for (QHash< DesktopButtonsView*, EffectWindow*>::iterator it = m_desktopButtonsViews.begin();
            it != m_desktopButtonsViews.end(); ++it) {
        if (it.value() && it.value() == w) {
            w->refWindow();
            break;
        }
    }
    effects->addRepaintFull();
}

void DesktopGridEffect::slotWindowDeleted(EffectWindow* w)
{
    if (w == windowMove)
        windowMove = 0;
    for (QHash< DesktopButtonsView*, EffectWindow*>::iterator it = m_desktopButtonsViews.begin();
            it != m_desktopButtonsViews.end(); ++it) {
        if (it.value() && it.value() == w) {
            it.key()->deleteLater();
            m_desktopButtonsViews.erase(it);
            break;
        }
    }
}

void DesktopGridEffect::slotWindowGeometryShapeChanged(EffectWindow* w, const QRect& old)
{
    Q_UNUSED(old)
    if (!activated)
        return;
    if (w == windowMove && wasWindowMove)
        return;
    if (isUsingPresentWindows()) {
        if (w->isOnAllDesktops()) {
            for (int i = 0; i < effects->numberOfDesktops(); i++) {
                WindowMotionManager& manager = m_managers[i*effects->numScreens()+w->screen()];
                m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
            }
        } else {
            WindowMotionManager& manager = m_managers[(w->desktop()-1)*effects->numScreens()+w->screen()];
            m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
        }
    }
}

void DesktopGridEffect::windowInputMouseEvent(Window, QEvent* e)
{
    if ((e->type() != QEvent::MouseMove
            && e->type() != QEvent::MouseButtonPress
            && e->type() != QEvent::MouseButtonRelease)
            || timeline.currentValue() != 1)  // Block user input during animations
        return;
    QMouseEvent* me = static_cast< QMouseEvent* >(e);
    for (QHash< DesktopButtonsView*, EffectWindow*>::iterator it = m_desktopButtonsViews.begin();
            it != m_desktopButtonsViews.end(); ++it) {
        DesktopButtonsView* view = it.key();
        if (!wasWindowMove && !wasDesktopMove && view->geometry().contains(me->pos())) {
            const QPoint widgetPos = view->mapFromGlobal(me->pos());
            const QPointF scenePos = view->mapToScene(widgetPos);
            QMouseEvent event(me->type(), widgetPos, me->pos(), me->button(), me->buttons(), me->modifiers());
            view->windowInputMouseEvent(&event);
            return;
        }
    }

    if (e->type() == QEvent::MouseMove) {
        int d = posToDesktop(me->pos());
        if (windowMove != NULL &&
                (me->pos() - dragStartPos).manhattanLength() > KGlobalSettings::dndEventDelay()) {
            // Handle window moving
            if (!wasWindowMove) { // Activate on move
                if (isUsingPresentWindows() && windowMove->isOnAllDesktops()) {
                    for (int i = 0; i < effects->numberOfDesktops(); ++i) {
                        WindowMotionManager& manager = m_managers[(i)*(effects->numScreens()) + windowMove->screen()];
                        if ((i + 1) == d) {
                            const QRectF transformedGeo = manager.transformedGeometry(windowMove);
                            const QPointF pos = scalePos(transformedGeo.topLeft().toPoint(), d, windowMove->screen());
                            const QSize size(scale[windowMove->screen()] *(float)transformedGeo.width(),
                                             scale[windowMove->screen()] *(float)transformedGeo.height());
                            m_windowMoveGeometry = QRect(pos.toPoint(), size);
                            m_windowMoveStartPoint = me->pos();
                        }
                        manager.unmanage(windowMove);
                        if (EffectWindow* modal = windowMove->findModal()) {
                            if (manager.isManaging(modal))
                                manager.unmanage(modal);
                        }
                        m_proxy->calculateWindowTransformations(manager.managedWindows(), windowMove->screen(), manager);
                    }
                } else if (isUsingPresentWindows()) {
                    WindowMotionManager& manager = m_managers[(windowMove->desktop()-1)*(effects->numScreens()) + windowMove->screen()];
                    const QRectF transformedGeo = manager.transformedGeometry(windowMove);
                    const QPointF pos = scalePos(transformedGeo.topLeft().toPoint(), windowMove->desktop(), windowMove->screen());
                    const QSize size(scale[windowMove->screen()] *(float)transformedGeo.width(),
                                     scale[windowMove->screen()] *(float)transformedGeo.height());
                    m_windowMoveGeometry = QRect(pos.toPoint(), size);
                    m_windowMoveStartPoint = me->pos();

                    manager.unmanage(windowMove);
                    if (EffectWindow* modal = windowMove->findModal()) {
                        if (manager.isManaging(modal))
                            manager.unmanage(modal);
                    }
                    m_proxy->calculateWindowTransformations(manager.managedWindows(), windowMove->screen(), manager);
                }
                XDefineCursor(display(), input, QCursor(Qt::ClosedHandCursor).handle());
            }
            wasWindowMove = true;
            if (windowMove->isMovable() && !isUsingPresentWindows()) {
                int screen = effects->screenNumber(me->pos());
                effects->moveWindow(windowMove, unscalePos(me->pos(), NULL) + windowMoveDiff, true, 1.0 / scale[screen]);
            }
            if (d != highlightedDesktop) {
                if (!windowMove->isOnAllDesktops())
                    effects->windowToDesktop(windowMove, d);   // Not true all desktop move
                const int screen = effects->screenNumber(me->pos());
                if (screen != windowMove->screen())
                    effects->windowToScreen(windowMove, screen);
            }
            effects->addRepaintFull();
        } else if ((me->buttons() & Qt::LeftButton) && !wasDesktopMove &&
                  (me->pos() - dragStartPos).manhattanLength() > KGlobalSettings::dndEventDelay()) {
            wasDesktopMove = true;
            XDefineCursor(display(), input, QCursor(Qt::ClosedHandCursor).handle());
        }
        if (d != highlightedDesktop) { // Highlight desktop
            if ((me->buttons() & Qt::LeftButton) && isValidMove && !wasWindowMove && d <= effects->numberOfDesktops()) {
                EffectWindowList windows = effects->stackingOrder();
                EffectWindowList stack;
                foreach (EffectWindow * w, windows) {
                    if (w->isOnAllDesktops())
                        continue;
                    if (w->isOnDesktop(highlightedDesktop)) {
                        effects->windowToDesktop(w, d);
                        if (isUsingPresentWindows()) {
                            m_managers[(d-1)*(effects->numScreens()) + w->screen()].manage(w);
                            m_managers[(highlightedDesktop-1)*(effects->numScreens()) + w->screen()].unmanage(w);
                        }
                    } else if (w->isOnDesktop(d))
                        stack << w;
                }
                foreach (EffectWindow * w, stack) {
                    effects->windowToDesktop(w, highlightedDesktop);
                    if (isUsingPresentWindows()) {
                        m_managers[(d-1)*(effects->numScreens()) + w->screen()].unmanage(w);
                        m_managers[(highlightedDesktop-1)*(effects->numScreens()) + w->screen()].manage(w);
                    }
                }
                if (isUsingPresentWindows()) {
                    for (int i = 0; i < effects->numScreens(); i++) {
                        WindowMotionManager& manager = m_managers[(d-1)*(effects->numScreens()) + i ];
                        WindowMotionManager& manager2 = m_managers[(highlightedDesktop-1)*(effects->numScreens()) + i ];
                        m_proxy->calculateWindowTransformations(manager.managedWindows(), i, manager);
                        m_proxy->calculateWindowTransformations(manager2.managedWindows(), i, manager2);
                    }
                    effects->addRepaintFull();
                }
            }
            setHighlightedDesktop(d);
        }
    }
    if (e->type() == QEvent::MouseButtonPress) {
        if (me->buttons() == Qt::LeftButton) {
            isValidMove = true;
//             QRect rect;
            dragStartPos = me->pos();
            bool isDesktop = (me->modifiers() & Qt::ControlModifier);
            EffectWindow* w = isDesktop ? NULL : windowAt(me->pos());
            if (w != NULL)
                isDesktop = w->isDesktop();
            if (w != NULL && !w->isDesktop() && (w->isMovable() || w->isMovableAcrossScreens() || isUsingPresentWindows())) {
                // Prepare it for moving
                windowMoveDiff = w->pos() - unscalePos(me->pos(), NULL);
                windowMove = w;
                effects->setElevatedWindow(windowMove, true);
            }
        } else if ((me->buttons() == Qt::MidButton || me->buttons() == Qt::RightButton) && windowMove == NULL) {
            EffectWindow* w = windowAt(me->pos());
            if (w != NULL) {
                if (w->isOnAllDesktops()) {
                    const int desktop = posToDesktop(me->pos());
                    effects->windowToDesktop(w, desktop);
                    if (isUsingPresentWindows()) {
                        for (int i = 0; i < effects->numberOfDesktops(); i++) {
                            if (i != desktop - 1) {
                                WindowMotionManager& manager = m_managers[ i*effects->numScreens() + w->screen()];
                                manager.unmanage(w);
                                m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
                            }
                        }
                    }
                } else {
                    if (isUsingPresentWindows()) {
                        const int desktop = w->desktop();
                        for (int i = 0; i < effects->numberOfDesktops(); i++) {
                            if (i != desktop - 1) {
                                WindowMotionManager& manager = m_managers[ i*effects->numScreens() + w->screen()];
                                manager.manage(w);
                                m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
                            }
                        }
                    }
                    effects->windowToDesktop(w, NET::OnAllDesktops);
                }
                effects->addRepaintFull();
            }
        }
    }
    if (e->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton) {
        isValidMove = false;
        if (!wasWindowMove && !wasDesktopMove) {
            setCurrentDesktop(posToDesktop(me->pos()));
            if (windowMove)
                effects->activateWindow(windowMove);
            setActive(false);
        }
        if (windowMove) {
            if (wasWindowMove) {
                if (isUsingPresentWindows()) {
                    if (windowMove->isOnAllDesktops()) {
                        const int targetDesktop = posToDesktop(cursorPos());
                        for (int i = 0; i < effects->numberOfDesktops(); ++i) {
                            WindowMotionManager& manager = m_managers[(i)*(effects->numScreens()) + windowMove->screen()];
                            manager.manage(windowMove);
                            if (EffectWindow* modal = windowMove->findModal())
                                manager.manage(modal);
                            if (i + 1 == targetDesktop) {
                                // for the desktop the window is dropped on, we use the current geometry
                                manager.setTransformedGeometry(windowMove, moveGeometryToDesktop(targetDesktop));
                            }
                            m_proxy->calculateWindowTransformations(manager.managedWindows(), windowMove->screen(), manager);
                        }
                    } else {
                        WindowMotionManager& manager = m_managers[(windowMove->desktop()-1)*(effects->numScreens()) + windowMove->screen()];
                        manager.manage(windowMove);
                        if (EffectWindow* modal = windowMove->findModal())
                            manager.manage(modal);
                        manager.setTransformedGeometry(windowMove, moveGeometryToDesktop(windowMove->desktop()));
                        m_proxy->calculateWindowTransformations(manager.managedWindows(), windowMove->screen(), manager);
                    }
                    effects->addRepaintFull();
                }
            }
            effects->setElevatedWindow(windowMove, false);
            windowMove = NULL;
            XDefineCursor(display(), input, QCursor(Qt::PointingHandCursor).handle());
        } else if (wasDesktopMove)
            XDefineCursor(display(), input, QCursor(Qt::PointingHandCursor).handle());
        wasWindowMove = false;
        wasDesktopMove = false;
    }
}

void DesktopGridEffect::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (timeline.currentValue() != 1)   // Block user input during animations
        return;
    if (windowMove != NULL)
        return;
    if (e->type() == QEvent::KeyPress) {
        // check for global shortcuts
        // HACK: keyboard grab disables the global shortcuts so we have to check for global shortcut (bug 156155)
        if (shortcut.contains(e->key() + e->modifiers())) {
            toggle();
            return;
        }

        int desktop = -1;
        // switch by F<number> or just <number>
        if (e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35)
            desktop = e->key() - Qt::Key_F1 + 1;
        else if (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9)
            desktop = e->key() == Qt::Key_0 ? 10 : e->key() - Qt::Key_0;
        if (desktop != -1) {
            if (desktop <= effects->numberOfDesktops()) {
                setHighlightedDesktop(desktop);
                setCurrentDesktop(desktop);
                setActive(false);
            }
            return;
        }
        switch(e->key()) {
            // Wrap only on autorepeat
        case Qt::Key_Left:
            setHighlightedDesktop(desktopToLeft(highlightedDesktop, !e->isAutoRepeat()));
            break;
        case Qt::Key_Right:
            setHighlightedDesktop(desktopToRight(highlightedDesktop, !e->isAutoRepeat()));
            break;
        case Qt::Key_Up:
            setHighlightedDesktop(desktopUp(highlightedDesktop, !e->isAutoRepeat()));
            break;
        case Qt::Key_Down:
            setHighlightedDesktop(desktopDown(highlightedDesktop, !e->isAutoRepeat()));
            break;
        case Qt::Key_Escape:
            setActive(false);
            return;
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
            setCurrentDesktop(highlightedDesktop);
            setActive(false);
            return;
        case Qt::Key_Plus:
            slotAddDesktop();
            break;
        case Qt::Key_Minus:
            slotRemoveDesktop();
            break;
        default:
            break;
        }
    }
}

bool DesktopGridEffect::borderActivated(ElectricBorder border)
{
    if (!borderActivate.contains(border))
        return false;
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return true;
    toggle();
    return true;
}

//-----------------------------------------------------------------------------
// Helper functions

// Transform a point to its position on the scaled grid
QPointF DesktopGridEffect::scalePos(const QPoint& pos, int desktop, int screen) const
{
    if (screen == -1)
        screen = effects->screenNumber(pos);
    QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
    QPoint desktopCell;
    if (orientation == Qt::Horizontal) {
        desktopCell.setX((desktop - 1) % gridSize.width() + 1);
        desktopCell.setY((desktop - 1) / gridSize.width() + 1);
    } else {
        desktopCell.setX((desktop - 1) / gridSize.height() + 1);
        desktopCell.setY((desktop - 1) % gridSize.height() + 1);
    }

    double progress = timeline.currentValue();
    QPointF point(
        interpolate(
            (
                (screenGeom.width() + unscaledBorder[screen]) *(desktopCell.x() - 1)
                - (screenGeom.width() + unscaledBorder[screen]) *(activeCell.x() - 1)
            ) + pos.x(),
            (
                (scaledSize[screen].width() + border) *(desktopCell.x() - 1)
                + scaledOffset[screen].x()
                + (pos.x() - screenGeom.x()) * scale[screen]
            ),
            progress),
        interpolate(
            (
                (screenGeom.height() + unscaledBorder[screen]) *(desktopCell.y() - 1)
                - (screenGeom.height() + unscaledBorder[screen]) *(activeCell.y() - 1)
            ) + pos.y(),
            (
                (scaledSize[screen].height() + border) *(desktopCell.y() - 1)
                + scaledOffset[screen].y()
                + (pos.y() - screenGeom.y()) * scale[screen]
            ),
            progress)
    );

    return point;
}

// Detransform a point to its position on the full grid
// TODO: Doesn't correctly interpolate (Final position is correct though), don't forget to copy to posToDesktop()
QPoint DesktopGridEffect::unscalePos(const QPoint& pos, int* desktop) const
{
    int screen = effects->screenNumber(pos);
    QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

    //double progress = timeline.currentValue();
    double scaledX = /*interpolate(
        ( pos.x() - screenGeom.x() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.width() + unscaledBorder[screen] ) + activeCell.x() - 1,*/
        (pos.x() - scaledOffset[screen].x() + double(border) / 2.0) / (scaledSize[screen].width() + border)/*,
        progress )*/;
    double scaledY = /*interpolate(
        ( pos.y() - screenGeom.y() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.height() + unscaledBorder[screen] ) + activeCell.y() - 1,*/
        (pos.y() - scaledOffset[screen].y() + double(border) / 2.0) / (scaledSize[screen].height() + border)/*,
        progress )*/;
    int gx = qBound(0, int(scaledX), gridSize.width() - 1);     // Zero-based
    int gy = qBound(0, int(scaledY), gridSize.height() - 1);
    scaledX -= gx;
    scaledY -= gy;
    if (desktop != NULL) {
        if (orientation == Qt::Horizontal)
            *desktop = gy * gridSize.width() + gx + 1;
        else
            *desktop = gx * gridSize.height() + gy + 1;
    }

    return QPoint(
               qBound(
                   screenGeom.x(),
                   qRound(
                       scaledX * (screenGeom.width() + unscaledBorder[screen])
                       - unscaledBorder[screen] / 2.0
                       + screenGeom.x()
                   ),
                   screenGeom.right()
               ),
               qBound(
                   screenGeom.y(),
                   qRound(
                       scaledY * (screenGeom.height() + unscaledBorder[screen])
                       - unscaledBorder[screen] / 2.0
                       + screenGeom.y()
                   ),
                   screenGeom.bottom()
               )
           );
}

int DesktopGridEffect::posToDesktop(const QPoint& pos) const
{
    // Copied from unscalePos()
    int screen = effects->screenNumber(pos);
    QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

    //double progress = timeline.currentValue();
    double scaledX = /*interpolate(
        ( pos.x() - screenGeom.x() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.width() + unscaledBorder[screen] ) + activeCell.x() - 1,*/
        (pos.x() - scaledOffset[screen].x() + double(border) / 2.0) / (scaledSize[screen].width() + border)/*,
        progress )*/;
    double scaledY = /*interpolate(
        ( pos.y() - screenGeom.y() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.height() + unscaledBorder[screen] ) + activeCell.y() - 1,*/
        (pos.y() - scaledOffset[screen].y() + double(border) / 2.0) / (scaledSize[screen].height() + border)/*,
        progress )*/;
    int gx = qBound(0, int(scaledX), gridSize.width() - 1);     // Zero-based
    int gy = qBound(0, int(scaledY), gridSize.height() - 1);
    scaledX -= gx;
    scaledY -= gy;
    if (orientation == Qt::Horizontal)
        return gy * gridSize.width() + gx + 1;
    return gx * gridSize.height() + gy + 1;
}

EffectWindow* DesktopGridEffect::windowAt(QPoint pos) const
{
    // Get stacking order top first
    EffectWindowList windows = effects->stackingOrder();
    EffectWindowList::Iterator begin = windows.begin();
    EffectWindowList::Iterator end = windows.end();
    --end;
    while (begin < end)
        qSwap(*begin++, *end--);

    int desktop;
    pos = unscalePos(pos, &desktop);
    if (desktop > effects->numberOfDesktops())
        return NULL;
    if (isUsingPresentWindows()) {
        const int screen = effects->screenNumber(pos);
        EffectWindow *w =
            m_managers.at((desktop - 1) * (effects->numScreens()) + screen).windowAtPoint(pos, false);
        if (w)
            return w;
        foreach (EffectWindow * w, windows) {
            if (w->isOnDesktop(desktop) && w->isDesktop() && w->geometry().contains(pos))
                return w;
        }
    } else {
        foreach (EffectWindow * w, windows) {
            if (w->isOnDesktop(desktop) && !w->isMinimized() && w->geometry().contains(pos))
                return w;
        }
    }
    return NULL;
}

void DesktopGridEffect::setCurrentDesktop(int desktop)
{
    if (orientation == Qt::Horizontal) {
        activeCell.setX((desktop - 1) % gridSize.width() + 1);
        activeCell.setY((desktop - 1) / gridSize.width() + 1);
    } else {
        activeCell.setX((desktop - 1) / gridSize.height() + 1);
        activeCell.setY((desktop - 1) % gridSize.height() + 1);
    }
    if (effects->currentDesktop() != desktop)
        effects->setCurrentDesktop(desktop);
}

void DesktopGridEffect::setHighlightedDesktop(int d)
{
    if (d == highlightedDesktop || d <= 0 || d > effects->numberOfDesktops())
        return;
    highlightedDesktop = d;
    effects->addRepaintFull();
}

int DesktopGridEffect::desktopToRight(int desktop, bool wrap) const
{
    // Copied from Workspace::desktopToRight()
    int dt = desktop - 1;
    if (orientation == Qt::Vertical) {
        dt += gridSize.height();
        if (dt >= effects->numberOfDesktops()) {
            if (wrap)
                dt -= effects->numberOfDesktops();
            else
                return desktop;
        }
    } else {
        int d = (dt % gridSize.width()) + 1;
        if (d >= gridSize.width()) {
            if (wrap)
                d -= gridSize.width();
            else
                return desktop;
        }
        dt = dt - (dt % gridSize.width()) + d;
    }
    return dt + 1;
}

int DesktopGridEffect::desktopToLeft(int desktop, bool wrap) const
{
    // Copied from Workspace::desktopToLeft()
    int dt = desktop - 1;
    if (orientation == Qt::Vertical) {
        dt -= gridSize.height();
        if (dt < 0) {
            if (wrap)
                dt += effects->numberOfDesktops();
            else
                return desktop;
        }
    } else {
        int d = (dt % gridSize.width()) - 1;
        if (d < 0) {
            if (wrap)
                d += gridSize.width();
            else
                return desktop;
        }
        dt = dt - (dt % gridSize.width()) + d;
    }
    return dt + 1;
}

int DesktopGridEffect::desktopUp(int desktop, bool wrap) const
{
    // Copied from Workspace::desktopUp()
    int dt = desktop - 1;
    if (orientation == Qt::Horizontal) {
        dt -= gridSize.width();
        if (dt < 0) {
            if (wrap)
                dt += effects->numberOfDesktops();
            else
                return desktop;
        }
    } else {
        int d = (dt % gridSize.height()) - 1;
        if (d < 0) {
            if (wrap)
                d += gridSize.height();
            else
                return desktop;
        }
        dt = dt - (dt % gridSize.height()) + d;
    }
    return dt + 1;
}

int DesktopGridEffect::desktopDown(int desktop, bool wrap) const
{
    // Copied from Workspace::desktopDown()
    int dt = desktop - 1;
    if (orientation == Qt::Horizontal) {
        dt += gridSize.width();
        if (dt >= effects->numberOfDesktops()) {
            if (wrap)
                dt -= effects->numberOfDesktops();
            else
                return desktop;
        }
    } else {
        int d = (dt % gridSize.height()) + 1;
        if (d >= gridSize.height()) {
            if (wrap)
                d -= gridSize.height();
            else
                return desktop;
        }
        dt = dt - (dt % gridSize.height()) + d;
    }
    return dt + 1;
}

//-----------------------------------------------------------------------------
// Activation

void DesktopGridEffect::toggle()
{
    setActive(!activated);
}

void DesktopGridEffect::setActive(bool active)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return; // Only one fullscreen effect at a time thanks
    if (active && isMotionManagerMovingWindows())
        return; // Still moving windows from last usage - don't activate
    if (activated == active)
        return; // Already in that state

    activated = active;
    if (activated && timeline.currentValue() == 0)
        setup();
    if (!activated) {
        if (isUsingPresentWindows()) {
            QList<WindowMotionManager>::iterator it;
            for (it = m_managers.begin(); it != m_managers.end(); ++it) {
                foreach (EffectWindow * w, (*it).managedWindows()) {
                    (*it).moveWindow(w, w->geometry());
                }
            }
        }
        setHighlightedDesktop(effects->currentDesktop());   // Ensure selected desktop is highlighted
        for (QHash< DesktopButtonsView*, EffectWindow*>::iterator it = m_desktopButtonsViews.begin();
                it != m_desktopButtonsViews.end(); ++it) {
            it.key()->hide();
        }
    }
    effects->addRepaintFull();
}

void DesktopGridEffect::setup()
{
    keyboardGrab = effects->grabKeyboard(this);
    input = effects->createInputWindow(this, 0, 0, displayWidth(), displayHeight(),
                                       Qt::PointingHandCursor);
    effects->setActiveFullScreenEffect(this);
    setHighlightedDesktop(effects->currentDesktop());

    // Soft highlighting
    qDeleteAll(hoverTimeline);
    hoverTimeline.clear();
    for (int i = 0; i < effects->numberOfDesktops(); i++) {
        QTimeLine *newTimeline = new QTimeLine(animationTime(zoomDuration), this);
        newTimeline->setCurveShape(QTimeLine::EaseInOutCurve);
        hoverTimeline.append(newTimeline);
    }
    hoverTimeline[effects->currentDesktop() - 1]->setCurrentTime(hoverTimeline[effects->currentDesktop() - 1]->duration());

    // Create desktop name textures if enabled
    if (desktopNameAlignment) {
        QFont font;
        font.setBold(true);
        font.setPointSize(12);
        for (int i = 0; i < effects->numberOfDesktops(); i++) {
            EffectFrame* frame = effects->effectFrame(EffectFrameUnstyled, false);
            frame->setFont(font);
            frame->setText(effects->desktopName(i + 1));
            frame->setAlignment(desktopNameAlignment);
            desktopNames.append(frame);
        }
    }
    setupGrid();
    setCurrentDesktop(effects->currentDesktop());

    // setup the motion managers
    if (m_usePresentWindows)
        m_proxy = static_cast<PresentWindowsEffectProxy*>(effects->getProxy("presentwindows"));
    if (isUsingPresentWindows()) {
        for (int i = 1; i <= effects->numberOfDesktops(); i++) {
            for (int j = 0; j < effects->numScreens(); j++) {
                WindowMotionManager manager;
                foreach (EffectWindow * w, effects->stackingOrder()) {
                    if (w->isOnDesktop(i) && w->screen() == j && !w->isDesktop() && !w->isDock() &&
                            w->isCurrentTab() && !w->isSkipSwitcher() && w->isOnCurrentActivity()) {
                        manager.manage(w);
                    }
                }
                m_proxy->calculateWindowTransformations(manager.managedWindows(), j, manager);
                m_managers.append(manager);
            }
        }
    }
    bool enableAdd = effects->numberOfDesktops() < 20;
    bool enableRemove = effects->numberOfDesktops() > 1;
    for (int i = 0; i < effects->numScreens(); ++i) {
        DesktopButtonsView* view = new DesktopButtonsView();
        view->setAddDesktopEnabled(enableAdd);
        view->setRemoveDesktopEnabled(enableRemove);
        connect(view, SIGNAL(addDesktop()), SLOT(slotAddDesktop()));
        connect(view, SIGNAL(removeDesktop()), SLOT(slotRemoveDesktop()));
        const QRect screenRect = effects->clientArea(FullScreenArea, i, 1);
        view->setGeometry(screenRect.right() + 1 - view->sceneRect().width(),
                          screenRect.bottom() + 1 - view->sceneRect().height(),
                          view->sceneRect().width(), view->sceneRect().height());
        view->show();
        m_desktopButtonsViews.insert(view, NULL);
    }
}

void DesktopGridEffect::setupGrid()
{
    // We need these variables for every paint so lets cache them
    int x, y;
    int numDesktops = effects->numberOfDesktops();
    switch(layoutMode) {
    default:
    case LayoutPager:
        orientation = Qt::Horizontal;
        gridSize = effects->desktopGridSize();
        // sanity check: pager may report incorrect size in case of one desktop
        if (numDesktops == 1) {
            gridSize = QSize(1, 1);
        }
        break;
    case LayoutAutomatic:
        y = sqrt(float(numDesktops)) + 0.5;
        x = float(numDesktops) / float(y) + 0.5;
        if (x * y < numDesktops)
            x++;
        orientation = Qt::Horizontal;
        gridSize.setWidth(x);
        gridSize.setHeight(y);
        break;
    case LayoutCustom:
        orientation = Qt::Horizontal;
        gridSize.setWidth(ceil(effects->numberOfDesktops() / double(customLayoutRows)));
        gridSize.setHeight(customLayoutRows);
        break;
    }
    scale.clear();
    unscaledBorder.clear();
    scaledSize.clear();
    scaledOffset.clear();
    for (int i = 0; i < effects->numScreens(); i++) {
        QRect geom = effects->clientArea(ScreenArea, i, 0);
        double sScale;
        if (gridSize.width() > gridSize.height())
            sScale = (geom.width() - border * (gridSize.width() + 1)) / double(geom.width() * gridSize.width());
        else
            sScale = (geom.height() - border * (gridSize.height() + 1)) / double(geom.height() * gridSize.height());
        double sBorder = border / sScale;
        QSizeF size(
            double(geom.width()) * sScale,
            double(geom.height()) * sScale
        );
        QPointF offset(
            geom.x() + (geom.width() - size.width() * gridSize.width() - border *(gridSize.width() - 1)) / 2.0,
            geom.y() + (geom.height() - size.height() * gridSize.height() - border *(gridSize.height() - 1)) / 2.0
        );
        scale.append(sScale);
        unscaledBorder.append(sBorder);
        scaledSize.append(size);
        scaledOffset.append(offset);
    }
}

void DesktopGridEffect::finish()
{
    if (desktopNameAlignment) {
        qDeleteAll(desktopNames);
        desktopNames.clear();
    }

    if (keyboardGrab)
        effects->ungrabKeyboard();
    keyboardGrab = false;
    effects->destroyInputWindow(input);
    effects->setActiveFullScreenEffect(0);
    if (isUsingPresentWindows()) {
        while (!m_managers.isEmpty()) {
            m_managers.first().unmanageAll();
            m_managers.removeFirst();
        }
        m_proxy = 0;
    }

    QHash< DesktopButtonsView*, EffectWindow* >::iterator i = m_desktopButtonsViews.begin();
    while (i != m_desktopButtonsViews.end()) {
        if (*i && (*i)->isDeleted())
            (*i)->unrefWindow();
        DesktopButtonsView *view = i.key();
        i = m_desktopButtonsViews.erase(i);
        view->deleteLater();
    }
}

void DesktopGridEffect::globalShortcutChanged(const QKeySequence& seq)
{
    shortcut = KShortcut(seq);
}

bool DesktopGridEffect::isMotionManagerMovingWindows() const
{
    if (isUsingPresentWindows()) {
        QList<WindowMotionManager>::const_iterator it;
        for (it = m_managers.begin(); it != m_managers.end(); ++it) {
            if ((*it).areWindowsMoving())
                return true;
        }
    }
    return false;
}

bool DesktopGridEffect::isUsingPresentWindows() const
{
    return (m_proxy != NULL);
}

// transforms the geometry of the moved window to a geometry on the desktop
// internal method only used when a window is dropped onto a desktop
QRectF DesktopGridEffect::moveGeometryToDesktop(int desktop) const
{
    QPointF point = unscalePos(m_windowMoveGeometry.topLeft() + cursorPos() - m_windowMoveStartPoint);
    const double scaleFactor = scale[ windowMove->screen()];
    if (posToDesktop(m_windowMoveGeometry.topLeft() + cursorPos() - m_windowMoveStartPoint) != desktop) {
        // topLeft is not on the desktop - check other corners
        // if all corners are not on the desktop the window is bigger than the desktop - no matter what it will look strange
        if (posToDesktop(m_windowMoveGeometry.topRight() + cursorPos() - m_windowMoveStartPoint) == desktop) {
            point = unscalePos(m_windowMoveGeometry.topRight() + cursorPos() - m_windowMoveStartPoint) -
                    QPointF(m_windowMoveGeometry.width(), 0) / scaleFactor;
        } else if (posToDesktop(m_windowMoveGeometry.bottomLeft() + cursorPos() - m_windowMoveStartPoint) == desktop) {
            point = unscalePos(m_windowMoveGeometry.bottomLeft() + cursorPos() - m_windowMoveStartPoint) -
                    QPointF(0, m_windowMoveGeometry.height()) / scaleFactor;
        } else if (posToDesktop(m_windowMoveGeometry.bottomRight() + cursorPos() - m_windowMoveStartPoint) == desktop) {
            point = unscalePos(m_windowMoveGeometry.bottomRight() + cursorPos() - m_windowMoveStartPoint) -
                    QPointF(m_windowMoveGeometry.width(), m_windowMoveGeometry.height()) / scaleFactor;
        }
    }
    return QRectF(point, m_windowMoveGeometry.size() / scaleFactor);
}

void DesktopGridEffect::slotAddDesktop()
{
    effects->setNumberOfDesktops(effects->numberOfDesktops() + 1);
}

void DesktopGridEffect::slotRemoveDesktop()
{
    effects->setNumberOfDesktops(effects->numberOfDesktops() - 1);
}

void DesktopGridEffect::slotNumberDesktopsChanged(int old)
{
    if (!activated)
        return;
    const int desktop = effects->numberOfDesktops();
    bool enableAdd = desktop < 20;
    bool enableRemove = desktop > 1;
    for (QHash< DesktopButtonsView*, EffectWindow* >::iterator it = m_desktopButtonsViews.begin();
            it != m_desktopButtonsViews.end(); ++it) {
        it.key()->setAddDesktopEnabled(enableAdd);
        it.key()->setRemoveDesktopEnabled(enableRemove);
    }
    if (old < desktop)
        desktopsAdded(old);
    else
        desktopsRemoved(old);
}

void DesktopGridEffect::desktopsAdded(int old)
{
    const int desktop = effects->numberOfDesktops();
    for (int i = old; i <= effects->numberOfDesktops(); i++) {
        // add a timeline for the new desktop
        QTimeLine *newTimeline = new QTimeLine(animationTime(zoomDuration), this);
        newTimeline->setCurveShape(QTimeLine::EaseInOutCurve);
        hoverTimeline.append(newTimeline);
    }

    // Create desktop name textures if enabled
    if (desktopNameAlignment) {
        QFont font;
        font.setBold(true);
        font.setPointSize(12);
        for (int i = old; i < desktop; i++) {
            EffectFrame* frame = effects->effectFrame(EffectFrameUnstyled, false);
            frame->setFont(font);
            frame->setText(effects->desktopName(i + 1));
            frame->setAlignment(desktopNameAlignment);
            desktopNames.append(frame);
        }
    }

    if (isUsingPresentWindows()) {
        for (int i = old; i <= effects->numberOfDesktops(); i++) {
            for (int j = 0; j < effects->numScreens(); j++) {
                WindowMotionManager manager;
                foreach (EffectWindow * w, effects->stackingOrder()) {
                    if (w->isOnDesktop(i) && w->screen() == j && !w->isDesktop() && !w->isDock() &&
                            w->isCurrentTab()) {
                        manager.manage(w);
                    }
                }
                m_proxy->calculateWindowTransformations(manager.managedWindows(), j, manager);
                m_managers.append(manager);
            }
        }
    }

    setupGrid();

    // and repaint
    effects->addRepaintFull();
}

void DesktopGridEffect::desktopsRemoved(int old)
{
    const int desktop = effects->numberOfDesktops();
    for (int i = desktop; i < old; i++) {
        delete hoverTimeline.takeLast();
        if (desktopNameAlignment) {
            delete desktopNames.last();
            desktopNames.removeLast();
        }
        if (isUsingPresentWindows()) {
            for (int j = 0; j < effects->numScreens(); ++j) {
                WindowMotionManager& manager = m_managers.last();
                manager.unmanageAll();
                m_managers.removeLast();
            }
        }
    }
    // add removed windows to the last desktop
    if (isUsingPresentWindows()) {
        for (int j = 0; j < effects->numScreens(); ++j) {
            WindowMotionManager& manager = m_managers[(desktop-1)*(effects->numScreens())+j ];
            foreach (EffectWindow * w, effects->stackingOrder()) {
                if (!manager.isManaging(w) && w->isOnDesktop(desktop) && w->screen() == j &&
                        !w->isDesktop() && !w->isDock() && w->isCurrentTab()) {
                    manager.manage(w);
                }
            }
            m_proxy->calculateWindowTransformations(manager.managedWindows(), j, manager);
        }
    }

    setupGrid();

    // and repaint
    effects->addRepaintFull();
}

bool DesktopGridEffect::isActive() const
{
    return timeline.currentValue() != 0 || activated || (isUsingPresentWindows() && isMotionManagerMovingWindows());
}

/************************************************
* DesktopButtonView
************************************************/
DesktopButtonsView::DesktopButtonsView(QWidget* parent)
    : QGraphicsView(parent)
{
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFrameShape(QFrame::NoFrame);
    QPalette pal = palette();
    pal.setColor(backgroundRole(), Qt::transparent);
    setPalette(pal);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // setup the scene
    QGraphicsScene* scene = new QGraphicsScene(this);
    m_addDesktopButton = new Plasma::PushButton();
    m_addDesktopButton->setIcon(KIcon("list-add"));
    m_removeDesktopButton = new Plasma::PushButton();
    m_removeDesktopButton->setIcon(KIcon("list-remove"));
    scene->addItem(m_addDesktopButton);
    scene->addItem(m_removeDesktopButton);
    connect(m_addDesktopButton, SIGNAL(clicked()), SIGNAL(addDesktop()));
    connect(m_removeDesktopButton, SIGNAL(clicked()), SIGNAL(removeDesktop()));

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout;
    layout->addItem(m_addDesktopButton);
    layout->addItem(m_removeDesktopButton);

    QGraphicsWidget *form = new QGraphicsWidget;
    form->setLayout(layout);
    form->setGeometry(0, 0, 64 * 2, 64);
    scene->addItem(form);

    m_frame = new Plasma::FrameSvg(this);
    m_frame->setImagePath("dialogs/background");
    m_frame->setCacheAllRenderedFrames(true);
    m_frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    qreal left, top, right, bottom;
    m_frame->getMargins(left, top, right, bottom);
    qreal width = form->size().width() + left + right;
    qreal height = form->size().height() + top + bottom;
    m_frame->resizeFrame(QSizeF(width, height));
    Plasma::WindowEffects::enableBlurBehind(winId(), true, m_frame->mask());
    Plasma::WindowEffects::overrideShadow(winId(), true);
    form->setPos(left, top);
    scene->setSceneRect(QRectF(QPointF(0, 0), QSizeF(width, height)));
    setScene(scene);
}

void DesktopButtonsView::windowInputMouseEvent(QMouseEvent* e)
{
    if (e->type() == QEvent::MouseMove) {
        mouseMoveEvent(e);
    } else if (e->type() == QEvent::MouseButtonPress) {
        mousePressEvent(e);
    } else if (e->type() == QEvent::MouseButtonDblClick) {
        mouseDoubleClickEvent(e);
    } else if (e->type() == QEvent::MouseButtonRelease) {
        mouseReleaseEvent(e);
    }
}

void DesktopButtonsView::setAddDesktopEnabled(bool enable)
{
    m_addDesktopButton->setEnabled(enable);
}

void DesktopButtonsView::setRemoveDesktopEnabled(bool enable)
{
    m_removeDesktopButton->setEnabled(enable);
}

void DesktopButtonsView::drawBackground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect)
    painter->setRenderHint(QPainter::Antialiasing);
    m_frame->paintFrame(painter);
}

} // namespace

#include "desktopgrid.moc"
