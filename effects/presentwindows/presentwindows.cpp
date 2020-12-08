/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "presentwindows.h"
//KConfigSkeleton
#include "presentwindowsconfig.h"
#include <QAction>
#include <KGlobalAccel>
#include <KLocalizedString>

#include <kwinglutils.h>

#include <QMouseEvent>
#include <netwm_def.h>

#include <QApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QGraphicsObject>
#include <QTimer>
#include <QVector2D>
#include <QVector4D>

#include <climits>
#include <cmath>

namespace KWin
{

PresentWindowsEffect::PresentWindowsEffect()
    : m_proxy(this)
    , m_activated(false)
    , m_ignoreMinimized(false)
    , m_decalOpacity(0.0)
    , m_hasKeyboardGrab(false)
    , m_mode(ModeCurrentDesktop)
    , m_managerWindow(nullptr)
    , m_needInitialSelection(false)
    , m_highlightedWindow(nullptr)
    , m_filterFrame(nullptr)
    , m_closeView(nullptr)
    , m_exposeAction(new QAction(this))
    , m_exposeAllAction(new QAction(this))
    , m_exposeClassAction(new QAction(this))
{
    initConfig<PresentWindowsConfig>();

    auto announceSupportProperties = [this] {
        m_atomDesktop = effects->announceSupportProperty("_KDE_PRESENT_WINDOWS_DESKTOP", this);
        m_atomWindows = effects->announceSupportProperty("_KDE_PRESENT_WINDOWS_GROUP", this);
    };
    announceSupportProperties();
    connect(effects, &EffectsHandler::xcbConnectionChanged, this, announceSupportProperties);

    QAction* exposeAction = m_exposeAction;
    exposeAction->setObjectName(QStringLiteral("Expose"));
    exposeAction->setText(i18n("Toggle Present Windows (Current desktop)"));
    KGlobalAccel::self()->setDefaultShortcut(exposeAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F9);
    KGlobalAccel::self()->setShortcut(exposeAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F9);
    shortcut = KGlobalAccel::self()->shortcut(exposeAction);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F9, exposeAction);
    connect(exposeAction, &QAction::triggered, this, &PresentWindowsEffect::toggleActive);

    QAction* exposeAllAction = m_exposeAllAction;
    exposeAllAction->setObjectName(QStringLiteral("ExposeAll"));
    exposeAllAction->setText(i18n("Toggle Present Windows (All desktops)"));
    KGlobalAccel::self()->setDefaultShortcut(exposeAllAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F10 << Qt::Key_LaunchC);
    KGlobalAccel::self()->setShortcut(exposeAllAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F10 << Qt::Key_LaunchC);
    shortcutAll = KGlobalAccel::self()->shortcut(exposeAllAction);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F10, exposeAllAction);
    effects->registerTouchpadSwipeShortcut(SwipeDirection::Down, exposeAllAction);
    connect(exposeAllAction, &QAction::triggered, this, &PresentWindowsEffect::toggleActiveAllDesktops);

    QAction* exposeClassAction = m_exposeClassAction;
    exposeClassAction->setObjectName(QStringLiteral("ExposeClass"));
    exposeClassAction->setText(i18n("Toggle Present Windows (Window class)"));
    KGlobalAccel::self()->setDefaultShortcut(exposeClassAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F7);
    KGlobalAccel::self()->setShortcut(exposeClassAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F7);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F7, exposeClassAction);
    connect(exposeClassAction, &QAction::triggered, this, &PresentWindowsEffect::toggleActiveClass);
    shortcutClass = KGlobalAccel::self()->shortcut(exposeClassAction);
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &PresentWindowsEffect::globalShortcutChanged);

    reconfigure(ReconfigureAll);
    connect(effects, &EffectsHandler::windowAdded, this, &PresentWindowsEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &PresentWindowsEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &PresentWindowsEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::windowFrameGeometryChanged, this, &PresentWindowsEffect::slotWindowFrameGeometryChanged);
    connect(effects, &EffectsHandler::propertyNotify, this, &PresentWindowsEffect::slotPropertyNotify);
    connect(effects, &EffectsHandler::numberScreensChanged, this,
        [this] {
            if (isActive())
                reCreateGrids();
        }
    );
    connect(effects, &EffectsHandler::screenAboutToLock, this, [this]() {
        setActive(false);
    });
}

PresentWindowsEffect::~PresentWindowsEffect()
{
    delete m_filterFrame;
    delete m_closeView;
}

void PresentWindowsEffect::reconfigure(ReconfigureFlags)
{
    PresentWindowsConfig::self()->read();
    foreach (ElectricBorder border, m_borderActivate) {
        effects->unreserveElectricBorder(border, this);
    }
    foreach (ElectricBorder border, m_borderActivateAll) {
        effects->unreserveElectricBorder(border, this);
    }
    m_borderActivate.clear();
    m_borderActivateAll.clear();

    foreach (int i, PresentWindowsConfig::borderActivate()) {
        m_borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }
    foreach (int i, PresentWindowsConfig::borderActivateAll()) {
        m_borderActivateAll.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }
    foreach (int i, PresentWindowsConfig::borderActivateClass()) {
        m_borderActivateClass.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }

    m_layoutMode = PresentWindowsConfig::layoutMode();
    m_showCaptions = PresentWindowsConfig::drawWindowCaptions();
    m_showIcons = PresentWindowsConfig::drawWindowIcons();
    m_doNotCloseWindows = !PresentWindowsConfig::allowClosingWindows();

    if (m_doNotCloseWindows) {
        delete m_closeView;
        m_closeView = nullptr;
    }

    m_ignoreMinimized = PresentWindowsConfig::ignoreMinimized();
    m_accuracy = PresentWindowsConfig::accuracy() * 20;
    m_fillGaps = PresentWindowsConfig::fillGaps();
    m_fadeDuration = double(animationTime(150));
    m_showPanel = PresentWindowsConfig::showPanel();
    m_leftButtonWindow = (WindowMouseAction)PresentWindowsConfig::leftButtonWindow();
    m_middleButtonWindow = (WindowMouseAction)PresentWindowsConfig::middleButtonWindow();
    m_rightButtonWindow = (WindowMouseAction)PresentWindowsConfig::rightButtonWindow();
    m_leftButtonDesktop = (DesktopMouseAction)PresentWindowsConfig::leftButtonDesktop();
    m_middleButtonDesktop = (DesktopMouseAction)PresentWindowsConfig::middleButtonDesktop();
    m_rightButtonDesktop = (DesktopMouseAction)PresentWindowsConfig::rightButtonDesktop();

    // touch screen edges
    const QVector<ElectricBorder> relevantBorders{ElectricLeft, ElectricTop, ElectricRight, ElectricBottom};
    for (auto e : relevantBorders) {
        effects->unregisterTouchBorder(e, m_exposeAction);
        effects->unregisterTouchBorder(e, m_exposeAllAction);
        effects->unregisterTouchBorder(e, m_exposeClassAction);
    }
    auto touchEdge = [&relevantBorders] (const QList<int> touchBorders, QAction *action) {
        for (int i : touchBorders) {
            if (!relevantBorders.contains(ElectricBorder(i))) {
                continue;
            }
            effects->registerTouchBorder(ElectricBorder(i), action);
        }
    };
    touchEdge(PresentWindowsConfig::touchBorderActivate(), m_exposeAction);
    touchEdge(PresentWindowsConfig::touchBorderActivateAll(), m_exposeAllAction);
    touchEdge(PresentWindowsConfig::touchBorderActivateClass(), m_exposeClassAction);
}

void* PresentWindowsEffect::proxy()
{
    return &m_proxy;
}

void PresentWindowsEffect::toggleActiveClass()
{
    if (!m_activated) {
        if (!effects->activeWindow())
            return;
        m_mode = ModeWindowClass;
        m_class = effects->activeWindow()->windowClass();
    }
    setActive(!m_activated);
}

//-----------------------------------------------------------------------------
// Screen painting

void PresentWindowsEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    m_motionManager.calculate(time);

    // We need to mark the screen as having been transformed otherwise there will be no repainting
    if (m_activated || m_motionManager.managingWindows())
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    if (m_activated)
        m_decalOpacity = qMin(1.0, m_decalOpacity + time / m_fadeDuration);
    else
        m_decalOpacity = qMax(0.0, m_decalOpacity - time / m_fadeDuration);

    effects->prePaintScreen(data, time);
}

void PresentWindowsEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    // Display the filter box
    if (!m_windowFilter.isEmpty())
        m_filterFrame->render(region);

    if (m_closeView)
        effects->renderEffectQuickView(m_closeView);
}

void PresentWindowsEffect::postPaintScreen()
{
    if (m_motionManager.areWindowsMoving())
        effects->addRepaintFull();
    else if (!m_activated && m_motionManager.managingWindows() && !(m_closeView && m_closeView->isVisible())) {
        // We have finished moving them back, stop processing
        m_motionManager.unmanageAll();

        DataHash::iterator i = m_windowData.begin();
        while (i != m_windowData.end()) {
            delete i.value().textFrame;
            delete i.value().iconFrame;
            ++i;
        }
        m_windowData.clear();

        foreach (EffectWindow * w, effects->stackingOrder()) {
            w->setData(WindowForceBlurRole, QVariant());
            w->setData(WindowForceBackgroundContrastRole, QVariant());
        }
        effects->setActiveFullScreenEffect(nullptr);
        effects->addRepaintFull();
    } else if (m_activated && m_needInitialSelection) {
        m_needInitialSelection = false;
        QMouseEvent me(QEvent::MouseMove, cursorPos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        windowInputMouseEvent(&me);
    }

    // Update windows that are changing brightness or opacity
    DataHash::const_iterator i;
    for (i = m_windowData.constBegin(); i != m_windowData.constEnd(); ++i) {
        if (i.value().opacity > 0.0 && i.value().opacity < 1.0)
            i.key()->addRepaintFull();
        if (i.key()->isDesktop() && !m_motionManager.isManaging(i.key())) {
            if (i.value().highlight != 0.3)
                i.key()->addRepaintFull();
        }
        else if (i.value().highlight > 0.0 && i.value().highlight < 1.0)
            i.key()->addRepaintFull();
    }

    effects->postPaintScreen();
}

//-----------------------------------------------------------------------------
// Window painting

void PresentWindowsEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    // TODO: We should also check to see if any windows are fading just in case fading takes longer
    //       than moving the windows when the effect is deactivated.
    if (m_activated || m_motionManager.areWindowsMoving() || m_closeView) {
        DataHash::iterator winData = m_windowData.find(w);
        if (winData == m_windowData.end()) {
            effects->prePaintWindow(w, data, time);
            return;
        }
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);   // Display always
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);

        // Calculate window's opacity
        // TODO: Minimized windows or windows not on the current desktop are only 75% visible?
        if (winData->visible) {
            if (winData->deleted)
                winData->opacity = qMax(0.0, winData->opacity - time / m_fadeDuration);
            else
                winData->opacity = qMin(/*(w->isMinimized() || !w->isOnCurrentDesktop()) ? 0.75 :*/ 1.0,
                                          winData->opacity + time / m_fadeDuration);
        } else
            winData->opacity = qMax(0.0, winData->opacity - time / m_fadeDuration);

        if (winData->opacity <= 0.0) {
            // don't disable painting for panels if show panel is set
            if (!(m_showPanel && w->isDock()))
                w->disablePainting(EffectWindow::PAINT_DISABLED);
        } else if (winData->opacity != 1.0)
            data.setTranslucent();

        const bool isInMotion = m_motionManager.isManaging(w);
        // Calculate window's brightness
        if (w == m_highlightedWindow || !m_activated)
            winData->highlight = qMin(1.0, winData->highlight + time / m_fadeDuration);
        else if (!isInMotion && w->isDesktop())
            winData->highlight = 0.3;
        else
            winData->highlight = qMax(0.0, winData->highlight - time / m_fadeDuration);

        // Closed windows
        if (winData->deleted) {
            data.setTranslucent();
            if (winData->opacity <= 0.0 && winData->referenced) {
                // it's possible that another effect has referenced the window
                // we have to keep the window in the list to prevent flickering
                winData->referenced = false;
                w->unrefWindow();
            } else
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
        }

        // desktop windows on other desktops (Plasma activity per desktop) should not be painted
        if (w->isDesktop() && !w->isOnCurrentDesktop())
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);

        if (isInMotion)
            data.setTransformed(); // We will be moving this window
    }
    effects->prePaintWindow(w, data, time);
}

void PresentWindowsEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (m_activated || m_motionManager.areWindowsMoving()) {
        DataHash::const_iterator winData = m_windowData.constFind(w);
        if (winData == m_windowData.constEnd() || (w->isDock() && m_showPanel)) {
            // in case the panel should be shown just display it without any changes
            effects->paintWindow(w, mask, region, data);
            return;
        }

        mask |= PAINT_WINDOW_LANCZOS;
        // Apply opacity and brightness
        data.multiplyOpacity(winData->opacity);
        data.multiplyBrightness(interpolate(0.40, 1.0, winData->highlight));

        if (m_motionManager.isManaging(w)) {
            if (w->isDesktop()) {
                effects->paintWindow(w, mask, region, data);
            }
            m_motionManager.apply(w, data);
            QRect rect = m_motionManager.transformedGeometry(w).toRect();

            if (m_activated && winData->highlight > 0.0) {
                // scale the window (interpolated by the highlight level) to at least 105% or to cover 1/16 of the screen size - yet keep it in screen bounds
                QRect area = effects->clientArea(FullScreenArea, w);

                QSizeF effSize(w->width()*data.xScale(), w->height()*data.yScale());
                const float xr = area.width()/effSize.width();
                const float yr = area.height()/effSize.height();
                float tScale = 0.0;
                if (xr < yr) {
                    tScale = qMax(xr/4.0, yr/32.0);
                } else {
                    tScale = qMax(xr/32.0, yr/4.0);
                }
                if (tScale < 1.05) {
                    tScale = 1.05;
                }
                if (effSize.width()*tScale > area.width())
                    tScale = area.width() / effSize.width();
                if (effSize.height()*tScale > area.height())
                    tScale = area.height() / effSize.height();

                const qreal scale = interpolate(1.0, tScale, winData->highlight);
                if (scale > 1.0) {
                    if (scale < tScale) // don't use lanczos during transition
                        mask &= ~PAINT_WINDOW_LANCZOS;

                    const float df = (tScale-1.0f)*0.5f;
                    int tx = qRound(rect.width()*df);
                    int ty = qRound(rect.height()*df);
                    QRect tRect(rect.adjusted(-tx, -ty, tx, ty));
                    tx = qMax(tRect.x(), area.x()) + qMin(0, area.right()-tRect.right());
                    ty = qMax(tRect.y(), area.y()) + qMin(0, area.bottom()-tRect.bottom());
                    tx = qRound((tx-rect.x())*winData->highlight);
                    ty = qRound((ty-rect.y())*winData->highlight);

                    rect.translate(tx,ty);
                    rect.setWidth(rect.width()*scale);
                    rect.setHeight(rect.height()*scale);

                    data *= QVector2D(scale, scale);
                    data += QPoint(tx, ty);
                }
            }

            if (m_motionManager.areWindowsMoving()) {
                mask &= ~PAINT_WINDOW_LANCZOS;
            }
            effects->paintWindow(w, mask, region, data);

            if (m_showIcons) {
                QPoint point(rect.x() + rect.width() * 0.95,
                             rect.y() + rect.height() * 0.95);
                winData->iconFrame->setPosition(point);
                if (effects->compositingType() == KWin::OpenGL2Compositing && data.shader) {
                    const float a = 0.9 * data.opacity() * m_decalOpacity * 0.75;
                    data.shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
                }
                winData->iconFrame->render(region, 0.9 * data.opacity() * m_decalOpacity, 0.75);
            }
            if (m_showCaptions) {
                QPoint point(rect.x() + rect.width() / 2,
                             rect.y() + rect.height() / 2);
                winData->textFrame->setPosition(point);
                if (effects->compositingType() == KWin::OpenGL2Compositing && data.shader) {
                    const float a = 0.9 * data.opacity() * m_decalOpacity * 0.75;
                    data.shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
                }
                winData->textFrame->render(region, 0.9 * data.opacity() * m_decalOpacity, 0.75);
            }
        } else {
            effects->paintWindow(w, mask, region, data);
        }
    } else
        effects->paintWindow(w, mask, region, data);
}

//-----------------------------------------------------------------------------
// User interaction

void PresentWindowsEffect::slotWindowAdded(EffectWindow *w)
{
    if (!m_activated)
        return;

    WindowData *winData = &m_windowData[w];
    winData->visible = isVisibleWindow(w);
    winData->opacity = 0.0;
    winData->highlight = 0.0;
    winData->textFrame = effects->effectFrame(EffectFrameUnstyled, false);

    QFont font;
    font.setBold(true);
    font.setPointSize(12);

    winData->textFrame->setFont(font);
    winData->iconFrame = effects->effectFrame(EffectFrameUnstyled, false);
    winData->iconFrame->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    winData->iconFrame->setIcon(w->icon());
    winData->iconFrame->setIconSize(QSize(32, 32));

    if (isSelectableWindow(w)) {
        m_motionManager.manage(w);
        rearrangeWindows();
    }
}

void PresentWindowsEffect::slotWindowClosed(EffectWindow *w)
{
    if (m_managerWindow == w)
        m_managerWindow = nullptr;

    DataHash::iterator winData = m_windowData.find(w);
    if (winData == m_windowData.end())
        return;

    winData->deleted = true;
    if (!winData->referenced) {
        winData->referenced = true;
        w->refWindow();
    }

    if (m_highlightedWindow == w)
        setHighlightedWindow(findFirstWindow());

    rearrangeWindows();

    foreach (EffectWindow *w, m_motionManager.managedWindows()) {
        winData = m_windowData.find(w);
        if (winData != m_windowData.end() && !winData->deleted)
           return; // found one that is not deleted? then we go on
    }
    setActive(false);     //else no need to keep this open
}

void PresentWindowsEffect::slotWindowDeleted(EffectWindow *w)
{
    DataHash::iterator winData = m_windowData.find(w);
    if (winData == m_windowData.end())
        return;

    delete winData->textFrame;
    delete winData->iconFrame;
    m_windowData.erase(winData);
    m_motionManager.unmanage(w);
}

void PresentWindowsEffect::slotWindowFrameGeometryChanged(EffectWindow* w, const QRect& old)
{
    Q_UNUSED(old)

    if (!m_activated)
        return;
    if (!m_windowData.contains(w))
        return;

    rearrangeWindows();
}

bool PresentWindowsEffect::borderActivated(ElectricBorder border)
{
    int mode = 0;
    if (m_borderActivate.contains(border))
        mode |= 1;
    else if (m_borderActivateAll.contains(border))
        mode |= 2;
    else if (m_borderActivateClass.contains(border))
        mode |= 4;

    if (!mode)
        return false;

    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return true;

    if (mode & 1)
        toggleActive();
    else if (mode & 2)
        toggleActiveAllDesktops();
    else if (mode & 4)
        toggleActiveClass();
    return true;
}

void PresentWindowsEffect::windowInputMouseEvent(QEvent *e)
{
    QMouseEvent* me = dynamic_cast< QMouseEvent* >(e);
    if (!me) {
        return;
    }
    me->setAccepted(false);

    if (m_closeView) {
        const bool contains = m_closeView->geometry().contains(me->pos());
        if (!m_closeView->isVisible() && contains) {
            updateCloseWindow();
        }
        m_closeView->forwardMouseEvent(e);
    }
    if (e->isAccepted()) {
        return;
    }
    inputEventUpdate(me->pos(), me->type(), me->button());
}

void PresentWindowsEffect::inputEventUpdate(const QPoint &pos, QEvent::Type type, Qt::MouseButton button)
{
    // Which window are we hovering over? Always trigger as we don't always get move events before clicking
    // We cannot use m_motionManager.windowAtPoint() as the window might not be visible
    EffectWindowList windows = m_motionManager.managedWindows();
    bool hovering = false;
    EffectWindow *highlightCandidate = nullptr;
    for (int i = 0; i < windows.size(); ++i) {
        DataHash::const_iterator winData = m_windowData.constFind(windows.at(i));
        if (winData == m_windowData.constEnd())
            continue;

        if (m_motionManager.transformedGeometry(windows.at(i)).contains(pos) &&
                winData->visible && !winData->deleted) {
            hovering = true;
            if (windows.at(i) && m_highlightedWindow != windows.at(i))
                highlightCandidate = windows.at(i);
            break;
        }
    }

    if (!hovering)
        setHighlightedWindow(nullptr);
    if (m_highlightedWindow && m_motionManager.transformedGeometry(m_highlightedWindow).contains(pos))
        updateCloseWindow();
    else if (m_closeView)
        m_closeView->hide();

    if (type == QEvent::MouseButtonRelease) {
        if (highlightCandidate)
            setHighlightedWindow(highlightCandidate);
        if (button == Qt::LeftButton) {
            if (hovering) {
                // mouse is hovering above a window - use MouseActionsWindow
                mouseActionWindow(m_leftButtonWindow);
            } else {
                // mouse is hovering above desktop - use MouseActionsDesktop
                mouseActionDesktop(m_leftButtonDesktop);
            }
        }
        if (button == Qt::MiddleButton) {
            if (hovering) {
                // mouse is hovering above a window - use MouseActionsWindow
                mouseActionWindow(m_middleButtonWindow);
            } else {
                // mouse is hovering above desktop - use MouseActionsDesktop
                mouseActionDesktop(m_middleButtonDesktop);
            }
        }
        if (button == Qt::RightButton) {
            if (hovering) {
                // mouse is hovering above a window - use MouseActionsWindow
                mouseActionWindow(m_rightButtonWindow);
            } else {
                // mouse is hovering above desktop - use MouseActionsDesktop
                mouseActionDesktop(m_rightButtonDesktop);
            }
        }
    } else if (highlightCandidate && !m_motionManager.areWindowsMoving())
        setHighlightedWindow(highlightCandidate);
}

bool PresentWindowsEffect::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(time)

    if (!m_activated) {
        return false;
    }

    // only if we don't track a touch id yet
    if (!m_touch.active) {
        m_touch.active = true;
        m_touch.id = id;
        inputEventUpdate(pos.toPoint());
    }
    return true;
}

bool PresentWindowsEffect::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)

    if (!m_activated) {
        return false;
    }
    if (m_touch.active && m_touch.id == id) {
        // only update for the touch id we track
        inputEventUpdate(pos.toPoint());
    }
    return true;
}

bool PresentWindowsEffect::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)

    if (!m_activated) {
        return false;
    }
    if (m_touch.active && m_touch.id == id) {
        m_touch.active = false;
        m_touch.id = 0;
        if (m_highlightedWindow) {
            mouseActionWindow(m_leftButtonWindow);
        }
    }
    return true;
}

void PresentWindowsEffect::mouseActionWindow(WindowMouseAction& action)
{
    switch(action) {
    case WindowActivateAction:
        if (m_highlightedWindow)
            effects->activateWindow(m_highlightedWindow);
        setActive(false);
        break;
    case WindowExitAction:
        setActive(false);
        break;
    case WindowToCurrentDesktopAction:
        if (m_highlightedWindow)
            effects->windowToDesktop(m_highlightedWindow, effects->currentDesktop());
        break;
    case WindowToAllDesktopsAction:
        if (m_highlightedWindow) {
            if (m_highlightedWindow->isOnAllDesktops())
                effects->windowToDesktop(m_highlightedWindow, effects->currentDesktop());
            else
                effects->windowToDesktop(m_highlightedWindow, NET::OnAllDesktops);
        }
        break;
    case WindowMinimizeAction:
        if (m_highlightedWindow) {
            if (m_highlightedWindow->isMinimized())
                m_highlightedWindow->unminimize();
            else
                m_highlightedWindow->minimize();
        }
        break;
    case WindowCloseAction:
        if (m_highlightedWindow) {
            m_highlightedWindow->closeWindow();
        }
        break;
    default:
        break;
    }
}

void PresentWindowsEffect::mouseActionDesktop(DesktopMouseAction& action)
{
    switch(action) {
    case DesktopActivateAction:
        if (m_highlightedWindow)
            effects->activateWindow(m_highlightedWindow);
        setActive(false);
        break;
    case DesktopExitAction:
        setActive(false);
        break;
    case DesktopShowDesktopAction:
        effects->setShowingDesktop(true);
        setActive(false);
    default:
        break;
    }
}

void PresentWindowsEffect::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        // check for global shortcuts
        // HACK: keyboard grab disables the global shortcuts so we have to check for global shortcut (bug 156155)
        if (m_mode == ModeCurrentDesktop && shortcut.contains(e->key() + e->modifiers())) {
            toggleActive();
            return;
        }
        if (m_mode == ModeAllDesktops && shortcutAll.contains(e->key() + e->modifiers())) {
            toggleActiveAllDesktops();
            return;
        }
        if (m_mode == ModeWindowClass && shortcutClass.contains(e->key() + e->modifiers())) {
            toggleActiveClass();
            return;
        }

        switch(e->key()) {
            // Wrap only if not auto-repeating
        case Qt::Key_Left:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, -1, 0, !e->isAutoRepeat()));
            break;
        case Qt::Key_Right:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 1, 0, !e->isAutoRepeat()));
            break;
        case Qt::Key_Up:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, -1, !e->isAutoRepeat()));
            break;
        case Qt::Key_Down:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, 1, !e->isAutoRepeat()));
            break;
        case Qt::Key_Home:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, -1000, 0, false));
            break;
        case Qt::Key_End:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 1000, 0, false));
            break;
        case Qt::Key_PageUp:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, -1000, false));
            break;
        case Qt::Key_PageDown:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, 1000, false));
            break;
        case Qt::Key_Backspace:
            if (!m_windowFilter.isEmpty()) {
                m_windowFilter.remove(m_windowFilter.length() - 1, 1);
                updateFilterFrame();
                rearrangeWindows();
            }
            return;
        case Qt::Key_Escape:
            setActive(false);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_highlightedWindow)
                effects->activateWindow(m_highlightedWindow);
            setActive(false);
            return;
        case Qt::Key_Tab:
            return; // Nothing at the moment
        case Qt::Key_Delete:
            if (!m_windowFilter.isEmpty()) {
                m_windowFilter.clear();
                updateFilterFrame();
                rearrangeWindows();
            }
            break;
        case 0:
            return; // HACK: Workaround for Qt bug on unbound keys (#178547)
        default:
            if (!e->text().isEmpty()) {
                m_windowFilter.append(e->text());
                updateFilterFrame();
                rearrangeWindows();
                return;
            }
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// Atom handling
void PresentWindowsEffect::slotPropertyNotify(EffectWindow* w, long a)
{
    if (m_atomDesktop == XCB_ATOM_NONE && m_atomWindows == XCB_ATOM_NONE) {
        return;
    }
    if (!w || (a != m_atomDesktop && a != m_atomWindows))
        return; // Not our atom

    if (a == m_atomDesktop) {
        QByteArray byteData = w->readProperty(m_atomDesktop, m_atomDesktop, 32);
        if (byteData.length() < 1) {
            // Property was removed, end present windows
            setActive(false);
            return;
        }
        auto* data = reinterpret_cast<uint32_t*>(byteData.data());

        if (!data[0]) {
            // Purposely ending present windows by issuing a NULL target
            setActive(false);
            return;
        }
        // present windows is active so don't do anything
        if (m_activated)
            return;

        int desktop = data[0];
        if (desktop > effects->numberOfDesktops())
            return;
        if (desktop == -1)
            toggleActiveAllDesktops();
        else {
            m_mode = ModeSelectedDesktop;
            m_desktop = desktop;
            m_managerWindow = w;
            setActive(true);
        }
    } else if (a == m_atomWindows) {
        QByteArray byteData = w->readProperty(m_atomWindows, m_atomWindows, 32);
        if (byteData.length() < 1) {
            // Property was removed, end present windows
            setActive(false);
            return;
        }
        auto* data = reinterpret_cast<uint32_t*>(byteData.data());

        if (!data[0]) {
            // Purposely ending present windows by issuing a NULL target
            setActive(false);
            return;
        }
        // present windows is active so don't do anything
        if (m_activated)
            return;

        // for security clear selected windows
        m_selectedWindows.clear();
        int length = byteData.length() / sizeof(data[0]);
        for (int i = 0; i < length; i++) {
            EffectWindow* foundWin = effects->findWindow(data[i]);
            if (!foundWin) {
                qCDebug(KWINEFFECTS) << "Invalid window targetted for present windows. Requested:" << data[i];
                continue;
            }
            m_selectedWindows.append(foundWin);
        }
        m_mode = ModeWindowGroup;
        m_managerWindow = w;
        setActive(true);
    }
}

//-----------------------------------------------------------------------------
// Window rearranging

void PresentWindowsEffect::rearrangeWindows()
{
    if (!m_activated)
        return;

    effects->addRepaintFull(); // Trigger the first repaint
    if (m_closeView)
        m_closeView->hide();

    // Work out which windows are on which screens
    EffectWindowList windowlist;
    QList<EffectWindowList> windowlists;
    for (int i = 0; i < effects->numScreens(); i++)
        windowlists.append(EffectWindowList());

    if (m_windowFilter.isEmpty()) {
        windowlist = m_motionManager.managedWindows();
        foreach (EffectWindow * w, m_motionManager.managedWindows()) {
            DataHash::iterator winData = m_windowData.find(w);
            if (winData == m_windowData.end() || winData->deleted)
                continue; // don't include closed windows
            windowlists[w->screen()].append(w);
            winData->visible = true;
        }
    } else {
        // Can we move this filtering somewhere else?
        foreach (EffectWindow * w, m_motionManager.managedWindows()) {
            DataHash::iterator winData = m_windowData.find(w);
            if (winData == m_windowData.end() || winData->deleted)
                continue; // don't include closed windows

            if (w->caption().contains(m_windowFilter, Qt::CaseInsensitive) ||
                    w->windowClass().contains(m_windowFilter, Qt::CaseInsensitive) ||
                    w->windowRole().contains(m_windowFilter, Qt::CaseInsensitive)) {
                windowlist.append(w);
                windowlists[w->screen()].append(w);
                winData->visible = true;
            } else
                winData->visible = false;
        }
    }
    if (windowlist.isEmpty()) {
        setHighlightedWindow(nullptr);
        return;
    }

    // We filtered out the highlighted window
    if (m_highlightedWindow) {
        DataHash::iterator winData = m_windowData.find(m_highlightedWindow);
        if (winData != m_windowData.end() && !winData->visible)
            setHighlightedWindow(findFirstWindow());
    } else
        setHighlightedWindow(findFirstWindow());

    int screens = effects->numScreens();
    for (int screen = 0; screen < screens; screen++) {
        EffectWindowList windows;
        windows = windowlists[screen];

        // Don't rearrange if the grid is the same size as what it was before to prevent
        // windows moving to a better spot if one was filtered out.
        if (m_layoutMode == LayoutRegularGrid &&
                m_gridSizes[screen].columns &&
                m_gridSizes[screen].rows &&
                windows.size() < m_gridSizes[screen].columns * m_gridSizes[screen].rows &&
                windows.size() > (m_gridSizes[screen].columns - 1) * m_gridSizes[screen].rows &&
                windows.size() > m_gridSizes[screen].columns *(m_gridSizes[screen].rows - 1))
            continue;

        // No point continuing if there is no windows to process
        if (!windows.count())
            continue;

        calculateWindowTransformations(windows, screen, m_motionManager);
    }

    // Resize text frames if required
    QFontMetrics* metrics = nullptr; // All fonts are the same
    foreach (EffectWindow * w, m_motionManager.managedWindows()) {
        DataHash::iterator winData = m_windowData.find(w);
        if (winData == m_windowData.end())
            continue;

        if (!metrics)
            metrics = new QFontMetrics(winData->textFrame->font());

        QRect geom = m_motionManager.targetGeometry(w).toRect();
        QString string = metrics->elidedText(w->caption(), Qt::ElideRight, geom.width() * 0.9);
        if (string != winData->textFrame->text())
            winData->textFrame->setText(string);
    }
    delete metrics;
}

void PresentWindowsEffect::calculateWindowTransformations(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager, bool external)
{
    if (m_layoutMode == LayoutRegularGrid)
        calculateWindowTransformationsClosest(windowlist, screen, motionManager);
    else if (m_layoutMode == LayoutFlexibleGrid)
        calculateWindowTransformationsKompose(windowlist, screen, motionManager);
    else
        calculateWindowTransformationsNatural(windowlist, screen, motionManager);

    // If called externally we don't need to remember this data
    if (external)
        m_windowData.clear();
}

static inline int distance(QPoint &pos1, QPoint &pos2)
{
    const int xdiff = pos1.x() - pos2.x();
    const int ydiff = pos1.y() - pos2.y();
    return int(sqrt(float(xdiff*xdiff + ydiff*ydiff)));
}

void PresentWindowsEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager)
{
    // This layout mode requires at least one window visible
    if (windowlist.count() == 0)
        return;

    QRect area = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
    if (m_showPanel)   // reserve space for the panel
        area = effects->clientArea(MaximizeArea, screen, effects->currentDesktop());
    int columns = int(ceil(sqrt(double(windowlist.count()))));
    int rows = int(ceil(windowlist.count() / double(columns)));

    // Remember the size for later
    // If we are using this layout externally we don't need to remember m_gridSizes.
    if (m_gridSizes.size() != 0) {
        m_gridSizes[screen].columns = columns;
        m_gridSizes[screen].rows = rows;
    }

    // Assign slots
    int slotWidth = area.width() / columns;
    int slotHeight = area.height() / rows;
    QVector<EffectWindow*> takenSlots;
    takenSlots.resize(rows*columns);
    takenSlots.fill(0);

    // precalculate all slot centers
    QVector<QPoint> slotCenters;
    slotCenters.resize(rows*columns);
    for (int x = 0; x < columns; ++x)
        for (int y = 0; y < rows; ++y) {
            slotCenters[x + y*columns] = QPoint(area.x() + slotWidth * x + slotWidth / 2,
                                                area.y() + slotHeight * y + slotHeight / 2);
        }

    // Assign each window to the closest available slot
    EffectWindowList tmpList = windowlist; // use a QLinkedList copy instead?
    QPoint otherPos;
    while (!tmpList.isEmpty()) {
        EffectWindow *w = tmpList.first();
        int slotCandidate = -1, slotCandidateDistance = INT_MAX;
        QPoint pos = w->geometry().center();

        for (int i = 0; i < columns*rows; ++i) { // all slots
            const int dist = distance(pos, slotCenters[i]);
            if (dist < slotCandidateDistance) { // window is interested in this slot
                EffectWindow *occupier = takenSlots[i];
                Q_ASSERT(occupier != w);
                if (!occupier || dist < distance((otherPos = occupier->geometry().center()), slotCenters[i])) {
                    // either nobody lives here, or we're better - takeover the slot if it's our best
                    slotCandidate = i;
                    slotCandidateDistance = dist;
                }
            }
        }
        Q_ASSERT(slotCandidate != -1);
        if (takenSlots[slotCandidate])
            tmpList << takenSlots[slotCandidate]; // occupier needs a new home now :p
        tmpList.removeAll(w);
        takenSlots[slotCandidate] = w; // ...and we rumble in =)
    }

    for (int slot = 0; slot < columns*rows; ++slot) {
        EffectWindow *w = takenSlots[slot];
        if (!w) // some slots might be empty
            continue;

        // Work out where the slot is
        QRect target(
            area.x() + (slot % columns) * slotWidth,
            area.y() + (slot / columns) * slotHeight,
            slotWidth, slotHeight);
        target.adjust(10, 10, -10, -10);   // Borders

        double scale;
        if (target.width() / double(w->width()) < target.height() / double(w->height())) {
            // Center vertically
            scale = target.width() / double(w->width());
            target.moveTop(target.top() + (target.height() - int(w->height() * scale)) / 2);
            target.setHeight(int(w->height() * scale));
        } else {
            // Center horizontally
            scale = target.height() / double(w->height());
            target.moveLeft(target.left() + (target.width() - int(w->width() * scale)) / 2);
            target.setWidth(int(w->width() * scale));
        }
        // Don't scale the windows too much
        if (scale > 2.0 || (scale > 1.0 && (w->width() > 300 || w->height() > 300))) {
            scale = (w->width() > 300 || w->height() > 300) ? 1.0 : 2.0;
            target = QRect(
                         target.center().x() - int(w->width() * scale) / 2,
                         target.center().y() - int(w->height() * scale) / 2,
                         scale * w->width(), scale * w->height());
        }
        motionManager.moveWindow(w, target);
    }
}

void PresentWindowsEffect::calculateWindowTransformationsKompose(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager)
{
    // This layout mode requires at least one window visible
    if (windowlist.count() == 0)
        return;

    QRect availRect = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
    if (m_showPanel)   // reserve space for the panel
        availRect = effects->clientArea(MaximizeArea, screen, effects->currentDesktop());
    std::sort(windowlist.begin(), windowlist.end());   // The location of the windows should not depend on the stacking order

    // Following code is taken from Kompose 0.5.4, src/komposelayout.cpp
    int spacing = 10;
    int rows, columns;
    double parentRatio = availRect.width() / (double)availRect.height();
    // Use more columns than rows when parent's width > parent's height
    if (parentRatio > 1) {
        columns = (int)ceil(sqrt((double)windowlist.count()));
        rows = (int)ceil((double)windowlist.count() / (double)columns);
    } else {
        rows = (int)ceil(sqrt((double)windowlist.count()));
        columns = (int)ceil((double)windowlist.count() / (double)rows);
    }
    //qCDebug(KWINEFFECTS) << "Using " << rows << " rows & " << columns << " columns for " << windowlist.count() << " clients";

    // Calculate width & height
    int w = (availRect.width() - (columns + 1) * spacing) / columns;
    int h = (availRect.height() - (rows + 1) * spacing) / rows;

    EffectWindowList::iterator it(windowlist.begin());
    QList<QRect> geometryRects;
    QList<int> maxRowHeights;
    // Process rows
    for (int i = 0; i < rows; ++i) {
        int xOffsetFromLastCol = 0;
        int maxHeightInRow = 0;
        // Process columns
        for (int j = 0; j < columns; ++j) {
            EffectWindow* window;

            // Check for end of List
            if (it == windowlist.end())
                break;
            window = *it;

            // Calculate width and height of widget
            double ratio = aspectRatio(window);

            int widgetw = 100;
            int widgeth = 100;
            int usableW = w;
            int usableH = h;

            // use width of two boxes if there is no right neighbour
            if (window == windowlist.last() && j != columns - 1) {
                usableW = 2 * w;
            }
            ++it; // We need access to the neighbour in the following
            // expand if right neighbour has ratio < 1
            if (j != columns - 1 && it != windowlist.end() && aspectRatio(*it) < 1) {
                int addW = w - widthForHeight(*it, h);
                if (addW > 0) {
                    usableW = w + addW;
                }
            }

            if (ratio == -1) {
                widgetw = w;
                widgeth = h;
            } else {
                double widthByHeight = widthForHeight(window, usableH);
                double heightByWidth = heightForWidth(window, usableW);
                if ((ratio >= 1.0 && heightByWidth <= usableH) ||
                        (ratio < 1.0 && widthByHeight > usableW)) {
                    widgetw = usableW;
                    widgeth = (int)heightByWidth;
                } else if ((ratio < 1.0 && widthByHeight <= usableW) ||
                          (ratio >= 1.0 && heightByWidth > usableH)) {
                    widgeth = usableH;
                    widgetw = (int)widthByHeight;
                }
                // Don't upscale large-ish windows
                if (widgetw > window->width() && (window->width() > 300 || window->height() > 300)) {
                    widgetw = window->width();
                    widgeth = window->height();
                }
            }

            // Set the Widget's size

            int alignmentXoffset = 0;
            int alignmentYoffset = 0;
            if (i == 0 && h > widgeth)
                alignmentYoffset = h - widgeth;
            if (j == 0 && w > widgetw)
                alignmentXoffset = w - widgetw;
            QRect geom(availRect.x() + j *(w + spacing) + spacing + alignmentXoffset + xOffsetFromLastCol,
                       availRect.y() + i *(h + spacing) + spacing + alignmentYoffset,
                       widgetw, widgeth);
            geometryRects.append(geom);

            // Set the x offset for the next column
            if (alignmentXoffset == 0)
                xOffsetFromLastCol += widgetw - w;
            if (maxHeightInRow < widgeth)
                maxHeightInRow = widgeth;
        }
        maxRowHeights.append(maxHeightInRow);
    }

    int topOffset = 0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            int pos = i * columns + j;
            if (pos >= windowlist.count())
                break;

            EffectWindow* window = windowlist[pos];
            QRect target = geometryRects[pos];
            target.setY(target.y() + topOffset);
            // @Marrtin: any idea what this is good for?
//             DataHash::iterator winData = m_windowData.find(window);
//             if (winData != m_windowData.end())
//                 winData->slot = pos;
            motionManager.moveWindow(window, target);

            //qCDebug(KWINEFFECTS) << "Window '" << window->caption() << "' gets moved to (" <<
            //        mWindowData[window].area.left() << "; " << mWindowData[window].area.right() <<
            //        "), scale: " << mWindowData[window].scale << endl;
        }
        if (maxRowHeights[i] - h > 0)
            topOffset += maxRowHeights[i] - h;
    }
}

void PresentWindowsEffect::calculateWindowTransformationsNatural(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager)
{
    // If windows do not overlap they scale into nothingness, fix by resetting. To reproduce
    // just have a single window on a Xinerama screen or have two windows that do not touch.
    // TODO: Work out why this happens, is most likely a bug in the manager.
    foreach (EffectWindow * w, windowlist)
        if (motionManager.transformedGeometry(w) == w->geometry())
            motionManager.reset(w);

    if (windowlist.count() == 1) {
        // Just move the window to its original location to save time
        if (effects->clientArea(FullScreenArea, windowlist[0]).contains(windowlist[0]->geometry())) {
            motionManager.moveWindow(windowlist[0], windowlist[0]->geometry());
            return;
        }
    }

    // As we are using pseudo-random movement (See "slot") we need to make sure the list
    // is always sorted the same way no matter which window is currently active.
    std::sort(windowlist.begin(), windowlist.end());

    QRect area = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
    if (m_showPanel)   // reserve space for the panel
        area = effects->clientArea(MaximizeArea, screen, effects->currentDesktop());
    QRect bounds = area;
    int direction = 0;
    QHash<EffectWindow*, QRect> targets;
    QHash<EffectWindow*, int> directions;
    foreach (EffectWindow * w, windowlist) {
        bounds = bounds.united(w->geometry());
        targets[w] = w->geometry();
        // Reuse the unused "slot" as a preferred direction attribute. This is used when the window
        // is on the edge of the screen to try to use as much screen real estate as possible.
        directions[w] = direction;
        direction++;
        if (direction == 4)
            direction = 0;
    }

    // Iterate over all windows, if two overlap push them apart _slightly_ as we try to
    // brute-force the most optimal positions over many iterations.
    bool overlap;
    do {
        overlap = false;
        foreach (EffectWindow * w, windowlist) {
            QRect *target_w = &targets[w];
            foreach (EffectWindow * e, windowlist) {
                if (w == e)
                    continue;

                QRect *target_e = &targets[e];
                if (target_w->adjusted(-5, -5, 5, 5).intersects(target_e->adjusted(-5, -5, 5, 5))) {
                    overlap = true;

                    // Determine pushing direction
                    QPoint diff(target_e->center() - target_w->center());
                    // Prevent dividing by zero and non-movement
                    if (diff.x() == 0 && diff.y() == 0)
                        diff.setX(1);
                    // Try to keep screen aspect ratio
                    //if (bounds.height() / bounds.width() > area.height() / area.width())
                    //    diff.setY(diff.y() / 2);
                    //else
                    //    diff.setX(diff.x() / 2);
                    // Approximate a vector of between 10px and 20px in magnitude in the same direction
                    diff *= m_accuracy / double(diff.manhattanLength());
                    // Move both windows apart
                    target_w->translate(-diff);
                    target_e->translate(diff);

                    // Try to keep the bounding rect the same aspect as the screen so that more
                    // screen real estate is utilised. We do this by splitting the screen into nine
                    // equal sections, if the window center is in any of the corner sections pull the
                    // window towards the outer corner. If it is in any of the other edge sections
                    // alternate between each corner on that edge. We don't want to determine it
                    // randomly as it will not produce consistant locations when using the filter.
                    // Only move one window so we don't cause large amounts of unnecessary zooming
                    // in some situations. We need to do this even when expanding later just in case
                    // all windows are the same size.
                    // (We are using an old bounding rect for this, hopefully it doesn't matter)
                    int xSection = (target_w->x() - bounds.x()) / (bounds.width() / 3);
                    int ySection = (target_w->y() - bounds.y()) / (bounds.height() / 3);
                    diff = QPoint(0, 0);
                    if (xSection != 1 || ySection != 1) { // Remove this if you want the center to pull as well
                        if (xSection == 1)
                            xSection = (directions[w] / 2 ? 2 : 0);
                        if (ySection == 1)
                            ySection = (directions[w] % 2 ? 2 : 0);
                    }
                    if (xSection == 0 && ySection == 0)
                        diff = QPoint(bounds.topLeft() - target_w->center());
                    if (xSection == 2 && ySection == 0)
                        diff = QPoint(bounds.topRight() - target_w->center());
                    if (xSection == 2 && ySection == 2)
                        diff = QPoint(bounds.bottomRight() - target_w->center());
                    if (xSection == 0 && ySection == 2)
                        diff = QPoint(bounds.bottomLeft() - target_w->center());
                    if (diff.x() != 0 || diff.y() != 0) {
                        diff *= m_accuracy / double(diff.manhattanLength());
                        target_w->translate(diff);
                    }

                    // Update bounding rect
                    bounds = bounds.united(*target_w);
                    bounds = bounds.united(*target_e);
                }
            }
        }
    } while (overlap);

    // Work out scaling by getting the most top-left and most bottom-right window coords.
    // The 20's and 10's are so that the windows don't touch the edge of the screen.
    double scale;
    if (bounds == area)
        scale = 1.0; // Don't add borders to the screen
    else if (area.width() / double(bounds.width()) < area.height() / double(bounds.height()))
        scale = (area.width() - 20) / double(bounds.width());
    else
        scale = (area.height() - 20) / double(bounds.height());
    // Make bounding rect fill the screen size for later steps
    bounds = QRect(
                 (bounds.x() * scale - (area.width() - 20 - bounds.width() * scale) / 2 - 10) / scale,
                 (bounds.y() * scale - (area.height() - 20 - bounds.height() * scale) / 2 - 10) / scale,
                 area.width() / scale,
                 area.height() / scale
             );

    // Move all windows back onto the screen and set their scale
    QHash<EffectWindow*, QRect>::iterator target = targets.begin();
    while (target != targets.end()) {
        target->setRect((target->x() - bounds.x()) * scale + area.x(),
                        (target->y() - bounds.y()) * scale + area.y(),
                        target->width() * scale,
                        target->height() * scale
                        );
        ++target;
    }

    // Try to fill the gaps by enlarging windows if they have the space
    if (m_fillGaps) {
        // Don't expand onto or over the border
        QRegion borderRegion(area.adjusted(-200, -200, 200, 200));
        borderRegion ^= area.adjusted(10 / scale, 10 / scale, -10 / scale, -10 / scale);

        bool moved;
        do {
            moved = false;
            foreach (EffectWindow * w, windowlist) {
                QRect oldRect;
                QRect *target = &targets[w];
                // This may cause some slight distortion if the windows are enlarged a large amount
                int widthDiff = m_accuracy;
                int heightDiff = heightForWidth(w, target->width() + widthDiff) - target->height();
                int xDiff = widthDiff / 2;  // Also move a bit in the direction of the enlarge, allows the
                int yDiff = heightDiff / 2; // center windows to be enlarged if there is gaps on the side.

                // heightDiff (and yDiff) will be re-computed after each successful enlargement attempt
                // so that the error introduced in the window's aspect ratio is minimized

                // Attempt enlarging to the top-right
                oldRect = *target;
                target->setRect(target->x() + xDiff,
                                target->y() - yDiff - heightDiff,
                                target->width() + widthDiff,
                                target->height() + heightDiff
                                );
                if (isOverlappingAny(w, targets, borderRegion))
                    *target = oldRect;
                else {
                    moved = true;
                    heightDiff = heightForWidth(w, target->width() + widthDiff) - target->height();
                    yDiff = heightDiff / 2;
                }

                // Attempt enlarging to the bottom-right
                oldRect = *target;
                target->setRect(
                                 target->x() + xDiff,
                                 target->y() + yDiff,
                                 target->width() + widthDiff,
                                 target->height() + heightDiff
                             );
                if (isOverlappingAny(w, targets, borderRegion))
                    *target = oldRect;
                else {
                    moved = true;
                    heightDiff = heightForWidth(w, target->width() + widthDiff) - target->height();
                    yDiff = heightDiff / 2;
                }

                // Attempt enlarging to the bottom-left
                oldRect = *target;
                target->setRect(
                                 target->x() - xDiff - widthDiff,
                                 target->y() + yDiff,
                                 target->width() + widthDiff,
                                 target->height() + heightDiff
                             );
                if (isOverlappingAny(w, targets, borderRegion))
                    *target = oldRect;
                else {
                    moved = true;
                    heightDiff = heightForWidth(w, target->width() + widthDiff) - target->height();
                    yDiff = heightDiff / 2;
                }

                // Attempt enlarging to the top-left
                oldRect = *target;
                target->setRect(
                                 target->x() - xDiff - widthDiff,
                                 target->y() - yDiff - heightDiff,
                                 target->width() + widthDiff,
                                 target->height() + heightDiff
                             );
                if (isOverlappingAny(w, targets, borderRegion))
                    *target = oldRect;
                else
                    moved = true;
            }
        } while (moved);

        // The expanding code above can actually enlarge windows over 1.0/2.0 scale, we don't like this
        // We can't add this to the loop above as it would cause a never-ending loop so we have to make
        // do with the less-than-optimal space usage with using this method.
        foreach (EffectWindow * w, windowlist) {
            QRect *target = &targets[w];
            double scale = target->width() / double(w->width());
            if (scale > 2.0 || (scale > 1.0 && (w->width() > 300 || w->height() > 300))) {
                scale = (w->width() > 300 || w->height() > 300) ? 1.0 : 2.0;
                target->setRect(
                                 target->center().x() - int(w->width() * scale) / 2,
                                 target->center().y() - int(w->height() * scale) / 2,
                                 w->width() * scale,
                                 w->height() * scale);
            }
        }
    }

    // Notify the motion manager of the targets
    foreach (EffectWindow * w, windowlist)
        motionManager.moveWindow(w, targets.value(w));
}

bool PresentWindowsEffect::isOverlappingAny(EffectWindow *w, const QHash<EffectWindow*, QRect> &targets, const QRegion &border)
{
    QHash<EffectWindow*, QRect>::const_iterator winTarget = targets.find(w);
    if (winTarget == targets.constEnd())
        return false;
    if (border.intersects(*winTarget))
        return true;

    // Is there a better way to do this?
    QHash<EffectWindow*, QRect>::const_iterator target;
    for (target = targets.constBegin(); target != targets.constEnd(); ++target) {
        if (target == winTarget)
            continue;
        if (winTarget->adjusted(-5, -5, 5, 5).intersects(target->adjusted(-5, -5, 5, 5)))
            return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Activation

void PresentWindowsEffect::setActive(bool active)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;
    if (m_activated == active)
        return;

    m_activated = active;
    if (m_activated) {
        effects->setShowingDesktop(false);
        m_needInitialSelection = true;
        m_closeButtonCorner = (Qt::Corner)effects->kwinOption(KWin::CloseButtonCorner).toInt();
        m_decalOpacity = 0.0;
        m_highlightedWindow = nullptr;
        m_windowFilter.clear();

        if (!(m_doNotCloseWindows || m_closeView)) {
            m_closeView = new CloseWindowView();
            connect(m_closeView, &EffectQuickView::repaintNeeded, this, []() {
                effects->addRepaintFull();
            });
            connect(m_closeView, &CloseWindowView::requestClose, this, &PresentWindowsEffect::closeWindow);
        }

        // Add every single window to m_windowData (Just calling [w] creates it)
        foreach (EffectWindow * w, effects->stackingOrder()) {
            DataHash::iterator winData;
            if ((winData = m_windowData.find(w)) != m_windowData.end()) {
                winData->visible = isVisibleWindow(w);
                continue; // Happens if we reactivate before the ending animation finishes
            }

            winData = m_windowData.insert(w, WindowData());
            winData->visible = isVisibleWindow(w);
            winData->deleted = false;
            winData->referenced = false;
            winData->opacity = 0.0;
            if (w->isOnCurrentDesktop() && !w->isMinimized())
                winData->opacity = 1.0;

            winData->highlight = 1.0;
            winData->textFrame = effects->effectFrame(EffectFrameUnstyled, false);

            QFont font;
            font.setBold(true);
            font.setPointSize(12);

            winData->textFrame->setFont(font);
            winData->iconFrame = effects->effectFrame(EffectFrameUnstyled, false);
            winData->iconFrame->setAlignment(Qt::AlignRight | Qt::AlignBottom);
            winData->iconFrame->setIcon(w->icon());
            winData->iconFrame->setIconSize(QSize(32, 32));
        }

        // Filter out special windows such as panels and taskbars
        foreach (EffectWindow * w, effects->stackingOrder()) {
            if (isSelectableWindow(w)) {
                m_motionManager.manage(w);
            }
        }

        if (m_motionManager.managedWindows().isEmpty() ||
                ((m_motionManager.managedWindows().count() == 1) && m_motionManager.managedWindows().first()->isOnCurrentDesktop() &&
                 (m_ignoreMinimized || !m_motionManager.managedWindows().first()->isMinimized()))) {
            // No point triggering if there is nothing to do
            m_activated = false;

            DataHash::iterator i = m_windowData.begin();
            while (i != m_windowData.end()) {
                delete i.value().textFrame;
                delete i.value().iconFrame;
                ++i;
            }
            m_windowData.clear();

            m_motionManager.unmanageAll();
            return;
        }

        // Create temporary input window to catch mouse events
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        m_hasKeyboardGrab = effects->grabKeyboard(this);
        effects->setActiveFullScreenEffect(this);

        reCreateGrids();
        rearrangeWindows();
        setHighlightedWindow(effects->activeWindow());

        foreach (EffectWindow * w, effects->stackingOrder()) {
            w->setData(WindowForceBlurRole, QVariant(true));
            w->setData(WindowForceBackgroundContrastRole, QVariant(true));
        }
    } else {
        m_needInitialSelection = false;
        if (m_highlightedWindow)
            effects->setElevatedWindow(m_highlightedWindow, false);

        // Fade in/out all windows
        EffectWindow *activeWindow = effects->activeWindow();
        int desktop = effects->currentDesktop();
        if (activeWindow && !activeWindow->isOnAllDesktops())
            desktop = activeWindow->desktop();
        foreach (EffectWindow * w, effects->stackingOrder()) {
            DataHash::iterator winData = m_windowData.find(w);
            if (winData != m_windowData.end())
                winData->visible = (w->isOnDesktop(desktop) || w->isOnAllDesktops()) &&
                                    !w->isMinimized();
        }
        if (m_closeView)
            m_closeView->hide();

        // Move all windows back to their original position
        foreach (EffectWindow * w, m_motionManager.managedWindows())
        m_motionManager.moveWindow(w, w->geometry());
        if (m_filterFrame) {
            m_filterFrame->free();
        }
        m_windowFilter.clear();
        m_selectedWindows.clear();

        effects->stopMouseInterception(this);
        if (m_hasKeyboardGrab)
            effects->ungrabKeyboard();
        m_hasKeyboardGrab = false;

        // destroy atom on manager window
        if (m_managerWindow) {
            if (m_mode == ModeSelectedDesktop && m_atomDesktop != XCB_ATOM_NONE)
                m_managerWindow->deleteProperty(m_atomDesktop);
            else if (m_mode == ModeWindowGroup && m_atomWindows != XCB_ATOM_NONE)
                m_managerWindow->deleteProperty(m_atomWindows);
            m_managerWindow = nullptr;
        }
    }

    effects->addRepaintFull(); // Trigger the first repaint
}

//-----------------------------------------------------------------------------
// Filter box

void PresentWindowsEffect::updateFilterFrame()
{
    QRect area = effects->clientArea(ScreenArea, effects->activeScreen(), effects->currentDesktop());
    if (!m_filterFrame) {
        m_filterFrame = effects->effectFrame(EffectFrameStyled, false);
        QFont font;
        font.setPointSize(font.pointSize() * 2);
        font.setBold(true);
        m_filterFrame->setFont(font);
    }
    m_filterFrame->setPosition(QPoint(area.x() + area.width() / 2, area.y() + area.height() / 2));
    m_filterFrame->setText(i18n("Filter:\n%1", m_windowFilter));
}

//-----------------------------------------------------------------------------
// Helper functions

bool PresentWindowsEffect::isSelectableWindow(EffectWindow *w)
{
    if (!w->isOnCurrentActivity())
        return false;
    if (w->isSpecialWindow() || w->isUtility())
        return false;
    if (w->isDeleted())
        return false;
    if (!w->acceptsFocus())
        return false;
    if (w->isSkipSwitcher())
        return false;
    if (m_ignoreMinimized && w->isMinimized())
        return false;

    switch(m_mode) {
    default:
    case ModeAllDesktops:
        return true;
    case ModeCurrentDesktop:
        return w->isOnCurrentDesktop();
    case ModeSelectedDesktop:
        return w->isOnDesktop(m_desktop);
    case ModeWindowGroup:
        return m_selectedWindows.contains(w);
    case ModeWindowClass:
        return m_class == w->windowClass();
    }
}

bool PresentWindowsEffect::isVisibleWindow(EffectWindow *w)
{
    if (w->isDesktop())
        return true;
    return isSelectableWindow(w);
}

void PresentWindowsEffect::setHighlightedWindow(EffectWindow *w)
{
    if (w == m_highlightedWindow || (w != nullptr && !m_motionManager.isManaging(w)))
        return;

    if (m_closeView)
        m_closeView->hide();
    if (m_highlightedWindow) {
        effects->setElevatedWindow(m_highlightedWindow, false);
        m_highlightedWindow->addRepaintFull(); // Trigger the first repaint
    }
    m_highlightedWindow = w;
    if (m_highlightedWindow) {
        effects->setElevatedWindow(m_highlightedWindow, true);
        m_highlightedWindow->addRepaintFull(); // Trigger the first repaint
    }

    updateCloseWindow();
}

void PresentWindowsEffect::updateCloseWindow()
{
    if (!m_closeView || m_doNotCloseWindows)
        return;

    if (!m_activated || !m_highlightedWindow || m_highlightedWindow->isDesktop()) {
        m_closeView->hide();
        return;
    }
    if (m_closeView->isVisible())
        return;

    const QRectF rect(m_motionManager.targetGeometry(m_highlightedWindow));
    if (2*m_closeView->geometry().width() > rect.width() && 2*m_closeView->geometry().height() > rect.height()) {
        // not for tiny windows (eg. with many windows) - they might become unselectable
        m_closeView->hide();
        return;
    }

    QRect cvr(QPoint(0,0), m_closeView->size());
    switch (m_closeButtonCorner)
    {
    case Qt::TopLeftCorner:
    default:
        cvr.moveTopLeft(rect.topLeft().toPoint()); break;
    case Qt::TopRightCorner:
        cvr.moveTopRight(rect.topRight().toPoint()); break;
    case Qt::BottomLeftCorner:
        cvr.moveBottomLeft(rect.bottomLeft().toPoint()); break;
    case Qt::BottomRightCorner:
        cvr.moveBottomRight(rect.bottomRight().toPoint()); break;
    }

    m_closeView->setGeometry(cvr);

    if (rect.contains(effects->cursorPos())) {
        m_closeView->show();
        m_closeView->disarm();
    }
    else
        m_closeView->hide();
}

void PresentWindowsEffect::closeWindow()
{
    if (m_highlightedWindow)
        m_highlightedWindow->closeWindow();
}

EffectWindow* PresentWindowsEffect::relativeWindow(EffectWindow *w, int xdiff, int ydiff, bool wrap) const
{
    if (!w)
        return m_motionManager.managedWindows().first();

    // TODO: Is it possible to select hidden windows?
    EffectWindow* next;
    QRect area = effects->clientArea(FullArea, 0, effects->currentDesktop());
    QRect detectRect;

    // Detect across the width of the desktop
    if (xdiff != 0) {
        if (xdiff > 0) {
            // Detect right
            for (int i = 0; i < xdiff; i++) {
                QRectF wArea = m_motionManager.transformedGeometry(w);
                detectRect = QRect(0, wArea.y(), area.width(), wArea.height());
                next = nullptr;

                foreach (EffectWindow * e, m_motionManager.managedWindows()) {
                    DataHash::const_iterator winData = m_windowData.find(e);
                    if (winData == m_windowData.end() || !winData->visible)
                        continue;

                    QRectF eArea = m_motionManager.transformedGeometry(e);
                    if (eArea.intersects(detectRect) &&
                            eArea.x() > wArea.x()) {
                        if (next == nullptr)
                            next = e;
                        else {
                            QRectF nArea = m_motionManager.transformedGeometry(next);
                            if (eArea.x() < nArea.x())
                                next = e;
                        }
                    }
                }
                if (next == nullptr) {
                    if (wrap)   // We are at the right-most window, now get the left-most one to wrap
                        return relativeWindow(w, -1000, 0, false);
                    break; // No more windows to the right
                }
                w = next;
            }
            return w;
        } else {
            // Detect left
            for (int i = 0; i < -xdiff; i++) {
                QRectF wArea = m_motionManager.transformedGeometry(w);
                detectRect = QRect(0, wArea.y(), area.width(), wArea.height());
                next = nullptr;

                foreach (EffectWindow * e, m_motionManager.managedWindows()) {
                    DataHash::const_iterator winData = m_windowData.find(e);
                    if (winData == m_windowData.end() || !winData->visible)
                        continue;

                    QRectF eArea = m_motionManager.transformedGeometry(e);
                    if (eArea.intersects(detectRect) &&
                            eArea.x() + eArea.width() < wArea.x() + wArea.width()) {
                        if (next == nullptr)
                            next = e;
                        else {
                            QRectF nArea = m_motionManager.transformedGeometry(next);
                            if (eArea.x() + eArea.width() > nArea.x() + nArea.width())
                                next = e;
                        }
                    }
                }
                if (next == nullptr) {
                    if (wrap)   // We are at the left-most window, now get the right-most one to wrap
                        return relativeWindow(w, 1000, 0, false);
                    break; // No more windows to the left
                }
                w = next;
            }
            return w;
        }
    }

    // Detect across the height of the desktop
    if (ydiff != 0) {
        if (ydiff > 0) {
            // Detect down
            for (int i = 0; i < ydiff; i++) {
                QRectF wArea = m_motionManager.transformedGeometry(w);
                detectRect = QRect(wArea.x(), 0, wArea.width(), area.height());
                next = nullptr;

                foreach (EffectWindow * e, m_motionManager.managedWindows()) {
                    DataHash::const_iterator winData = m_windowData.find(e);
                    if (winData == m_windowData.end() || !winData->visible)
                        continue;

                    QRectF eArea = m_motionManager.transformedGeometry(e);
                    if (eArea.intersects(detectRect) &&
                            eArea.y() > wArea.y()) {
                        if (next == nullptr)
                            next = e;
                        else {
                            QRectF nArea = m_motionManager.transformedGeometry(next);
                            if (eArea.y() < nArea.y())
                                next = e;
                        }
                    }
                }
                if (next == nullptr) {
                    if (wrap)   // We are at the bottom-most window, now get the top-most one to wrap
                        return relativeWindow(w, 0, -1000, false);
                    break; // No more windows to the bottom
                }
                w = next;
            }
            return w;
        } else {
            // Detect up
            for (int i = 0; i < -ydiff; i++) {
                QRectF wArea = m_motionManager.transformedGeometry(w);
                detectRect = QRect(wArea.x(), 0, wArea.width(), area.height());
                next = nullptr;

                foreach (EffectWindow * e, m_motionManager.managedWindows()) {
                    DataHash::const_iterator winData = m_windowData.find(e);
                    if (winData == m_windowData.end() || !winData->visible)
                        continue;

                    QRectF eArea = m_motionManager.transformedGeometry(e);
                    if (eArea.intersects(detectRect) &&
                            eArea.y() + eArea.height() < wArea.y() + wArea.height()) {
                        if (next == nullptr)
                            next = e;
                        else {
                            QRectF nArea = m_motionManager.transformedGeometry(next);
                            if (eArea.y() + eArea.height() > nArea.y() + nArea.height())
                                next = e;
                        }
                    }
                }
                if (next == nullptr) {
                    if (wrap)   // We are at the top-most window, now get the bottom-most one to wrap
                        return relativeWindow(w, 0, 1000, false);
                    break; // No more windows to the top
                }
                w = next;
            }
            return w;
        }
    }

    abort(); // Should never get here
}

EffectWindow* PresentWindowsEffect::findFirstWindow() const
{
    EffectWindow *topLeft = nullptr;
    QRectF topLeftGeometry;

    foreach (EffectWindow * w, m_motionManager.managedWindows()) {
        DataHash::const_iterator winData = m_windowData.find(w);
        if (winData == m_windowData.end())
            continue;
        QRectF geometry = m_motionManager.transformedGeometry(w);
        if (winData->visible == false)
            continue; // Not visible
        if (winData->deleted)
            continue; // Window has been closed
        if (topLeft == nullptr) {
            topLeft = w;
            topLeftGeometry = geometry;
        } else if (geometry.x() < topLeftGeometry.x() || geometry.y() < topLeftGeometry.y()) {
            topLeft = w;
            topLeftGeometry = geometry;
        }
    }
    return topLeft;
}

void PresentWindowsEffect::globalShortcutChanged(QAction *action, const QKeySequence& seq)
{
    if (action->objectName() == QStringLiteral("Expose")) {
        shortcut.clear();
        shortcut.append(seq);
    } else if (action->objectName() == QStringLiteral("ExposeAll")) {
        shortcutAll.clear();
        shortcutAll.append(seq);
    } else if (action->objectName() == QStringLiteral("ExposeClass")) {
        shortcutClass.clear();
        shortcutClass.append(seq);
    }
}

bool PresentWindowsEffect::isActive() const
{
    return (m_activated || m_motionManager.managingWindows()) && !effects->isScreenLocked();
}

void PresentWindowsEffect::reCreateGrids()
{
    m_gridSizes.clear();
    for (int i = 0; i < effects->numScreens(); ++i) {
        m_gridSizes.append(GridSize());
    }
    rearrangeWindows();
}

CloseWindowView::CloseWindowView(QObject *parent)
    : EffectQuickScene(parent)
{
    setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/presentwindows/main.qml"))));
    if (QQuickItem *item = rootItem()) {
        connect(item, SIGNAL(clicked()), this, SLOT(clicked()));
        setGeometry(QRect(QPoint(), QSize(item->implicitWidth(), item->implicitHeight())));
    }
    m_armTimer.restart();
}

void CloseWindowView::clicked()
{
    // 50ms until the window is elevated (seen!) and 300ms more to be "realized" by the user.
    if (m_armTimer.hasExpired(350)) {
        emit requestClose();
    }
}

void CloseWindowView::disarm()
{
    m_armTimer.restart();
}


} // namespace
