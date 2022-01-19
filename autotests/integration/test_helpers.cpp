/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "screenlockerwatcher.h"
#include "wayland_server.h"
#include "workspace.h"
#include "inputmethod.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shadow.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/subcompositor.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/textinput.h>
#include <KWayland/Client/appmenu.h>
#include <KWaylandServer/display.h>

//screenlocker
#include <KScreenLocker/KsldApp>

#include <QThread>

// system
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace KWayland::Client;

namespace KWin
{
namespace Test
{

LayerShellV1::~LayerShellV1()
{
    destroy();
}

LayerSurfaceV1::~LayerSurfaceV1()
{
    destroy();
}

void LayerSurfaceV1::zwlr_layer_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height)
{
    Q_EMIT configureRequested(serial, QSize(width, height));
}

void LayerSurfaceV1::zwlr_layer_surface_v1_closed()
{
    Q_EMIT closeRequested();
}

XdgShell::~XdgShell()
{
    destroy();
}

XdgSurface::XdgSurface(XdgShell *shell, KWayland::Client::Surface *surface, QObject *parent)
    : QObject(parent)
    , QtWayland::xdg_surface(shell->get_xdg_surface(*surface))
    , m_surface(surface)
{
}

XdgSurface::~XdgSurface()
{
    destroy();
}

KWayland::Client::Surface *XdgSurface::surface() const
{
    return m_surface;
}

void XdgSurface::xdg_surface_configure(uint32_t serial)
{
    Q_EMIT configureRequested(serial);
}

XdgToplevel::XdgToplevel(XdgSurface *surface, QObject *parent)
    : QObject(parent)
    , QtWayland::xdg_toplevel(surface->get_toplevel())
    , m_xdgSurface(surface)
{
}

XdgToplevel::~XdgToplevel()
{
    destroy();
}

XdgSurface *XdgToplevel::xdgSurface() const
{
    return m_xdgSurface.data();
}

void XdgToplevel::xdg_toplevel_configure(int32_t width, int32_t height, wl_array *states)
{
    States requestedStates;

    const uint32_t *stateData = static_cast<const uint32_t *>(states->data);
    const size_t stateCount = states->size / sizeof(uint32_t);

    for (size_t i = 0; i < stateCount; ++i) {
        switch (stateData[i]) {
        case QtWayland::xdg_toplevel::state_maximized:
            requestedStates |= State::Maximized;
            break;
        case QtWayland::xdg_toplevel::state_fullscreen:
            requestedStates |= State::Fullscreen;
            break;
        case QtWayland::xdg_toplevel::state_resizing:
            requestedStates |= State::Resizing;
            break;
        case QtWayland::xdg_toplevel::state_activated:
            requestedStates |= State::Activated;
            break;
        }
    }

    Q_EMIT configureRequested(QSize(width, height), requestedStates);
}

void XdgToplevel::xdg_toplevel_close()
{
    Q_EMIT closeRequested();
}

XdgPositioner::XdgPositioner(XdgShell *shell)
    : QtWayland::xdg_positioner(shell->create_positioner())
{
}

XdgPositioner::~XdgPositioner()
{
    destroy();
}

XdgPopup::XdgPopup(XdgSurface *surface, XdgSurface *parentSurface, XdgPositioner *positioner, QObject *parent)
    : QObject(parent)
    , QtWayland::xdg_popup(surface->get_popup(parentSurface->object(), positioner->object()))
    , m_xdgSurface(surface)
{
}

XdgPopup::~XdgPopup()
{
    destroy();
}

XdgSurface *XdgPopup::xdgSurface() const
{
    return m_xdgSurface.data();
}

void XdgPopup::xdg_popup_configure(int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_EMIT configureRequested(QRect(x, y, width, height));
}

void XdgPopup::xdg_popup_popup_done()
{
    Q_EMIT doneReceived();
}

XdgDecorationManagerV1::~XdgDecorationManagerV1()
{
    destroy();
}

XdgToplevelDecorationV1::XdgToplevelDecorationV1(XdgDecorationManagerV1 *manager,
                                                 XdgToplevel *toplevel, QObject *parent)
    : QObject(parent)
    , QtWayland::zxdg_toplevel_decoration_v1(manager->get_toplevel_decoration(toplevel->object()))
{
}

XdgToplevelDecorationV1::~XdgToplevelDecorationV1()
{
    destroy();
}

void XdgToplevelDecorationV1::zxdg_toplevel_decoration_v1_configure(uint32_t m)
{
    Q_EMIT configureRequested(mode(m));
}

IdleInhibitManagerV1::~IdleInhibitManagerV1()
{
    destroy();
}

IdleInhibitorV1::IdleInhibitorV1(IdleInhibitManagerV1 *manager, KWayland::Client::Surface *surface)
    : QtWayland::zwp_idle_inhibitor_v1(manager->create_inhibitor(*surface))
{
}

IdleInhibitorV1::~IdleInhibitorV1()
{
    destroy();
}

static struct {
    ConnectionThread *connection = nullptr;
    EventQueue *queue = nullptr;
    KWayland::Client::Compositor *compositor = nullptr;
    SubCompositor *subCompositor = nullptr;
    ServerSideDecorationManager *decoration = nullptr;
    ShadowManager *shadowManager = nullptr;
    XdgShell *xdgShell = nullptr;
    ShmPool *shm = nullptr;
    Seat *seat = nullptr;
    PlasmaShell *plasmaShell = nullptr;
    PlasmaWindowManagement *windowManagement = nullptr;
    PointerConstraints *pointerConstraints = nullptr;
    Registry *registry = nullptr;
    WaylandOutputManagementV2 *outputManagementV2 = nullptr;
    QThread *thread = nullptr;
    QVector<Output*> outputs;
    QVector<WaylandOutputDeviceV2 *> outputDevicesV2;
    IdleInhibitManagerV1 *idleInhibitManagerV1 = nullptr;
    AppMenuManager *appMenu = nullptr;
    XdgDecorationManagerV1 *xdgDecorationManagerV1 = nullptr;
    TextInputManager *textInputManager = nullptr;
    QtWayland::zwp_input_panel_v1 *inputPanelV1 = nullptr;
    MockInputMethod *inputMethodV1 = nullptr;
    QtWayland::zwp_input_method_context_v1 *inputMethodContextV1 = nullptr;
    LayerShellV1 *layerShellV1 = nullptr;
    TextInputManagerV3 *textInputManagerV3 = nullptr;
} s_waylandConnection;

AbstractClient *inputPanelClient()
{
    return s_waylandConnection.inputMethodV1->client();
}

MockInputMethod *inputMethod()
{
    return s_waylandConnection.inputMethodV1;
}

KWayland::Client::Surface *inputPanelSurface()
{
    return s_waylandConnection.inputMethodV1->inputPanelSurface();
}

MockInputMethod::MockInputMethod(struct wl_registry *registry, int id, int version)
    : QtWayland::zwp_input_method_v1(registry, id, version)
{

}
MockInputMethod::~MockInputMethod()
{
}

void MockInputMethod::zwp_input_method_v1_activate(struct ::zwp_input_method_context_v1 *context)
{
    Q_UNUSED(context)
    if (!m_inputSurface) {
        m_inputSurface = Test::createSurface();
        m_inputMethodSurface = Test::createInputPanelSurfaceV1(m_inputSurface, s_waylandConnection.outputs.first());
    }
    m_context = context;
    m_client = Test::renderAndWaitForShown(m_inputSurface, QSize(1280, 400), Qt::blue);

    Q_EMIT activate();
}

void MockInputMethod::zwp_input_method_v1_deactivate(struct ::zwp_input_method_context_v1 *context)
{
    QCOMPARE(context, m_context);
    zwp_input_method_context_v1_destroy(context);
    m_context = nullptr;

    if (m_inputSurface) {
        m_inputSurface->release();
        m_inputSurface->destroy();
        delete m_inputSurface;
        m_inputSurface = nullptr;
        delete m_inputMethodSurface;
        m_inputMethodSurface = nullptr;
    }
}

bool setupWaylandConnection(AdditionalWaylandInterfaces flags)
{
    if (s_waylandConnection.connection) {
        return false;
    }

    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        return false;
    }
    KWin::waylandServer()->display()->createClient(sx[0]);
    // setup connection
    s_waylandConnection.connection = new ConnectionThread;
    QSignalSpy connectedSpy(s_waylandConnection.connection, &ConnectionThread::connected);
    if (!connectedSpy.isValid()) {
        return false;
    }
    s_waylandConnection.connection->setSocketFd(sx[1]);

    s_waylandConnection.thread = new QThread(kwinApp());
    s_waylandConnection.connection->moveToThread(s_waylandConnection.thread);
    s_waylandConnection.thread->start();

    s_waylandConnection.connection->initConnection();
    if (!connectedSpy.wait()) {
        return false;
    }

    s_waylandConnection.queue = new EventQueue;
    s_waylandConnection.queue->setup(s_waylandConnection.connection);
    if (!s_waylandConnection.queue->isValid()) {
        return false;
    }

    Registry *registry = new Registry;
    s_waylandConnection.registry = registry;
    registry->setEventQueue(s_waylandConnection.queue);

    QObject::connect(registry, &Registry::outputAnnounced, [=](quint32 name, quint32 version) {
        Output* output = registry->createOutput(name, version, s_waylandConnection.registry);
        s_waylandConnection.outputs << output;
        QObject::connect(output, &Output::removed, [=]() {
            output->deleteLater();
            s_waylandConnection.outputs.removeOne(output);
        });
        QObject::connect(output, &Output::destroyed, [=]() {
            s_waylandConnection.outputs.removeOne(output);
        });
    });

    QObject::connect(registry, &Registry::interfaceAnnounced, [=](const QByteArray &interface, quint32 name, quint32 version) {
        if (flags & AdditionalWaylandInterface::InputMethodV1) {
            if (interface == QByteArrayLiteral("zwp_input_method_v1")) {
                s_waylandConnection.inputMethodV1 = new MockInputMethod(*registry, name, version);
            } else if (interface == QByteArrayLiteral("zwp_input_panel_v1")) {
                s_waylandConnection.inputPanelV1 = new QtWayland::zwp_input_panel_v1(*registry, name, version);
            }
        }
        if (flags & AdditionalWaylandInterface::LayerShellV1) {
            if (interface == QByteArrayLiteral("zwlr_layer_shell_v1")) {
                s_waylandConnection.layerShellV1 = new LayerShellV1();
                s_waylandConnection.layerShellV1->init(*registry, name, version);
            }
        }
        if (flags & AdditionalWaylandInterface::TextInputManagerV3) {
            // do something
            if (interface == QByteArrayLiteral("zwp_text_input_manager_v3")) {
                s_waylandConnection.textInputManagerV3 = new TextInputManagerV3();
                s_waylandConnection.textInputManagerV3->init(*registry, name, version);
            }
        }
        if (interface == QByteArrayLiteral("xdg_wm_base")) {
            s_waylandConnection.xdgShell = new XdgShell();
            s_waylandConnection.xdgShell->init(*registry, name, version);
        }
        if (flags & AdditionalWaylandInterface::XdgDecorationV1) {
            if (interface == zxdg_decoration_manager_v1_interface.name) {
                s_waylandConnection.xdgDecorationManagerV1 = new XdgDecorationManagerV1();
                s_waylandConnection.xdgDecorationManagerV1->init(*registry, name, version);
                return;
            }
        }
        if (flags & AdditionalWaylandInterface::IdleInhibitV1) {
            if (interface == zwp_idle_inhibit_manager_v1_interface.name) {
                s_waylandConnection.idleInhibitManagerV1 = new IdleInhibitManagerV1();
                s_waylandConnection.idleInhibitManagerV1->init(*registry, name, version);
                return;
            }
        }
        if (flags & AdditionalWaylandInterface::OutputDeviceV2) {
            if (interface == kde_output_device_v2_interface.name) {
                WaylandOutputDeviceV2 *device = new WaylandOutputDeviceV2(name);
                device->init(*registry, name, version);

                s_waylandConnection.outputDevicesV2 << device;

                QObject::connect(device, &WaylandOutputDeviceV2::destroyed, [=]() {
                    s_waylandConnection.outputDevicesV2.removeOne(device);
                    device->deleteLater();
                });

                QObject::connect(registry, &KWayland::Client::Registry::interfaceRemoved, device, [name, device](const quint32 &interfaceName) {
                    if (name == interfaceName) {
                        s_waylandConnection.outputDevicesV2.removeOne(device);
                        device->deleteLater();
                    }
                });

                return;
            }
        }
        if (flags & AdditionalWaylandInterface::OutputManagementV2) {
            if (interface == kde_output_management_v2_interface.name) {
                s_waylandConnection.outputManagementV2 = new WaylandOutputManagementV2(*registry, name, version);
                return;
            }
        }
    });

    QSignalSpy allAnnounced(registry, &Registry::interfacesAnnounced);
    if (!allAnnounced.isValid()) {
        return false;
    }
    registry->create(s_waylandConnection.connection);
    if (!registry->isValid()) {
        return false;
    }
    registry->setup();
    if (!allAnnounced.wait()) {
        return false;
    }

    s_waylandConnection.compositor = registry->createCompositor(registry->interface(Registry::Interface::Compositor).name, registry->interface(Registry::Interface::Compositor).version);
    if (!s_waylandConnection.compositor->isValid()) {
        return false;
    }
    s_waylandConnection.subCompositor = registry->createSubCompositor(registry->interface(Registry::Interface::SubCompositor).name, registry->interface(Registry::Interface::SubCompositor).version);
    if (!s_waylandConnection.subCompositor->isValid()) {
        return false;
    }
    s_waylandConnection.shm = registry->createShmPool(registry->interface(Registry::Interface::Shm).name, registry->interface(Registry::Interface::Shm).version);
    if (!s_waylandConnection.shm->isValid()) {
        return false;
    }
    if (flags.testFlag(AdditionalWaylandInterface::Seat)) {
        s_waylandConnection.seat = registry->createSeat(registry->interface(Registry::Interface::Seat).name, registry->interface(Registry::Interface::Seat).version);
        if (!s_waylandConnection.seat->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::ShadowManager)) {
        s_waylandConnection.shadowManager = registry->createShadowManager(registry->interface(Registry::Interface::Shadow).name,
                                                                          registry->interface(Registry::Interface::Shadow).version);
        if (!s_waylandConnection.shadowManager->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::Decoration)) {
        s_waylandConnection.decoration = registry->createServerSideDecorationManager(registry->interface(Registry::Interface::ServerSideDecorationManager).name,
                                                                                    registry->interface(Registry::Interface::ServerSideDecorationManager).version);
        if (!s_waylandConnection.decoration->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::PlasmaShell)) {
        s_waylandConnection.plasmaShell = registry->createPlasmaShell(registry->interface(Registry::Interface::PlasmaShell).name,
                                                                     registry->interface(Registry::Interface::PlasmaShell).version);
        if (!s_waylandConnection.plasmaShell->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::WindowManagement)) {
        s_waylandConnection.windowManagement = registry->createPlasmaWindowManagement(registry->interface(Registry::Interface::PlasmaWindowManagement).name,
                                                                                     registry->interface(Registry::Interface::PlasmaWindowManagement).version);
        if (!s_waylandConnection.windowManagement->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::PointerConstraints)) {
        s_waylandConnection.pointerConstraints = registry->createPointerConstraints(registry->interface(Registry::Interface::PointerConstraintsUnstableV1).name,
                                                                                   registry->interface(Registry::Interface::PointerConstraintsUnstableV1).version);
        if (!s_waylandConnection.pointerConstraints->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::AppMenu)) {
        s_waylandConnection.appMenu = registry->createAppMenuManager(registry->interface(Registry::Interface::AppMenu).name, registry->interface(Registry::Interface::AppMenu).version);
        if (!s_waylandConnection.appMenu->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::TextInputManagerV2)) {
        s_waylandConnection.textInputManager = registry->createTextInputManager(registry->interface(Registry::Interface::TextInputManagerUnstableV2).name, registry->interface(Registry::Interface::TextInputManagerUnstableV2).version);
        if (!s_waylandConnection.textInputManager->isValid()) {
            return false;
        }
    }

    return true;
}

void destroyWaylandConnection()
{
    delete s_waylandConnection.compositor;
    s_waylandConnection.compositor = nullptr;
    delete s_waylandConnection.subCompositor;
    s_waylandConnection.subCompositor = nullptr;
    delete s_waylandConnection.windowManagement;
    s_waylandConnection.windowManagement = nullptr;
    delete s_waylandConnection.plasmaShell;
    s_waylandConnection.plasmaShell = nullptr;
    delete s_waylandConnection.decoration;
    s_waylandConnection.decoration = nullptr;
    delete s_waylandConnection.decoration;
    s_waylandConnection.decoration = nullptr;
    delete s_waylandConnection.seat;
    s_waylandConnection.seat = nullptr;
    delete s_waylandConnection.pointerConstraints;
    s_waylandConnection.pointerConstraints = nullptr;
    delete s_waylandConnection.xdgShell;
    s_waylandConnection.xdgShell = nullptr;
    delete s_waylandConnection.shadowManager;
    s_waylandConnection.shadowManager = nullptr;
    delete s_waylandConnection.idleInhibitManagerV1;
    s_waylandConnection.idleInhibitManagerV1 = nullptr;
    delete s_waylandConnection.shm;
    s_waylandConnection.shm = nullptr;
    delete s_waylandConnection.queue;
    s_waylandConnection.queue = nullptr;
    delete s_waylandConnection.registry;
    s_waylandConnection.registry = nullptr;
    delete s_waylandConnection.appMenu;
    s_waylandConnection.appMenu = nullptr;
    delete s_waylandConnection.xdgDecorationManagerV1;
    s_waylandConnection.xdgDecorationManagerV1 = nullptr;
    delete s_waylandConnection.textInputManager;
    s_waylandConnection.textInputManager = nullptr;
    delete s_waylandConnection.inputPanelV1;
    s_waylandConnection.inputPanelV1 = nullptr;
    delete s_waylandConnection.layerShellV1;
    s_waylandConnection.layerShellV1 = nullptr;
    delete s_waylandConnection.outputManagementV2;
    s_waylandConnection.outputManagementV2 = nullptr;
    if (s_waylandConnection.thread) {
        QSignalSpy spy(s_waylandConnection.connection, &QObject::destroyed);
        s_waylandConnection.connection->deleteLater();
        if (spy.isEmpty()) {
            QVERIFY(spy.wait());
        }
        s_waylandConnection.thread->quit();
        s_waylandConnection.thread->wait();
        delete s_waylandConnection.thread;
        s_waylandConnection.thread = nullptr;
        s_waylandConnection.connection = nullptr;
    }
    s_waylandConnection.outputs.clear();
    s_waylandConnection.outputDevicesV2.clear();
}

ConnectionThread *waylandConnection()
{
    return s_waylandConnection.connection;
}

KWayland::Client::Compositor *waylandCompositor()
{
    return s_waylandConnection.compositor;
}

SubCompositor *waylandSubCompositor()
{
    return s_waylandConnection.subCompositor;
}

ShadowManager *waylandShadowManager()
{
    return s_waylandConnection.shadowManager;
}

ShmPool *waylandShmPool()
{
    return s_waylandConnection.shm;
}

Seat *waylandSeat()
{
    return s_waylandConnection.seat;
}

ServerSideDecorationManager *waylandServerSideDecoration()
{
    return s_waylandConnection.decoration;
}

PlasmaShell *waylandPlasmaShell()
{
    return s_waylandConnection.plasmaShell;
}

PlasmaWindowManagement *waylandWindowManagement()
{
    return s_waylandConnection.windowManagement;
}

PointerConstraints *waylandPointerConstraints()
{
    return s_waylandConnection.pointerConstraints;
}

AppMenuManager* waylandAppMenuManager()
{
    return s_waylandConnection.appMenu;
}

KWin::Test::WaylandOutputManagementV2 *waylandOutputManagementV2()
{
    return s_waylandConnection.outputManagementV2;
}

TextInputManager *waylandTextInputManager()
{
    return s_waylandConnection.textInputManager;
}

TextInputManagerV3 *waylandTextInputManagerV3()
{
    return s_waylandConnection.textInputManagerV3;
}

QVector<KWayland::Client::Output *> waylandOutputs()
{
    return s_waylandConnection.outputs;
}

QVector<KWin::Test::WaylandOutputDeviceV2 *> waylandOutputDevicesV2()
{
    return s_waylandConnection.outputDevicesV2;
}

bool waitForWaylandSurface(AbstractClient *client)
{
    if (client->surface()) {
        return true;
    }
    QSignalSpy surfaceChangedSpy(client, &Toplevel::surfaceChanged);
    return surfaceChangedSpy.wait();
}

bool waitForWaylandPointer()
{
    if (!s_waylandConnection.seat) {
        return false;
    }
    QSignalSpy hasPointerSpy(s_waylandConnection.seat, &Seat::hasPointerChanged);
    if (!hasPointerSpy.isValid()) {
        return false;
    }
    return hasPointerSpy.wait();
}

bool waitForWaylandTouch()
{
    if (!s_waylandConnection.seat) {
        return false;
    }
    QSignalSpy hasTouchSpy(s_waylandConnection.seat, &Seat::hasTouchChanged);
    if (!hasTouchSpy.isValid()) {
        return false;
    }
    return hasTouchSpy.wait();
}

bool waitForWaylandKeyboard()
{
    if (!s_waylandConnection.seat) {
        return false;
    }
    QSignalSpy hasKeyboardSpy(s_waylandConnection.seat, &Seat::hasKeyboardChanged);
    if (!hasKeyboardSpy.isValid()) {
        return false;
    }
    return hasKeyboardSpy.wait();
}

void render(KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format)
{
    QImage img(size, format);
    img.fill(color);
    render(surface, img);
}

void render(KWayland::Client::Surface *surface, const QImage &img)
{
    surface->attachBuffer(s_waylandConnection.shm->createBuffer(img));
    surface->damage(QRect(QPoint(0, 0), img.size()));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
}

AbstractClient *waitForWaylandWindowShown(int timeout)
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    if (!clientAddedSpy.isValid()) {
        return nullptr;
    }
    if (!clientAddedSpy.wait(timeout)) {
        return nullptr;
    }
    return clientAddedSpy.first().first().value<AbstractClient *>();
}

AbstractClient *renderAndWaitForShown(KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format, int timeout)
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    if (!clientAddedSpy.isValid()) {
        return nullptr;
    }
    render(surface, size, color, format);
    flushWaylandConnection();
    if (!clientAddedSpy.wait(timeout)) {
        return nullptr;
    }
    return clientAddedSpy.first().first().value<AbstractClient *>();
}

void flushWaylandConnection()
{
    if (s_waylandConnection.connection) {
        s_waylandConnection.connection->flush();
    }
}

KWayland::Client::Surface *createSurface(QObject *parent)
{
    if (!s_waylandConnection.compositor) {
        return nullptr;
    }
    auto s = s_waylandConnection.compositor->createSurface(parent);
    if (!s->isValid()) {
        delete s;
        return nullptr;
    }
    return s;
}

SubSurface *createSubSurface(KWayland::Client::Surface *surface, KWayland::Client::Surface *parentSurface, QObject *parent)
{
    if (!s_waylandConnection.subCompositor) {
        return nullptr;
    }
    auto s = s_waylandConnection.subCompositor->createSubSurface(surface, parentSurface, parent);
    if (!s->isValid()) {
        delete s;
        return nullptr;
    }
    return s;
}

LayerSurfaceV1 *createLayerSurfaceV1(KWayland::Client::Surface *surface, const QString &scope, Output *output, LayerShellV1::layer layer)
{
    LayerShellV1 *shell = s_waylandConnection.layerShellV1;
    if (!shell) {
        qWarning() << "Could not create a layer surface because the layer shell global is not bound";
        return nullptr;
    }

    struct ::wl_output *nativeOutput = nullptr;
    if (output) {
        nativeOutput = *output;
    }

    LayerSurfaceV1 *shellSurface = new LayerSurfaceV1();
    shellSurface->init(shell->get_layer_surface(*surface, nativeOutput, layer, scope));

    return shellSurface;
}

QtWayland::zwp_input_panel_surface_v1 *createInputPanelSurfaceV1(KWayland::Client::Surface *surface, Output *output)
{
    if (!s_waylandConnection.inputPanelV1) {
        qWarning() << "Unable to create the input panel surface. The interface input_panel global is not bound";
        return nullptr;
    }
    QtWayland::zwp_input_panel_surface_v1 *s = new QtWayland::zwp_input_panel_surface_v1(s_waylandConnection.inputPanelV1->get_input_panel_surface(*surface));

    if (!s->isInitialized()) {
        delete s;
        return nullptr;
    }

    s->set_toplevel(output->output(), QtWayland::zwp_input_panel_surface_v1::position_center_bottom);

    return s;
}

static void waitForConfigured(XdgSurface *shellSurface)
{
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface, &XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.isValid());

    shellSurface->surface()->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->ack_configure(surfaceConfigureRequestedSpy.last().first().toUInt());
}

XdgToplevel *createXdgToplevelSurface(KWayland::Client::Surface *surface, QObject *parent)
{
    return createXdgToplevelSurface(surface, CreationSetup::CreateAndConfigure, parent);
}

XdgToplevel *createXdgToplevelSurface(KWayland::Client::Surface *surface, CreationSetup configureMode, QObject *parent)
{
    XdgShell *shell = s_waylandConnection.xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_toplevel surface because xdg_wm_base global is not bound";
        return nullptr;
    }

    XdgSurface *xdgSurface = new XdgSurface(shell, surface);
    XdgToplevel *xdgToplevel = new XdgToplevel(xdgSurface, parent);

    if (configureMode == CreationSetup::CreateAndConfigure) {
        waitForConfigured(xdgSurface);
    }

    return xdgToplevel;
}

XdgPositioner *createXdgPositioner()
{
    XdgShell *shell = s_waylandConnection.xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_positioner object because xdg_wm_base global is not bound";
        return nullptr;
    }

    return new XdgPositioner(shell);
}

XdgPopup *createXdgPopupSurface(KWayland::Client::Surface *surface, XdgSurface *parentSurface, XdgPositioner *positioner,
                                CreationSetup configureMode, QObject *parent)
{
    XdgShell *shell = s_waylandConnection.xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_popup surface because xdg_wm_base global is not bound";
        return nullptr;
    }

    XdgSurface *xdgSurface = new XdgSurface(shell, surface);
    XdgPopup *xdgPopup = new XdgPopup(xdgSurface, parentSurface, positioner, parent);

    if (configureMode == CreationSetup::CreateAndConfigure) {
        waitForConfigured(xdgSurface);
    }

    return xdgPopup;
}

XdgToplevelDecorationV1 *createXdgToplevelDecorationV1(XdgToplevel *toplevel, QObject *parent)
{
    XdgDecorationManagerV1 *manager = s_waylandConnection.xdgDecorationManagerV1;

    if (!manager) {
        qWarning() << "Could not create an xdg_toplevel_decoration_v1 because xdg_decoration_manager_v1 global is not bound";
        return nullptr;
    }

    return new XdgToplevelDecorationV1(manager, toplevel, parent);
}

IdleInhibitorV1 *createIdleInhibitorV1(KWayland::Client::Surface *surface)
{
    IdleInhibitManagerV1 *manager = s_waylandConnection.idleInhibitManagerV1;
    if (!manager) {
        qWarning() << "Could not create an idle_inhibitor_v1 because idle_inhibit_manager_v1 global is not bound";
        return nullptr;
    }

    return new IdleInhibitorV1(manager, surface);
}

bool waitForWindowDestroyed(AbstractClient *client)
{
    QSignalSpy destroyedSpy(client, &QObject::destroyed);
    if (!destroyedSpy.isValid()) {
        return false;
    }
    return destroyedSpy.wait();
}

bool lockScreen()
{
    if (waylandServer()->isScreenLocked()) {
        return false;
    }
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    if (!lockStateChangedSpy.isValid()) {
        return false;
    }
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    if (lockStateChangedSpy.count() != 1) {
        return false;
    }
    if (!waylandServer()->isScreenLocked()) {
        return false;
    }
    if (!ScreenLockerWatcher::self()->isLocked()) {
        QSignalSpy lockedSpy(ScreenLockerWatcher::self(), &ScreenLockerWatcher::locked);
        if (!lockedSpy.isValid()) {
            return false;
        }
        if (!lockedSpy.wait()) {
            return false;
        }
        if (!ScreenLockerWatcher::self()->isLocked()) {
            return false;
        }
    }
    return true;
}

bool unlockScreen()
{
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    if (!lockStateChangedSpy.isValid()) {
        return false;
    }
    using namespace ScreenLocker;
    const auto children = KSldApp::self()->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
    if (waylandServer()->isScreenLocked()) {
        lockStateChangedSpy.wait();
    }
    if (waylandServer()->isScreenLocked()) {
        return true;
    }
    if (ScreenLockerWatcher::self()->isLocked()) {
        QSignalSpy lockedSpy(ScreenLockerWatcher::self(), &ScreenLockerWatcher::locked);
        if (!lockedSpy.isValid()) {
            return false;
        }
        if (!lockedSpy.wait()) {
            return false;
        }
        if (ScreenLockerWatcher::self()->isLocked()) {
            return false;
        }
    }
    return true;
}

void initWaylandWorkspace()
{
    QSignalSpy workspaceInitializedSpy(waylandServer(), &WaylandServer::initialized);
    waylandServer()->initWorkspace();
    QVERIFY(workspaceInitializedSpy.count() || workspaceInitializedSpy.wait());
}

WaylandOutputManagementV2::WaylandOutputManagementV2(struct ::wl_registry *registry, int id, int version)
    : QObject()
    , QtWayland::kde_output_management_v2()
{
    init(registry, id, version);
}

WaylandOutputConfigurationV2 *WaylandOutputManagementV2::createConfiguration()
{
    return new WaylandOutputConfigurationV2(create_configuration());
}

WaylandOutputConfigurationV2::WaylandOutputConfigurationV2(struct ::kde_output_configuration_v2 *object)
    : QObject()
    , QtWayland::kde_output_configuration_v2()
{
    init(object);
}

void WaylandOutputConfigurationV2::kde_output_configuration_v2_applied()
{
    Q_EMIT applied();
}
void WaylandOutputConfigurationV2::kde_output_configuration_v2_failed()
{
    Q_EMIT failed();
}

WaylandOutputDeviceV2Mode::WaylandOutputDeviceV2Mode(struct ::kde_output_device_mode_v2 *object)
    : QtWayland::kde_output_device_mode_v2(object)
{
}

WaylandOutputDeviceV2Mode::~WaylandOutputDeviceV2Mode()
{
    kde_output_device_mode_v2_destroy(object());
}

void WaylandOutputDeviceV2Mode::kde_output_device_mode_v2_size(int32_t width, int32_t height)
{
    m_size = QSize(width, height);
}

void WaylandOutputDeviceV2Mode::kde_output_device_mode_v2_refresh(int32_t refresh)
{
    m_refreshRate = refresh;
}

void WaylandOutputDeviceV2Mode::kde_output_device_mode_v2_preferred()
{
    m_preferred = true;
}

void WaylandOutputDeviceV2Mode::kde_output_device_mode_v2_removed()
{
    Q_EMIT removed();
}

int WaylandOutputDeviceV2Mode::refreshRate() const
{
    return m_refreshRate;
}

QSize WaylandOutputDeviceV2Mode::size() const
{
    return m_size;
}

bool WaylandOutputDeviceV2Mode::preferred() const
{
    return m_preferred;
}

bool WaylandOutputDeviceV2Mode::operator==(const WaylandOutputDeviceV2Mode &other)
{
    return m_size == other.m_size && m_refreshRate == other.m_refreshRate && m_preferred == other.m_preferred;
}

WaylandOutputDeviceV2Mode *WaylandOutputDeviceV2Mode::get(struct ::kde_output_device_mode_v2 *object)
{
    auto mode = QtWayland::kde_output_device_mode_v2::fromObject(object);
    return static_cast<WaylandOutputDeviceV2Mode *>(mode);
}

WaylandOutputDeviceV2::WaylandOutputDeviceV2(int id)
    : QObject()
    , kde_output_device_v2()
    , m_id(id)
{
}

WaylandOutputDeviceV2::~WaylandOutputDeviceV2()
{
    qDeleteAll(m_modes);

    kde_output_device_v2_destroy(object());
}

void WaylandOutputDeviceV2::kde_output_device_v2_geometry(int32_t x,
                                                          int32_t y,
                                                          int32_t physical_width,
                                                          int32_t physical_height,
                                                          int32_t subpixel,
                                                          const QString &make,
                                                          const QString &model,
                                                          int32_t transform)
{
    m_pos = QPoint(x, y);
    m_physicalSize = QSize(physical_width, physical_height);
    m_subpixel = subpixel;
    m_manufacturer = make;
    m_model = model;
    m_transform = transform;
}

void WaylandOutputDeviceV2::kde_output_device_v2_current_mode(struct ::kde_output_device_mode_v2 *mode)
{
    auto m = WaylandOutputDeviceV2Mode::get(mode);

    if (*m == *m_mode) {
        // unchanged
        return;
    }
    m_mode = m;
}

void WaylandOutputDeviceV2::kde_output_device_v2_mode(struct ::kde_output_device_mode_v2 *mode)
{
    WaylandOutputDeviceV2Mode *m = new WaylandOutputDeviceV2Mode(mode);
    // last mode sent is the current one
    m_mode = m;
    m_modes.append(m);

    connect(m, &WaylandOutputDeviceV2Mode::removed, this, [this, m]() {
        m_modes.removeOne(m);
        if (m_mode == m) {
            if (!m_modes.isEmpty()) {
                m_mode = m_modes.first();
            } else {
                // was last mode
                qFatal("KWaylandBackend: no output modes available anymore, this seems like a compositor bug");
            }
        }

        delete m;
    });
}

QString WaylandOutputDeviceV2::modeId() const
{
    return QString::number(m_modes.indexOf(m_mode));
}

WaylandOutputDeviceV2Mode *WaylandOutputDeviceV2::deviceModeFromId(const int modeId) const
{
    return m_modes.at(modeId);
}

QString WaylandOutputDeviceV2::modeName(const WaylandOutputDeviceV2Mode *m) const
{
    return QString::number(m->size().width()) + QLatin1Char('x') + QString::number(m->size().height()) + QLatin1Char('@')
        + QString::number(qRound(m->refreshRate() / 1000.0));
}

QString WaylandOutputDeviceV2::name() const
{
    return QStringLiteral("%1 %2").arg(m_manufacturer, m_model);
}

QDebug operator<<(QDebug dbg, const WaylandOutputDeviceV2 *output)
{
    dbg << "WaylandOutput(Id:" << output->id() << ", Name:" << QString(output->manufacturer() + QLatin1Char(' ') + output->model()) << ")";
    return dbg;
}

void WaylandOutputDeviceV2::kde_output_device_v2_done()
{
    Q_EMIT done();
}

void WaylandOutputDeviceV2::kde_output_device_v2_scale(wl_fixed_t factor)
{
    m_factor = wl_fixed_to_double(factor);
}

void WaylandOutputDeviceV2::kde_output_device_v2_edid(const QString &edid)
{
    m_edid = QByteArray::fromBase64(edid.toUtf8());
}

void WaylandOutputDeviceV2::kde_output_device_v2_enabled(int32_t enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        Q_EMIT enabledChanged();
    }
}

void WaylandOutputDeviceV2::kde_output_device_v2_uuid(const QString &uuid)
{
    m_uuid = uuid;
}

void WaylandOutputDeviceV2::kde_output_device_v2_serial_number(const QString &serialNumber)
{
    m_serialNumber = serialNumber;
}

void WaylandOutputDeviceV2::kde_output_device_v2_eisa_id(const QString &eisaId)
{
    m_eisaId = eisaId;
}

void WaylandOutputDeviceV2::kde_output_device_v2_capabilities(uint32_t flags)
{
    m_flags = flags;
}

void WaylandOutputDeviceV2::kde_output_device_v2_overscan(uint32_t overscan)
{
    m_overscan = overscan;
}

void WaylandOutputDeviceV2::kde_output_device_v2_vrr_policy(uint32_t vrr_policy)
{
    m_vrr_policy = vrr_policy;
}

void WaylandOutputDeviceV2::kde_output_device_v2_rgb_range(uint32_t rgb_range)
{
    m_rgbRange = rgb_range;
}

QByteArray WaylandOutputDeviceV2::edid() const
{
    return m_edid;
}

bool WaylandOutputDeviceV2::enabled() const
{
    return m_enabled;
}

int WaylandOutputDeviceV2::id() const
{
    return m_id;
}

qreal WaylandOutputDeviceV2::scale() const
{
    return m_factor;
}

QString WaylandOutputDeviceV2::manufacturer() const
{
    return m_manufacturer;
}

QString WaylandOutputDeviceV2::model() const
{
    return m_model;
}

QPoint WaylandOutputDeviceV2::globalPosition() const
{
    return m_pos;
}

QSize WaylandOutputDeviceV2::pixelSize() const
{
    return m_mode->size();
}

int WaylandOutputDeviceV2::refreshRate() const
{
    return m_mode->refreshRate();
}

uint32_t WaylandOutputDeviceV2::vrrPolicy() const
{
    return m_vrr_policy;
}

uint32_t WaylandOutputDeviceV2::overscan() const
{
    return m_overscan;
}

uint32_t WaylandOutputDeviceV2::capabilities() const
{
    return m_flags;
}

uint32_t WaylandOutputDeviceV2::rgbRange() const
{
    return m_rgbRange;
}

}
}
