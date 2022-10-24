/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include "core/inputdevice.h"
#include "input.h"

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
        LastType
    };
    Q_ENUM(TriggerType);

    explicit ButtonRebindsFilter();
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool tabletPadButtonEvent(uint button, bool pressed, const KWin::TabletPadId &tabletPadId, uint time) override;

private:
    void loadConfig(const KConfigGroup &group);
    void insert(TriggerType type, const Trigger &trigger, const QStringList &action);
    bool send(TriggerType type, const Trigger &trigger, bool pressed, uint timestamp);
    bool sendKeySequence(const QKeySequence &sequence, bool pressed, uint time);

    InputDevice m_inputDevice;
    QHash<Trigger, QKeySequence> m_actions[LastType];
    KConfigWatcher::Ptr m_configWatcher;
};
