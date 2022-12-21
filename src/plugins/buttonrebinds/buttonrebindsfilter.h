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
    QString sysName() const override;
    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    void setLeds(KWin::LEDs leds) override;
    KWin::LEDs leds() const override;

    bool isKeyboard() const override;
    bool isAlphaNumericKeyboard() const override;
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
        LastType
    };
    Q_ENUM(TriggerType);
    struct TabletToolButton
    {
        quint32 button;
    };
    struct MouseButton
    {
        quint32 button;
    };

    explicit ButtonRebindsFilter();
    bool pointerEvent(KWin::MouseEvent *event, quint32 nativeButton) override;
    bool tabletPadButtonEvent(uint button, bool pressed, const KWin::TabletPadId &tabletPadId, std::chrono::microseconds time) override;
    bool tabletToolButtonEvent(uint button, bool pressed, const KWin::TabletToolId &tabletToolId, std::chrono::microseconds time) override;

private:
    void loadConfig(const KConfigGroup &group);
    void insert(TriggerType type, const Trigger &trigger, const QStringList &action);
    bool send(TriggerType type, const Trigger &trigger, bool pressed, std::chrono::microseconds timestamp);
    bool sendKeySequence(const QKeySequence &sequence, bool pressed, std::chrono::microseconds time);
    bool sendMouseButton(quint32 button, bool pressed, std::chrono::microseconds time);
    bool sendTabletToolButton(quint32 button, bool pressed, std::chrono::microseconds time);

    InputDevice m_inputDevice;
    std::array<QHash<Trigger, std::variant<QKeySequence, MouseButton, TabletToolButton>>, LastType> m_actions;
    KConfigWatcher::Ptr m_configWatcher;
    std::optional<KWin::TabletToolId> m_tabletTool;
};
