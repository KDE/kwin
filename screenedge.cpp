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

// KWin
#include "gestures.h"
#include <x11client.h>
#include "cursor.h"
#include "main.h"
#include "platform.h"
#include "screens.h"
#include "utils.h"
#include <workspace.h>
#include "virtualdesktops.h"
#ifdef KWIN_UNIT_TEST
#include "plugins/platforms/x11/standalone/edge.h"
#endif
// DBus generated
#include "screenlocker_interface.h"
// frameworks
#include <KConfigGroup>
// Qt
#include <QAction>
#include <QMouseEvent>
#include <QSharedPointer>
#include <QTimer>
#include <QVector>
#include <QTextStream>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QWidget>

namespace KWin {

// Mouse should not move more than this many pixels
static const int DISTANCE_RESET = 30;

Edge::Edge(ScreenEdges *parent)
    : QObject(parent)
    , m_edges(parent)
    , m_border(ElectricNone)
    , m_action(ElectricActionNone)
    , m_reserved(0)
    , m_approaching(false)
    , m_lastApproachingFactor(0)
    , m_blocked(false)
    , m_pushBackBlocked(false)
    , m_client(nullptr)
    , m_gesture(new SwipeGesture(this))
{
    m_gesture->setMinimumFingerCount(1);
    m_gesture->setMaximumFingerCount(1);
    connect(m_gesture, &Gesture::triggered, this,
        [this] {
            stopApproaching();
            if (m_client) {
                m_client->showOnScreenEdge();
                unreserve();
                return;
            }
            handleTouchAction();
            handleTouchCallback();
        }, Qt::QueuedConnection
    );
    connect(m_gesture, &SwipeGesture::started, this, &Edge::startApproaching);
    connect(m_gesture, &SwipeGesture::cancelled, this, &Edge::stopApproaching);
    connect(m_gesture, &SwipeGesture::progress, this,
        [this] (qreal progress) {
            int factor = progress * 256.0f;
            if (m_lastApproachingFactor != factor) {
                m_lastApproachingFactor = factor;
                emit approaching(border(), m_lastApproachingFactor/256.0f, m_approachGeometry);
            }
        }
    );
    connect(this, &Edge::activatesForTouchGestureChanged, this,
        [this] {
            if (isReserved()) {
                if (activatesForTouchGesture()) {
                    m_edges->gestureRecognizer()->registerGesture(m_gesture);
                } else {
                    m_edges->gestureRecognizer()->unregisterGesture(m_gesture);
                }
            }
        }
    );
}

Edge::~Edge()
{
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
    connect(object, SIGNAL(destroyed(QObject*)), SLOT(unreserve(QObject*)));
    m_callBacks.insert(object, QByteArray(slot));
    reserve();
}

void Edge::reserveTouchCallBack(QAction *action)
{
    if (m_touchActions.contains(action)) {
        return;
    }
    connect(action, &QAction::destroyed, this,
        [this, action] {
            unreserveTouchCallBack(action);
        }
    );
    m_touchActions << action;
    reserve();
}

void Edge::unreserveTouchCallBack(QAction *action)
{
    auto it = std::find_if(m_touchActions.begin(), m_touchActions.end(), [action] (QAction *a) { return a == action; });
    if (it != m_touchActions.end()) {
        m_touchActions.erase(it);
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
    if (m_callBacks.contains(object)) {
        m_callBacks.remove(object);
        disconnect(object, SIGNAL(destroyed(QObject*)), this, SLOT(unreserve(QObject*)));
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
        auto c = Workspace::self()->moveResizeClient();
        if (c && !c->isResize()) {
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
    if (!m_touchActions.isEmpty()) {
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
    if (isRight() && cursorPos.x() != (m_geometry.x() + m_geometry.width() -1)) {
        return false;
    }
    if (isTop() && cursorPos.y() != m_geometry.y()) {
        return false;
    }
    if (isBottom() && cursorPos.y() != (m_geometry.y() + m_geometry.height() -1)) {
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
    AbstractClient *movingClient = Workspace::self()->moveResizeClient();
    if ((edges()->isDesktopSwitchingMovingClients() && movingClient && !movingClient->isResize()) ||
        (edges()->isDesktopSwitching() && isScreenEdge())) {
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
        OrgFreedesktopScreenSaverInterface interface(QStringLiteral("org.freedesktop.ScreenSaver"),
                                                     QStringLiteral("/ScreenSaver"),
                                                     QDBusConnection::sessionBus());
        if (interface.isValid()) {
            interface.Lock();
        }
        return true;
    }
    case ElectricActionKRunner: { // open krunner
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.krunner"),
                                           QStringLiteral("/App"),
                                           QStringLiteral("org.kde.krunner.App"),
                                           QStringLiteral("display")
            )
        );
        return true;
    }
    case ElectricActionActivityManager: { // open activity manager
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                           QStringLiteral("/PlasmaShell"),
                                           QStringLiteral("org.kde.PlasmaShell"),
                                           QStringLiteral("toggleActivityManager")
            )
        );
        return true;
    }
    case ElectricActionApplicationLauncher: {
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                           QStringLiteral("/PlasmaShell"),
                                           QStringLiteral("org.kde.PlasmaShell"),
                                           QStringLiteral("activateLauncherMenu")
            )
        );
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
    for (QHash<QObject *, QByteArray>::iterator it = m_callBacks.begin();
        it != m_callBacks.end();
        ++it) {
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
    if (m_touchActions.isEmpty()) {
        return;
    }
    m_touchActions.first()->trigger();
}

void Edge::switchDesktop(const QPoint &cursorPos)
{
    QPoint pos(cursorPos);
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    const uint oldDesktop = vds->current();
    uint desktop = oldDesktop;
    const int OFFSET = 2;
    if (isLeft()) {
        const uint interimDesktop = desktop;
        desktop = vds->toLeft(desktop, vds->isNavigationWrappingAround());
        if (desktop != interimDesktop)
            pos.setX(screens()->size().width() - 1 - OFFSET);
    } else if (isRight()) {
        const uint interimDesktop = desktop;
        desktop = vds->toRight(desktop, vds->isNavigationWrappingAround());
        if (desktop != interimDesktop)
            pos.setX(OFFSET);
    }
    if (isTop()) {
        const uint interimDesktop = desktop;
        desktop = vds->above(desktop, vds->isNavigationWrappingAround());
        if (desktop != interimDesktop)
            pos.setY(screens()->size().height() - 1 - OFFSET);
    } else if (isBottom()) {
        const uint interimDesktop = desktop;
        desktop = vds->below(desktop, vds->isNavigationWrappingAround());
        if (desktop != interimDesktop)
            pos.setY(OFFSET);
    }
#ifndef KWIN_UNIT_TEST
    if (AbstractClient *c = Workspace::self()->moveResizeClient()) {
        if (c->rules()->checkDesktop(desktop) != int(desktop)) {
            // user attempts to move a client to another desktop where it is ruleforced to not be
            return;
        }
    }
#endif
    vds->setCurrent(desktop);
    if (vds->current() != oldDesktop) {
        m_pushBackBlocked = true;
        Cursors::self()->mouse()->setPos(pos);
        QSharedPointer<QMetaObject::Connection> me(new QMetaObject::Connection);
        *me = QObject::connect(QCoreApplication::eventDispatcher(),
                               &QAbstractEventDispatcher::aboutToBlock, this,
                               [this, me](){
            QObject::disconnect(*me);
            const_cast<QSharedPointer<QMetaObject::Connection>*>(&me)->reset(nullptr);
            m_pushBackBlocked = false;
        }
        );
    }
}

void Edge::pushCursorBack(const QPoint &cursorPos)
{
    if (m_pushBackBlocked)
        return;
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
    const int size = m_edges->cornerOffset();
    if (isCorner()) {
        if (isRight()) {
            x = x - size +1;
        }
        if (isBottom()) {
            y = y - size +1;
        }
        width = size;
        height = size;
    } else {
        if (isLeft()) {
            y += size + 1;
            width = size;
            height = height - size * 2;
        } else if (isRight()) {
            x = x - size + 1;
            y += size;
            width = size;
            height = height - size * 2;
        } else if (isTop()) {
            x += size;
            width = width - size * 2;
            height = size;
        } else if (isBottom()) {
            x += size;
            y = y - size +1;
            width = width - size * 2;
            height = size;
        }
    }
    m_approachGeometry = QRect(x, y, width, height);
    doGeometryUpdate();

    if (isScreenEdge()) {
        m_gesture->setStartGeometry(m_geometry);
        m_gesture->setMinimumDelta(screens()->size(screens()->number(m_geometry.center())) * 0.2);
    }
}

void Edge::checkBlocking()
{
    if (isCorner()) {
        return;
    }
    bool newValue = false;
    if (AbstractClient *client = Workspace::self()->activeClient()) {
        newValue = client->isFullScreen() && client->frameGeometry().contains(m_geometry.center());
    }
    if (newValue == m_blocked) {
        return;
    }
    const bool wasTouch = activatesForTouchGesture();
    m_blocked = newValue;
    if (wasTouch != activatesForTouchGesture()) {
        emit activatesForTouchGestureChanged();
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
        m_edges->gestureRecognizer()->registerGesture(m_gesture);
    }
    doActivate();
}

void Edge::doActivate()
{
}

void Edge::deactivate()
{
    m_edges->gestureRecognizer()->unregisterGesture(m_gesture);
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
    emit approaching(border(), 0.0, m_approachGeometry);
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
    emit approaching(border(), 0.0, m_approachGeometry);
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
            return qMax(qAbs(corner.x() - point.x()), qAbs(corner.y() - point.y()));
        };
        switch (border()) {
        case ElectricTopLeft:
            factor = (cornerDistance(approachGeometry().topLeft())<<8) / edgeDistance;
            break;
        case ElectricTopRight:
            factor = (cornerDistance(approachGeometry().topRight())<<8) / edgeDistance;
            break;
        case ElectricBottomRight:
            factor = (cornerDistance(approachGeometry().bottomRight())<<8) / edgeDistance;
            break;
        case ElectricBottomLeft:
            factor = (cornerDistance(approachGeometry().bottomLeft())<<8) / edgeDistance;
            break;
        case ElectricTop:
            factor = (qAbs(point.y() - approachGeometry().y())<<8) / edgeDistance;
            break;
        case ElectricRight:
            factor = (qAbs(point.x() - approachGeometry().right())<<8) / edgeDistance;
            break;
        case ElectricBottom:
            factor = (qAbs(point.y() - approachGeometry().bottom())<<8) / edgeDistance;
            break;
        case ElectricLeft:
            factor = (qAbs(point.x() - approachGeometry().x())<<8) / edgeDistance;
            break;
        default:
            break;
        }
        factor = 256 - factor;
        if (m_lastApproachingFactor != factor) {
            m_lastApproachingFactor = factor;
            emit approaching(border(), m_lastApproachingFactor/256.0f, m_approachGeometry);
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

void Edge::setTouchAction(ElectricBorderAction action) {
    const bool wasTouch = activatesForTouchGesture();
    m_touchAction = action;
    if (wasTouch != activatesForTouchGesture()) {
        emit activatesForTouchGestureChanged();
    }
}

void Edge::setClient(AbstractClient *client)
{
    const bool wasTouch = activatesForTouchGesture();
    m_client = client;
    if (wasTouch != activatesForTouchGesture()) {
        emit activatesForTouchGestureChanged();
    }
}

/**********************************************************
 * ScreenEdges
 *********************************************************/
KWIN_SINGLETON_FACTORY(ScreenEdges)

ScreenEdges::ScreenEdges(QObject *parent)
    : QObject(parent)
    , m_desktopSwitching(false)
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
    m_cornerOffset = (Screens::self()->physicalDpiX(0) + Screens::self()->physicalDpiY(0) + 5) / 6;

    connect(workspace(), &Workspace::clientRemoved, this, &ScreenEdges::deleteEdgeForClient);
}

ScreenEdges::~ScreenEdges()
{
    s_self = nullptr;
}

void ScreenEdges::init()
{
    reconfigure();
    updateLayout();
    recreateEdges();
}
static ElectricBorderAction electricBorderAction(const QString& name)
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
    // TODO: migrate settings to a group ScreenEdges
    KConfigGroup windowsConfig = m_config->group("Windows");
    setTimeThreshold(windowsConfig.readEntry("ElectricBorderDelay", 150));
    setReActivationThreshold(qMax(timeThreshold() + 50, windowsConfig.readEntry("ElectricBorderCooldown", 350)));
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
    setActionForBorder(ElectricTopLeft,     &m_actionTopLeft,
                       electricBorderAction(borderConfig.readEntry("TopLeft", "None")));
    setActionForBorder(ElectricTop,         &m_actionTop,
                       electricBorderAction(borderConfig.readEntry("Top", "None")));
    setActionForBorder(ElectricTopRight,    &m_actionTopRight,
                       electricBorderAction(borderConfig.readEntry("TopRight", "None")));
    setActionForBorder(ElectricRight,       &m_actionRight,
                       electricBorderAction(borderConfig.readEntry("Right", "None")));
    setActionForBorder(ElectricBottomRight, &m_actionBottomRight,
                       electricBorderAction(borderConfig.readEntry("BottomRight", "None")));
    setActionForBorder(ElectricBottom,      &m_actionBottom,
                       electricBorderAction(borderConfig.readEntry("Bottom", "None")));
    setActionForBorder(ElectricBottomLeft,  &m_actionBottomLeft,
                       electricBorderAction(borderConfig.readEntry("BottomLeft", "None")));
    setActionForBorder(ElectricLeft,        &m_actionLeft,
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
        for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
            if ((*it)->border() == border) {
                (*it)->reserve();
            }
        }
    }
    if (newValue == ElectricActionNone) {
        // have to unreserve
        for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
            if ((*it)->border() == border) {
                (*it)->unreserve();
            }
        }
    }
    *oldValue = newValue;
    // update action on all Edges for given border
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->setAction(newValue);
        }
    }
}

void ScreenEdges::setActionForTouchBorder(ElectricBorder border, ElectricBorderAction newValue)
{
    auto it = m_touchActions.find(border);
    ElectricBorderAction oldValue = ElectricActionNone;
    if (it != m_touchActions.end()) {
        oldValue = it.value();
    }
    if (oldValue == newValue) {
        return;
    }
    if (oldValue == ElectricActionNone) {
        // have to reserve
        for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
            if ((*it)->border() == border) {
                (*it)->reserve();
            }
        }
    }
    if (newValue == ElectricActionNone) {
        // have to unreserve
        for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
            if ((*it)->border() == border) {
                (*it)->unreserve();
            }
        }

        m_touchActions.erase(it);
    } else {
        m_touchActions.insert(border, newValue);
    }
    // update action on all Edges for given border
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->setTouchAction(newValue);
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
    if (screens()->count() == 1) {
        return true;
    }
    if (screen.x() == fullArea.x()) {
        return true;
    }
    // the screen is also on the left in case of a vertical layout with a second screen
    // more to the left. In that case no screen ends left of screen's x coord
    for (int i=0; i<screens()->count(); ++i) {
        const QRect otherGeo = screens()->geometry(i);
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (otherGeo.x() + otherGeo.width() <= screen.x()) {
            // other screen is completely in the left
            return false;
        }
    }
    // did not find a screen left of our current screen, so it is the left most
    return true;
}

static bool isRightScreen(const QRect &screen, const QRect &fullArea)
{
    if (screens()->count() == 1) {
        return true;
    }
    if (screen.x() + screen.width() == fullArea.x() + fullArea.width()) {
        return true;
    }
    // the screen is also on the right in case of a vertical layout with a second screen
    // more to the right. In that case no screen starts right of this screen
    for (int i=0; i<screens()->count(); ++i) {
        const QRect otherGeo = screens()->geometry(i);
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (otherGeo.x() >= screen.x() + screen.width()) {
            // other screen is completely in the right
            return false;
        }
    }
    // did not find a screen right of our current screen, so it is the right most
    return true;
}

static bool isTopScreen(const QRect &screen, const QRect &fullArea)
{
    if (screens()->count() == 1) {
        return true;
    }
    if (screen.y() == fullArea.y()) {
        return true;
    }
    // the screen is also top most in case of a horizontal layout with a second screen
    // more to the top. In that case no screen ends above screen's y coord
    for (int i=0; i<screens()->count(); ++i) {
        const QRect otherGeo = screens()->geometry(i);
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (otherGeo.y() + otherGeo.height() <= screen.y()) {
            // other screen is completely above
            return false;
        }
    }
    // did not find a screen above our current screen, so it is the top most
    return true;
}

static bool isBottomScreen(const QRect &screen, const QRect &fullArea)
{
    if (screens()->count() == 1) {
        return true;
    }
    if (screen.y() + screen.height() == fullArea.y() + fullArea.height()) {
        return true;
    }
    // the screen is also bottom most in case of a horizontal layout with a second screen
    // more below. In that case no screen starts below screen's y coord + height
    for (int i=0; i<screens()->count(); ++i) {
        const QRect otherGeo = screens()->geometry(i);
        if (otherGeo == screen) {
            // that's our screen to test
            continue;
        }
        if (otherGeo.y() >= screen.y() + screen.height()) {
            // other screen is completely below
            return false;
        }
    }
    // did not find a screen below our current screen, so it is the bottom most
    return true;
}

void ScreenEdges::recreateEdges()
{
    QList<Edge*> oldEdges(m_edges);
    m_edges.clear();
    const QRect fullArea = screens()->geometry();
    QRegion processedRegion;
    for (int i=0; i<screens()->count(); ++i) {
        const QRegion screen = QRegion(screens()->geometry(i)).subtracted(processedRegion);
        processedRegion += screen;
        for (const QRect &screenPart : screen) {
            if (isLeftScreen(screenPart, fullArea)) {
                // left most screen
                createVerticalEdge(ElectricLeft, screenPart, fullArea);
            }
            if (isRightScreen(screenPart, fullArea)) {
                // right most screen
                createVerticalEdge(ElectricRight, screenPart, fullArea);
            }
            if (isTopScreen(screenPart, fullArea)) {
                // top most screen
                createHorizontalEdge(ElectricTop, screenPart, fullArea);
            }
            if (isBottomScreen(screenPart, fullArea)) {
                // bottom most screen
                createHorizontalEdge(ElectricBottom, screenPart, fullArea);
            }
        }
    }
    // copy over the effect/script reservations from the old edges
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        Edge *edge = *it;
        for (auto oldIt = oldEdges.constBegin();
                oldIt != oldEdges.constEnd();
                ++oldIt) {
            Edge *oldEdge = *oldIt;
            if (oldEdge->client()) {
                // show the client again and don't recreate the edge
                oldEdge->client()->showOnScreenEdge();
                continue;
            }
            if (oldEdge->border() != edge->border()) {
                continue;
            }
            const QHash<QObject *, QByteArray> &callbacks = oldEdge->callBacks();
            for (QHash<QObject *, QByteArray>::const_iterator callback = callbacks.begin();
                    callback != callbacks.end();
                    ++callback) {
                edge->reserve(callback.key(), callback.value().constData());
            }
            const auto touchCallBacks = oldEdge->touchCallBacks();
            for (auto a : touchCallBacks) {
                edge->reserveTouchCallBack(a);
            }
        }
    }
    qDeleteAll(oldEdges);
}

void ScreenEdges::createVerticalEdge(ElectricBorder border, const QRect &screen, const QRect &fullArea)
{
    if (border != ElectricRight && border != KWin::ElectricLeft) {
        return;
    }
    int y = screen.y();
    int height = screen.height();
    const int x = (border == ElectricLeft) ? screen.x() : screen.x() + screen.width() -1;
    if (isTopScreen(screen, fullArea)) {
        // also top most screen
        height -= m_cornerOffset;
        y += m_cornerOffset;
        // create top left/right edge
        const ElectricBorder edge = (border == ElectricLeft) ? ElectricTopLeft : ElectricTopRight;
        m_edges << createEdge(edge, x, screen.y(), 1, 1);
    }
    if (isBottomScreen(screen, fullArea)) {
        // also bottom most screen
        height -= m_cornerOffset;
        // create bottom left/right edge
        const ElectricBorder edge = (border == ElectricLeft) ? ElectricBottomLeft : ElectricBottomRight;
        m_edges << createEdge(edge, x, screen.y() + screen.height() -1, 1, 1);
    }
    if (height <= m_cornerOffset) {
        // An overlap with another output is near complete. We ignore this border.
        return;
    }
    m_edges << createEdge(border, x, y, 1, height);
}

void ScreenEdges::createHorizontalEdge(ElectricBorder border, const QRect &screen, const QRect &fullArea)
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
    const int y = (border == ElectricTop) ? screen.y() : screen.y() + screen.height() - 1;
    m_edges << createEdge(border, x, y, width, 1);
}

Edge *ScreenEdges::createEdge(ElectricBorder border, int x, int y, int width, int height, bool createAction)
{
#ifdef KWIN_UNIT_TEST
    Edge *edge = new WindowBasedEdge(this);
#else
    Edge *edge = kwinApp()->platform()->createScreenEdge(this);
#endif
    // Edges can not have negative size.
    Q_ASSERT(width >= 0);
    Q_ASSERT(height >= 0);

    edge->setBorder(border);
    edge->setGeometry(QRect(x, y, width, height));
    if (createAction) {
        const ElectricBorderAction action = actionForEdge(edge);
        if (action != KWin::ElectricActionNone) {
            edge->reserve();
            edge->setAction(action);
        }
        const ElectricBorderAction touchAction = actionForTouchEdge(edge);
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
    connect(edge, SIGNAL(approaching(ElectricBorder,qreal,QRect)), SIGNAL(approaching(ElectricBorder,qreal,QRect)));
    if (edge->isScreenEdge()) {
        connect(this, SIGNAL(checkBlocking()), edge, SLOT(checkBlocking()));
    }
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
    auto it = m_touchActions.find(edge->border());
    if (it != m_touchActions.end()) {
        return it.value();
    }
    return ElectricActionNone;
}

void ScreenEdges::reserveDesktopSwitching(bool isToReserve, Qt::Orientations o)
{
    if (!o)
        return;
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        Edge *edge = *it;
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
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->reserve(object, slot);
        }
    }
}

void ScreenEdges::unreserve(ElectricBorder border, QObject *object)
{
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->unreserve(object);
        }
    }
}

void ScreenEdges::reserve(AbstractClient *client, ElectricBorder border)
{
    bool hadBorder = false;
    auto it = m_edges.begin();
    while (it != m_edges.end()) {
        if ((*it)->client() == client) {
            hadBorder = true;
            delete *it;
            it = m_edges.erase(it);
        } else {
            it++;
        }
    }

    if (border != ElectricNone) {
        createEdgeForClient(client, border);
    } else {
        if (hadBorder) // show again
            client->showOnScreenEdge();
    }
}

void ScreenEdges::reserveTouch(ElectricBorder border, QAction *action)
{
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->reserveTouchCallBack(action);
        }
    }
}

void ScreenEdges::unreserveTouch(ElectricBorder border, QAction *action)
{
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        if ((*it)->border() == border) {
            (*it)->unreserveTouchCallBack(action);
        }
    }
}

void ScreenEdges::createEdgeForClient(AbstractClient *client, ElectricBorder border)
{
    int y = 0;
    int x = 0;
    int width = 0;
    int height = 0;
    const QRect geo = client->frameGeometry();
    const QRect fullArea = workspace()->clientArea(FullArea, 0, 1);
    for (int i = 0; i < screens()->count(); ++i) {
        const QRect screen = screens()->geometry(i);
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
        Edge *edge = createEdge(border, x, y, width, height, false);
        edge->setClient(client);
        m_edges.append(edge);
        edge->reserve();
    } else {
        // we could not create an edge window, so don't allow the window to hide
        client->showOnScreenEdge();
    }
}

void ScreenEdges::deleteEdgeForClient(AbstractClient* c)
{
    auto it = m_edges.begin();
    while (it != m_edges.end()) {
        if ((*it)->client() == c) {
            delete *it;
            it = m_edges.erase(it);
        } else {
            it++;
        }
    }
}

void ScreenEdges::check(const QPoint &pos, const QDateTime &now, bool forceNoPushBack)
{
    bool activatedForClient = false;
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        if (!(*it)->isReserved()) {
            continue;
        }
        if (!(*it)->activatesForPointer()) {
            continue;
        }
        if ((*it)->approachGeometry().contains(pos)) {
            (*it)->startApproaching();
        }
        if ((*it)->client() != nullptr && activatedForClient) {
            (*it)->markAsTriggered(pos, now);
            continue;
        }
        if ((*it)->check(pos, now, forceNoPushBack)) {
            if ((*it)->client()) {
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
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        Edge *edge = *it;
        if (!edge->isReserved()) {
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
        for (auto it = m_edges.constBegin(); it != m_edges.constEnd(); ++it) {
            if ((*it)->client()) {
                (*it)->markAsTriggered(event->globalPos(), QDateTime::fromMSecsSinceEpoch(event->timestamp(), Qt::UTC));
            }
        }
    }
    return activated;
}

bool ScreenEdges::handleEnterNotifiy(xcb_window_t window, const QPoint &point, const QDateTime &timestamp)
{
    bool activated = false;
    bool activatedForClient = false;
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        Edge *edge = *it;
        if (!edge || edge->window() == XCB_WINDOW_NONE) {
            continue;
        }
        if (!edge->isReserved()) {
            continue;
        }
        if (!edge->activatesForPointer()) {
            continue;
        }
        if (edge->window() == window) {
            if (edge->check(point, timestamp)) {
                if ((*it)->client()) {
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
        for (auto it = m_edges.constBegin(); it != m_edges.constEnd(); ++it) {
            if ((*it)->client()) {
                (*it)->markAsTriggered(point, timestamp);
            }
        }
    }
    return activated;
}

bool ScreenEdges::handleDndNotify(xcb_window_t window, const QPoint &point)
{
    for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
        Edge *edge = *it;
        if (!edge || edge->window() == XCB_WINDOW_NONE) {
            continue;
        }
        if (edge->isReserved() && edge->window() == window) {
            updateXTime();
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

QVector< xcb_window_t > ScreenEdges::windows() const
{
    QVector<xcb_window_t> wins;
    for (auto it = m_edges.constBegin();
            it != m_edges.constEnd();
            ++it) {
        Edge *edge = *it;
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

} //namespace
