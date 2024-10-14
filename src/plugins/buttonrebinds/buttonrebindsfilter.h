/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redono.de>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"
#include <array>
#include <optional>
#include <variant>

#include "core/inputdevice.h"
#include "input.h"
#include "input_event.h"

#include <QKeySequence>

class InputDevice : public KWin::InputDevice
{
    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    bool isKeyboard() const override;
    bool isPointer() const override;
    bool isTouchpad() const override;
    bool isTouch() const override;
    bool isTabletTool() const override;
    bool isTabletPad() const override;
    bool isTabletModeSwitch() const override;
    bool isLidSwitch() const override;
};

struct Trigger
{
    QString device;
    uint button;
    bool operator==(const Trigger &o) const
    {
        return button == o.button && device == o.device;
    }
};

class ButtonRebindsFilter : public KWin::Plugin, public KWin::InputEventFilter
{
    Q_OBJECT
public:
    enum TriggerType {
        Pointer,
        TabletPad,
        TabletToolButtonType,
        TabletDial,
        LastType
    };
    Q_ENUM(TriggerType)
    using SingleKeybind = QKeySequence;
    struct AxisKeybind
    {
        QKeySequence up;
        QKeySequence down;
    };
    struct TabletToolButton
    {
        quint32 button;
    };
    struct MouseButton
    {
        quint32 button;
        Qt::KeyboardModifiers modifiers;
    };
    struct DisabledButton
    {
    };
    struct ScrollWheel
    {
    };

    explicit ButtonRebindsFilter();
    ~ButtonRebindsFilter() override;
    bool pointerButton(KWin::PointerButtonEvent *event) override;
    bool tabletToolProximityEvent(KWin::TabletToolProximityEvent *event) override;
    bool tabletToolAxisEvent(KWin::TabletToolAxisEvent *event) override;
    bool tabletToolTipEvent(KWin::TabletToolTipEvent *event) override;
    bool tabletPadButtonEvent(KWin::TabletPadButtonEvent *event) override;
    bool tabletToolButtonEvent(KWin::TabletToolButtonEvent *event) override;
    bool tabletPadDialEvent(KWin::TabletPadDialEvent *event) override;

private:
    void loadConfig(const KConfigGroup &group);
    void insert(TriggerType type, const Trigger &trigger, const QStringList &action);
    bool send(TriggerType type, const Trigger &trigger, bool pressed, double delta, std::chrono::microseconds timestamp);
    bool sendKeySequence(const QKeySequence &sequence, bool pressed, std::chrono::microseconds time);
    bool sendKeyModifiers(const Qt::KeyboardModifiers &modifiers, bool pressed, std::chrono::microseconds time);
    bool sendMouseButton(quint32 button, bool pressed, std::chrono::microseconds time);
    bool sendMousePosition(QPointF position, std::chrono::microseconds time);
    bool sendMouseFrame();
    bool sendTabletToolButton(quint32 button, bool pressed, std::chrono::microseconds time);
    bool sendScrollWheel(double delta, std::chrono::microseconds time);

    std::unique_ptr<InputDevice> m_inputDevice;
    std::array<QHash<Trigger, std::variant<SingleKeybind, AxisKeybind, MouseButton, TabletToolButton, DisabledButton, ScrollWheel>>, LastType> m_actions;
    KConfigWatcher::Ptr m_configWatcher;
    QPointer<KWin::InputDeviceTabletTool> m_tabletTool;
    QPointF m_cursorPos, m_tabletCursorPos;
};
