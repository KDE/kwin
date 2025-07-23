/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// own
#include "dbusinterface.h"
#include "compositingadaptor.h"
#include "pluginsadaptor.h"
#include "virtualdesktopmanageradaptor.h"

// kwin
#include "compositor.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "debug_console.h"
#include "kwinadaptor.h"
#include "main.h"
#include "placement.h"
#include "pluginmanager.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif

// Qt
#include <QDBusConnection>
#include <QOpenGLContext>

namespace KWin
{

DBusInterface::DBusInterface(QObject *parent)
    : QObject(parent)
    , m_serviceName(QStringLiteral("org.kde.KWin"))
{
    (void)new KWinAdaptor(this);

    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/KWin"), this);
    dbus.registerService(m_serviceName);
    dbus.connect(QString(), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reloadConfig"),
                 Workspace::self(), SLOT(slotReloadConfig()));

    connect(Workspace::self(), &Workspace::showingDesktopChanged, this, &DBusInterface::onShowingDesktopChanged);
}

DBusInterface::~DBusInterface()
{
    QDBusConnection::sessionBus().unregisterService(m_serviceName);
}

bool DBusInterface::showingDesktop() const
{
    return workspace()->showingDesktop();
}

void DBusInterface::reconfigure()
{
    Workspace::self()->reconfigure();
}

void DBusInterface::killWindow()
{
    Workspace::self()->slotKillWindow();
}

QString DBusInterface::supportInformation()
{
    return Workspace::self()->supportInformation();
}

QString DBusInterface::activeOutputName()
{
    return Workspace::self()->activeOutput()->name();
}

bool DBusInterface::startActivity(const QString &in0)
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return false;
    }
    return Workspace::self()->activities()->start(in0);
#else
    return false;
#endif
}

bool DBusInterface::stopActivity(const QString &in0)
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return false;
    }
    return Workspace::self()->activities()->stop(in0);
#else
    return false;
#endif
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
    VirtualDesktopManager::self()->moveTo(VirtualDesktopManager::Direction::Next);
}

void DBusInterface::previousDesktop()
{
    VirtualDesktopManager::self()->moveTo(VirtualDesktopManager::Direction::Previous);
}

void DBusInterface::showDebugConsole()
{
    DebugConsole *console = new DebugConsole;
    console->show();
}

void DBusInterface::replace()
{
    QCoreApplication::exit(133);
}

namespace
{
QVariantMap clientToVariantMap(const Window *c)
{
    return
    {
        {QStringLiteral("resourceClass"), c->resourceClass()},
            {QStringLiteral("resourceName"), c->resourceName()},
            {QStringLiteral("desktopFile"), c->desktopFileName()},
            {QStringLiteral("role"), c->windowRole()},
            {QStringLiteral("caption"), c->captionNormal()},
            {QStringLiteral("clientMachine"), c->wmClientMachine(true)},
            {QStringLiteral("localhost"), c->isLocalhost()},
            {QStringLiteral("type"), int(c->windowType())},
            {QStringLiteral("x"), c->x()},
            {QStringLiteral("y"), c->y()},
            {QStringLiteral("width"), c->width()},
            {QStringLiteral("height"), c->height()},
            {QStringLiteral("desktops"), c->desktopIds()},
            {QStringLiteral("minimized"), c->isMinimized()},
            {QStringLiteral("fullscreen"), c->isFullScreen()},
            {QStringLiteral("keepAbove"), c->keepAbove()},
            {QStringLiteral("keepBelow"), c->keepBelow()},
            {QStringLiteral("noBorder"), c->noBorder()},
            {QStringLiteral("skipTaskbar"), c->skipTaskbar()},
            {QStringLiteral("skipPager"), c->skipPager()},
            {QStringLiteral("skipSwitcher"), c->skipSwitcher()},
            {QStringLiteral("maximizeHorizontal"), c->maximizeMode() & MaximizeHorizontal},
            {QStringLiteral("maximizeVertical"), c->maximizeMode() & MaximizeVertical},
            {QStringLiteral("uuid"), c->internalId().toString()},
#if KWIN_BUILD_ACTIVITIES
            {QStringLiteral("activities"), c->activities()},
#endif
            {QStringLiteral("layer"), c->layer()},
    };
}
}

QVariantMap DBusInterface::queryWindowInfo()
{
    m_replyQueryWindowInfo = message();
    setDelayedReply(true);
    kwinApp()->startInteractiveWindowSelection(
        [this](Window *t) {
            if (!t) {
                QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createErrorReply(
                    QStringLiteral("org.kde.KWin.Error.UserCancel"),
                    QStringLiteral("User cancelled the query")));
                return;
            }
            if (t->isClient()) {
                QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createReply(clientToVariantMap(t)));
            } else {
                QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createErrorReply(
                    QStringLiteral("org.kde.KWin.Error.InvalidWindow"),
                    QStringLiteral("Tried to query information about an unmanaged window")));
            }
        });
    return QVariantMap{};
}

QVariantMap DBusInterface::getWindowInfo(const QString &uuid)
{
    const auto window = workspace()->findWindow(QUuid::fromString(uuid));
    if (window) {
        return clientToVariantMap(window);
    } else {
        return {};
    }
}

void DBusInterface::showDesktop(bool show)
{
    workspace()->setShowingDesktop(show, true);

    auto m = message();
    if (m.service().isEmpty()) {
        return;
    }

    // Keep track of whatever D-Bus client asked to show the desktop. If
    // they disappear from the bus, cancel the show desktop state so we do
    // not end up in a state where we are stuck showing the desktop.
    static QPointer<QDBusServiceWatcher> watcher;

    if (show) {
        if (watcher) {
            // If we get a second call to `showDesktop(true)`, drop the previous
            // watcher and watch the new client. That way, we simply always
            // track the last state.
            watcher->deleteLater();
        }

        watcher = new QDBusServiceWatcher(m.service(), QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForUnregistration, this);
        connect(watcher, &QDBusServiceWatcher::serviceUnregistered, []() {
            workspace()->setShowingDesktop(false, true);
            watcher->deleteLater();
        });
    } else if (watcher) {
        // Someone cancelled showing the desktop, so there's no more need to
        // watch to cancel the show desktop state.
        watcher->deleteLater();
    }
}

void DBusInterface::lockForRemote(bool lock)
{
    workspace()->lockForRemote(lock);
}

void DBusInterface::onShowingDesktopChanged(bool show, bool /*animated*/)
{
    Q_EMIT showingDesktopChanged(show);
}

CompositorDBusInterface::CompositorDBusInterface(Compositor *parent)
    : QObject(parent)
    , m_compositor(parent)
{
    connect(m_compositor, &Compositor::compositingToggled, this, &CompositorDBusInterface::compositingToggled);
    new CompositingAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Compositor"), this);
    dbus.connect(QString(), QStringLiteral("/Compositor"), QStringLiteral("org.kde.kwin.Compositing"),
                 QStringLiteral("reinit"), this, SLOT(reinitialize()));
}

QString CompositorDBusInterface::compositingType() const
{
    switch (m_compositor->backend()->compositingType()) {
    case OpenGLCompositing:
        if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
            return QStringLiteral("gles");
        } else {
            return QStringLiteral("gl2");
        }
    case QPainterCompositing:
        return QStringLiteral("qpainter");
    case NoCompositing:
    default:
        return QStringLiteral("none");
    }
}

bool CompositorDBusInterface::isActive() const
{
    return m_compositor->isActive();
}

bool CompositorDBusInterface::isCompositingPossible() const
{
    return true;
}

QString CompositorDBusInterface::compositingNotPossibleReason() const
{
    return QString();
}

bool CompositorDBusInterface::isOpenGLBroken() const
{
    return false;
}

bool CompositorDBusInterface::platformRequiresCompositing() const
{
    return true;
}

void CompositorDBusInterface::reinitialize()
{
    m_compositor->reinitialize();
}

QStringList CompositorDBusInterface::supportedOpenGLPlatformInterfaces() const
{
    return {QStringLiteral("egl")};
}

VirtualDesktopManagerDBusInterface::VirtualDesktopManagerDBusInterface(VirtualDesktopManager *parent)
    : QObject(parent)
    , m_manager(parent)
{
    qDBusRegisterMetaType<KWin::DBusDesktopDataStruct>();
    qDBusRegisterMetaType<KWin::DBusDesktopDataVector>();

    new VirtualDesktopManagerAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/VirtualDesktopManager"),
                                                 QStringLiteral("org.kde.KWin.VirtualDesktopManager"),
                                                 this);

    connect(m_manager, &VirtualDesktopManager::currentChanged, this, [this]() {
        Q_EMIT currentChanged(m_manager->currentDesktop()->id());
    });

    connect(m_manager, &VirtualDesktopManager::countChanged, this, [this](uint previousCount, uint newCount) {
        Q_EMIT countChanged(newCount);
        Q_EMIT desktopsChanged(desktops());
    });

    connect(m_manager, &VirtualDesktopManager::navigationWrappingAroundChanged, this, [this]() {
        Q_EMIT navigationWrappingAroundChanged(isNavigationWrappingAround());
    });

    connect(m_manager, &VirtualDesktopManager::rowsChanged, this, &VirtualDesktopManagerDBusInterface::rowsChanged);

    const QList<VirtualDesktop *> allDesks = m_manager->desktops();
    for (auto *vd : allDesks) {
        connect(vd, &VirtualDesktop::x11DesktopNumberChanged, this, [this, vd]() {
            DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
            Q_EMIT desktopDataChanged(vd->id(), data);
            Q_EMIT desktopsChanged(desktops());
        });
        connect(vd, &VirtualDesktop::nameChanged, this, [this, vd]() {
            DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
            Q_EMIT desktopDataChanged(vd->id(), data);
            Q_EMIT desktopsChanged(desktops());
        });
    }
    connect(m_manager, &VirtualDesktopManager::desktopAdded, this, [this](VirtualDesktop *vd) {
        connect(vd, &VirtualDesktop::x11DesktopNumberChanged, this, [this, vd]() {
            DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
            Q_EMIT desktopDataChanged(vd->id(), data);
            Q_EMIT desktopsChanged(desktops());
        });
        connect(vd, &VirtualDesktop::nameChanged, this, [this, vd]() {
            DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
            Q_EMIT desktopDataChanged(vd->id(), data);
            Q_EMIT desktopsChanged(desktops());
        });
        DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
        Q_EMIT desktopCreated(vd->id(), data);
        Q_EMIT desktopsChanged(desktops());
    });
    connect(m_manager, &VirtualDesktopManager::desktopRemoved, this, [this](VirtualDesktop *vd) {
        Q_EMIT desktopRemoved(vd->id());
        Q_EMIT desktopsChanged(desktops());
    });
}

uint VirtualDesktopManagerDBusInterface::count() const
{
    return m_manager->count();
}

void VirtualDesktopManagerDBusInterface::setRows(uint rows)
{
    if (static_cast<uint>(m_manager->grid().height()) == rows) {
        return;
    }

    m_manager->setRows(rows);
    m_manager->save();
}

uint VirtualDesktopManagerDBusInterface::rows() const
{
    return m_manager->rows();
}

void VirtualDesktopManagerDBusInterface::setCurrent(const QString &id)
{
    if (m_manager->currentDesktop()->id() == id) {
        return;
    }

    auto *vd = m_manager->desktopForId(id);
    if (vd) {
        m_manager->setCurrent(vd);
    }
}

QString VirtualDesktopManagerDBusInterface::current() const
{
    return m_manager->currentDesktop()->id();
}

void VirtualDesktopManagerDBusInterface::setNavigationWrappingAround(bool wraps)
{
    if (m_manager->isNavigationWrappingAround() == wraps) {
        return;
    }

    m_manager->setNavigationWrappingAround(wraps);
}

bool VirtualDesktopManagerDBusInterface::isNavigationWrappingAround() const
{
    return m_manager->isNavigationWrappingAround();
}

DBusDesktopDataVector VirtualDesktopManagerDBusInterface::desktops() const
{
    const auto desks = m_manager->desktops();
    DBusDesktopDataVector desktopVect;
    desktopVect.reserve(m_manager->count());

    std::transform(desks.constBegin(), desks.constEnd(),
                   std::back_inserter(desktopVect),
                   [](const VirtualDesktop *vd) {
                       return DBusDesktopDataStruct{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
                   });

    return desktopVect;
}

void VirtualDesktopManagerDBusInterface::createDesktop(uint position, const QString &name)
{
    m_manager->createVirtualDesktop(position, name);
}

void VirtualDesktopManagerDBusInterface::setDesktopName(const QString &id, const QString &name)
{
    VirtualDesktop *vd = m_manager->desktopForId(id);
    if (!vd) {
        return;
    }
    if (vd->name() == name) {
        return;
    }

    vd->setName(name);
    m_manager->save();
}

void VirtualDesktopManagerDBusInterface::removeDesktop(const QString &id)
{
    m_manager->removeVirtualDesktop(id);
}

PluginManagerDBusInterface::PluginManagerDBusInterface(PluginManager *manager)
    : QObject(manager)
    , m_manager(manager)
{
    new PluginsAdaptor(this);

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Plugins"),
                                                 QStringLiteral("org.kde.KWin.Plugins"),
                                                 this);
}

QStringList PluginManagerDBusInterface::loadedPlugins() const
{
    return m_manager->loadedPlugins();
}

QStringList PluginManagerDBusInterface::availablePlugins() const
{
    return m_manager->availablePlugins();
}

bool PluginManagerDBusInterface::LoadPlugin(const QString &name)
{
    return m_manager->loadPlugin(name);
}

void PluginManagerDBusInterface::UnloadPlugin(const QString &name)
{
    m_manager->unloadPlugin(name);
}

} // namespace

#include "moc_dbusinterface.cpp"
