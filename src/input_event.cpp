/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "input_event.h"

namespace KWin
{

MouseEvent::MouseEvent(QEvent::Type type, const QPointF &pos, Qt::MouseButton button,
                       Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, std::chrono::microseconds timestamp,
                       const QPointF &delta, const QPointF &deltaNonAccelerated, InputDevice *device)
    : QMouseEvent(type, pos, pos, button, buttons, modifiers)
    , m_delta(delta)
    , m_deltaUnccelerated(deltaNonAccelerated)
    , m_timestamp(timestamp)
    , m_device(device)
{
    setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count());
}

WheelEvent::WheelEvent(const QPointF &pos, qreal delta, qint32 deltaV120, Qt::Orientation orientation,
                       Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, InputRedirection::PointerAxisSource source,
                       std::chrono::microseconds timestamp, InputDevice *device)
    : QWheelEvent(pos, pos, QPoint(), (orientation == Qt::Horizontal) ? QPoint(delta, 0) : QPoint(0, delta), buttons, modifiers, Qt::NoScrollPhase, false)
    , m_device(device)
    , m_orientation(orientation)
    , m_delta(delta)
    , m_deltaV120(deltaV120)
    , m_source(source)
    , m_timestamp(timestamp)
{
    setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count());
}

std::chrono::microseconds WheelEvent::timestamp() const
{
    return m_timestamp;
}

KeyEvent::KeyEvent(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers, quint32 code, quint32 keysym,
                   const QString &text, bool autorepeat, std::chrono::microseconds timestamp, InputDevice *device)
    : QKeyEvent(type, key, modifiers, code, keysym, 0, text, autorepeat)
    , m_device(device)
    , m_timestamp(timestamp)
{
    setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count());
}

std::chrono::microseconds KeyEvent::timestamp() const
{
    return m_timestamp;
}

SwitchEvent::SwitchEvent(State state, std::chrono::microseconds timestamp, InputDevice *device)
    : QEvent(QEvent::User)
    , m_state(state)
    , m_timestamp(timestamp)
    , m_device(device)
{
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
TabletEvent::TabletEvent(Type t, const QPointingDevice *dev, const QPointF &pos, const QPointF &globalPos,
                         qreal pressure, float xTilt, float yTilt,
                         float tangentialPressure, qreal rotation, float z,
                         Qt::KeyboardModifiers keyState, Qt::MouseButton button, Qt::MouseButtons buttons, const TabletToolId &tabletId)
    : QTabletEvent(t, dev, pos, globalPos, pressure, xTilt, yTilt, tangentialPressure, rotation, z, keyState, button, buttons)
    , m_id(tabletId)
{
}
#else
TabletEvent::TabletEvent(Type t, const QPointF &pos, const QPointF &globalPos,
                         int device, int pointerType, qreal pressure, int xTilt, int yTilt,
                         qreal tangentialPressure, qreal rotation, int z,
                         Qt::KeyboardModifiers keyState, qint64 uniqueID,
                         Qt::MouseButton button, Qt::MouseButtons buttons, const TabletToolId &tabletId)
    : QTabletEvent(t, pos, globalPos, device, pointerType, pressure, xTilt, yTilt, tangentialPressure, rotation, z, keyState, uniqueID, button, buttons)
    , m_id(tabletId)
{
}
#endif

}
