/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// own
#include "dbusinterface.h"
// kwin
#include "composite.h"
#include "effects.h"
#include "kwinadaptor.h"
#include "workspace.h"

namespace KWin
{

DBusInterface::DBusInterface(QObject *parent)
    : QObject(parent)
{
    (void) new KWinAdaptor(this);

    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/KWin", this);
    dbus.registerService("org.kde.KWin");
    connect(Compositor::self(), SIGNAL(compositingToggled(bool)), SIGNAL(compositingToggled(bool)));
    dbus.connect(QString(), "/KWin", "org.kde.KWin", "reloadConfig",
                 Workspace::self(), SLOT(slotReloadConfig()));
    dbus.connect(QString(), "/KWin", "org.kde.KWin", "reinitCompositing",
                 Compositor::self(), SLOT(slotReinitialize()));
}

DBusInterface::~DBusInterface()
{
}

// wrap void methods with no arguments to Workspace
#define WRAP(name) \
void DBusInterface::name() \
{\
    Workspace::self()->name();\
}

WRAP(cascadeDesktop)
WRAP(circulateDesktopApplications)
WRAP(killWindow)
WRAP(nextDesktop)
WRAP(previousDesktop)
WRAP(reconfigure)
WRAP(unclutterDesktop)

#undef WRAP

// wrap returning methods with no arguments to Workspace
#define WRAP( rettype, name ) \
rettype DBusInterface::name( ) \
{\
    return Workspace::self()->name(); \
}

WRAP(int, currentDesktop)
WRAP(QList<int>, decorationSupportedColors)
WRAP(QString, supportInformation)
WRAP(bool, waitForCompositingSetup)

#undef WRAP

// wrap returning methods with one argument to Workspace
#define WRAP( rettype, name, argtype ) \
rettype DBusInterface::name( argtype arg ) \
{\
    return Workspace::self()->name(arg); \
}

WRAP(bool, setCurrentDesktop, int)
WRAP(bool, startActivity, const QString &)
WRAP(bool, stopActivity, const QString &)

#undef WRAP

void DBusInterface::doNotManage(const QString &name)
{
    Workspace::self()->doNotManage(name);
}

void DBusInterface::showWindowMenuAt(qlonglong winId, int x, int y)
{
    Workspace::self()->showWindowMenuAt(winId, x, y);
}

// wrap returning methods with no arguments to COMPOSITOR
#define WRAP( rettype, name ) \
rettype DBusInterface::name( ) \
{\
    return Compositor::self()->name(); \
}

WRAP(QString, compositingNotPossibleReason)
WRAP(QString, compositingType)

#undef WRAP

bool DBusInterface::compositingPossible()
{
    return Compositor::self()->isCompositingPossible();
}

bool DBusInterface::openGLIsBroken()
{
    return Compositor::self()->isOpenGLBroken();
}

bool DBusInterface::compositingActive()
{
    return Compositor::self()->isActive();
}

void DBusInterface::toggleCompositing()
{
    Compositor::self()->toggleCompositing();
}

// wrap returning QStringList methods with no argument to EffectsHandlerImpl
#define WRAP( name ) \
QStringList DBusInterface::name( ) \
{\
    if (effects) { \
        return static_cast< EffectsHandlerImpl* >(effects)->name(); \
    } \
    return QStringList(); \
}

WRAP(activeEffects)
WRAP(listOfEffects)
WRAP(loadedEffects)

#undef WRAP

// wrap void methods with one argument to EffectsHandlerImpl
#define WRAP( name, argtype ) \
void DBusInterface::name( argtype arg ) \
{\
    if (effects) { \
        static_cast< EffectsHandlerImpl* >(effects)->name(arg); \
    } \
}

WRAP(loadEffect, const QString &)
WRAP(reconfigureEffect, const QString &)
WRAP(toggleEffect, const QString &)
WRAP(unloadEffect, const QString &)

#undef WRAP

QString DBusInterface::supportInformationForEffect(const QString &name)
{
    if (effects) {
        static_cast< EffectsHandlerImpl* >(effects)->supportInformation(name);
    }
    return QString();
}

} // namespace
