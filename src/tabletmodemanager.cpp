/*
    SPDX-FileCopyrightText: 2018 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#include "tabletmodemanager.h"

#include "backends/fakeinput/fakeinputdevice.h"
#include "backends/libinput/device.h"
#include "core/inputdevice.h"
#include "input.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "main.h"
#include "wayland_server.h"

#include <QDBusConnection>

namespace KWin
{

static bool shouldIgnoreDevice(InputDevice *device)
{
    if (qobject_cast<FakeInputDevice*>(device)) {
        return true;
    }

    auto libinput_device = qobject_cast<LibInput::Device *>(device);
    if (!libinput_device) {
        return false;
    }

    bool ignore = false;
    if (auto udev = libinput_device_get_udev_device(libinput_device->device()); udev) {
        ignore = udev_device_has_tag(udev, "kwin-ignore-tablet-mode");
        udev_device_unref(udev);
    }
    return ignore;
}

class TabletModeSwitchEventSpy : public QObject, public InputEventSpy
{
public:
    explicit TabletModeSwitchEventSpy(TabletModeManager *parent)
        : QObject(parent)
        , m_parent(parent)
    {
    }

    void switchEvent(SwitchEvent *event) override
    {
        if (!event->device()->isTabletModeSwitch()) {
            return;
        }

        switch (event->state()) {
        case SwitchEvent::State::Off:
            m_parent->setIsTablet(false);
            break;
        case SwitchEvent::State::On:
            m_parent->setIsTablet(true);
            break;
        default:
            Q_UNREACHABLE();
        }
    }

private:
    TabletModeManager *const m_parent;
};

class TabletModeTouchpadRemovedSpy : public QObject
{
public:
    explicit TabletModeTouchpadRemovedSpy(TabletModeManager *parent)
        : QObject(parent)
        , m_parent(parent)
    {
        connect(input(), &InputRedirection::deviceAdded, this, &TabletModeTouchpadRemovedSpy::refresh);
        connect(input(), &InputRedirection::deviceRemoved, this, &TabletModeTouchpadRemovedSpy::refresh);

        check();
    }

    void refresh(InputDevice *inputDevice)
    {
        if (inputDevice->isTouch() || inputDevice->isPointer()) {
            check();
        }
    }

    void check()
    {
        const auto devices = input()->devices();
        const bool hasTouch = std::any_of(devices.constBegin(), devices.constEnd(), [](InputDevice *device) {
            return device->isTouch() && !shouldIgnoreDevice(device);
        });
        m_parent->setTabletModeAvailable(hasTouch);

        const bool hasPointer = std::any_of(devices.constBegin(), devices.constEnd(), [](InputDevice *device) {
            return device->isPointer() && !shouldIgnoreDevice(device);
        });
        m_parent->setIsTablet(hasTouch && !hasPointer);
    }

private:
    TabletModeManager *const m_parent;
};

TabletModeManager::TabletModeManager()
{
    if (waylandServer()) {
        if (input()->hasTabletModeSwitch()) {
            input()->installInputEventSpy(new TabletModeSwitchEventSpy(this));
        } else {
            hasTabletModeInputChanged(false);
        }
    }

    KSharedConfig::Ptr kwinSettings = kwinApp()->config();
    m_settingsWatcher = KConfigWatcher::create(kwinSettings);
    connect(m_settingsWatcher.data(), &KConfigWatcher::configChanged, this, &KWin::TabletModeManager::refreshSettings);
    refreshSettings();

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin"),
                                                 QStringLiteral("org.kde.KWin.TabletModeManager"),
                                                 this,
                                                 // NOTE: slots must be exported for properties to work correctly
                                                 QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllSlots);

    if (waylandServer()) {
        connect(input(), &InputRedirection::hasTabletModeSwitchChanged, this, &TabletModeManager::hasTabletModeInputChanged);
    }
}

void KWin::TabletModeManager::refreshSettings()
{
    KSharedConfig::Ptr kwinSettings = kwinApp()->config();
    KConfigGroup cg = kwinSettings->group("Input");
    const QString tabletModeConfig = cg.readPathEntry("TabletMode", QStringLiteral("auto"));
    const bool oldEffectiveTabletMode = effectiveTabletMode();
    if (tabletModeConfig == QStringLiteral("on")) {
        m_configuredMode = ConfiguredMode::On;
        if (!m_detecting) {
            Q_EMIT tabletModeAvailableChanged(true);
        }
    } else if (tabletModeConfig == QStringLiteral("off")) {
        m_configuredMode = ConfiguredMode::Off;
    } else {
        m_configuredMode = ConfiguredMode::Auto;
    }
    if (effectiveTabletMode() != oldEffectiveTabletMode) {
        Q_EMIT tabletModeChanged(effectiveTabletMode());
    }
}

void KWin::TabletModeManager::hasTabletModeInputChanged(bool set)
{
    if (set) {
        input()->installInputEventSpy(new TabletModeSwitchEventSpy(this));
        setTabletModeAvailable(true);
    } else {
        auto spy = new TabletModeTouchpadRemovedSpy(this);
        connect(input(), &InputRedirection::hasTabletModeSwitchChanged, spy, [spy](bool set) {
            if (set) {
                spy->deleteLater();
            }
        });
    }
}

bool TabletModeManager::isTabletModeAvailable() const
{
    return m_detecting;
}

bool TabletModeManager::effectiveTabletMode() const
{
    switch (m_configuredMode) {
    case ConfiguredMode::Off:
        return false;
    case ConfiguredMode::On:
        return true;
    case ConfiguredMode::Auto:
    default:
        if (!waylandServer()) {
            return false;
        } else {
            return m_isTabletMode;
        }
    }
}

bool TabletModeManager::isTablet() const
{
    return m_isTabletMode;
}

void TabletModeManager::setIsTablet(bool tablet)
{
    if (m_isTabletMode == tablet) {
        return;
    }

    const bool oldTabletMode = effectiveTabletMode();
    m_isTabletMode = tablet;
    if (effectiveTabletMode() != oldTabletMode) {
        Q_EMIT tabletModeChanged(effectiveTabletMode());
    }
}

void KWin::TabletModeManager::setTabletModeAvailable(bool detecting)
{
    if (m_detecting == detecting) {
        return;
    }

    m_detecting = detecting;
    Q_EMIT tabletModeAvailableChanged(isTabletModeAvailable());
}

KWin::TabletModeManager::ConfiguredMode KWin::TabletModeManager::configuredMode() const
{
    return m_configuredMode;
}

} // namespace KWin
