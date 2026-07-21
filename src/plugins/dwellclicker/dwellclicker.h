/*
    SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include "core/inputdevice.h"
#include "input.h"
#include "input_event.h"

#include <QTimer>

class DwellClickerInputDevice : public KWin::InputDevice
{
    Q_OBJECT

public:
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

class DwellClickerFilter : public KWin::Plugin, public KWin::InputEventFilter
{
    Q_OBJECT

public:
    explicit DwellClickerFilter();
    ~DwellClickerFilter() override;

    bool pointerMotion(KWin::PointerMotionEvent *event) override;

private:
    void click();
    void loadConfig(const KConfigGroup &group);

    std::unique_ptr<DwellClickerInputDevice> m_inputDevice;
    KConfigWatcher::Ptr m_configWatcher;
    QTimer m_dwellTimer;
    bool m_enabled = false;
};
