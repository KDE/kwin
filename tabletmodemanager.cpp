/*
 * Copyright 2018 Marco Martin <mart@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "tabletmodemanager.h"
#include "input.h"
#include "input_event.h"
#include "input_event_spy.h"

#include "libinput/device.h"
#include "libinput/connection.h"

#include <QTimer>
#include <QDBusConnection>

using namespace KWin;

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
        auto c = LibInput::Connection::self();
        connect(c, &LibInput::Connection::deviceAdded, this, &TabletModeTouchpadRemovedSpy::refresh);
        connect(c, &LibInput::Connection::deviceRemoved, this, &TabletModeTouchpadRemovedSpy::refresh);

        check();
    }

    void refresh(LibInput::Device* d) {
        if (!d->isTouch() && !d->isPointer())
            return;
        check();
    }

    void check() {
        if (!LibInput::Connection::self()) {
            qDebug() << "no libinput :(";
            return;
        }
        const auto devices = LibInput::Connection::self()->devices();
        const bool hasTouch = std::any_of(devices.constBegin(), devices.constEnd(), [](LibInput::Device* device){ return device->isTouch(); });
        m_parent->setTabletModeAvailable(hasTouch);

        const bool hasPointer = std::any_of(devices.constBegin(), devices.constEnd(), [](LibInput::Device* device){ return device->isPointer(); });
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
        auto setupDetector = [this] {
            auto spy = new TabletModeTouchpadRemovedSpy(this);
            connect(input(), &InputRedirection::hasTabletModeSwitchChanged, spy, [spy](bool set){
                if (set)
                    spy->deleteLater();
            });
        };
        if (LibInput::Connection::self())
            setupDetector();
        else
            QTimer::singleShot(2000, this, setupDetector);
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
    emit tabletModeChanged(tablet);
}

void KWin::TabletModeManager::setTabletModeAvailable(bool detecting)
{
    if (m_detecting != detecting) {
        m_detecting = detecting;
        tabletModeAvailableChanged(detecting);
    }
}
