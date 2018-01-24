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

#if HAVE_INPUT
#include "libinput/device.h"
#endif

#include <QDBusConnection>

using namespace KWin;

KWIN_SINGLETON_FACTORY_VARIABLE(TabletModeManager, s_manager)

class KWin::TabletModeInputEventSpy : public InputEventSpy
{
public:
    explicit TabletModeInputEventSpy(TabletModeManager *parent);

    void switchEvent(SwitchEvent *event) override;
private:
    TabletModeManager *m_parent;
};

TabletModeInputEventSpy::TabletModeInputEventSpy(TabletModeManager *parent)
    : m_parent(parent)
{
}

void TabletModeInputEventSpy::switchEvent(SwitchEvent *event)
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



TabletModeManager::TabletModeManager(QObject *parent)
    : QObject(parent),
      m_spy(new TabletModeInputEventSpy(this))
{
    input()->installInputEventSpy(m_spy);

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin"),
                                                 QStringLiteral("org.kde.KWin.TabletModeManager"),
                                                 this,
                                                 QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals
    );

    connect(input(), &InputRedirection::hasTabletModeSwitchChanged, this, &TabletModeManager::tabletModeAvailableChanged);
}

bool TabletModeManager::isTabletModeAvailable() const
{
    return input()->hasTabletModeSwitch();
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
