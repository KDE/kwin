/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// own
#include "dbusinterface.h"
#include "compositingadaptor.h"
#include "virtualdesktopmanageradaptor.h"

// kwin
#include "abstract_client.h"
#include "atoms.h"
#include "composite.h"
#include "debug_console.h"
#include "main.h"
#include "placement.h"
#include "platform.h"
#include "kwinadaptor.h"
#include "scene.h"
#include "workspace.h"
#include "virtualdesktops.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif

// Qt
#include <QOpenGLContext>
#include <QDBusServiceWatcher>

namespace KWin
{

DBusInterface::DBusInterface(QObject *parent)
    : QObject(parent)
    , m_serviceName(QStringLiteral("org.kde.KWin"))
{
    (void) new KWinAdaptor(this);

    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/KWin"), this);
    const QByteArray dBusSuffix = qgetenv("KWIN_DBUS_SERVICE_SUFFIX");
    if (!dBusSuffix.isNull()) {
        m_serviceName = m_serviceName + QLatin1Char('.') + dBusSuffix;
    }
    if (!dbus.registerService(m_serviceName)) {
        QDBusServiceWatcher *dog = new QDBusServiceWatcher(m_serviceName, dbus, QDBusServiceWatcher::WatchForUnregistration, this);
        connect (dog, SIGNAL(serviceUnregistered(QString)), SLOT(becomeKWinService(QString)));
    } else {
        announceService();
    }
    dbus.connect(QString(), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reloadConfig"),
                 Workspace::self(), SLOT(slotReloadConfig()));
    connect(kwinApp(), &Application::x11ConnectionChanged, this, &DBusInterface::announceService);
}

void DBusInterface::becomeKWinService(const QString &service)
{
    // TODO: this watchdog exists to make really safe that we at some point get the service
    // but it's probably no longer needed since we explicitly unregister the service with the deconstructor
    if (service == m_serviceName && QDBusConnection::sessionBus().registerService(m_serviceName) && sender()) {
        sender()->deleteLater(); // bye doggy :'(
        announceService();
    }
}

DBusInterface::~DBusInterface()
{
    QDBusConnection::sessionBus().unregisterService(m_serviceName);
    // KApplication automatically also grabs org.kde.kwin, so it's often been used externally - ensure to free it as well
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.kwin"));
    if (kwinApp()->x11Connection()) {
        xcb_delete_property(kwinApp()->x11Connection(), kwinApp()->x11RootWindow(), atoms->kwin_dbus_service);
    }
}

void DBusInterface::announceService()
{
    if (!kwinApp()->x11Connection()) {
        return;
    }
    const QByteArray service = m_serviceName.toUtf8();
    xcb_change_property(kwinApp()->x11Connection(), XCB_PROP_MODE_REPLACE, kwinApp()->x11RootWindow(), atoms->kwin_dbus_service,
                        atoms->utf8_string, 8, service.size(), service.constData());
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

#undef WRAP

bool DBusInterface::startActivity(const QString &in0)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return false;
    }
    return Activities::self()->start(in0);
#else
    Q_UNUSED(in0)
    return false;
#endif
}

bool DBusInterface::stopActivity(const QString &in0)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return false;
    }
    return Activities::self()->stop(in0);
#else
    Q_UNUSED(in0)
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
    VirtualDesktopManager::self()->moveTo<DesktopNext>();
}

void DBusInterface::previousDesktop()
{
    VirtualDesktopManager::self()->moveTo<DesktopPrevious>();
}

void DBusInterface::showDebugConsole()
{
    DebugConsole *console = new DebugConsole;
    console->show();
}

namespace {
QVariantMap clientToVariantMap(const AbstractClient *c)
{
    return {
        {QStringLiteral("resourceClass"), c->resourceClass()},
        {QStringLiteral("resourceName"), c->resourceName()},
        {QStringLiteral("desktopFile"), c->desktopFileName()},
        {QStringLiteral("role"), c->windowRole()},
        {QStringLiteral("caption"), c->captionNormal()},
        {QStringLiteral("clientMachine"), c->wmClientMachine(true)},
        {QStringLiteral("localhost"), c->isLocalhost()},
        {QStringLiteral("type"), c->windowType()},
        {QStringLiteral("x"), c->x()},
        {QStringLiteral("y"), c->y()},
        {QStringLiteral("width"), c->width()},
        {QStringLiteral("height"), c->height()},
        {QStringLiteral("x11DesktopNumber"), c->desktop()},
        {QStringLiteral("minimized"), c->isMinimized()},
        {QStringLiteral("shaded"), c->isShade()},
        {QStringLiteral("fullscreen"), c->isFullScreen()},
        {QStringLiteral("keepAbove"), c->keepAbove()},
        {QStringLiteral("keepBelow"), c->keepBelow()},
        {QStringLiteral("noBorder"), c->noBorder()},
        {QStringLiteral("skipTaskbar"), c->skipTaskbar()},
        {QStringLiteral("skipPager"), c->skipPager()},
        {QStringLiteral("skipSwitcher"), c->skipSwitcher()},
        {QStringLiteral("maximizeHorizontal"), c->maximizeMode() & MaximizeHorizontal},
        {QStringLiteral("maximizeVertical"), c->maximizeMode() & MaximizeVertical}
    };
}
}

QVariantMap DBusInterface::queryWindowInfo()
{
    m_replyQueryWindowInfo = message();
    setDelayedReply(true);
    kwinApp()->platform()->startInteractiveWindowSelection(
        [this] (Toplevel *t) {
            if (auto c = qobject_cast<AbstractClient*>(t)) {
                QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createReply(clientToVariantMap(c)));
            } else {
                QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createErrorReply(QString(), QString()));
            }
        }
    );
    return QVariantMap{};
}

QVariantMap DBusInterface::getWindowInfo(const QString &uuid)
{
    const auto id = QUuid::fromString(uuid);
    const auto client = workspace()->findAbstractClient([&id] (const AbstractClient *c) { return c->internalId() == id; });
    if (client) {
        return clientToVariantMap(client);
    } else {
        return {};
    }
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

QString CompositorDBusInterface::compositingNotPossibleReason() const
{
    return kwinApp()->platform()->compositingNotPossibleReason();
}

QString CompositorDBusInterface::compositingType() const
{
    if (!m_compositor->scene()) {
        return QStringLiteral("none");
    }
    switch (m_compositor->scene()->compositingType()) {
    case XRenderCompositing:
        return QStringLiteral("xrender");
    case OpenGL2Compositing:
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
    return kwinApp()->platform()->compositingPossible();
}

bool CompositorDBusInterface::isOpenGLBroken() const
{
    return kwinApp()->platform()->openGLCompositingIsBroken();
}

bool CompositorDBusInterface::platformRequiresCompositing() const
{
    return kwinApp()->platform()->requiresCompositing();
}

void CompositorDBusInterface::resume()
{
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        static_cast<X11Compositor*>(m_compositor)->resume(X11Compositor::ScriptSuspend);
    }
}

void CompositorDBusInterface::suspend()
{
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        static_cast<X11Compositor*>(m_compositor)->suspend(X11Compositor::ScriptSuspend);
    }
}

void CompositorDBusInterface::reinitialize()
{
    m_compositor->reinitialize();
}

QStringList CompositorDBusInterface::supportedOpenGLPlatformInterfaces() const
{
    QStringList interfaces;
    bool supportsGlx = false;
#if HAVE_EPOXY_GLX
    supportsGlx = (kwinApp()->operationMode() == Application::OperationModeX11);
#endif
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        supportsGlx = false;
    }
    if (supportsGlx) {
        interfaces << QStringLiteral("glx");
    }
    interfaces << QStringLiteral("egl");
    return interfaces;
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
        this
    );

    connect(m_manager, &VirtualDesktopManager::currentChanged, this,
        [this](uint previousDesktop, uint newDesktop) {
            Q_UNUSED(previousDesktop);
            Q_UNUSED(newDesktop);
            emit currentChanged(m_manager->currentDesktop()->id());
        }
    );

    connect(m_manager, &VirtualDesktopManager::countChanged, this,
        [this](uint previousCount, uint newCount) {
            Q_UNUSED(previousCount);
            emit countChanged(newCount);
            emit desktopsChanged(desktops());
        }
    );

    connect(m_manager, &VirtualDesktopManager::navigationWrappingAroundChanged, this,
        [this]() {
            emit navigationWrappingAroundChanged(isNavigationWrappingAround());
        }
    );

    connect(m_manager, &VirtualDesktopManager::rowsChanged, this, &VirtualDesktopManagerDBusInterface::rowsChanged);

    for (auto *vd : m_manager->desktops()) {
        connect(vd, &VirtualDesktop::x11DesktopNumberChanged, this,
            [this, vd]() {
                DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
                emit desktopDataChanged(vd->id(), data);
                emit desktopsChanged(desktops());
            }
        );
        connect(vd, &VirtualDesktop::nameChanged, this,
            [this, vd]() {
                DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
                emit desktopDataChanged(vd->id(), data);
                emit desktopsChanged(desktops());
            }
        );
    }
    connect(m_manager, &VirtualDesktopManager::desktopCreated, this,
        [this](VirtualDesktop *vd) {
            connect(vd, &VirtualDesktop::x11DesktopNumberChanged, this,
                [this, vd]() {
                    DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
                    emit desktopDataChanged(vd->id(), data);
                    emit desktopsChanged(desktops());
                }
            );
            connect(vd, &VirtualDesktop::nameChanged, this,
                [this, vd]() {
                    DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
                    emit desktopDataChanged(vd->id(), data);
                    emit desktopsChanged(desktops());
                }
            );
            DBusDesktopDataStruct data{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
            emit desktopCreated(vd->id(), data);
            emit desktopsChanged(desktops());
        }
    );
    connect(m_manager, &VirtualDesktopManager::desktopRemoved, this,
        [this](VirtualDesktop *vd) {
            emit desktopRemoved(vd->id());
            emit desktopsChanged(desktops());
        }
    );
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

    auto *vd = m_manager->desktopForId(id.toUtf8());
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
        [] (const VirtualDesktop *vd) {
            return DBusDesktopDataStruct{.position = vd->x11DesktopNumber() - 1, .id = vd->id(), .name = vd->name()};
        }
    );

    return desktopVect;
}

void VirtualDesktopManagerDBusInterface::createDesktop(uint position, const QString &name)
{
    m_manager->createVirtualDesktop(position, name);
}

void VirtualDesktopManagerDBusInterface::setDesktopName(const QString &id, const QString &name)
{
    VirtualDesktop *vd = m_manager->desktopForId(id.toUtf8());
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
    m_manager->removeVirtualDesktop(id.toUtf8());
}

} // namespace
