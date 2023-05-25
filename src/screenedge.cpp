/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    Since the functionality provided in this class has been moved from
    class Workspace, it is not clear who exactly has written the code.
    The list below contains the copyright holders of the class Workspace.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenedge.h"

#include <config-kwin.h>

#include "core/output.h"
#include "cursor.h"
#include "effects.h"
#include "gestures.h"
#include "main.h"
#include "utils/common.h"
#include "virtualdesktops.h"
#include <workspace.h>
#include <x11window.h>
// DBus generated
#if KWIN_BUILD_SCREENLOCKER
#include "screenlocker_interface.h"
#endif
// frameworks
#include <KConfigGroup>
// Qt
#include <QAbstractEventDispatcher>
#include <QAction>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QTextStream>
#include <QTimer>
#include <QWidget>

namespace KWin
{

// Mouse should not move more than this many pixels
static const int DISTANCE_RESET = 30;

// How large the touch target of the area recognizing touch gestures is
static const int TOUCH_TARGET = 3;

// How far the user needs to swipe before triggering an action.
static const int MINIMUM_DELTA = 44;

TouchCallback::TouchCallback(QAction *touchUpAction, TouchCallback::CallbackFunction progressCallback)
    : m_touchUpAction(touchUpAction)
    , m_progressCallback(progressCallback)
{
}

TouchCallback::~TouchCallback()
{
}

QAction *TouchCallback::touchUpAction() const
{
    return m_touchUpAction;
}

void TouchCallback::progressCallback(ElectricBorder border, const QPointF &deltaProgress, Output *output) const
{
    if (m_progressCallback) {
        m_progressCallback(border, deltaProgress, output);
    }
}

bool TouchCallback::hasProgressCallback() const
{
    return m_progressCallback != nullptr;
}

Edge::Edge(ScreenEdges *parent)
    : m_edges(parent)
    , m_border(ElectricNone)
    , m_action(ElectricActionNone)
    , m_reserved(0)
    , m_approaching(false)
    , m_lastApproachingFactor(0)
    , m_blocked(false)
    , m_pushBackBlocked(false)
    , m_client(nullptr)
    , m_output(nullptr)
    , m_gesture(std::make_unique<SwipeGesture>())
{
    m_gesture->setMinimumFingerCount(1);
    m_gesture->setMaximumFingerCount(1);
    connect(
        m_gesture.get(), &Gesture::triggered, this, [this]() {
            stopApproaching();
            if (m_client) {
                m_client->showOnScreenEdge();
                unreserve();
                return;
            }
            handleTouchAction();
            handleTouchCallback();
        },
        Qt::QueuedConnection);
    connect(m_gesture.get(), &SwipeGesture::started, this, &Edge::startApproaching);
    connect(m_gesture.get(), &SwipeGesture::cancelled, this, &Edge::stopApproaching);
    connect(m_gesture.get(), &SwipeGesture::cancelled, this, [this]() {
        if (!m_touchCallbacks.isEmpty() && m_touchCallbacks.constFirst().hasProgressCallback()) {
            handleTouchCallback();
        }
    });
    connect(m_gesture.get(), &SwipeGesture::progress, this, [this](qreal progress) {
        int factor = progress * 256.0f;
        if (m_lastApproachingFactor != factor) {
            m_lastApproachingFactor = factor;
            Q_EMIT approaching(border(), m_lastApproachingFactor / 256.0f, m_approachGeometry);
        }
    });
    connect(m_gesture.get(), &SwipeGesture::deltaProgress, this, [this](const QPointF &progressDelta) {
        if (!m_touchCallbacks.isEmpty()) {
            m_touchCallbacks.constFirst().progressCallback(border(), progressDelta, m_output);
        }
    });
    connect(this, &Edge::activatesForTouchGestureChanged, this, [this]() {
        if (isReserved()) {
            if (activatesForTouchGesture()) {
                m_edges->gestureRecognizer()->registerSwipeGesture(m_gesture.get());
            } else {
                m_edges->gestureRecognizer()->unregisterSwipeGesture(m_gesture.get());
            }
        }
    });
}

Edge::~Edge()
{
    stopApproaching();
}

void Edge::reserve()
{
    m_reserved++;
    if (m_reserved == 1) {
        // got activated
        activate();
    }
}

void Edge::reserve(QObject *object, const char *slot)
{
    connect(object, &QObject::destroyed, this, qOverload<QObject *>(&Edge::unreserve));
    m_callBacks.insert(object, QByteArray(slot));
    reserve();
}

void Edge::reserveTouchCallBack(QAction *action, TouchCallback::CallbackFunction callback)
{
    if (std::any_of(m_touchCallbacks.constBegin(), m_touchCallbacks.constEnd(), [action](const TouchCallback &c) {
            return c.touchUpAction() == action;
        })) {
        return;
    }
    reserveTouchCallBack(TouchCallback(action, callback));
}

void Edge::reserveTouchCallBack(const TouchCallback &callback)
{
    if (std::any_of(m_touchCallbacks.constBegin(), m_touchCallbacks.constEnd(), [callback](const TouchCallback &c) {
            return c.touchUpAction() == callback.touchUpAction();
        })) {
        return;
    }
    connect(callback.touchUpAction(), &QAction::destroyed, this, [this, callback]() {
        unreserveTouchCallBack(callback.touchUpAction());
    });
    m_touchCallbacks << callback;
    reserve();
}

void Edge::unreserveTouchCallBack(QAction *action)
{
    auto it = std::find_if(m_touchCallbacks.begin(), m_touchCallbacks.end(), [action](const TouchCallback &c) {
        return c.touchUpAction() == action;
    });
    if (it != m_touchCallbacks.end()) {
        m_touchCallbacks.erase(it);
        unreserve();
    }
}

void Edge::unreserve()
{
    m_reserved--;
    if (m_reserved == 0) {
        // got deactivated
        stopApproaching();
        deactivate();
    }
}
void Edge::unreserve(QObject *object)
{
    if (m_callBacks.remove(object) > 0) {
        disconnect(object, &QObject::destroyed, this, qOverload<QObject *>(&Edge::unreserve));
        unreserve();
    }
}

bool Edge::activatesForPointer() const
{
    if (m_client) {
        return true;
    }
    if (m_edges->isDesktopSwitching()) {
        return true;
    }
    if (m_edges->isDesktopSwitchingMovingClients()) {
        auto c = Workspace::self()->moveResizeWindow();
        if (c && !c->isInteractiveResize()) {
            return true;
        }
    }
    if (!m_callBacks.isEmpty()) {
        return true;
    }
    if (m_action != ElectricActionNone) {
        return true;
    }
    return false;
}

bool Edge::activatesForTouchGesture() const
{
    if (!isScreenEdge()) {
        return false;
    }
    if (m_blocked) {
        return false;
    }
    if (m_client) {
        return true;
    }
    if (m_touchAction != ElectricActionNone) {
        return true;
    }
    if (!m_touchCallbacks.isEmpty()) {
        return true;
    }
    return false;
}

bool Edge::triggersFor(const QPoint &cursorPos) const
{
    if (isBlocked()) {
        return false;
    }
    if (!activatesForPointer()) {
        return false;
    }
    if (!m_geometry.contains(cursorPos)) {
        return false;
    }
    if (isLeft() && cursorPos.x() != m_geometry.x()) {
        return false;
    }
    if (isRight() && cursorPos.x() != (m_geometry.x() + m_geometry.width() - 1)) {
        return false;
    }
    if (isTop() && cursorPos.y() != m_geometry.y()) {
        return false;
    }
    if (isBottom() && cursorPos.y() != (m_geometry.y() + m_geometry.height() - 1)) {
        return false;
    }
    return true;
}

bool Edge::check(const QPoint &cursorPos, const QDateTime &triggerTime, bool forceNoPushBack)
{
    if (!triggersFor(cursorPos)) {
        return false;
    }
    if (m_lastTrigger.isValid() && // still in cooldown
        m_lastTrigger.msecsTo(triggerTime) < edges()->reActivationThreshold() - edges()->timeThreshold()) {
        // Reset the time, so the user has to actually keep the mouse still for this long to retrigger
        m_lastTrigger = triggerTime;
        return false;
    }
    // no pushback so we have to activate at once
    bool directActivate = forceNoPushBack || edges()->cursorPushBackDistance().isNull() || m_client;
    if (directActivate || canActivate(cursorPos, triggerTime)) {
        markAsTriggered(cursorPos, triggerTime);
        handle(cursorPos);
        return true;
    } else {
        pushCursorBack(cursorPos);
        m_triggeredPoint = cursorPos;
    }
    return false;
}

void Edge::markAsTriggered(const QPoint &cursorPos, const QDateTime &triggerTime)
{
    m_lastTrigger = triggerTime;
    m_lastReset = QDateTime(); // invalidate
    m_triggeredPoint = cursorPos;
}

bool Edge::canActivate(const QPoint &cursorPos, const QDateTime &triggerTime)
{
    // we check whether either the timer has explicitly been invalidated (successful trigger) or is
    // bigger than the reactivation threshold (activation "aborted", usually due to moving away the cursor
    // from the corner after successful activation)
    // either condition means that "this is the first event in a new attempt"
    if (!m_lastReset.isValid() || m_lastReset.msecsTo(triggerTime) > edges()->reActivationThreshold()) {
        m_lastReset = triggerTime;
        return false;
    }
    if (m_lastTrigger.isValid() && m_lastTrigger.msecsTo(triggerTime) < edges()->reActivationThreshold() - edges()->timeThreshold()) {
        return false;
    }
    if (m_lastReset.msecsTo(triggerTime) < edges()->timeThreshold()) {
        return false;
    }
    // does the check on position make any sense at all?
    if ((cursorPos - m_triggeredPoint).manhattanLength() > DISTANCE_RESET) {
        return false;
    }
    return true;
}

void Edge::handle(const QPoint &cursorPos)
{
    Window *movingClient = Workspace::self()->moveResizeWindow();
    if ((edges()->isDesktopSwitchingMovingClients() && movingClient && !movingClient->isInteractiveResize()) || (edges()->isDesktopSwitching() && isScreenEdge())) {
        // always switch desktops in case:
        // moving a Client and option for switch on client move is enabled
        // or switch on screen edge is enabled
        switchDesktop(cursorPos);
        return;
    }
    if (movingClient) {
        // if we are moving a window we don't want to trigger the actions. This just results in
        // problems, e.g. Desktop Grid activated or screen locker activated which just cannot
        // work as we hold a grab.
        return;
    }

    if (m_client) {
        pushCursorBack(cursorPos);
        m_client->showOnScreenEdge();
        unreserve();
        return;
    }

    if (handlePointerAction() || handleByCallback()) {
        pushCursorBack(cursorPos);
        return;
    }
    if (edges()->isDesktopSwitching() && isCorner()) {
        // try again desktop switching for the corner
        switchDesktop(cursorPos);
    }
}

bool Edge::handleAction(ElectricBorderAction action)
{
    switch (action) {
    case ElectricActionShowDesktop: {
        Workspace::self()->setShowingDesktop(!Workspace::self()->showingDesktop());
        return true;
    }
    case ElectricActionLockScreen: { // Lock the screen
#if KWIN_BUILD_SCREENLOCKER
        OrgFreedesktopScreenSaverInterface interface(QStringLiteral("org.freedesktop.ScreenSaver"),
                                                     QStringLiteral("/ScreenSaver"),
                                                     QDBusConnection::sessionBus());
        if (interface.isValid()) {
            interface.Lock();
        }
        return true;
#else
        return false;
#endif
    }
    case ElectricActionKRunner: { // open krunner
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.krunner"),
                                           QStringLiteral("/App"),
                                           QStringLiteral("org.kde.krunner.App"),
                                           QStringLiteral("display")));
        return true;
    }
    case ElectricActionActivityManager: { // open activity manager
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                           QStringLiteral("/PlasmaShell"),
                                           QStringLiteral("org.kde.PlasmaShell"),
                                           QStringLiteral("toggleActivityManager")));
        return true;
    }
    case ElectricActionApplicationLauncher: {
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                           QStringLiteral("/PlasmaShell"),
                                           QStringLiteral("org.kde.PlasmaShell"),
                                           QStringLiteral("activateLauncherMenu")));
        return true;
    }
    default:
        return false;
    }
}

bool Edge::handleByCallback()
{
    if (m_callBacks.isEmpty()) {
        return false;
    }
    for (auto it = m_callBacks.begin(); it != m_callBacks.end(); ++it) {
        bool retVal = false;
        QMetaObject::invokeMethod(it.key(), it.value().constData(), Q_RETURN_ARG(bool, retVal), Q_ARG(ElectricBorder, m_border));
        if (retVal) {
            return true;
        }
    }
    return false;
}

void Edge::handleTouchCallback()
{
    if (!m_touchCallbacks.isEmpty()) {
        m_touchCallbacks.constFirst().touchUpAction()->trigger();
    }
}

void Edge::switchDesktop(const QPoint &cursorPos)
{
    QPoint pos(cursorPos);
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    VirtualDesktop *oldDesktop = vds->currentDesktop();
    VirtualDesktop *desktop = oldDesktop;
    const int OFFSET = 2;
    if (isLeft()) {
        const VirtualDesktop *interimDesktop = desktop;
        desktop = vds->toLeft(desktop, vds->isNavigationWrappingAround());
        if (desktop != interimDesktop) {
            pos.setX(workspace()->geometry().width() - 1 - OFFSET);
        }
    } else if (isRight()) {
        const VirtualDesktop *interimDesktop = desktop;
        desktop = vds->toRight(desktop, vds->isNavigationWrappingAround());
        if (desktop != interimDesktop) {
            pos.setX(OFFSET);
        }
    }
    if (isTop()) {
        const VirtualDesktop *interimDesktop = desktop;
        desktop = vds->above(desktop, vds->isNavigationWrappingAround());
        if (desktop != interimDesktop) {
            pos.setY(workspace()->geometry().height() - 1 - OFFSET);
        }
    } else if (isBottom()) {
        const VirtualDesktop *interimDesktop = desktop;
        desktop = vds->below(desktop, vds->isNavigationWrappingAround());
        if (desktop != interimDesktop) {
            pos.setY(OFFSET);
        }
    }
    if (Window *c = Workspace::self()->moveResizeWindow()) {
        const QVector<VirtualDesktop *> desktops{desktop};
        if (c->rules()->checkDesktops(desktops) != desktops) {
            // user attempts to move a client to another desktop where it is ruleforced to not be
            return;
        }
    }
    vds->setCurrent(desktop);
    if (vds->currentDesktop() != oldDesktop) {
        m_pushBackBlocked = true;
        Cursors::self()->mouse()->setPos(pos);
        QMetaObject::Connection *me = new QMetaObject::Connection();
        *me = QObject::connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, [this, me]() {
            QObject::disconnect(*me);
            delete me;
            m_pushBackBlocked = false;
        });
    }
}

void Edge::pushCursorBack(const QPoint &cursorPos)
{
    if (m_pushBackBlocked) {
        return;
    }
    int x = cursorPos.x();
    int y = cursorPos.y();
    const QSize &distance = edges()->cursorPushBackDistance();
    if (isLeft()) {
        x += distance.width();
    }
    if (isRight()) {
        x -= distance.width();
    }
    if (isTop()) {
        y += distance.height();
    }
    if (isBottom()) {
        y -= distance.height();
    }
    Cursors::self()->mouse()->setPos(x, y);
}

void Edge::setGeometry(const QRect &geometry)
{
    if (m_geometry == geometry) {
        return;
    }
    m_geometry = geometry;
    int x = m_geometry.x();
    int y = m_geometry.y();
    int width = m_geometry.width();
    int height = m_geometry.height();
    const int offset = m_edges->cornerOffset();
    if (isCorner()) {
        if (isRight()) {
            x = x + width - offset;
        }
        if (isBottom()) {
            y = y + height - offset;
        }
        width = offset;
        height = offset;
    } else {
        if (isLeft()) {
            y += offset;
            width = offset;
            height = height - offset * 2;
        } else if (isRight()) {
            x = x + width - offset;
            y += offset;
            width = offset;
            height = height - offset * 2;
        } else if (isTop()) {
            x += offset;
            width = width - offset * 2;
            height = offset;
        } else if (isBottom()) {
            x += offset;
            y = y + height - offset;
            width = width - offset * 2;
            height = offset;
        }
    }
    m_approachGeometry = QRect(x, y, width, height);
    doGeometryUpdate();

    if (isScreenEdge()) {
        const Output *output = workspace()->outputAt(m_geometry.center());
        m_gesture->setStartGeometry(m_geometry);
        m_gesture->setMinimumDelta(QPointF(MINIMUM_DELTA, MINIMUM_DELTA) / output->scale());
    }
}

void Edge::checkBlocking()
{
    Window *client = Workspace::self()->activeWindow();
    const bool newValue = !m_edges->remainActiveOnFullscreen() && client && client->isFullScreen() && client->frameGeometry().contains(m_geometry.center()) && !(effects && effects->hasActiveFullScreenEffect());
    if (newValue == m_blocked) {
        return;
    }
    const bool wasTouch = activatesForTouchGesture();
    m_blocked = newValue;
    if (m_blocked && m_approaching) {
        stopApproaching();
    }
    if (wasTouch != activatesForTouchGesture()) {
        Q_EMIT activatesForTouchGestureChanged();
    }
    doUpdateBlocking();
}

void Edge::doUpdateBlocking()
{
}

void Edge::doGeometryUpdate()
{
}

void Edge::activate()
{
    if (activatesForTouchGesture()) {
        m_edges->gestureRecognizer()->registerSwipeGesture(m_gesture.get());
    }
    doActivate();
}

void Edge::doActivate()
{
}

void Edge::deactivate()
{
    m_edges->gestureRecognizer()->unregisterSwipeGesture(m_gesture.get());
    doDeactivate();
}

void Edge::doDeactivate()
{
}

void Edge::startApproaching()
{
    if (m_approaching) {
        return;
    }
    m_approaching = true;
    doStartApproaching();
    m_lastApproachingFactor = 0;
    Q_EMIT approaching(border(), 0.0, m_approachGeometry);
}

void Edge::doStartApproaching()
{
}

void Edge::stopApproaching()
{
    if (!m_approaching) {
        return;
    }
    m_approaching = false;
    doStopApproaching();
    m_lastApproachingFactor = 0;
    Q_EMIT approaching(border(), 0.0, m_approachGeometry);
}

void Edge::doStopApproaching()
{
}

void Edge::updateApproaching(const QPoint &point)
{
    if (approachGeometry().contains(point)) {
        int factor = 0;
        const int edgeDistance = m_edges->cornerOffset();
        auto cornerDistance = [=](const QPoint &corner) {
            return std::max(std::abs(corner.x() - point.x()), std::abs(corner.y() - point.y()));
        };
        switch (border()) {
        case ElectricTopLeft:
            factor = (cornerDistance(approachGeometry().topLeft()) << 8) / edgeDistance;
            break;
        case ElectricTopRight:
            factor = (cornerDistance(approachGeometry().topRight()) << 8) / edgeDistance;
            break;
        case ElectricBottomRight:
            factor = (cornerDistance(approachGeometry().bottomRight()) << 8) / edgeDistance;
            break;
        case ElectricBottomLeft:
            factor = (cornerDistance(approachGeometry().bottomLeft()) << 8) / edgeDistance;
            break;
        case ElectricTop:
            factor = (std::abs(point.y() - approachGeometry().y()) << 8) / edgeDistance;
            break;
        case ElectricRight:
            factor = (std::abs(point.x() - approachGeometry().right()) << 8) / edgeDistance;
            break;
        case ElectricBottom:
            factor = (std::abs(point.y() - approachGeometry().bottom()) << 8) / edgeDistance;
            break;
        case ElectricLeft:
            factor = (std::abs(point.x() - approachGeometry().x()) << 8) / edgeDistance;
            break;
        default:
            break;
        }
        factor = 256 - factor;
        if (m_lastApproachingFactor != factor) {
            m_lastApproachingFactor = factor;
            Q_EMIT approaching(border(), m_lastApproachingFactor / 256.0f, m_approachGeometry);
        }
    } else {
        stopApproaching();
    }
}

quint32 Edge::window() const
{
    return 0;
}

quint32 Edge::approachWindow() const
{
    return 0;
}

void Edge::setBorder(ElectricBorder border)
{
    m_border = border;
    switch (m_border) {
    case ElectricTop:
        m_gesture->setDirection(SwipeGesture::Direction::Down);
        break;
    case ElectricRight:
        m_gesture->setDirection(SwipeGesture::Direction::Left);
        break;
    case ElectricBottom:
        m_gesture->setDirection(SwipeGesture::Direction::Up);
        break;
    case ElectricLeft:
        m_gesture->setDirection(SwipeGesture::Direction::Right);
        break;
    default:
        break;
    }
}

void Edge::setTouchAction(ElectricBorderAction action)
{
    const bool wasTouch = activatesForTouchGesture();
    m_touchAction = action;
    if (wasTouch != activatesForTouchGesture()) {
        Q_EMIT activatesForTouchGestureChanged();
    }
}

void Edge::setClient(Window *client)
{
    const bool wasTouch = activatesForTouchGesture();
    m_client = client;
    if (wasTouch != activatesForTouchGesture()) {
        Q_EMIT activatesForTouchGestureChanged();
    }
}

void Edge::setOutput(Output *output)
{
    m_output = output;
}

Output *Edge::output() const
{
    return m_output;
}

/**********************************************************
 * ScreenEdges
 *********************************************************/

ScreenEdges::ScreenEdges()
    : m_desktopSwitching(false)
    , m_desktopSwitchingMovingClients(false)
    , m_timeThreshold(0)
    , m_reactivateThreshold(0)
    , m_virtualDesktopLayout({})
    , m_actionTopLeft(ElectricActionNone)
    , m_actionTop(ElectricActionNone)
    , m_actionTopRight(ElectricActionNone)
    , m_actionRight(ElectricActionNone)
    , m_actionBottomRight(ElectricActionNone)
    , m_actionBottom(ElectricActionNone)
    , m_actionBottomLeft(ElectricActionNone)
    , m_actionLeft(ElectricActionNone)
    , m_gestureRecognizer(new GestureRecognizer(this))
{
    const int gridUnit = QFontMetrics(QFontDatabase::systemFont(QFontDatabase::GeneralFont)).boundingRect(QLatin1Char('M')).height();
    m_cornerOffset = 4 * gridUnit;

    connect(workspace(), &Workspace::windowRemoved, this, &ScreenEdges::deleteEdgeForClient);
}

void ScreenEdges::init()
{
    reconfigure();
    updateLayout();
    recreateEdges();
}
static ElectricBorderAction electricBorderAction(const QString &name)
{
    QString lowerName = name.toLower();
    if (lowerName == QStringLiteral("showdesktop")) {
        return ElectricActionShowDesktop;
    } else if (lowerName == QStringLiteral("lockscreen")) {
        return ElectricActionLockScreen;
    } else if (lowerName == QLatin1String("krunner")) {
        return ElectricActionKRunner;
    } else if (lowerName == QLatin1String("activitymanager")) {
        return ElectricActionActivityManager;
    } else if (lowerName == QLatin1String("applicationlauncher")) {
        return ElectricActionApplicationLauncher;
    }
    return ElectricActionNone;
}

void ScreenEdges::reconfigure()
{
    if (!m_config) {
        return;
    }
    KConfigGroup screenEdgesConfig = m_config->group("ScreenEdges");
    setRemainActiveOnFullscreen(screenEdgesConfig.readEntry("RemainActiveOnFullscreen", false));

    // TODO: migrate settings to a group ScreenEdges
    KConfigGroup windowsConfig = m_config->group("Windows");
    setTimeThreshold(windowsConfig.readEntry("ElectricBorderDelay", 150));
    setReActivationThreshold(std::max(timeThreshold() + 50, windowsConfig.readEntry("ElectricBorderCooldown", 350)));
    int desktopSwitching = windowsConfig.readEntry("ElectricBorders", static_cast<int>(ElectricDisabled));
    if (desktopSwitching == ElectricDisabled) {
        setDesktopSwitching(false);
        setDesktopSwitchingMovingClients(false);
    } else if (desktopSwitching == ElectricMoveOnly) {
        setDesktopSwitching(false);
        setDesktopSwitchingMovingClients(true);
    } else if (desktopSwitching == ElectricAlways) {
        setDesktopSwitching(true);
        setDesktopSwitchingMovingClients(true);
    }
    const int pushBack = windowsConfig.readEntry("ElectricBorderPushbackPixels", 1);
    m_cursorPushBackDistance = QSize(pushBack, pushBack);

    KConfigGroup borderConfig = m_config->group("ElectricBorders");
    setActionForBorder(ElectricTopLeft, &m_actionTopLeft,
                       electricBorderAction(borderConfig.readEntry("TopLeft", "None")));
    setActionForBorder(ElectricTop, &m_actionTop,
                       electricBorderAction(borderConfig.readEntry("Top", "None")));
    setActionForBorder(ElectricTopRight, &m_actionTopRight,
                       electricBorderAction(borderConfig.readEntry("TopRight", "None")));
    setActionForBorder(ElectricRight, &m_actionRight,
                       electricBorderAction(borderConfig.readEntry("Right", "None")));
    setActionForBorder(ElectricBottomRight, &m_actionBottomRight,
                       electricBorderAction(borderConfig.readEntry("BottomRight", "None")));
    setActionForBorder(ElectricBottom, &m_actionBottom,
                       electricBorderAction(borderConfig.readEntry("Bottom", "None")));
    setActionForBorder(ElectricBottomLeft, &m_actionBottomLeft,
                       electricBorderAction(borderConfig.readEntry("BottomLeft", "None")));
    setActionForBorder(ElectricLeft, &m_actionLeft,
                       electricBorderAction(borderConfig.readEntry("Left", "None")));

    borderConfig = m_config->group("TouchEdges");
    setActionForTouchBorder(ElectricTop, electricBorderAction(borderConfig.readEntry("Top", "None")));
    setActionForTouchBorder(ElectricRight, electricBorderAction(borderConfig.readEntry("Right", "None")));
    setActionForTouchBorder(ElectricBottom, electricBorderAction(borderConfig.readEntry("Bottom", "None")));
    setActionForTouchBorder(ElectricLeft, electricBorderAction(borderConfig.readEntry("Left", "None")));
}

void ScreenEdges::setActionForBorder(ElectricBorder border, ElectricBorderAction *oldValue, ElectricBorderAction newValue)
{
    if (*oldValue == newValue) {
        return;
    }
    if (*oldValue == ElectricActionNone) {
        // have to reserve
        for (const auto &edge : m_edges) {
            if (edge->border() == border) {
                edge->reserve();
            }
        }
    }
    if (newValue == ElectricActionNone) {
        // have to unreserve
        for (const auto &edge : m_edges) {
            if (edge->border() == border) {
                edge->unreserve();
            }
        }
    }
    *oldValue = newValue;
    // update action on all Edges for given border
    for (const auto &edge : m_edges) {
        if (edge->border() == border) {
            edge->setAction(newValue);
        }
    }
}

void ScreenEdges::setActionForTouchBorder(ElectricBorder border, ElectricBorderAction newValue)
{
    auto it = m_touchCallbacks.find(border);
    ElectricBorderAction oldValue = ElectricActionNone;
    if (it != m_touchCallbacks.end()) {
        oldValue = it.value();
    }
    if (oldValue == newValue) {
        return;
    }
    if (oldValue == ElectricActionNone) {
        // have to reserve
        for (const auto &edge : m_edges) {
            if (edge->border() == border) {
                edge->reserve();
            }
        }
    }
    if (newValue == ElectricActionNone) {
        // have to unreserve
        for (const auto &edge : m_edges) {
            if (edge->border() == border) {
                edge->unreserve();
            }
        }

        m_touchCallbacks.erase(it);
    } else {
        m_touchCallbacks.insert(border, newValue);
    }
    // update action on all Edges for given border
    for (const auto &edge : m_edges) {
        if (edge->border() == border) {
            edge->setTouchAction(newValue);
        }
    }
}

void ScreenEdges::updateLayout()
{
    const QSize desktopMatrix = VirtualDesktopManager::self()->grid().size();
    Qt::Orientations newLayout = {};
    if (desktopMatrix.width() > 1) {
        newLayout |= Qt::Horizontal;
    }
    if (desktopMatrix.height() > 1) {
        newLayout |= Qt::Vertical;
    }
    if (newLayout == m_virtualDesktopLayout) {
        return;
    }
    if (isDesktopSwitching()) {
        reserveDesktopSwitching(false, m_virtualDesktopLayout);
    }
    m_virtualDesktopLayout = newLayout;
    if (isDesktopSwitching()) {
        reserveDesktopSwitching(true, m_virtualDesktopLayout);
    }
}

static bool isLeftScreen(const QRect &screen, const QRect &fullArea)
{
    const auto outputs = workspace()->outputs();
    if (outputs.count() == 1) {
        return true;
    }
    if (screen.x() == fullArea.x()) {
        return true;
    }
    // If any other screen has a right edge against our left edge, then this screen is not a left screen
    for (const Output *output : outputs) {
        const QRect otherGeo = output->geometry();
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (screen.x() == otherGeo.x() + otherGeo.width()
            && screen.y() < otherGeo.y() + otherGeo.height()
            && screen.y() + screen.height() > otherGeo.y()) {
            // There is a screen to the left
            return false;
        }
    }
    // No screen exists to the left, so this is a left screen
    return true;
}

static bool isRightScreen(const QRect &screen, const QRect &fullArea)
{
    const auto outputs = workspace()->outputs();
    if (outputs.count() == 1) {
        return true;
    }
    if (screen.x() + screen.width() == fullArea.x() + fullArea.width()) {
        return true;
    }
    // If any other screen has any left edge against any of our right edge, then this screen is not a right screen
    for (const Output *output : outputs) {
        const QRect otherGeo = output->geometry();
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (screen.x() + screen.width() == otherGeo.x()
            && screen.y() < otherGeo.y() + otherGeo.height()
            && screen.y() + screen.height() > otherGeo.y()) {
            // There is a screen to the right
            return false;
        }
    }
    // No screen exists to the right, so this is a right screen
    return true;
}

static bool isTopScreen(const QRect &screen, const QRect &fullArea)
{
    const auto outputs = workspace()->outputs();
    if (outputs.count() == 1) {
        return true;
    }
    if (screen.y() == fullArea.y()) {
        return true;
    }
    // If any other screen has any bottom edge against any of our top edge, then this screen is not a top screen
    for (const Output *output : outputs) {
        const QRect otherGeo = output->geometry();
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (screen.y() == otherGeo.y() + otherGeo.height()
            && screen.x() < otherGeo.x() + otherGeo.width()
            && screen.x() + screen.width() > otherGeo.x()) {
            // There is a screen to the top
            return false;
        }
    }
    // No screen exists to the top, so this is a top screen
    return true;
}

static bool isBottomScreen(const QRect &screen, const QRect &fullArea)
{
    const auto outputs = workspace()->outputs();
    if (outputs.count() == 1) {
        return true;
    }
    if (screen.y() + screen.height() == fullArea.y() + fullArea.height()) {
        return true;
    }
    // If any other screen has any top edge against any of our bottom edge, then this screen is not a bottom screen
    for (const Output *output : outputs) {
        const QRect otherGeo = output->geometry();
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (screen.y() + screen.height() == otherGeo.y()
            && screen.x() < otherGeo.x() + otherGeo.width()
            && screen.x() + screen.width() > otherGeo.x()) {
            // There is a screen to the bottom
            return false;
        }
    }
    // No screen exists to the bottom, so this is a bottom screen
    return true;
}

bool ScreenEdges::remainActiveOnFullscreen() const
{
    return m_remainActiveOnFullscreen;
}

void ScreenEdges::recreateEdges()
{
    std::vector<std::unique_ptr<Edge>> oldEdges = std::move(m_edges);
    m_edges.clear();
    const QRect fullArea = workspace()->geometry();
    QRegion processedRegion;

    const auto outputs = workspace()->outputs();
    for (Output *output : outputs) {
        const QRegion screen = QRegion(output->geometry()).subtracted(processedRegion);
        processedRegion += screen;
        for (const QRect &screenPart : screen) {
            if (isLeftScreen(screenPart, fullArea)) {
                // left most screen
                createVerticalEdge(ElectricLeft, screenPart, fullArea, output);
            }
            if (isRightScreen(screenPart, fullArea)) {
                // right most screen
                createVerticalEdge(ElectricRight, screenPart, fullArea, output);
            }
            if (isTopScreen(screenPart, fullArea)) {
                // top most screen
                createHorizontalEdge(ElectricTop, screenPart, fullArea, output);
            }
            if (isBottomScreen(screenPart, fullArea)) {
                // bottom most screen
                createHorizontalEdge(ElectricBottom, screenPart, fullArea, output);
            }
        }
    }
    // copy over the effect/script reservations from the old edges
    for (const auto &edge : m_edges) {
        for (const auto &oldEdge : oldEdges) {
            if (oldEdge->client()) {
                // show the client again and don't recreate the edge
                oldEdge->client()->showOnScreenEdge();
                continue;
            }
            if (oldEdge->border() != edge->border()) {
                continue;
            }
            const QHash<QObject *, QByteArray> &callbacks = oldEdge->callBacks();
            for (auto callback = callbacks.begin(); callback != callbacks.end(); callback++) {
                edge->reserve(callback.key(), callback.value().constData());
            }
            const auto touchCallBacks = oldEdge->touchCallBacks();
            for (auto c : touchCallBacks) {
                edge->reserveTouchCallBack(c);
            }
        }
    }
}

void ScreenEdges::createVerticalEdge(ElectricBorder border, const QRect &screen, const QRect &fullArea, Output *output)
{
    if (border != ElectricRight && border != KWin::ElectricLeft) {
        return;
    }
    int y = screen.y();
    int height = screen.height();
    const int x = (border == ElectricLeft) ? screen.x() : screen.x() + screen.width() - TOUCH_TARGET;
    if (isTopScreen(screen, fullArea)) {
        // also top most screen
        height -= m_cornerOffset;
        y += m_cornerOffset;
        // create top left/right edge
        const ElectricBorder edge = (border == ElectricLeft) ? ElectricTopLeft : ElectricTopRight;
        m_edges.push_back(createEdge(edge, x, screen.y(), TOUCH_TARGET, TOUCH_TARGET, output));
    }
    if (isBottomScreen(screen, fullArea)) {
        // also bottom most screen
        height -= m_cornerOffset;
        // create bottom left/right edge
        const ElectricBorder edge = (border == ElectricLeft) ? ElectricBottomLeft : ElectricBottomRight;
        m_edges.push_back(createEdge(edge, x, screen.y() + screen.height() - TOUCH_TARGET, TOUCH_TARGET, TOUCH_TARGET, output));
    }
    if (height <= m_cornerOffset) {
        // An overlap with another output is near complete. We ignore this border.
        return;
    }
    m_edges.push_back(createEdge(border, x, y, TOUCH_TARGET, height, output));
}

void ScreenEdges::createHorizontalEdge(ElectricBorder border, const QRect &screen, const QRect &fullArea, Output *output)
{
    if (border != ElectricTop && border != ElectricBottom) {
        return;
    }
    int x = screen.x();
    int width = screen.width();
    if (isLeftScreen(screen, fullArea)) {
        // also left most - adjust only x and width
        x += m_cornerOffset;
        width -= m_cornerOffset;
    }
    if (isRightScreen(screen, fullArea)) {
        // also right most edge
        width -= m_cornerOffset;
    }
    if (width <= m_cornerOffset) {
        // An overlap with another output is near complete. We ignore this border.
        return;
    }
    const int y = (border == ElectricTop) ? screen.y() : screen.y() + screen.height() - TOUCH_TARGET;
    m_edges.push_back(createEdge(border, x, y, width, TOUCH_TARGET, output));
}

std::unique_ptr<Edge> ScreenEdges::createEdge(ElectricBorder border, int x, int y, int width, int height, Output *output, bool createAction)
{
    std::unique_ptr<Edge> edge = kwinApp()->createScreenEdge(this);
    // Edges can not have negative size.
    Q_ASSERT(width >= 0);
    Q_ASSERT(height >= 0);

    edge->setBorder(border);
    edge->setGeometry(QRect(x, y, width, height));
    edge->setOutput(output);
    if (createAction) {
        const ElectricBorderAction action = actionForEdge(edge.get());
        if (action != KWin::ElectricActionNone) {
            edge->reserve();
            edge->setAction(action);
        }
        const ElectricBorderAction touchAction = actionForTouchEdge(edge.get());
        if (touchAction != KWin::ElectricActionNone) {
            edge->reserve();
            edge->setTouchAction(touchAction);
        }
    }
    if (isDesktopSwitching()) {
        if (edge->isCorner()) {
            edge->reserve();
        } else {
            if ((m_virtualDesktopLayout & Qt::Horizontal) && (edge->isLeft() || edge->isRight())) {
                edge->reserve();
            }
            if ((m_virtualDesktopLayout & Qt::Vertical) && (edge->isTop() || edge->isBottom())) {
                edge->reserve();
            }
        }
    }
    connect(edge.get(), &Edge::approaching, this, &ScreenEdges::approaching);
    connect(this, &ScreenEdges::checkBlocking, edge.get(), &Edge::checkBlocking);
    return edge;
}

ElectricBorderAction ScreenEdges::actionForEdge(Edge *edge) const
{
    switch (edge->border()) {
    case ElectricTopLeft:
        return m_actionTopLeft;
    case ElectricTop:
        return m_actionTop;
    case ElectricTopRight:
        return m_actionTopRight;
    case ElectricRight:
        return m_actionRight;
    case ElectricBottomRight:
        return m_actionBottomRight;
    case ElectricBottom:
        return m_actionBottom;
    case ElectricBottomLeft:
        return m_actionBottomLeft;
    case ElectricLeft:
        return m_actionLeft;
    default:
        // fall through
        break;
    }
    return ElectricActionNone;
}

ElectricBorderAction ScreenEdges::actionForTouchEdge(Edge *edge) const
{
    auto it = m_touchCallbacks.find(edge->border());
    if (it != m_touchCallbacks.end()) {
        return it.value();
    }
    return ElectricActionNone;
}

ElectricBorderAction ScreenEdges::actionForTouchBorder(ElectricBorder border) const
{
    return m_touchCallbacks.value(border);
}

void ScreenEdges::reserveDesktopSwitching(bool isToReserve, Qt::Orientations o)
{
    if (!o) {
        return;
    }
    for (const auto &edge : m_edges) {
        if (edge->isCorner()) {
            isToReserve ? edge->reserve() : edge->unreserve();
        } else {
            if ((m_virtualDesktopLayout & Qt::Horizontal) && (edge->isLeft() || edge->isRight())) {
                isToReserve ? edge->reserve() : edge->unreserve();
            }
            if ((m_virtualDesktopLayout & Qt::Vertical) && (edge->isTop() || edge->isBottom())) {
                isToReserve ? edge->reserve() : edge->unreserve();
            }
        }
    }
}

void ScreenEdges::reserve(ElectricBorder border, QObject *object, const char *slot)
{
    for (const auto &edge : m_edges) {
        if (edge->border() == border) {
            edge->reserve(object, slot);
        }
    }
}

void ScreenEdges::unreserve(ElectricBorder border, QObject *object)
{
    for (const auto &edge : m_edges) {
        if (edge->border() == border) {
            edge->unreserve(object);
        }
    }
}

void ScreenEdges::reserve(Window *client, ElectricBorder border)
{
    const auto it = std::remove_if(m_edges.begin(), m_edges.end(), [client](const auto &edge) {
        return edge->client() == client;
    });
    const bool hadBorder = it != m_edges.end();
    m_edges.erase(it, m_edges.end());

    if (border != ElectricNone) {
        createEdgeForClient(client, border);
    } else {
        if (hadBorder) { // show again
            client->showOnScreenEdge();
        }
    }
}

void ScreenEdges::reserveTouch(ElectricBorder border, QAction *action, TouchCallback::CallbackFunction callback)
{
    for (const auto &edge : m_edges) {
        if (edge->border() == border) {
            edge->reserveTouchCallBack(action, callback);
        }
    }
}

void ScreenEdges::unreserveTouch(ElectricBorder border, QAction *action)
{
    for (const auto &edge : m_edges) {
        if (edge->border() == border) {
            edge->unreserveTouchCallBack(action);
        }
    }
}

void ScreenEdges::createEdgeForClient(Window *client, ElectricBorder border)
{
    int y = 0;
    int x = 0;
    int width = 0;
    int height = 0;
    const QRect geo = client->frameGeometry().toRect();
    const QRect fullArea = workspace()->geometry();

    const auto outputs = workspace()->outputs();
    Output *foundOutput = nullptr;
    for (Output *output : outputs) {
        foundOutput = output;
        const QRect screen = output->geometry();
        if (!screen.contains(geo)) {
            // ignoring Clients having a geometry overlapping with multiple screens
            // this would make the code more complex. If it's needed in future it can be added
            continue;
        }
        const bool bordersTop = (screen.y() == geo.y());
        const bool bordersLeft = (screen.x() == geo.x());
        const bool bordersBottom = (screen.y() + screen.height() == geo.y() + geo.height());
        const bool bordersRight = (screen.x() + screen.width() == geo.x() + geo.width());

        if (bordersTop && border == ElectricTop) {
            if (!isTopScreen(screen, fullArea)) {
                continue;
            }
            y = geo.y();
            x = geo.x();
            height = 1;
            width = geo.width();
            break;
        }
        if (bordersBottom && border == ElectricBottom) {
            if (!isBottomScreen(screen, fullArea)) {
                continue;
            }
            y = geo.y() + geo.height() - 1;
            x = geo.x();
            height = 1;
            width = geo.width();
            break;
        }
        if (bordersLeft && border == ElectricLeft) {
            if (!isLeftScreen(screen, fullArea)) {
                continue;
            }
            x = geo.x();
            y = geo.y();
            width = 1;
            height = geo.height();
            break;
        }
        if (bordersRight && border == ElectricRight) {
            if (!isRightScreen(screen, fullArea)) {
                continue;
            }
            x = geo.x() + geo.width() - 1;
            y = geo.y();
            width = 1;
            height = geo.height();
            break;
        }
    }

    if (width > 0 && height > 0) {
        m_edges.push_back(createEdge(border, x, y, width, height, foundOutput, false));
        Edge *edge = m_edges.back().get();
        edge->setClient(client);
        edge->reserve();
    } else {
        // we could not create an edge window, so don't allow the window to hide
        client->showOnScreenEdge();
    }
}

void ScreenEdges::deleteEdgeForClient(Window *window)
{
    const auto it = std::remove_if(m_edges.begin(), m_edges.end(), [window](const auto &edge) {
        return edge->client() == window;
    });
    m_edges.erase(it, m_edges.end());
}

void ScreenEdges::check(const QPoint &pos, const QDateTime &now, bool forceNoPushBack)
{
    bool activatedForClient = false;
    for (const auto &edge : m_edges) {
        if (!edge->isReserved() || edge->isBlocked()) {
            continue;
        }
        if (!edge->activatesForPointer()) {
            continue;
        }
        if (edge->approachGeometry().contains(pos)) {
            edge->startApproaching();
        }
        if (edge->client() != nullptr && activatedForClient) {
            edge->markAsTriggered(pos, now);
            continue;
        }
        if (edge->check(pos, now, forceNoPushBack)) {
            if (edge->client()) {
                activatedForClient = true;
            }
        }
    }
}

bool ScreenEdges::isEntered(QMouseEvent *event)
{
    if (event->type() != QEvent::MouseMove) {
        return false;
    }
    bool activated = false;
    bool activatedForClient = false;
    for (const auto &edge : m_edges) {
        if (!edge->isReserved() || edge->isBlocked()) {
            continue;
        }
        if (!edge->activatesForPointer()) {
            continue;
        }
        if (edge->approachGeometry().contains(event->globalPos())) {
            if (!edge->isApproaching()) {
                edge->startApproaching();
            } else {
                edge->updateApproaching(event->globalPos());
            }
        } else {
            if (edge->isApproaching()) {
                edge->stopApproaching();
            }
        }
        if (edge->geometry().contains(event->globalPos())) {
            if (edge->check(event->globalPos(), QDateTime::fromMSecsSinceEpoch(event->timestamp(), Qt::UTC))) {
                if (edge->client()) {
                    activatedForClient = true;
                }
            }
        }
    }
    if (activatedForClient) {
        for (const auto &edge : m_edges) {
            if (edge) {
                edge->markAsTriggered(event->globalPos(), QDateTime::fromMSecsSinceEpoch(event->timestamp(), Qt::UTC));
            }
        }
    }
    return activated;
}

bool ScreenEdges::handleEnterNotifiy(xcb_window_t window, const QPoint &point, const QDateTime &timestamp)
{
    bool activated = false;
    bool activatedForClient = false;
    for (const auto &edge : m_edges) {
        if (!edge || edge->window() == XCB_WINDOW_NONE) {
            continue;
        }
        if (!edge->isReserved() || edge->isBlocked()) {
            continue;
        }
        if (!edge->activatesForPointer()) {
            continue;
        }
        if (edge->window() == window) {
            if (edge->check(point, timestamp)) {
                if (edge->client()) {
                    activatedForClient = true;
                }
            }
            activated = true;
            break;
        }
        if (edge->approachWindow() == window) {
            edge->startApproaching();
            // TODO: if it's a corner, it should also trigger for other windows
            return true;
        }
    }
    if (activatedForClient) {
        for (const auto &edge : m_edges) {
            if (edge->client()) {
                edge->markAsTriggered(point, timestamp);
            }
        }
    }
    return activated;
}

bool ScreenEdges::handleDndNotify(xcb_window_t window, const QPoint &point)
{
    for (const auto &edge : m_edges) {
        if (!edge || edge->window() == XCB_WINDOW_NONE) {
            continue;
        }
        if (edge->isReserved() && edge->window() == window) {
            kwinApp()->updateXTime();
            edge->check(point, QDateTime::fromMSecsSinceEpoch(xTime(), Qt::UTC), true);
            return true;
        }
    }
    return false;
}

void ScreenEdges::ensureOnTop()
{
    Xcb::restackWindowsWithRaise(windows());
}

QVector<xcb_window_t> ScreenEdges::windows() const
{
    QVector<xcb_window_t> wins;
    for (const auto &edge : m_edges) {
        xcb_window_t w = edge->window();
        if (w != XCB_WINDOW_NONE) {
            wins.append(w);
        }
        // TODO:  lambda
        w = edge->approachWindow();
        if (w != XCB_WINDOW_NONE) {
            wins.append(w);
        }
    }
    return wins;
}

void ScreenEdges::setRemainActiveOnFullscreen(bool remainActive)
{
    m_remainActiveOnFullscreen = remainActive;
}

const std::vector<std::unique_ptr<Edge>> &ScreenEdges::edges() const
{
    return m_edges;
}

} // namespace
