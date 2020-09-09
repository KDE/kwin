/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopgrid.h"
// KConfigSkeleton
#include "desktopgridconfig.h"

#include "../presentwindows/presentwindows_proxy.h"
#include "../effect_builtins.h"

#include <QAction>
#include <QApplication>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <netwm_def.h>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QVector2D>
#include <QMatrix4x4>

#include <QQuickItem>
#include <QQmlContext>
#include <KWaylandServer/surface_interface.h>

#include <cmath>

namespace KWin
{

// WARNING, TODO: This effect relies on the desktop layout being EWMH-compliant.

DesktopGridEffect::DesktopGridEffect()
    : activated(false)
    , timeline()
    , keyboardGrab(false)
    , wasWindowMove(false)
    , wasWindowCopy(false)
    , wasDesktopMove(false)
    , isValidMove(false)
    , windowMove(nullptr)
    , windowMoveDiff()
    , gridSize()
    , orientation(Qt::Horizontal)
    , activeCell(1, 1)
    , scale()
    , unscaledBorder()
    , scaledSize()
    , scaledOffset()
    , m_proxy(nullptr)
    , m_activateAction(new QAction(this))
{
    initConfig<DesktopGridConfig>();
    // Load shortcuts
    QAction* a = m_activateAction;
    a->setObjectName(QStringLiteral("ShowDesktopGrid"));
    a->setText(i18n("Show Desktop Grid"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F8);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F8);
    shortcut = KGlobalAccel::self()->shortcut(a);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F8, a);
    effects->registerTouchpadSwipeShortcut(SwipeDirection::Up, a);
    connect(a, &QAction::triggered, this, &DesktopGridEffect::toggle);
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &DesktopGridEffect::globalShortcutChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &DesktopGridEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &DesktopGridEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &DesktopGridEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::numberDesktopsChanged, this, &DesktopGridEffect::slotNumberDesktopsChanged);
    connect(effects, &EffectsHandler::windowFrameGeometryChanged, this, &DesktopGridEffect::slotWindowFrameGeometryChanged);
    connect(effects, &EffectsHandler::numberScreensChanged, this, &DesktopGridEffect::setup);

    connect(effects, &EffectsHandler::screenAboutToLock, this, [this]() {
        setActive(false);
        if (keyboardGrab) {
            effects->ungrabKeyboard();
            keyboardGrab = false;
        }
    });

    // Load all other configuration details
    reconfigure(ReconfigureAll);
}

DesktopGridEffect::~DesktopGridEffect()
{
}

void DesktopGridEffect::reconfigure(ReconfigureFlags)
{
    DesktopGridConfig::self()->read();

    foreach (ElectricBorder border, borderActivate) {
        effects->unreserveElectricBorder(border, this);
    }
    borderActivate.clear();
    foreach (int i, DesktopGridConfig::borderActivate()) {
        borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }

    // TODO: rename zoomDuration to duration
    zoomDuration = animationTime(DesktopGridConfig::zoomDuration() != 0 ? DesktopGridConfig::zoomDuration() : 300);
    timeline.setEasingCurve(QEasingCurve::InOutSine);
    timeline.setDuration(zoomDuration);

    border = DesktopGridConfig::borderWidth();
    desktopNameAlignment = Qt::Alignment(DesktopGridConfig::desktopNameAlignment());
    layoutMode = DesktopGridConfig::layoutMode();
    customLayoutRows = DesktopGridConfig::customLayoutRows();
    m_usePresentWindows = DesktopGridConfig::presentWindows();

    // deactivate and activate all touch border
    const QVector<ElectricBorder> relevantBorders{ElectricLeft, ElectricTop, ElectricRight, ElectricBottom};
    for (auto e : relevantBorders) {
        effects->unregisterTouchBorder(e, m_activateAction);
    }
    const auto touchBorders = DesktopGridConfig::touchBorderActivate();
    for (int i : touchBorders) {
        if (!relevantBorders.contains(ElectricBorder(i))) {
            continue;
        }
        effects->registerTouchBorder(ElectricBorder(i), m_activateAction);
    }
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

    for (auto const &w : effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant(true));
    }

    effects->prePaintScreen(data, time);
}

void DesktopGridEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
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
    for (EffectQuickScene *view : m_desktopButtons) {
        view->rootItem()->setOpacity(timeline.currentValue());
        effects->renderEffectQuickView(view);
    }

    if (isUsingPresentWindows() && windowMove && wasWindowMove) {
        // the moving window has to be painted on top of all desktops
        QPoint diff = cursorPos() - m_windowMoveStartPoint;
        QRect geo = m_windowMoveGeometry.translated(diff);
        WindowPaintData d(windowMove, data.projectionMatrix());
        d *= QVector2D((qreal)geo.width() / (qreal)windowMove->width(), (qreal)geo.height() / (qreal)windowMove->height());
        d += QPoint(geo.left() - windowMove->x(), geo.top() - windowMove->y());
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

    for (auto &w : effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
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
        if (isUsingPresentWindows() && w == windowMove && wasWindowMove &&
            ((!wasWindowCopy && sourceDesktop == paintingDesktop) ||
             (sourceDesktop != highlightedDesktop && highlightedDesktop == paintingDesktop))) {
            return; // will be painted on top of all other windows
        }

        qreal xScale = data.xScale();
        qreal yScale = data.yScale();

        data.multiplyBrightness(1.0 - (0.3 * (1.0 - hoverTimeline[paintingDesktop - 1]->currentValue())));

        for (int screen = 0; screen < effects->numScreens(); screen++) {
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
                if (w->isDesktop())
                    quadsAdded = false;
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
            d.setXScale(interpolate(1, xScale * scale[screen] * (float)transformedGeo.width() / (float)w->geometry().width(), progress));
            d.setYScale(interpolate(1, yScale * scale[screen] * (float)transformedGeo.height() / (float)w->geometry().height(), progress));
            d += QPoint(qRound(newPos.x() - w->x()), qRound(newPos.y() - w->y()));

            if (isUsingPresentWindows() && (w->isDock() || w->isSkipSwitcher())) {
                // fade out panels if present windows is used
                d.multiplyOpacity((1.0 - timeline.currentValue()));
            }
            if (isUsingPresentWindows() && w->isMinimized()) {
                d.multiplyOpacity(timeline.currentValue());
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
        if (!isRelevantWithPresentWindows(w))
            return; // don't add
        foreach (const int i, desktopList(w)) {
            WindowMotionManager& manager = m_managers[ i*effects->numScreens()+w->screen()];
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
        windowMove = nullptr;
    }
    if (isUsingPresentWindows()) {
        foreach (const int i, desktopList(w)) {
            WindowMotionManager& manager = m_managers[i*effects->numScreens()+w->screen()];
            manager.unmanage(w);
            m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
        }
    }
    effects->addRepaintFull();
}

void DesktopGridEffect::slotWindowDeleted(EffectWindow* w)
{
    if (w == windowMove)
        windowMove = nullptr;
    if (isUsingPresentWindows()) {
        for (QList<WindowMotionManager>::iterator it = m_managers.begin(),
                                                 end = m_managers.end(); it != end; ++it) {
            it->unmanage(w);
        }
    }
}

void DesktopGridEffect::slotWindowFrameGeometryChanged(EffectWindow* w, const QRect& old)
{
    Q_UNUSED(old)
    if (!activated)
        return;
    if (w == windowMove && wasWindowMove)
        return;
    if (isUsingPresentWindows()) {
        foreach (const int i, desktopList(w)) {
            WindowMotionManager& manager = m_managers[i*effects->numScreens()+w->screen()];
            m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
        }
    }
}

void DesktopGridEffect::windowInputMouseEvent(QEvent* e)
{
    if ((e->type() != QEvent::MouseMove
            && e->type() != QEvent::MouseButtonPress
            && e->type() != QEvent::MouseButtonRelease)
            || timeline.currentValue() != 1)  // Block user input during animations
        return;
    QMouseEvent* me = static_cast< QMouseEvent* >(e);
    if (!(wasWindowMove || wasDesktopMove)) {
        for (EffectQuickScene *view : m_desktopButtons) {
            view->forwardMouseEvent(me);
            if (e->isAccepted()) {
                return;
            }
        }
    }

    if (e->type() == QEvent::MouseMove) {
        int d = posToDesktop(me->pos());
        if (windowMove != nullptr &&
                (me->pos() - dragStartPos).manhattanLength() > QApplication::startDragDistance()) {
            // Handle window moving
            if (!wasWindowMove) { // Activate on move
                if (isUsingPresentWindows()) {
                    foreach (const int i, desktopList(windowMove)) {
                        WindowMotionManager& manager = m_managers[(i)*(effects->numScreens()) + windowMove->screen()];
                        if ((i + 1) == sourceDesktop) {
                            const QRectF transformedGeo = manager.transformedGeometry(windowMove);
                            const QPointF pos = scalePos(transformedGeo.topLeft().toPoint(), sourceDesktop, windowMove->screen());
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
                    wasWindowMove = true;
                }
            }
            if (windowMove->isMovable() && !isUsingPresentWindows()) {
                wasWindowMove = true;
                int screen = effects->screenNumber(me->pos());
                effects->moveWindow(windowMove, unscalePos(me->pos(), nullptr) + windowMoveDiff, true, 1.0 / scale[screen]);
            }
            if (wasWindowMove) {
                if (effects->waylandDisplay() && (me->modifiers() & Qt::ControlModifier)) {
                    wasWindowCopy = true;
                    effects->defineCursor(Qt::DragCopyCursor);
                } else {
                    wasWindowCopy = false;
                    effects->defineCursor(Qt::ClosedHandCursor);
                }
                if (d != highlightedDesktop) {
                    auto desktops = windowMove->desktops();
                    if (!desktops.contains(d)) {
                        desktops.append(d);
                    }
                    if (highlightedDesktop != sourceDesktop || !wasWindowCopy) {
                        desktops.removeOne(highlightedDesktop);
                    }
                    effects->windowToDesktops(windowMove, desktops);
                    const int screen = effects->screenNumber(me->pos());
                    if (screen != windowMove->screen())
                        effects->windowToScreen(windowMove, screen);
                }
                effects->addRepaintFull();
            }
        } else if ((me->buttons() & Qt::LeftButton) && !wasDesktopMove &&
                  (me->pos() - dragStartPos).manhattanLength() > QApplication::startDragDistance()) {
            wasDesktopMove = true;
            effects->defineCursor(Qt::ClosedHandCursor);
        }
        if (d != highlightedDesktop) { // Highlight desktop
            if ((me->buttons() & Qt::LeftButton) && isValidMove && !wasWindowMove && d <= effects->numberOfDesktops()) {
                EffectWindowList windows = effects->stackingOrder();
                EffectWindowList stack[3];
                for (EffectWindowList::const_iterator it = windows.constBegin(),
                                                      end = windows.constEnd(); it != end; ++it) {
                    EffectWindow *w = const_cast<EffectWindow*>(*it); // we're not really touching it here but below
                    if (w->isOnAllDesktops())
                        continue;
                    if (w->isOnDesktop(highlightedDesktop))
                        stack[0] << w;
                    if (w->isOnDesktop(d))
                        stack[1] << w;
                    if (w->isOnDesktop(m_originalMovingDesktop))
                        stack[2] << w;
                }
                const int desks[4] = {highlightedDesktop, d, m_originalMovingDesktop, highlightedDesktop};
                for (int i = 0; i < 3; ++i ) {
                    if (desks[i] == desks[i+1])
                        continue;
                    foreach (EffectWindow *w, stack[i]) {
                        auto desktops = w->desktops();
                        desktops.removeOne(desks[i]);
                        desktops.append(desks[i+1]);
                        effects->windowToDesktops(w, desktops);

                        if (isUsingPresentWindows()) {
                            m_managers[(desks[i]-1)*(effects->numScreens()) + w->screen()].unmanage(w);
                            m_managers[(desks[i+1]-1)*(effects->numScreens()) + w->screen()].manage(w);
                        }
                    }
                }
                if (isUsingPresentWindows()) {
                    for (int i = 0; i < effects->numScreens(); i++) {
                        for (int j = 0; j < 3; ++j) {
                            WindowMotionManager& manager = m_managers[(desks[j]-1)*(effects->numScreens()) + i ];
                            m_proxy->calculateWindowTransformations(manager.managedWindows(), i, manager);
                        }
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
            dragStartPos = me->pos();
            sourceDesktop = posToDesktop(me->pos());
            bool isDesktop = (me->modifiers() & Qt::ShiftModifier);
            EffectWindow* w = isDesktop ? nullptr : windowAt(me->pos());
            if (w != nullptr)
                isDesktop = w->isDesktop();
            if (isDesktop)
                m_originalMovingDesktop = posToDesktop(me->pos());
            else
                m_originalMovingDesktop = 0;
            if (w != nullptr && !w->isDesktop() && (w->isMovable() || w->isMovableAcrossScreens() || isUsingPresentWindows())) {
                // Prepare it for moving
                windowMoveDiff = w->pos() - unscalePos(me->pos(), nullptr);
                windowMove = w;
                effects->setElevatedWindow(windowMove, true);
            }
        } else if ((me->buttons() == Qt::MiddleButton || me->buttons() == Qt::RightButton) && windowMove == nullptr) {
            EffectWindow* w = windowAt(me->pos());
            if (w && w->isDesktop()) {
                w = nullptr;
            }
            if (w != nullptr) {
                const int desktop = posToDesktop(me->pos());
                if (w->isOnAllDesktops()) {
                    effects->windowToDesktop(w, desktop);
                } else {
                    effects->windowToDesktop(w, NET::OnAllDesktops);
                }
                const bool isOnAllDesktops = w->isOnAllDesktops();
                if (isUsingPresentWindows()) {
                    for (int i = 0; i < effects->numberOfDesktops(); i++) {
                        if (i != desktop - 1) {
                            WindowMotionManager& manager = m_managers[ i*effects->numScreens() + w->screen()];
                            if (isOnAllDesktops)
                                manager.manage(w);
                            else
                                manager.unmanage(w);
                            m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), manager);
                        }
                    }
                }
                effects->addRepaintFull();
            }
        }
    }
    if (e->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton) {
        isValidMove = false;
        if (windowMove)
            effects->activateWindow(windowMove);
        if (wasWindowMove || wasDesktopMove) { // reset pointer
            effects->defineCursor(Qt::PointingHandCursor);
        } else { // click -> exit
            const int desk = posToDesktop(me->pos());
            if (desk > effects->numberOfDesktops())
                return; // don't quit when missing desktop
            setCurrentDesktop(desk);
            setActive(false);
        }
        if (windowMove) {
            if (wasWindowMove && isUsingPresentWindows()) {
                const int targetDesktop = posToDesktop(cursorPos());
                foreach (const int i, desktopList(windowMove)) {
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
                effects->addRepaintFull();
            }
            effects->setElevatedWindow(windowMove, false);
            windowMove = nullptr;
        }
        wasWindowMove = false;
        wasWindowCopy = false;
        wasDesktopMove = false;
    }
}

void DesktopGridEffect::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (timeline.currentValue() != 1)   // Block user input during animations
        return;
    if (windowMove != nullptr)
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
    if (desktop != nullptr) {
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

    double scaledX = (pos.x() - scaledOffset[screen].x() + double(border) / 2.0) / (scaledSize[screen].width() + border);
    double scaledY = (pos.y() - scaledOffset[screen].y() + double(border) / 2.0) / (scaledSize[screen].height() + border);
    int gx = qBound(0, int(scaledX), gridSize.width() - 1);     // Zero-based
    int gy = qBound(0, int(scaledY), gridSize.height() - 1);
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
        return nullptr;
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
            if (w->isOnDesktop(desktop) && w->isOnCurrentActivity() && !w->isMinimized() && w->geometry().contains(pos))
                return w;
        }
    }
    return nullptr;
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
    if (highlightedDesktop > 0 && highlightedDesktop <= hoverTimeline.count())
        hoverTimeline[highlightedDesktop-1]->setCurrentTime(qMin(hoverTimeline[highlightedDesktop-1]->currentTime(),
                                                                 hoverTimeline[highlightedDesktop-1]->duration()));
    highlightedDesktop = d;
    if (highlightedDesktop <= hoverTimeline.count())
        hoverTimeline[highlightedDesktop-1]->setCurrentTime(qMax(hoverTimeline[highlightedDesktop-1]->currentTime(), 0));
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
    if (activated) {
        effects->setShowingDesktop(false);
        if (timeline.currentValue() == 0)
            setup();
    } else {
        if (isUsingPresentWindows()) {
            QList<WindowMotionManager>::iterator it;
            for (it = m_managers.begin(); it != m_managers.end(); ++it) {
                foreach (EffectWindow * w, (*it).managedWindows()) {
                    (*it).moveWindow(w, w->geometry());
                }
            }
        }
        QTimer::singleShot(zoomDuration + 1, this,
            [this] {
                if (activated)
                    return;
                for (EffectQuickScene *view : m_desktopButtons) {
                    view->hide();
                }
            }
        );
        setHighlightedDesktop(effects->currentDesktop());   // Ensure selected desktop is highlighted
    }
    effects->addRepaintFull();
}

void DesktopGridEffect::setup()
{
    if (!isActive())
        return;
    if (!keyboardGrab) {
        keyboardGrab = effects->grabKeyboard(this);
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        effects->setActiveFullScreenEffect(this);
    }
    setHighlightedDesktop(effects->currentDesktop());

    // Soft highlighting
    qDeleteAll(hoverTimeline);
    hoverTimeline.clear();
    for (int i = 0; i < effects->numberOfDesktops(); i++) {
        QTimeLine *newTimeline = new QTimeLine(zoomDuration, this);
        newTimeline->setEasingCurve(QEasingCurve::InOutSine);
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
        m_proxy = static_cast<PresentWindowsEffectProxy*>(effects->getProxy(BuiltInEffects::nameForEffect(BuiltInEffect::PresentWindows)));
    if (isUsingPresentWindows()) {
        m_proxy->reCreateGrids(); // revalidation on multiscreen, bug #351724
        for (int i = 1; i <= effects->numberOfDesktops(); i++) {
            for (int j = 0; j < effects->numScreens(); j++) {
                WindowMotionManager manager;
                foreach (EffectWindow * w, effects->stackingOrder()) {
                    if (w->isOnDesktop(i) && w->screen() == j &&isRelevantWithPresentWindows(w)) {
                        manager.manage(w);
                    }
                }
                m_proxy->calculateWindowTransformations(manager.managedWindows(), j, manager);
                m_managers.append(manager);
            }
        }
    }

    auto it = m_desktopButtons.begin();
    const int n = DesktopGridConfig::showAddRemove() ? effects->numScreens() : 0;
    for (int i = 0; i < n; ++i) {
        EffectQuickScene *view;
        QSize size;
        if (it == m_desktopButtons.end()) {
            view = new EffectQuickScene(this);

            connect(view, &EffectQuickView::repaintNeeded, this, []() {
                effects->addRepaintFull();
            });

            view->rootContext()->setContextProperty("effects", effects);
            view->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/desktopgrid/main.qml"))));

            QQuickItem *rootItem = view->rootItem();
            if (!rootItem) {
                delete view;
                continue;
            }

            m_desktopButtons.append(view);
            it = m_desktopButtons.end(); // changed through insert!

            size = QSize(rootItem->implicitWidth(), rootItem->implicitHeight());
        } else {
            view = *it;
            ++it;
            size = view->size();
        }
        const QRect screenRect = effects->clientArea(FullScreenArea, i, 1);
        view->show(); // pseudo show must happen before geometry changes
        const QPoint position(screenRect.right() - border/3 - size.width(),
                              screenRect.bottom() - border/3 - size.height());
        view->setGeometry(QRect(position, size));
    }
    while (it != m_desktopButtons.end()) {
        (*it)->deleteLater();
        it = m_desktopButtons.erase(it);
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
    effects->stopMouseInterception(this);
    effects->setActiveFullScreenEffect(nullptr);
    if (isUsingPresentWindows()) {
        while (!m_managers.isEmpty()) {
            m_managers.first().unmanageAll();
            m_managers.removeFirst();
        }
        m_proxy = nullptr;
    }
}

void DesktopGridEffect::globalShortcutChanged(QAction *action, const QKeySequence& seq)
{
    if (action->objectName() != QStringLiteral("ShowDesktopGrid")) {
        return;
    }
    shortcut.clear();
    shortcut.append(seq);
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
    return (m_proxy != nullptr);
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

void DesktopGridEffect::slotNumberDesktopsChanged(uint old)
{
    if (!activated)
        return;
    const uint desktop = effects->numberOfDesktops();
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
        QTimeLine *newTimeline = new QTimeLine(zoomDuration, this);
        newTimeline->setEasingCurve(QEasingCurve::InOutSine);
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
        for (int i = old+1; i <= effects->numberOfDesktops(); ++i) {
            for (int j = 0; j < effects->numScreens(); ++j) {
                WindowMotionManager manager;
                foreach (EffectWindow * w, effects->stackingOrder()) {
                    if (w->isOnDesktop(i) && w->screen() == j &&isRelevantWithPresentWindows(w)) {
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
                if (manager.isManaging(w))
                    continue;
                if (w->isOnDesktop(desktop) && w->screen() == j && isRelevantWithPresentWindows(w)) {
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
//TODO: kill this function? or at least keep a consistent numeration with desktops starting from 1
QVector<int> DesktopGridEffect::desktopList(const EffectWindow *w) const
{
    if (w->isOnAllDesktops()) {
        static QVector<int> allDesktops;
        if (allDesktops.count() != effects->numberOfDesktops()) {
            allDesktops.resize(effects->numberOfDesktops());
            for (int i = 0; i < effects->numberOfDesktops(); ++i)
                allDesktops[i] = i;
        }
        return allDesktops;
    }

    QVector<int> desks;
    desks.resize(w->desktops().count());
    int i = 0;
    for (const int desk : w->desktops()) {
        desks[i++] = desk-1;
    }
    return desks;
}

bool DesktopGridEffect::isActive() const
{
    return (timeline.currentValue() != 0 || activated || (isUsingPresentWindows() && isMotionManagerMovingWindows())) && !effects->isScreenLocked();
}

bool DesktopGridEffect::isRelevantWithPresentWindows(EffectWindow *w) const
{
    if (w->isSpecialWindow() || w->isUtility()) {
        return false;
    }

    if (w->isSkipSwitcher()) {
        return false;
    }

    if (w->isDeleted()) {
        return false;
    }

    if (!w->acceptsFocus()) {
        return false;
    }

    if (!w->isOnCurrentActivity()) {
        return false;
    }

    return true;
}

} // namespace

