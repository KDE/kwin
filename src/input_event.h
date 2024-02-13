/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"

#include <QInputEvent>
#include <chrono>

namespace KWin
{

class InputDevice;

class MouseEvent : public QMouseEvent
{
public:
    explicit MouseEvent(QEvent::Type type, const QPointF &pos, Qt::MouseButton button, Qt::MouseButtons buttons,
                        Qt::KeyboardModifiers modifiers, std::chrono::microseconds timestamp,
                        const QPointF &delta, const QPointF &deltaNonAccelerated, InputDevice *device);

    QPointF delta() const
    {
        return m_delta;
    }

    QPointF deltaUnaccelerated() const
    {
        return m_deltaUnccelerated;
    }

    std::chrono::microseconds timestamp() const
    {
        return m_timestamp;
    }

    InputDevice *device() const
    {
        return m_device;
    }

    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const
    {
        return m_modifiersRelevantForShortcuts;
    }

    void setModifiersRelevantForGlobalShortcuts(const Qt::KeyboardModifiers &mods)
    {
        m_modifiersRelevantForShortcuts = mods;
    }

    quint32 nativeButton() const
    {
        return m_nativeButton;
    }

    void setNativeButton(quint32 button)
    {
        m_nativeButton = button;
    }

private:
    QPointF m_delta;
    QPointF m_deltaUnccelerated;
    std::chrono::microseconds m_timestamp;
    InputDevice *m_device;
    Qt::KeyboardModifiers m_modifiersRelevantForShortcuts = Qt::KeyboardModifiers();
    quint32 m_nativeButton = 0;
};

// TODO: Don't derive from QWheelEvent, this event is quite domain specific.
class WheelEvent : public QWheelEvent
{
public:
    explicit WheelEvent(const QPointF &pos, qreal delta, qint32 deltaV120, Qt::Orientation orientation,
                        Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, InputRedirection::PointerAxisSource source,
                        std::chrono::microseconds timestamp, InputDevice *device);

    Qt::Orientation orientation() const
    {
        return m_orientation;
    }

    qreal delta() const
    {
        return m_delta;
    }

    qint32 deltaV120() const
    {
        return m_deltaV120;
    }

    InputRedirection::PointerAxisSource axisSource() const
    {
        return m_source;
    }

    InputDevice *device() const
    {
        return m_device;
    }

    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const
    {
        return m_modifiersRelevantForShortcuts;
    }

    void setModifiersRelevantForGlobalShortcuts(const Qt::KeyboardModifiers &mods)
    {
        m_modifiersRelevantForShortcuts = mods;
    }

    std::chrono::microseconds timestamp() const;

private:
    InputDevice *m_device;
    Qt::Orientation m_orientation;
    qreal m_delta;
    qint32 m_deltaV120;
    InputRedirection::PointerAxisSource m_source;
    Qt::KeyboardModifiers m_modifiersRelevantForShortcuts = Qt::KeyboardModifiers();
    const std::chrono::microseconds m_timestamp;
};

class KeyEvent : public QKeyEvent
{
public:
    explicit KeyEvent(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers, quint32 code, quint32 keysym,
                      const QString &text, bool autorepeat, std::chrono::microseconds timestamp, InputDevice *device);

    InputDevice *device() const
    {
        return m_device;
    }

    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const
    {
        return m_modifiersRelevantForShortcuts;
    }

    void setModifiersRelevantForGlobalShortcuts(const Qt::KeyboardModifiers &mods)
    {
        m_modifiersRelevantForShortcuts = mods;
    }

    std::chrono::microseconds timestamp() const;

private:
    InputDevice *m_device;
    Qt::KeyboardModifiers m_modifiersRelevantForShortcuts = Qt::KeyboardModifiers();
    const std::chrono::microseconds m_timestamp;
};

class SwitchEvent : public QEvent
{
public:
    enum class State {
        Off,
        On
    };
    explicit SwitchEvent(State state, std::chrono::microseconds timestamp, InputDevice *device);

    State state() const
    {
        return m_state;
    }

    std::chrono::microseconds timestamp() const
    {
        return m_timestamp;
    }

    InputDevice *device() const
    {
        return m_device;
    }

private:
    State m_state;
    std::chrono::microseconds m_timestamp;
    InputDevice *m_device;
};

class TabletToolId
{
public:
    QString deviceSysName;
    InputRedirection::TabletToolType m_toolType;
    QVector<InputRedirection::Capability> m_capabilities;
    quint64 m_serialId;
    quint64 m_uniqueId;
    void *m_deviceGroupData;
    QString m_name;
};

class TabletPadId
{
public:
    const QString name;
    void *data;
};

class TabletEvent : public QTabletEvent
{
public:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    TabletEvent(Type t, const QPointingDevice *dev, const QPointF &pos, const QPointF &globalPos,
                qreal pressure, float xTilt, float yTilt,
                float tangentialPressure, qreal rotation, float z,
                Qt::KeyboardModifiers keyState, Qt::MouseButton button, Qt::MouseButtons buttons, const TabletToolId &tabletId);
#else
    TabletEvent(Type t, const QPointF &pos, const QPointF &globalPos,
                int device, int pointerType, qreal pressure, int xTilt, int yTilt,
                qreal tangentialPressure, qreal rotation, int z,
                Qt::KeyboardModifiers keyState, qint64 uniqueID,
                Qt::MouseButton button, Qt::MouseButtons buttons, const TabletToolId &tabletId);
#endif

    const TabletToolId &tabletId() const
    {
        return m_id;
    }

private:
    const TabletToolId m_id;
};

}
