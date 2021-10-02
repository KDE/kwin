/*
    SPDX-FileCopyrightText: 2018 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#include "tabletmodemanager.h"
#include "input.h"
#include "inputdevice.h"
#include "input_event.h"
#include "input_event_spy.h"

#include <QDBusConnection>

namespace KWin
{

KWIN_SINGLETON_FACTORY_VARIABLE(TabletModeManager, s_manager)

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
    TabletModeManager * const m_parent;
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
        const bool hasTouch = std::any_of(devices.constBegin(), devices.constEnd(), [](InputDevice *device) { return device->isTouch(); });
        m_parent->setTabletModeAvailable(hasTouch);

        const bool hasPointer = std::any_of(devices.constBegin(), devices.constEnd(), [](InputDevice *device) { return device->isPointer(); });
        m_parent->setIsTablet(hasTouch && !hasPointer);
    }

private:
    TabletModeManager * const m_parent;
};

TabletModeManager::TabletModeManager(QObject *parent)
    : QObject(parent)
{
    if (input()->hasTabletModeSwitch()) {
        input()->installInputEventSpy(new TabletModeSwitchEventSpy(this));
    } else {
        hasTabletModeInputChanged(false);
    }

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin"),
                                                 QStringLiteral("org.kde.KWin.TabletModeManager"),
                                                 this,
                                                 QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals
    );

    connect(input(), &InputRedirection::hasTabletModeSwitchChanged, this, &TabletModeManager::hasTabletModeInputChanged);
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

bool TabletModeManager::isTablet() const
{
    return m_isTabletMode;
}

void TabletModeManager::setIsTablet(bool tablet)
{
    if (m_isTabletMode == tablet) {
        return;
    }

    m_isTabletMode = tablet;
    Q_EMIT tabletModeChanged(tablet);
}

void KWin::TabletModeManager::setTabletModeAvailable(bool detecting)
{
    if (m_detecting != detecting) {
        m_detecting = detecting;
        Q_EMIT tabletModeAvailableChanged(detecting);
    }
}

} // namespace KWin
