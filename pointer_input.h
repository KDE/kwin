/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_POINTER_INPUT_H
#define KWIN_POINTER_INPUT_H

#include "input.h"
#include "cursor.h"
#include "xcursortheme.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QPointF>

class QWindow;

namespace KWaylandServer
{
class SurfaceInterface;
}

namespace KWin
{
class CursorImage;
class InputRedirection;
class Toplevel;
class CursorShape;

namespace Decoration
{
class DecoratedClientImpl;
}

namespace LibInput
{
class Device;
}

uint32_t qtMouseButtonToButton(Qt::MouseButton button);

class KWIN_EXPORT PointerInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit PointerInputRedirection(InputRedirection *parent);
    ~PointerInputRedirection() override;

    void init() override;

    void updateAfterScreenChange();
    bool supportsWarping() const;
    void warp(const QPointF &pos);

    QPointF pos() const {
        return m_pos;
    }
    Qt::MouseButtons buttons() const {
        return m_qtButtons;
    }
    bool areButtonsPressed() const;

    void setEffectsOverrideCursor(Qt::CursorShape shape);
    void removeEffectsOverrideCursor();
    void setWindowSelectionCursor(const QByteArray &shape);
    void removeWindowSelectionCursor();

    void updatePointerConstraints();

    void setEnableConstraints(bool set);

    bool isConstrained() const {
        return m_confined || m_locked;
    }

    bool focusUpdatesBlocked() override;

    /**
     * @internal
     */
    void processMotion(const QPointF &pos, uint32_t time, LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processMotion(const QPointF &pos, const QSizeF &delta, const QSizeF &deltaNonAccelerated, uint32_t time, quint64 timeUsec, LibInput::Device *device);
    /**
     * @internal
     */
    void processButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time, LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processAxis(InputRedirection::PointerAxis axis, qreal delta, qint32 discreteDelta, InputRedirection::PointerAxisSource source, uint32_t time, LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureUpdate(const QSizeF &delta, quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureEnd(quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureCancelled(quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureEnd(quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureCancelled(quint32 time, KWin::LibInput::Device *device = nullptr);

private:
    void cleanupInternalWindow(QWindow *old, QWindow *now) override;
    void cleanupDecoration(Decoration::DecoratedClientImpl *old, Decoration::DecoratedClientImpl *now) override;

    void focusUpdate(Toplevel *focusOld, Toplevel *focusNow) override;

    QPointF position() const override;

    void updateOnStartMoveResize();
    void updateToReset();
    void updatePosition(const QPointF &pos);
    void updateButton(uint32_t button, InputRedirection::PointerButtonState state);
    void warpXcbOnSurfaceLeft(KWaylandServer::SurfaceInterface *surface);
    QPointF applyPointerConfinement(const QPointF &pos) const;
    void disconnectConfinedPointerRegionConnection();
    void disconnectLockedPointerAboutToBeUnboundConnection();
    void disconnectPointerConstraintsConnection();
    void breakPointerConstraints(KWaylandServer::SurfaceInterface *surface);
    CursorImage *m_cursor;
    bool m_supportsWarping;
    QPointF m_pos;
    QHash<uint32_t, InputRedirection::PointerButtonState> m_buttons;
    Qt::MouseButtons m_qtButtons;
    QMetaObject::Connection m_focusGeometryConnection;
    QMetaObject::Connection m_internalWindowConnection;
    QMetaObject::Connection m_constraintsConnection;
    QMetaObject::Connection m_constraintsActivatedConnection;
    QMetaObject::Connection m_confinedPointerRegionConnection;
    QMetaObject::Connection m_lockedPointerAboutToBeUnboundConnection;
    QMetaObject::Connection m_decorationGeometryConnection;
    bool m_confined = false;
    bool m_locked = false;
    bool m_enableConstraints = true;
};

class WaylandCursorImage : public QObject
{
    Q_OBJECT
public:
    explicit WaylandCursorImage(QObject *parent = nullptr);

    struct Image {
        QImage image;
        QPoint hotspot;
    };

    void loadThemeCursor(const CursorShape &shape, Image *cursorImage);
    void loadThemeCursor(const QByteArray &name, Image *cursorImage);

Q_SIGNALS:
    void themeChanged();

private:
    bool loadThemeCursor_helper(const QByteArray &name, Image *cursorImage);
    bool ensureCursorTheme();
    void invalidateCursorTheme();

    KXcursorTheme m_cursorTheme;
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

    QImage image() const;
    QPoint hotSpot() const;
    void markAsRendered();

Q_SIGNALS:
    void changed();

private:
    void reevaluteSource();
    void update();
    void updateServerCursor();
    void updateDecoration();
    void updateDecorationCursor();
    void updateMoveResize();
    void updateDrag();
    void updateDragCursor();

    void loadThemeCursor(CursorShape shape, WaylandCursorImage::Image *image);
    void loadThemeCursor(const QByteArray &shape, WaylandCursorImage::Image *image);

    enum class CursorSource {
        LockScreen,
        EffectsOverride,
        MoveResize,
        PointerSurface,
        Decoration,
        DragAndDrop,
        Fallback,
        WindowSelector
    };
    void setSource(CursorSource source);

    PointerInputRedirection *m_pointer;
    CursorSource m_currentSource = CursorSource::Fallback;
    WaylandCursorImage m_waylandImage;

    WaylandCursorImage::Image m_effectsCursor;
    WaylandCursorImage::Image m_decorationCursor;
    QMetaObject::Connection m_decorationConnection;
    WaylandCursorImage::Image m_fallbackCursor;
    WaylandCursorImage::Image m_moveResizeCursor;
    WaylandCursorImage::Image m_windowSelectionCursor;
    QElapsedTimer m_surfaceRenderedTimer;
    struct {
        WaylandCursorImage::Image cursor;
        QMetaObject::Connection connection;
    } m_drag;
    struct {
        QMetaObject::Connection connection;
        WaylandCursorImage::Image cursor;
    } m_serverCursor;
};

/**
 * @brief Implementation using the InputRedirection framework to get pointer positions.
 *
 * Does not support warping of cursor.
 */
class InputRedirectionCursor : public KWin::Cursor
{
    Q_OBJECT
public:
    explicit InputRedirectionCursor(QObject *parent);
    ~InputRedirectionCursor() override;
protected:
    void doSetPos() override;
    void doStartCursorTracking() override;
    void doStopCursorTracking() override;
private Q_SLOTS:
    void slotPosChanged(const QPointF &pos);
    void slotPointerButtonChanged();
    void slotModifiersChanged(Qt::KeyboardModifiers mods, Qt::KeyboardModifiers oldMods);
private:
    Qt::MouseButtons m_currentButtons;
};

}

#endif
