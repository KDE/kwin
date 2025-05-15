/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "cursor.h"
#include "input.h"
#include "utils/cursortheme.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QPointer>

class QWindow;

namespace KWin
{
class Window;
class CursorImage;
class InputDevice;
class InputRedirection;
class CursorShape;
class ShapeCursorSource;
class SurfaceCursorSource;
class PointerSurfaceCursor;
class SurfaceInterface;

namespace Decoration
{
class DecoratedWindowImpl;
}

class KWIN_EXPORT PointerInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit PointerInputRedirection(InputRedirection *parent);
    ~PointerInputRedirection() override;

    void init() override;

    CursorTheme cursorTheme() const; // TODO: Make it a Cursor property

    void updateAfterScreenChange();
    bool supportsWarping() const;
    void warp(const QPointF &pos);

    QPointF pos() const
    {
        return m_pos;
    }
    Qt::MouseButtons buttons() const
    {
        return m_qtButtons;
    }
    bool areButtonsPressed() const;

    void setEffectsOverrideCursor(Qt::CursorShape shape);
    void removeEffectsOverrideCursor();
    void setWindowSelectionCursor(const QByteArray &shape);
    void removeWindowSelectionCursor();

    void updatePointerConstraints();

    void setEnableConstraints(bool set);

    bool isConstrained() const
    {
        return m_confined || m_locked;
    }

    bool focusUpdatesBlocked() override;

    /**
     * @internal
     */
    void processMotionAbsolute(const QPointF &pos, std::chrono::microseconds time, InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processMotion(const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time, InputDevice *device);
    /**
     * @internal
     */
    void processButton(uint32_t button, PointerButtonState state, std::chrono::microseconds time, InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processAxis(PointerAxis axis, qreal delta, qint32 deltaV120, PointerAxisSource source, bool inverted, std::chrono::microseconds time, InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processHoldGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processHoldGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processHoldGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processStrokeGestureBegin(const QList<QPointF> &points, std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processStrokeGestureUpdate(const QList<QPointF> &points, std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processStrokeGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processStrokeGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device = nullptr);
    /**
     * @internal
     */
    void processFrame(KWin::InputDevice *device = nullptr);

private:
    enum class EdgeBarrierType {
        NormalBarrier,
        WindowMoveBarrier,
        // WindowResize is separate from WindowMove since there is edge snapping during resize, so a different resistance might be desirable
        WindowResizeBarrier,
        EdgeElementBarrier,
        CornerBarrier,
    };
    void processWarp(const QPointF &pos, std::chrono::microseconds time, InputDevice *device = nullptr);
    enum class MotionType {
        Motion,
        Warp
    };
    void processMotionInternal(const QPointF &pos, const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time, InputDevice *device, MotionType type);
    void cleanupDecoration(Decoration::DecoratedWindowImpl *old, Decoration::DecoratedWindowImpl *now) override;

    void focusUpdate(Window *focusOld, Window *focusNow) override;

    QPointF position() const override;

    void updateOnStartMoveResize();
    void updateToReset();
    void updatePosition(const QPointF &pos, std::chrono::microseconds time);
    void updateButton(uint32_t button, PointerButtonState state);
    QPointF applyEdgeBarrier(const QPointF &pos, const Output *currentOutput, std::chrono::microseconds time);
    EdgeBarrierType edgeBarrierType(const QPointF &pos, const QRectF &lastOutputGeometry) const;
    qreal edgeBarrier(EdgeBarrierType type) const;
    QPointF applyPointerConfinement(const QPointF &pos) const;
    void disconnectConfinedPointerRegionConnection();
    void disconnectLockedPointerAboutToBeUnboundConnection();
    void disconnectPointerConstraintsConnection();
    void breakPointerConstraints(SurfaceInterface *surface);
    CursorImage *m_cursor;
    QPointF m_pos;
    QHash<uint32_t, PointerButtonState> m_buttons;
    Qt::MouseButtons m_qtButtons;
    QMetaObject::Connection m_focusGeometryConnection;
    QMetaObject::Connection m_constraintsConnection;
    QMetaObject::Connection m_constraintsActivatedConnection;
    QMetaObject::Connection m_confinedPointerRegionConnection;
    QMetaObject::Connection m_lockedPointerAboutToBeUnboundConnection;
    QMetaObject::Connection m_decorationGeometryConnection;
    QMetaObject::Connection m_decorationDestroyedConnection;
    QMetaObject::Connection m_decorationClosedConnection;
    bool m_confined = false;
    bool m_locked = false;
    bool m_enableConstraints = true;
    bool m_lastOutputWasPlaceholder = true;
    QPointF m_movementInEdgeBarrier;
    std::chrono::microseconds m_lastMoveTime = std::chrono::microseconds::zero();
    friend class PositionUpdateBlocker;
    EdgeBarrierType m_lastEdgeBarrierType = EdgeBarrierType::NormalBarrier;
};

class WaylandCursorImage : public QObject
{
    Q_OBJECT
public:
    explicit WaylandCursorImage(QObject *parent = nullptr);

    CursorTheme theme() const;

Q_SIGNALS:
    void themeChanged();

private:
    void updateCursorTheme();

    CursorTheme m_cursorTheme;
};

class CursorImage : public QObject
{
    Q_OBJECT
public:
    explicit CursorImage(PointerInputRedirection *parent = nullptr);
    ~CursorImage() override;

    void setEffectsOverrideCursor(Qt::CursorShape shape);
    void removeEffectsOverrideCursor();
    void setWindowSelectionCursor(const QByteArray &shape);
    void removeWindowSelectionCursor();

    CursorTheme theme() const;
    CursorSource *source() const;
    void setSource(CursorSource *source);

    void updateCursorOutputs(const QPointF &pos);

Q_SIGNALS:
    void changed();

private:
    void reevaluteSource();
    void updateServerCursor(const std::variant<PointerSurfaceCursor *, QByteArray> &cursor);
    void updateDecoration();
    void updateDecorationCursor();
    void updateMoveResize();
    void updateDragCursor();

    void handleFocusedSurfaceChanged();

    PointerInputRedirection *m_pointer;
    CursorSource *m_currentSource = nullptr;
    WaylandCursorImage m_waylandImage;

    std::unique_ptr<ShapeCursorSource> m_effectsCursor;
    std::unique_ptr<ShapeCursorSource> m_fallbackCursor;
    std::unique_ptr<ShapeCursorSource> m_moveResizeCursor;
    std::unique_ptr<ShapeCursorSource> m_windowSelectionCursor;
    std::unique_ptr<ShapeCursorSource> m_dragCursor;

    struct
    {
        std::unique_ptr<ShapeCursorSource> cursor;
        QMetaObject::Connection connection;
    } m_decoration;
    struct
    {
        QMetaObject::Connection connection;
        std::unique_ptr<SurfaceCursorSource> surface;
        std::unique_ptr<ShapeCursorSource> shape;
        CursorSource *cursor = nullptr;
    } m_serverCursor;
};
}
