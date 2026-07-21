/*
    SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dwellclicker.h"

#include <linux/input-event-codes.h>

using namespace std::literals;

QString DwellClickerInputDevice::name() const
{
    return QStringLiteral("dwell clicker device");
}

void DwellClickerInputDevice::setEnabled(bool enabled)
{
}

bool DwellClickerInputDevice::isEnabled() const
{
    return true;
}

bool DwellClickerInputDevice::isKeyboard() const
{
    return false;
}

bool DwellClickerInputDevice::isLidSwitch() const
{
    return false;
}

bool DwellClickerInputDevice::isPointer() const
{
    return true;
}

bool DwellClickerInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool DwellClickerInputDevice::isTabletPad() const
{
    return false;
}

bool DwellClickerInputDevice::isTabletTool() const
{
    return false;
}

bool DwellClickerInputDevice::isTouch() const
{
    return false;
}

bool DwellClickerInputDevice::isTouchpad() const
{
    return false;
}

DwellClickerFilter::DwellClickerFilter()
    : KWin::Plugin()
    , KWin::InputEventFilter(KWin::InputFilterOrder::DwellClicker)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("kaccessrc")))
{
    const QLatin1StringView groupName("DwellClicker");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));

    connect(&m_dwellTimer, &QTimer::timeout, this, &DwellClickerFilter::click);
    m_dwellTimer.setSingleShot(true);
}

DwellClickerFilter::~DwellClickerFilter()
{
    if (m_inputDevice) {
        KWin::input()->removeInputDevice(m_inputDevice.get());
    }
}

bool DwellClickerFilter::pointerMotion(KWin::PointerMotionEvent *event)
{
    m_dwellTimer.start();

    return false;
}

void DwellClickerFilter::click()
{
    const auto clickTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
    Q_EMIT m_inputDevice->pointerButtonChanged(BTN_LEFT, KWin::PointerButtonState::Pressed, clickTime, m_inputDevice.get());
    Q_EMIT m_inputDevice->pointerFrame(m_inputDevice.get());

    const auto releaseTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
    Q_EMIT m_inputDevice->pointerButtonChanged(BTN_LEFT, KWin::PointerButtonState::Released, releaseTime, m_inputDevice.get());
    Q_EMIT m_inputDevice->pointerFrame(m_inputDevice.get());
}

void DwellClickerFilter::loadConfig(const KConfigGroup &group)
{
    const bool newEnabled = group.readEntry<bool>("Enabled", false);

    if (m_enabled && !newEnabled) {
        KWin::input()->uninstallInputEventFilter(this);
        KWin::input()->removeInputDevice(m_inputDevice.get());
        m_inputDevice = nullptr;
        m_dwellTimer.stop();
    }

    if (!m_enabled && newEnabled) {
        m_inputDevice = std::make_unique<DwellClickerInputDevice>();
        KWin::input()->addInputDevice(m_inputDevice.get());
        KWin::input()->installInputEventFilter(this);
    }

    m_enabled = newEnabled;
    if (m_enabled) {
        m_dwellTimer.setInterval(group.readEntry("Timeout", 500));
    }
}
