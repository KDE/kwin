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
// TODO: remove together with deprecated methods
#include "client.h"
#include "composite.h"
#include "decorations.h"
#include "effects.h"
#include "kwinadaptor.h"
#include "workspace.h"
#include "virtualdesktops.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif

// Qt
#include <QDBusServiceWatcher>

namespace KWin
{

DBusInterface::DBusInterface(QObject *parent)
    : QObject(parent)
{
    (void) new KWinAdaptor(this);

    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/KWin", this);
    if (!dbus.registerService("org.kde.KWin")) {
        QDBusServiceWatcher *dog = new QDBusServiceWatcher("org.kde.KWin", dbus, QDBusServiceWatcher::WatchForUnregistration, this);
        connect (dog, SIGNAL(serviceUnregistered(QString)), SLOT(becomeKWinService(QString)));
    }
    connect(Compositor::self(), SIGNAL(compositingToggled(bool)), SIGNAL(compositingToggled(bool)));
    dbus.connect(QString(), "/KWin", "org.kde.KWin", "reloadConfig",
                 Workspace::self(), SLOT(slotReloadConfig()));
    dbus.connect(QString(), "/KWin", "org.kde.KWin", "reinitCompositing",
                 Compositor::self(), SLOT(slotReinitialize()));
}

void DBusInterface::becomeKWinService(const QString &service)
{
    // TODO: this watchdog exists to make really safe that we at some point get the service
    // but it's probably no longer needed since we explicitly unregister the service with the deconstructor
    if (service == "org.kde.KWin" && QDBusConnection::sessionBus().registerService("org.kde.KWin") && sender()) {
        sender()->deleteLater(); // bye doggy :'(
    }
}

DBusInterface::~DBusInterface()
{
    QDBusConnection::sessionBus().unregisterService("org.kde.KWin"); // this is the long standing legal service
    // KApplication automatically also grabs org.kde.kwin, so it's often been used externally - ensure to free it as well
    QDBusConnection::sessionBus().unregisterService("org.kde.kwin");
}

void DBusInterface::circulateDesktopApplications()
{
    Workspace *ws = Workspace::self();
    const uint desktop = VirtualDesktopManager::self()->current();
    const QList<Client*> &desktops = ws->desktopList();
    if (desktops.count() > 1) {
        bool change_active = ws->activeClient()->isDesktop();
        ws->raiseClient(ws->findDesktop(false, desktop));
        if (change_active)   // if the previously topmost Desktop was active, activate this new one
            ws->activateClient(ws->findDesktop(true, desktop));
    }
    // if there's no active client, make desktop the active one
    if (desktops.count() > 0 && ws->activeClient() == NULL && ws->mostRecentlyActivatedClient() == NULL)
        ws->activateClient(ws->findDesktop(true, desktop));
}

// wrap void methods with no arguments to Workspace
#define WRAP(name) \
void DBusInterface::name() \
{\
    Workspace::self()->name();\
}

WRAP(reconfigure)

#undef WRAP

void DBusInterface::killWindow()
{
    Workspace::self()->slotKillWindow();
}

#define WRAP(name) \
void DBusInterface::name() \
{\
    Placement::self()->name();\
}

WRAP(cascadeDesktop)
WRAP(unclutterDesktop)

#undef WRAP

// wrap returning methods with no arguments to Workspace
#define WRAP( rettype, name ) \
rettype DBusInterface::name( ) \
{\
    return Workspace::self()->name(); \
}

WRAP(QString, supportInformation)
WRAP(bool, waitForCompositingSetup)

#undef WRAP

bool DBusInterface::startActivity(const QString &in0)
{
#ifdef KWIN_BUILD_ACTIVITIES
    return Activities::self()->start(in0);
#else
    return false;
#endif
}

bool DBusInterface::stopActivity(const QString &in0)
{
#ifdef KWIN_BUILD_ACTIVITIES
    return Activities::self()->stop(in0);
#else
    return false;
#endif
}

void DBusInterface::doNotManage(const QString &name)
{
    Q_UNUSED(name)
}

void DBusInterface::showWindowMenuAt(qlonglong winId, int x, int y)
{
    Q_UNUSED(winId)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Workspace::self()->slotWindowOperations();
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

int DBusInterface::currentDesktop()
{
    return VirtualDesktopManager::self()->current();
}

bool DBusInterface::setCurrentDesktop(int desktop)
{
    return VirtualDesktopManager::self()->setCurrent(desktop);
}

void DBusInterface::nextDesktop()
{
    VirtualDesktopManager::self()->moveTo<DesktopNext>();
}

void DBusInterface::previousDesktop()
{
    VirtualDesktopManager::self()->moveTo<DesktopPrevious>();
}

QList< int > DBusInterface::decorationSupportedColors()
{
    return decorationPlugin()->supportedColors();
}

} // namespace
