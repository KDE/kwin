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

#pragma once
// KWin
#include "effect/globals.h"
// KDE includes
#include <KSharedConfig>
// Qt
#include <QList>
#include <QObject>
#include <QRect>

#include <memory>
#include <xcb/xcb.h>

class QAction;

namespace KWin
{

class Window;
class Output;
class GestureRecognizer;
class ScreenEdges;
class SwipeGesture;

class TouchCallback
{
public:
    using CallbackFunction = std::function<void(ElectricBorder border, const QPointF &, Output *output)>;
    explicit TouchCallback(QAction *touchUpAction, TouchCallback::CallbackFunction progressCallback);
    ~TouchCallback();

    QAction *touchUpAction() const;
    void progressCallback(ElectricBorder border, const QPointF &deltaProgress, Output *output) const;
    bool hasProgressCallback() const;

private:
    QAction *m_touchUpAction = nullptr;
    TouchCallback::CallbackFunction m_progressCallback;
};

class KWIN_EXPORT Edge : public QObject
{
    Q_OBJECT
public:
    explicit Edge(ScreenEdges *parent);
    ~Edge() override;
    bool isLeft() const;
    bool isTop() const;
    bool isRight() const;
    bool isBottom() const;
    bool isCorner() const;
    bool isScreenEdge() const;
    bool triggersFor(const QPoint &cursorPos) const;
    bool check(const QPoint &cursorPos, const std::chrono::microseconds &triggerTime, bool forceNoPushBack = false);
    void markAsTriggered(const QPoint &cursorPos, const std::chrono::microseconds &triggerTime);
    bool isReserved() const;
    bool isRevealed() const;
    const QRect &approachGeometry() const;

    ElectricBorder border() const;
    void reserve(QObject *object, const char *slot);
    const QHash<QObject *, QByteArray> &callBacks() const;
    void reserveTouchCallBack(QAction *action, TouchCallback::CallbackFunction callback = nullptr);
    void unreserveTouchCallBack(QAction *action);
    void startApproaching();
    void stopApproaching();
    bool isApproaching() const;
    void setClient(Window *client);
    Window *client() const;
    void setClientInteracted(bool interacted);
    bool isClientInteracted() const;
    void setOutput(Output *output);
    Output *output() const;
    const QRect &geometry() const;
    void setTouchAction(ElectricBorderAction action);

    bool activatesForPointer() const;
    bool activatesForTouchGesture() const;

public Q_SLOTS:
    void reveal();
    void unreveal();
    void reserve();
    void unreserve();
    void unreserve(QObject *object);
    void setBorder(ElectricBorder border);
    void setAction(ElectricBorderAction action);
    void setGeometry(const QRect &geometry);
    void updateApproaching(const QPointF &point);
    void checkBlocking();
Q_SIGNALS:
    void approaching(ElectricBorder border, qreal factor, const QRect &geometry);
    void activatesForTouchGestureChanged();

protected:
    ScreenEdges *edges();
    const ScreenEdges *edges() const;
    bool isBlocked() const;

private:
    void activate();
    void deactivate();
    bool canActivate(const QPoint &cursorPos, const std::chrono::microseconds &triggerTime);
    void handle(const QPoint &cursorPos);
    bool handleAction(ElectricBorderAction action);
    bool handlePointerAction()
    {
        return handleAction(m_action);
    }
    bool handleTouchAction()
    {
        return handleAction(m_touchAction);
    }
    bool handleByCallback();
    void handleTouchCallback();
    void switchDesktop(const QPoint &cursorPos);
    void pushCursorBack(const QPoint &cursorPos);
    void reserveTouchCallBack(const TouchCallback &callback);
    QList<TouchCallback> touchCallBacks() const
    {
        return m_touchCallbacks;
    }
    ScreenEdges *m_edges;
    ElectricBorder m_border;
    ElectricBorderAction m_action;
    ElectricBorderAction m_touchAction = ElectricActionNone;
    int m_reserved;
    QRect m_geometry;
    QRect m_approachGeometry;
    std::optional<std::chrono::microseconds> m_lastTrigger = std::nullopt;
    std::optional<std::chrono::microseconds> m_lastReset = std::nullopt;
    QPoint m_triggeredPoint;
    QHash<QObject *, QByteArray> m_callBacks;
    bool m_approaching;
    int m_lastApproachingFactor;
    bool m_blocked;
    bool m_revealed = true;
    bool m_clientInteracted = false;
    bool m_pushBackBlocked;
    Window *m_client;
    Output *m_output;
    std::unique_ptr<SwipeGesture> m_gesture;
    QList<TouchCallback> m_touchCallbacks;
    friend class ScreenEdges;
};

/**
 * @short Class for controlling screen edges.
 *
 * The screen edge functionality is split into three parts:
 * @li This manager class ScreenEdges
 * @li abstract class @ref Edge
 * @li specific implementation of @ref Edge, e.g. WindowBasedEdge
 *
 * The ScreenEdges creates an @ref Edge for each screen edge which is also an edge in the
 * combination of all screens. E.g. if there are two screens, no Edge is created between the screens,
 * but at all other edges even if the screens have a different dimension.
 *
 * In addition at each corner of the overall display geometry an one-pixel large @ref Edge is
 * created. No matter how many screens there are, there will only be exactly four of these corner
 * edges. This is motivated by Fitts's Law which show that it's easy to trigger such a corner, but
 * it would be very difficult to trigger a corner between two screens (one pixel target not visually
 * outlined).
 *
 * The ScreenEdges are used for one of the following functionality:
 * @li switch virtual desktop (see property @ref desktopSwitching)
 * @li switch virtual desktop when moving a window (see property @ref desktopSwitchingMovingClients)
 * @li trigger a pre-defined action (see properties @ref actionTop and similar)
 * @li trigger an externally configured action (e.g. Effect, Script, see @ref reserve, @ref unreserve)
 *
 * An @ref Edge is only active if there is at least one of the possible actions "reserved" for this
 * edge. The idea is to not block the screen edge if nothing could be triggered there, so that the
 * user e.g. can configure nothing on the top edge, which tends to interfere with full screen apps
 * having a hidden panel there. On X11 (currently only supported backend) the @ref Edge is
 * represented by a WindowBasedEdge which creates an input only window for the geometry and
 * reacts on enter notify events. If the edge gets reserved for the first time a window is created
 * and mapped, once the edge gets unreserved again, the window gets destroyed.
 *
 * When the mouse enters one of the screen edges the following values are used to determine whether
 * the action should be triggered or the cursor be pushed back
 * @li Time difference between two entering events is not larger than a certain threshold
 * @li Time difference between two entering events is larger than @ref timeThreshold
 * @li Time difference between two activations is larger than @ref reActivateThreshold
 * @li Distance between two enter events is not larger than a defined pixel distance
 * These checks are performed in @ref Edge
 *
 * @todo change way how Effects/Scripts can reserve an edge and are notified.
 */
class KWIN_EXPORT ScreenEdges : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool desktopSwitching READ isDesktopSwitching)
    Q_PROPERTY(bool desktopSwitchingMovingClients READ isDesktopSwitchingMovingClients)
    Q_PROPERTY(QSize cursorPushBackDistance READ cursorPushBackDistance)
    Q_PROPERTY(int actionTopLeft READ actionTopLeft)
    Q_PROPERTY(int actionTop READ actionTop)
    Q_PROPERTY(int actionTopRight READ actionTopRight)
    Q_PROPERTY(int actionRight READ actionRight)
    Q_PROPERTY(int actionBottomRight READ actionBottomRight)
    Q_PROPERTY(int actionBottom READ actionBottom)
    Q_PROPERTY(int actionBottomLeft READ actionBottomLeft)
    Q_PROPERTY(int actionLeft READ actionLeft)
public:
    explicit ScreenEdges();
    /**
     * @internal
     */
    void setConfig(KSharedConfig::Ptr config);
    /**
     * Initialize the screen edges.
     * @internal
     */
    void init();
    /**
     * Check, if @p pos is in the approach geometry of any edge.
     */
    bool inApproachGeometry(const QPoint &pos) const;
    /**
     * The (dpi dependent) length, reserved for the active corners of each edge - 1/3"
     */
    int cornerOffset() const;
    /**
     * Mark the specified screen edge as reserved. This method is provided for external activation
     * like effects and scripts. When the effect/script does no longer need the edge it is supposed
     * to call @ref unreserve.
     * @param border the screen edge to mark as reserved
     * @param object The object on which the @p callback needs to be invoked
     * @param callback The method name to be invoked - uses QMetaObject::invokeMethod
     * @see unreserve
     * @todo: add pointer to script/effect
     */
    void reserve(ElectricBorder border, QObject *object, const char *callback);
    /**
     * Mark the specified screen edge as unreserved. This method is provided for external activation
     * like effects and scripts. This method is only allowed to be called if @ref reserve had been
     * called before for the same @p border. An unbalanced calling of reserve/unreserve leads to the
     * edge never being active or never being able to deactivate again.
     * @param border the screen edge to mark as unreserved
     * @param object the object on which the callback had been invoked
     * @see reserve
     * @todo: add pointer to script/effect
     */
    void unreserve(ElectricBorder border, QObject *object);
    /**
     * Reserves an edge for the @p client. The idea behind this is to show the @p client if the
     * screen edge which the @p client borders gets triggered.
     *
     * When first called it is tried to create an Edge for the client. This is only done if the
     * client borders with a screen edge specified by @p border. If the client doesn't border the
     * screen edge, no Edge gets created and the client is shown again. Otherwise there would not
     * be a possibility to show the client again.
     *
     * On subsequent calls for the client no new Edge is created, but the existing one gets reused
     * and if the client is already hidden, the Edge gets reserved.
     *
     * The Edge gets automatically destroyed if the client gets released.
     * @param client The Client for which an Edge should be reserved
     * @param border The border which the client wants to use, only proper borders are supported (no corners)
     */
    bool reserve(KWin::Window *client, ElectricBorder border);
    void unreserve(KWin::Window *client);
    void reveal(KWin::Window *client);
    void unreveal(KWin::Window *client);

    /**
     * Mark the specified screen edge as reserved for touch gestures. This method is provided for
     * external activation like effects and scripts.
     * When the effect/script does no longer need the edge it is supposed
     * to call @ref unreserveTouch.
     * @param border the screen edge to mark as reserved
     * @param action The action which gets triggered
     * @see unreserveTouch
     * @since 5.10
     */
    void reserveTouch(ElectricBorder border, QAction *action, TouchCallback::CallbackFunction callback = nullptr);
    /**
     * Unreserves the specified @p border from activating the @p action for touch gestures.
     * @see reserveTouch
     * @since 5.10
     */
    void unreserveTouch(ElectricBorder border, QAction *action);

    /**
     * Reserve desktop switching for screen edges, if @p isToReserve is @c true. Unreserve otherwise.
     * @param isToReserve indicated whether desktop switching should be reserved or unreseved
     * @param o Qt orientations
     */
    void reserveDesktopSwitching(bool isToReserve, Qt::Orientations o);
    bool isEntered(const QPointF &pos, std::chrono::microseconds timestamp);
    bool touchDown(const QPointF &pos);

    bool isDesktopSwitching() const;
    bool isDesktopSwitchingMovingClients() const;
    const QSize &cursorPushBackDistance() const;
    /**
     * Minimum time between the push back of the cursor and the activation by re-entering the edge.
     */
    std::chrono::milliseconds timeThreshold() const;
    /**
     * Minimum time between triggers
     */
    std::chrono::milliseconds reActivationThreshold() const;
    ElectricBorderAction actionTopLeft() const;
    ElectricBorderAction actionTop() const;
    ElectricBorderAction actionTopRight() const;
    ElectricBorderAction actionRight() const;
    ElectricBorderAction actionBottomRight() const;
    ElectricBorderAction actionBottom() const;
    ElectricBorderAction actionBottomLeft() const;
    ElectricBorderAction actionLeft() const;

    ElectricBorderAction actionForTouchBorder(ElectricBorder border) const;

    GestureRecognizer *gestureRecognizer() const
    {
        return m_gestureRecognizer;
    }

    bool remainActiveOnFullscreen() const;
    const std::vector<std::unique_ptr<Edge>> &edges() const;

public Q_SLOTS:
    void reconfigure();
    /**
     * Updates the layout of virtual desktops and adjust the reserved borders in case of
     * virtual desktop switching on edges.
     */
    void updateLayout();
    /**
     * Recreates all edges e.g. after the screen size changes.
     */
    void recreateEdges();

Q_SIGNALS:
    /**
     * Signal emitted during approaching of mouse towards @p border. The @p factor indicates how
     * far away the mouse is from the approaching area. The values are clamped into [0.0,1.0] with
     * @c 0.0 meaning far away from the border, @c 1.0 in trigger distance.
     */
    void approaching(ElectricBorder border, qreal factor, const QRect &geometry);
    void checkBlocking();

private:
    enum {
        ElectricDisabled = 0,
        ElectricMoveOnly = 1,
        ElectricAlways = 2,
    };
    void setDesktopSwitching(bool enable);
    void setDesktopSwitchingMovingClients(bool enable);
    void setCursorPushBackDistance(const QSize &distance);
    void setTimeThreshold(std::chrono::milliseconds threshold);
    void setReActivationThreshold(std::chrono::milliseconds threshold);
    void createHorizontalEdge(ElectricBorder border, const QRect &screen, const QRect &fullArea, Output *output);
    void createVerticalEdge(ElectricBorder border, const QRect &screen, const QRect &fullArea, Output *output);
    std::unique_ptr<Edge> createEdge(ElectricBorder border, int x, int y, int width, int height, Output *output, bool createAction = true);
    void setActionForBorder(ElectricBorder border, ElectricBorderAction *oldValue, ElectricBorderAction newValue);
    void setActionForTouchBorder(ElectricBorder border, ElectricBorderAction newValue);
    void setRemainActiveOnFullscreen(bool remainActive);
    ElectricBorderAction actionForEdge(Edge *edge) const;
    ElectricBorderAction actionForTouchEdge(Edge *edge) const;
    Edge *edgeForClient(Window *client) const;
    Edge *createEdgeForClient(Window *client);
    void deleteEdgeForClient(Window *client);
    bool m_desktopSwitching;
    bool m_desktopSwitchingMovingClients;
    QSize m_cursorPushBackDistance;
    std::chrono::milliseconds m_timeThreshold = std::chrono::milliseconds::zero();
    std::chrono::milliseconds m_reactivateThreshold = std::chrono::milliseconds::zero();
    Qt::Orientations m_virtualDesktopLayout;
    std::vector<std::unique_ptr<Edge>> m_edges;
    KSharedConfig::Ptr m_config;
    ElectricBorderAction m_actionTopLeft;
    ElectricBorderAction m_actionTop;
    ElectricBorderAction m_actionTopRight;
    ElectricBorderAction m_actionRight;
    ElectricBorderAction m_actionBottomRight;
    ElectricBorderAction m_actionBottom;
    ElectricBorderAction m_actionBottomLeft;
    ElectricBorderAction m_actionLeft;
    QMap<ElectricBorder, ElectricBorderAction> m_touchCallbacks;
    const int m_cornerOffset;
    GestureRecognizer *m_gestureRecognizer;
    bool m_remainActiveOnFullscreen = false;
};

/**********************************************************
 * Inlines Edge
 *********************************************************/

inline bool Edge::isBottom() const
{
    return m_border == ElectricBottom || m_border == ElectricBottomLeft || m_border == ElectricBottomRight;
}

inline bool Edge::isLeft() const
{
    return m_border == ElectricLeft || m_border == ElectricTopLeft || m_border == ElectricBottomLeft;
}

inline bool Edge::isRight() const
{
    return m_border == ElectricRight || m_border == ElectricTopRight || m_border == ElectricBottomRight;
}

inline bool Edge::isTop() const
{
    return m_border == ElectricTop || m_border == ElectricTopLeft || m_border == ElectricTopRight;
}

inline bool Edge::isCorner() const
{
    return m_border == ElectricTopLeft
        || m_border == ElectricTopRight
        || m_border == ElectricBottomRight
        || m_border == ElectricBottomLeft;
}

inline bool Edge::isScreenEdge() const
{
    return m_border == ElectricLeft
        || m_border == ElectricRight
        || m_border == ElectricTop
        || m_border == ElectricBottom;
}

inline bool Edge::isReserved() const
{
    return m_reserved != 0;
}

inline bool Edge::isRevealed() const
{
    return m_revealed;
}

inline void Edge::setAction(ElectricBorderAction action)
{
    m_action = action;
}

inline ScreenEdges *Edge::edges()
{
    return m_edges;
}

inline const ScreenEdges *Edge::edges() const
{
    return m_edges;
}

inline const QRect &Edge::geometry() const
{
    return m_geometry;
}

inline const QRect &Edge::approachGeometry() const
{
    return m_approachGeometry;
}

inline ElectricBorder Edge::border() const
{
    return m_border;
}

inline const QHash<QObject *, QByteArray> &Edge::callBacks() const
{
    return m_callBacks;
}

inline bool Edge::isBlocked() const
{
    return m_blocked;
}

inline Window *Edge::client() const
{
    return m_client;
}

inline bool Edge::isClientInteracted() const
{
    return m_clientInteracted;
}

inline bool Edge::isApproaching() const
{
    return m_approaching;
}

/**********************************************************
 * Inlines ScreenEdges
 *********************************************************/
inline void ScreenEdges::setConfig(KSharedConfig::Ptr config)
{
    m_config = config;
}

inline int ScreenEdges::cornerOffset() const
{
    return m_cornerOffset;
}

inline const QSize &ScreenEdges::cursorPushBackDistance() const
{
    return m_cursorPushBackDistance;
}

inline bool ScreenEdges::isDesktopSwitching() const
{
    return m_desktopSwitching;
}

inline bool ScreenEdges::isDesktopSwitchingMovingClients() const
{
    return m_desktopSwitchingMovingClients;
}

inline std::chrono::milliseconds ScreenEdges::reActivationThreshold() const
{
    return m_reactivateThreshold;
}

inline std::chrono::milliseconds ScreenEdges::timeThreshold() const
{
    return m_timeThreshold;
}

inline void ScreenEdges::setCursorPushBackDistance(const QSize &distance)
{
    m_cursorPushBackDistance = distance;
}

inline void ScreenEdges::setDesktopSwitching(bool enable)
{
    if (enable == m_desktopSwitching) {
        return;
    }
    m_desktopSwitching = enable;
    reserveDesktopSwitching(enable, m_virtualDesktopLayout);
}

inline void ScreenEdges::setDesktopSwitchingMovingClients(bool enable)
{
    m_desktopSwitchingMovingClients = enable;
}

inline void ScreenEdges::setReActivationThreshold(std::chrono::milliseconds threshold)
{
    Q_ASSERT(threshold >= m_timeThreshold);
    m_reactivateThreshold = threshold;
}

inline void ScreenEdges::setTimeThreshold(std::chrono::milliseconds threshold)
{
    m_timeThreshold = threshold;
}

#define ACTION(name)                                      \
    inline ElectricBorderAction ScreenEdges::name() const \
    {                                                     \
        return m_##name;                                  \
    }

ACTION(actionTopLeft)
ACTION(actionTop)
ACTION(actionTopRight)
ACTION(actionRight)
ACTION(actionBottomRight)
ACTION(actionBottom)
ACTION(actionBottomLeft)
ACTION(actionLeft)

#undef ACTION

}
