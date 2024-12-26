/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <config-kwin.h>

#include <QSignalSpy>

#include "kwin_wayland_test.h"

#if KWIN_BUILD_SCREENLOCKER
#include "screenlockerwatcher.h"
#endif
#include "input_event.h"
#include "inputmethod.h"
#include "wayland/display.h"
#include "wayland_server.h"
#include "workspace.h"
#include <wayland-zkde-screencast-unstable-v1-client-protocol.h>

#include <KWayland/Client/appmenu.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shadow.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/subcompositor.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/textinput.h>

// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif

#include <QFutureWatcher>
#include <QThread>
#include <QtConcurrentRun>

// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <xf86drm.h>

namespace KWin
{
namespace Test
{

static std::unique_ptr<Connection> s_waylandConnection;

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
    return m_xdgSurface.get();
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
    return m_xdgSurface.get();
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

ScreenEdgeManagerV1::~ScreenEdgeManagerV1()
{
    destroy();
}

AutoHideScreenEdgeV1::AutoHideScreenEdgeV1(ScreenEdgeManagerV1 *manager, KWayland::Client::Surface *surface, uint32_t border)
    : QtWayland::kde_auto_hide_screen_edge_v1(manager->get_auto_hide_screen_edge(border, *surface))
{
}

AutoHideScreenEdgeV1::~AutoHideScreenEdgeV1()
{
    destroy();
}

CursorShapeManagerV1::~CursorShapeManagerV1()
{
    destroy();
}

CursorShapeDeviceV1::CursorShapeDeviceV1(CursorShapeManagerV1 *manager, KWayland::Client::Pointer *pointer)
    : QtWayland::wp_cursor_shape_device_v1(manager->get_pointer(*pointer))
{
}

CursorShapeDeviceV1::~CursorShapeDeviceV1()
{
    destroy();
}

FakeInput::~FakeInput()
{
    destroy();
}

SecurityContextManagerV1::~SecurityContextManagerV1()
{
    destroy();
}

XdgWmDialogV1::~XdgWmDialogV1()
{
    destroy();
}

XdgDialogV1::XdgDialogV1(XdgWmDialogV1 *wm, XdgToplevel *toplevel)
    : QtWayland::xdg_dialog_v1(wm->get_xdg_dialog(toplevel->object()))
{
}

XdgDialogV1::~XdgDialogV1()
{
    destroy();
}

MockInputMethod *inputMethod()
{
    return s_waylandConnection->inputMethodV1;
}

KWayland::Client::Surface *inputPanelSurface()
{
    return s_waylandConnection->inputMethodV1->inputPanelSurface();
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
    if (!m_inputSurface) {
        m_inputSurface = Test::createSurface();
        m_inputMethodSurface = Test::createInputPanelSurfaceV1(m_inputSurface.get(), s_waylandConnection->outputs.first(), m_mode);
    }
    m_context = context;

    switch (m_mode) {
    case Mode::TopLevel:
        Test::render(m_inputSurface.get(), QSize(1280, 400), Qt::blue);
        break;
    case Mode::Overlay:
        Test::render(m_inputSurface.get(), QSize(200, 50), Qt::blue);
        break;
    }

    Q_EMIT activate();
}

void MockInputMethod::setMode(MockInputMethod::Mode mode)
{
    m_mode = mode;
}

void MockInputMethod::zwp_input_method_v1_deactivate(struct ::zwp_input_method_context_v1 *context)
{
    QCOMPARE(context, m_context);
    zwp_input_method_context_v1_destroy(context);
    m_context = nullptr;

    if (m_inputSurface) {
        m_inputSurface->release();
        m_inputSurface->destroy();
        m_inputSurface.reset();
        m_inputMethodSurface.reset();
    }
}

bool setupWaylandConnection(AdditionalWaylandInterfaces flags)
{
    if (s_waylandConnection) {
        return false;
    }
    s_waylandConnection = Connection::setup(flags);
    return bool(s_waylandConnection);
}

void destroyWaylandConnection()
{
    s_waylandConnection.reset();
}

std::unique_ptr<Connection> Connection::setup(AdditionalWaylandInterfaces flags)
{
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        return nullptr;
    }
    KWin::waylandServer()->display()->createClient(sx[0]);
    // setup connection
    auto connection = std::make_unique<Connection>();
    connection->connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(connection->connection, &KWayland::Client::ConnectionThread::connected);
    if (!connectedSpy.isValid()) {
        return nullptr;
    }
    connection->connection->setSocketFd(sx[1]);

    connection->thread = new QThread(kwinApp());
    connection->connection->moveToThread(connection->thread);
    connection->thread->start();

    connection->connection->initConnection();
    if (!connectedSpy.wait()) {
        return nullptr;
    }

    connection->queue = new KWayland::Client::EventQueue;
    connection->queue->setup(connection->connection);
    if (!connection->queue->isValid()) {
        return nullptr;
    }

    KWayland::Client::Registry *registry = new KWayland::Client::Registry;
    connection->registry = registry;
    registry->setEventQueue(connection->queue);

    QObject::connect(registry, &KWayland::Client::Registry::outputAnnounced, [c = connection.get()](quint32 name, quint32 version) {
        KWayland::Client::Output *output = c->registry->createOutput(name, version, c->registry);
        c->outputs << output;
        QObject::connect(output, &KWayland::Client::Output::removed, [=]() {
            output->deleteLater();
            c->outputs.removeOne(output);
        });
        QObject::connect(output, &KWayland::Client::Output::destroyed, [=]() {
            c->outputs.removeOne(output);
        });
    });

    QObject::connect(registry, &KWayland::Client::Registry::interfaceAnnounced, [c = connection.get(), flags](const QByteArray &interface, quint32 name, quint32 version) {
        if (flags & AdditionalWaylandInterface::InputMethodV1) {
            if (interface == QByteArrayLiteral("zwp_input_method_v1")) {
                c->inputMethodV1 = new MockInputMethod(*c->registry, name, version);
            } else if (interface == QByteArrayLiteral("zwp_input_panel_v1")) {
                c->inputPanelV1 = new QtWayland::zwp_input_panel_v1(*c->registry, name, version);
            }
        }
        if (flags & AdditionalWaylandInterface::LayerShellV1) {
            if (interface == QByteArrayLiteral("zwlr_layer_shell_v1")) {
                c->layerShellV1 = new LayerShellV1();
                c->layerShellV1->init(*c->registry, name, version);
            }
        }
        if (flags & AdditionalWaylandInterface::TextInputManagerV3) {
            // do something
            if (interface == QByteArrayLiteral("zwp_text_input_manager_v3")) {
                c->textInputManagerV3 = new TextInputManagerV3();
                c->textInputManagerV3->init(*c->registry, name, version);
            }
        }
        if (interface == QByteArrayLiteral("xdg_wm_base")) {
            c->xdgShell = new XdgShell();
            c->xdgShell->init(*c->registry, name, version);
        }
        if (flags & AdditionalWaylandInterface::XdgDecorationV1) {
            if (interface == zxdg_decoration_manager_v1_interface.name) {
                c->xdgDecorationManagerV1 = new XdgDecorationManagerV1();
                c->xdgDecorationManagerV1->init(*c->registry, name, version);
                return;
            }
        }
        if (flags & AdditionalWaylandInterface::IdleInhibitV1) {
            if (interface == zwp_idle_inhibit_manager_v1_interface.name) {
                c->idleInhibitManagerV1 = new IdleInhibitManagerV1();
                c->idleInhibitManagerV1->init(*c->registry, name, version);
                return;
            }
        }
        if (flags & AdditionalWaylandInterface::OutputDeviceV2) {
            if (interface == kde_output_device_v2_interface.name) {
                WaylandOutputDeviceV2 *device = new WaylandOutputDeviceV2(name);
                device->init(*c->registry, name, version);

                c->outputDevicesV2 << device;

                QObject::connect(device, &WaylandOutputDeviceV2::destroyed, [=]() {
                    c->outputDevicesV2.removeOne(device);
                    device->deleteLater();
                });

                QObject::connect(c->registry, &KWayland::Client::Registry::interfaceRemoved, device, [c, name, device](const quint32 &interfaceName) {
                    if (name == interfaceName) {
                        c->outputDevicesV2.removeOne(device);
                        device->deleteLater();
                    }
                });

                return;
            }
        }
        if (flags & AdditionalWaylandInterface::OutputManagementV2) {
            if (interface == kde_output_management_v2_interface.name) {
                c->outputManagementV2 = new WaylandOutputManagementV2(*c->registry, name, version);
                return;
            }
        }
        if (flags & AdditionalWaylandInterface::FractionalScaleManagerV1) {
            if (interface == wp_fractional_scale_manager_v1_interface.name) {
                c->fractionalScaleManagerV1 = new FractionalScaleManagerV1();
                c->fractionalScaleManagerV1->init(*c->registry, name, version);
                return;
            }
        }
        if (flags & AdditionalWaylandInterface::ScreencastingV1) {
            if (interface == zkde_screencast_unstable_v1_interface.name) {
                c->screencastingV1 = new ScreencastingV1();
                c->screencastingV1->init(*c->registry, name, version);
                return;
            }
        }
        if (flags & AdditionalWaylandInterface::ScreenEdgeV1) {
            if (interface == kde_screen_edge_manager_v1_interface.name) {
                c->screenEdgeManagerV1 = new ScreenEdgeManagerV1();
                c->screenEdgeManagerV1->init(*c->registry, name, version);
                return;
            }
        }
        if (flags & AdditionalWaylandInterface::CursorShapeV1) {
            if (interface == wp_cursor_shape_manager_v1_interface.name) {
                c->cursorShapeManagerV1 = new CursorShapeManagerV1();
                c->cursorShapeManagerV1->init(*c->registry, name, version);
            }
        }
        if (flags & AdditionalWaylandInterface::FakeInput) {
            if (interface == org_kde_kwin_fake_input_interface.name) {
                c->fakeInput = new FakeInput();
                c->fakeInput->init(*c->registry, name, version);
            }
        }
        if (flags & AdditionalWaylandInterface::SecurityContextManagerV1) {
            if (interface == wp_security_context_manager_v1_interface.name) {
                c->securityContextManagerV1 = new SecurityContextManagerV1();
                c->securityContextManagerV1->init(*c->registry, name, version);
            }
        }
        if (flags & AdditionalWaylandInterface::XdgDialogV1) {
            if (interface == xdg_wm_dialog_v1_interface.name) {
                c->xdgWmDialogV1 = new XdgWmDialogV1();
                c->xdgWmDialogV1->init(*c->registry, name, version);
            }
        }
        if (flags & AdditionalWaylandInterface::ColorManagement) {
            if (interface == xx_color_manager_v4_interface.name) {
                c->colorManager = std::make_unique<XXColorManagerV4>(*c->registry, name, version);
            }
        }
    });

    QSignalSpy allAnnounced(registry, &KWayland::Client::Registry::interfacesAnnounced);
    if (!allAnnounced.isValid()) {
        return nullptr;
    }
    registry->create(connection->connection);
    if (!registry->isValid()) {
        return nullptr;
    }
    registry->setup();
    if (!allAnnounced.wait()) {
        return nullptr;
    }

    connection->compositor = registry->createCompositor(registry->interface(KWayland::Client::Registry::Interface::Compositor).name, registry->interface(KWayland::Client::Registry::Interface::Compositor).version);
    if (!connection->compositor->isValid()) {
        return nullptr;
    }
    connection->subCompositor = registry->createSubCompositor(registry->interface(KWayland::Client::Registry::Interface::SubCompositor).name, registry->interface(KWayland::Client::Registry::Interface::SubCompositor).version);
    if (!connection->subCompositor->isValid()) {
        return nullptr;
    }
    connection->shm = registry->createShmPool(registry->interface(KWayland::Client::Registry::Interface::Shm).name, registry->interface(KWayland::Client::Registry::Interface::Shm).version);
    if (!connection->shm->isValid()) {
        return nullptr;
    }
    if (flags.testFlag(AdditionalWaylandInterface::Seat)) {
        connection->seat = registry->createSeat(registry->interface(KWayland::Client::Registry::Interface::Seat).name, registry->interface(KWayland::Client::Registry::Interface::Seat).version);
        if (!connection->seat->isValid()) {
            return nullptr;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::ShadowManager)) {
        connection->shadowManager = registry->createShadowManager(registry->interface(KWayland::Client::Registry::Interface::Shadow).name,
                                                                  registry->interface(KWayland::Client::Registry::Interface::Shadow).version);
        if (!connection->shadowManager->isValid()) {
            return nullptr;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::PlasmaShell)) {
        connection->plasmaShell = registry->createPlasmaShell(registry->interface(KWayland::Client::Registry::Interface::PlasmaShell).name,
                                                              registry->interface(KWayland::Client::Registry::Interface::PlasmaShell).version);
        if (!connection->plasmaShell->isValid()) {
            return nullptr;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::WindowManagement)) {
        connection->windowManagement = registry->createPlasmaWindowManagement(registry->interface(KWayland::Client::Registry::Interface::PlasmaWindowManagement).name,
                                                                              registry->interface(KWayland::Client::Registry::Interface::PlasmaWindowManagement).version);
        if (!connection->windowManagement->isValid()) {
            return nullptr;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::PointerConstraints)) {
        connection->pointerConstraints = registry->createPointerConstraints(registry->interface(KWayland::Client::Registry::Interface::PointerConstraintsUnstableV1).name,
                                                                            registry->interface(KWayland::Client::Registry::Interface::PointerConstraintsUnstableV1).version);
        if (!connection->pointerConstraints->isValid()) {
            return nullptr;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::AppMenu)) {
        connection->appMenu = registry->createAppMenuManager(registry->interface(KWayland::Client::Registry::Interface::AppMenu).name, registry->interface(KWayland::Client::Registry::Interface::AppMenu).version);
        if (!connection->appMenu->isValid()) {
            return nullptr;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::TextInputManagerV2)) {
        connection->textInputManager = registry->createTextInputManager(registry->interface(KWayland::Client::Registry::Interface::TextInputManagerUnstableV2).name, registry->interface(KWayland::Client::Registry::Interface::TextInputManagerUnstableV2).version);
        if (!connection->textInputManager->isValid()) {
            return nullptr;
        }
    }

    return connection;
}

Connection::~Connection()
{
    delete compositor;
    compositor = nullptr;
    delete subCompositor;
    subCompositor = nullptr;
    delete windowManagement;
    windowManagement = nullptr;
    delete plasmaShell;
    plasmaShell = nullptr;
    delete seat;
    seat = nullptr;
    delete pointerConstraints;
    pointerConstraints = nullptr;
    delete xdgShell;
    xdgShell = nullptr;
    delete shadowManager;
    shadowManager = nullptr;
    delete idleInhibitManagerV1;
    idleInhibitManagerV1 = nullptr;
    delete shm;
    shm = nullptr;
    delete registry;
    registry = nullptr;
    delete appMenu;
    appMenu = nullptr;
    delete xdgDecorationManagerV1;
    xdgDecorationManagerV1 = nullptr;
    delete textInputManager;
    textInputManager = nullptr;
    delete inputPanelV1;
    inputPanelV1 = nullptr;
    delete layerShellV1;
    layerShellV1 = nullptr;
    delete outputManagementV2;
    outputManagementV2 = nullptr;
    delete fractionalScaleManagerV1;
    fractionalScaleManagerV1 = nullptr;
    delete screencastingV1;
    screencastingV1 = nullptr;
    delete screenEdgeManagerV1;
    screenEdgeManagerV1 = nullptr;
    delete cursorShapeManagerV1;
    cursorShapeManagerV1 = nullptr;
    delete fakeInput;
    fakeInput = nullptr;
    delete securityContextManagerV1;
    securityContextManagerV1 = nullptr;
    delete xdgWmDialogV1;
    xdgWmDialogV1 = nullptr;
    colorManager.reset();

    delete queue; // Must be destroyed last
    queue = nullptr;

    if (thread) {
        connection->deleteLater();
        thread->quit();
        thread->wait();
        delete thread;
        thread = nullptr;
        connection = nullptr;
    }
    outputs.clear();
    outputDevicesV2.clear();
}

KWayland::Client::ConnectionThread *waylandConnection()
{
    return s_waylandConnection->connection;
}

KWayland::Client::Compositor *waylandCompositor()
{
    return s_waylandConnection->compositor;
}

KWayland::Client::SubCompositor *waylandSubCompositor()
{
    return s_waylandConnection->subCompositor;
}

KWayland::Client::ShadowManager *waylandShadowManager()
{
    return s_waylandConnection->shadowManager;
}

KWayland::Client::ShmPool *waylandShmPool()
{
    return s_waylandConnection->shm;
}

KWayland::Client::Seat *waylandSeat()
{
    return s_waylandConnection->seat;
}

KWayland::Client::PlasmaShell *waylandPlasmaShell()
{
    return s_waylandConnection->plasmaShell;
}

KWayland::Client::PlasmaWindowManagement *waylandWindowManagement()
{
    return s_waylandConnection->windowManagement;
}

KWayland::Client::PointerConstraints *waylandPointerConstraints()
{
    return s_waylandConnection->pointerConstraints;
}

KWayland::Client::AppMenuManager *waylandAppMenuManager()
{
    return s_waylandConnection->appMenu;
}

KWin::Test::WaylandOutputManagementV2 *waylandOutputManagementV2()
{
    return s_waylandConnection->outputManagementV2;
}

KWayland::Client::TextInputManager *waylandTextInputManager()
{
    return s_waylandConnection->textInputManager;
}

TextInputManagerV3 *waylandTextInputManagerV3()
{
    return s_waylandConnection->textInputManagerV3;
}

QList<KWayland::Client::Output *> waylandOutputs()
{
    return s_waylandConnection->outputs;
}

KWayland::Client::Output *waylandOutput(const QString &name)
{
    for (KWayland::Client::Output *output : std::as_const(s_waylandConnection->outputs)) {
        if (output->name() == name) {
            return output;
        }
    }
    return nullptr;
}

ScreencastingV1 *screencasting()
{
    return s_waylandConnection->screencastingV1;
}

QList<KWin::Test::WaylandOutputDeviceV2 *> waylandOutputDevicesV2()
{
    return s_waylandConnection->outputDevicesV2;
}

FakeInput *waylandFakeInput()
{
    return s_waylandConnection->fakeInput;
}

SecurityContextManagerV1 *waylandSecurityContextManagerV1()
{
    return s_waylandConnection->securityContextManagerV1;
}

XXColorManagerV4 *colorManager()
{
    return s_waylandConnection->colorManager.get();
}

bool waitForWaylandSurface(Window *window)
{
    if (window->surface()) {
        return true;
    }
    QSignalSpy surfaceChangedSpy(window, &Window::surfaceChanged);
    return surfaceChangedSpy.wait();
}

bool waitForWaylandPointer()
{
    if (!s_waylandConnection->seat) {
        return false;
    }
    return waitForWaylandPointer(s_waylandConnection->seat);
}

bool waitForWaylandPointer(KWayland::Client::Seat *seat)
{
    QSignalSpy hasPointerSpy(seat, &KWayland::Client::Seat::hasPointerChanged);
    return hasPointerSpy.wait();
}

bool waitForWaylandTouch()
{
    if (!s_waylandConnection->seat) {
        return false;
    }
    return waitForWaylandTouch(s_waylandConnection->seat);
}

bool waitForWaylandTouch(KWayland::Client::Seat *seat)
{
    QSignalSpy hasTouchSpy(seat, &KWayland::Client::Seat::hasTouchChanged);
    return hasTouchSpy.wait();
}

bool waitForWaylandKeyboard()
{
    if (!s_waylandConnection->seat) {
        return false;
    }
    return waitForWaylandKeyboard(s_waylandConnection->seat);
}

bool waitForWaylandKeyboard(KWayland::Client::Seat *seat)
{
    QSignalSpy hasKeyboardSpy(seat, &KWayland::Client::Seat::hasKeyboardChanged);
    return hasKeyboardSpy.wait();
}

void render(KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format)
{
    render(s_waylandConnection->shm, surface, size, color, format);
}

void render(KWayland::Client::ShmPool *shm, KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format)
{
    QImage img(size, format);
    img.fill(color);
    render(surface, img);
}

void render(KWayland::Client::Surface *surface, const QImage &img)
{
    render(s_waylandConnection->shm, surface, img);
}

void render(KWayland::Client::ShmPool *shm, KWayland::Client::Surface *surface, const QImage &img)
{
    surface->attachBuffer(shm->createBuffer(img));
    surface->damage(QRect(QPoint(0, 0), img.size()));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
}

Window *waitForWaylandWindowShown(int timeout)
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    if (!windowAddedSpy.isValid()) {
        return nullptr;
    }
    if (!windowAddedSpy.wait(timeout)) {
        return nullptr;
    }
    return windowAddedSpy.first().first().value<Window *>();
}

Window *renderAndWaitForShown(KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format, int timeout)
{
    return renderAndWaitForShown(s_waylandConnection->shm, surface, size, color, format, timeout);
}

Window *renderAndWaitForShown(KWayland::Client::ShmPool *shm, KWayland::Client::Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format, int timeout)
{
    QImage img(size, format);
    img.fill(color);
    return renderAndWaitForShown(shm, surface, img, timeout);
}

Window *renderAndWaitForShown(KWayland::Client::Surface *surface, const QImage &img, int timeout)
{
    return renderAndWaitForShown(s_waylandConnection->shm, surface, img, timeout);
}

Window *renderAndWaitForShown(KWayland::Client::ShmPool *shm, KWayland::Client::Surface *surface, const QImage &img, int timeout)
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    if (!windowAddedSpy.isValid()) {
        return nullptr;
    }
    render(shm, surface, img);
    flushWaylandConnection();
    if (!windowAddedSpy.wait(timeout)) {
        return nullptr;
    }
    return windowAddedSpy.first().first().value<Window *>();
}

void flushWaylandConnection()
{
    if (s_waylandConnection) {
        s_waylandConnection->connection->flush();
    }
}

class WaylandSyncPoint : public QObject
{
    Q_OBJECT

public:
    explicit WaylandSyncPoint(KWayland::Client::ConnectionThread *connection, KWayland::Client::EventQueue *eventQueue)
    {
        static const wl_callback_listener listener = {
            .done = [](void *data, wl_callback *callback, uint32_t callback_data) {
            auto syncPoint = static_cast<WaylandSyncPoint *>(data);
            Q_EMIT syncPoint->done();
        },
        };

        m_callback = wl_display_sync(connection->display());
        eventQueue->addProxy(m_callback);
        wl_callback_add_listener(m_callback, &listener, this);
    }

    ~WaylandSyncPoint() override
    {
        wl_callback_destroy(m_callback);
    }

Q_SIGNALS:
    void done();

private:
    wl_callback *m_callback;
};

bool waylandSync()
{
    WaylandSyncPoint syncPoint(s_waylandConnection->connection, s_waylandConnection->queue);
    QSignalSpy doneSpy(&syncPoint, &WaylandSyncPoint::done);
    return doneSpy.wait();
}

std::unique_ptr<KWayland::Client::Surface> createSurface()
{
    if (!s_waylandConnection->compositor) {
        return nullptr;
    }
    return createSurface(s_waylandConnection->compositor);
}

std::unique_ptr<KWayland::Client::Surface> createSurface(KWayland::Client::Compositor *compositor)
{
    std::unique_ptr<KWayland::Client::Surface> s{compositor->createSurface()};
    return s->isValid() ? std::move(s) : nullptr;
}

std::unique_ptr<KWayland::Client::SubSurface> createSubSurface(KWayland::Client::Surface *surface, KWayland::Client::Surface *parentSurface)
{
    if (!s_waylandConnection->subCompositor) {
        return nullptr;
    }
    std::unique_ptr<KWayland::Client::SubSurface> s(s_waylandConnection->subCompositor->createSubSurface(surface, parentSurface));
    if (!s->isValid()) {
        return nullptr;
    }
    return s;
}

std::unique_ptr<LayerSurfaceV1> createLayerSurfaceV1(KWayland::Client::Surface *surface, const QString &scope, KWayland::Client::Output *output, LayerShellV1::layer layer)
{
    LayerShellV1 *shell = s_waylandConnection->layerShellV1;
    if (!shell) {
        qWarning() << "Could not create a layer surface because the layer shell global is not bound";
        return nullptr;
    }

    struct ::wl_output *nativeOutput = nullptr;
    if (output) {
        nativeOutput = *output;
    }

    auto shellSurface = std::make_unique<LayerSurfaceV1>();
    shellSurface->init(shell->get_layer_surface(*surface, nativeOutput, layer, scope));

    return shellSurface;
}

std::unique_ptr<QtWayland::zwp_input_panel_surface_v1> createInputPanelSurfaceV1(KWayland::Client::Surface *surface, KWayland::Client::Output *output, MockInputMethod::Mode mode)
{
    if (!s_waylandConnection->inputPanelV1) {
        qWarning() << "Unable to create the input panel surface. The interface input_panel global is not bound";
        return nullptr;
    }
    auto s = std::make_unique<QtWayland::zwp_input_panel_surface_v1>(s_waylandConnection->inputPanelV1->get_input_panel_surface(*surface));
    if (!s->isInitialized()) {
        return nullptr;
    }

    switch (mode) {
    case MockInputMethod::Mode::TopLevel:
        s->set_toplevel(output->output(), QtWayland::zwp_input_panel_surface_v1::position_center_bottom);
        break;
    case MockInputMethod::Mode::Overlay:
        s->set_overlay_panel();
        break;
    }

    return s;
}

std::unique_ptr<FractionalScaleV1> createFractionalScaleV1(KWayland::Client::Surface *surface)
{
    if (!s_waylandConnection->fractionalScaleManagerV1) {
        qWarning() << "Unable to create fractional scale surface. The global is not bound";
        return nullptr;
    }
    auto scale = std::make_unique<FractionalScaleV1>();
    scale->init(s_waylandConnection->fractionalScaleManagerV1->get_fractional_scale(*surface));

    return scale;
}

static void waitForConfigured(XdgSurface *shellSurface)
{
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface, &XdgSurface::configureRequested);

    shellSurface->surface()->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->ack_configure(surfaceConfigureRequestedSpy.last().first().toUInt());
}

std::unique_ptr<XdgToplevel> createXdgToplevelSurface(KWayland::Client::Surface *surface)
{
    return createXdgToplevelSurface(surface, CreationSetup::CreateAndConfigure);
}

std::unique_ptr<XdgToplevel> createXdgToplevelSurface(XdgShell *shell, KWayland::Client::Surface *surface)
{
    return createXdgToplevelSurface(shell, surface, CreationSetup::CreateAndConfigure);
}

std::unique_ptr<XdgToplevel> createXdgToplevelSurface(KWayland::Client::Surface *surface, CreationSetup configureMode)
{
    XdgShell *shell = s_waylandConnection->xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_toplevel surface because xdg_wm_base global is not bound";
        return nullptr;
    }

    return createXdgToplevelSurface(shell, surface, configureMode);
}

std::unique_ptr<XdgToplevel> createXdgToplevelSurface(XdgShell *shell, KWayland::Client::Surface *surface, CreationSetup configureMode)
{
    XdgSurface *xdgSurface = new XdgSurface(shell, surface);
    std::unique_ptr<XdgToplevel> xdgToplevel = std::make_unique<XdgToplevel>(xdgSurface);

    if (configureMode == CreationSetup::CreateAndConfigure) {
        waitForConfigured(xdgSurface);
    }

    return xdgToplevel;
}

std::unique_ptr<XdgToplevel> createXdgToplevelSurface(KWayland::Client::Surface *surface, std::function<void(XdgToplevel *toplevel)> setup)
{
    XdgShell *shell = s_waylandConnection->xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_toplevel surface because xdg_wm_base global is not bound";
        return nullptr;
    }

    return createXdgToplevelSurface(shell, surface, setup);
}

std::unique_ptr<XdgToplevel> createXdgToplevelSurface(XdgShell *shell, KWayland::Client::Surface *surface, std::function<void(XdgToplevel *toplevel)> setup)
{
    XdgSurface *xdgSurface = new XdgSurface(shell, surface);
    std::unique_ptr<XdgToplevel> xdgToplevel = std::make_unique<XdgToplevel>(xdgSurface);

    setup(xdgToplevel.get());
    waitForConfigured(xdgSurface);

    return xdgToplevel;
}

std::unique_ptr<XdgPositioner> createXdgPositioner()
{
    XdgShell *shell = s_waylandConnection->xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_positioner object because xdg_wm_base global is not bound";
        return nullptr;
    }

    return std::make_unique<XdgPositioner>(shell);
}

std::unique_ptr<XdgPopup> createXdgPopupSurface(KWayland::Client::Surface *surface, XdgSurface *parentSurface, XdgPositioner *positioner, CreationSetup configureMode)
{
    XdgShell *shell = s_waylandConnection->xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_popup surface because xdg_wm_base global is not bound";
        return nullptr;
    }

    XdgSurface *xdgSurface = new XdgSurface(shell, surface);
    std::unique_ptr<XdgPopup> xdgPopup = std::make_unique<XdgPopup>(xdgSurface, parentSurface, positioner);

    if (configureMode == CreationSetup::CreateAndConfigure) {
        waitForConfigured(xdgSurface);
    }

    return xdgPopup;
}

std::unique_ptr<XdgToplevelDecorationV1> createXdgToplevelDecorationV1(XdgToplevel *toplevel)
{
    XdgDecorationManagerV1 *manager = s_waylandConnection->xdgDecorationManagerV1;

    if (!manager) {
        qWarning() << "Could not create an xdg_toplevel_decoration_v1 because xdg_decoration_manager_v1 global is not bound";
        return nullptr;
    }

    return std::make_unique<XdgToplevelDecorationV1>(manager, toplevel);
}

std::unique_ptr<IdleInhibitorV1> createIdleInhibitorV1(KWayland::Client::Surface *surface)
{
    IdleInhibitManagerV1 *manager = s_waylandConnection->idleInhibitManagerV1;
    if (!manager) {
        qWarning() << "Could not create an idle_inhibitor_v1 because idle_inhibit_manager_v1 global is not bound";
        return nullptr;
    }

    return std::make_unique<IdleInhibitorV1>(manager, surface);
}

std::unique_ptr<AutoHideScreenEdgeV1> createAutoHideScreenEdgeV1(KWayland::Client::Surface *surface, uint32_t border)
{
    ScreenEdgeManagerV1 *manager = s_waylandConnection->screenEdgeManagerV1;
    if (!manager) {
        qWarning() << "Could not create an kde_auto_hide_screen_edge_v1 because kde_screen_edge_manager_v1 global is not bound";
        return nullptr;
    }

    return std::make_unique<AutoHideScreenEdgeV1>(manager, surface, border);
}

std::unique_ptr<CursorShapeDeviceV1> createCursorShapeDeviceV1(KWayland::Client::Pointer *pointer)
{
    CursorShapeManagerV1 *manager = s_waylandConnection->cursorShapeManagerV1;
    if (!manager) {
        qWarning() << "Could not create a wp_cursor_shape_device_v1 because wp_cursor_shape_manager_v1 global is not bound";
        return nullptr;
    }

    return std::make_unique<CursorShapeDeviceV1>(manager, pointer);
}

std::unique_ptr<XdgDialogV1> createXdgDialogV1(XdgToplevel *toplevel)
{
    XdgWmDialogV1 *wm = s_waylandConnection->xdgWmDialogV1;
    if (!wm) {
        qWarning() << "Could not create a xdg_dialog_v1 because xdg_wm_dialog_v1 global is not bound";
        return nullptr;
    }
    return std::make_unique<XdgDialogV1>(wm, toplevel);
}

bool waitForWindowClosed(Window *window)
{
    QSignalSpy closedSpy(window, &Window::closed);
    if (!closedSpy.isValid()) {
        return false;
    }
    return closedSpy.wait();
}

#if KWIN_BUILD_SCREENLOCKER
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
    if (!kwinApp()->screenLockerWatcher()->isLocked()) {
        QSignalSpy lockedSpy(kwinApp()->screenLockerWatcher(), &ScreenLockerWatcher::locked);
        if (!lockedSpy.isValid()) {
            return false;
        }
        if (!lockedSpy.wait()) {
            return false;
        }
        if (!kwinApp()->screenLockerWatcher()->isLocked()) {
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
    if (kwinApp()->screenLockerWatcher()->isLocked()) {
        QSignalSpy lockedSpy(kwinApp()->screenLockerWatcher(), &ScreenLockerWatcher::locked);
        if (!lockedSpy.isValid()) {
            return false;
        }
        if (!lockedSpy.wait()) {
            return false;
        }
        if (kwinApp()->screenLockerWatcher()->isLocked()) {
            return false;
        }
    }
    return true;
}
#endif // KWIN_BUILD_LOCKSCREEN

bool renderNodeAvailable()
{
    const int deviceCount = drmGetDevices2(0, nullptr, 0);
    if (deviceCount <= 0) {
        return false;
    }

    QList<drmDevice *> devices(deviceCount);
    if (drmGetDevices2(0, devices.data(), devices.size()) < 0) {
        return false;
    }
    auto deviceCleanup = qScopeGuard([&devices]() {
        drmFreeDevices(devices.data(), devices.size());
    });

    return std::any_of(devices.constBegin(), devices.constEnd(), [](drmDevice *device) {
        return device->available_nodes & (1 << DRM_NODE_RENDER);
    });
}

#if KWIN_BUILD_X11
void XcbConnectionDeleter::operator()(xcb_connection_t *pointer)
{
    xcb_disconnect(pointer);
};

Test::XcbConnectionPtr createX11Connection()
{
    QFutureWatcher<xcb_connection_t *> watcher;
    QEventLoop e;
    e.connect(&watcher, &QFutureWatcher<xcb_connection_t *>::finished, &e, &QEventLoop::quit);
    QFuture<xcb_connection_t *> future = QtConcurrent::run([]() {
        return xcb_connect(nullptr, nullptr);
    });
    watcher.setFuture(future);
    e.exec();
    return Test::XcbConnectionPtr(future.result());
}
#endif

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

bool WaylandOutputDeviceV2Mode::operator==(const WaylandOutputDeviceV2Mode &other) const
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

VirtualInputDeviceTabletTool::VirtualInputDeviceTabletTool(QObject *parent)
    : InputDeviceTabletTool(parent)
{
}

void VirtualInputDeviceTabletTool::setSerialId(quint64 serialId)
{
    m_serialId = serialId;
}

void VirtualInputDeviceTabletTool::setUniqueId(quint64 uniqueId)
{
    m_uniqueId = uniqueId;
}

void VirtualInputDeviceTabletTool::setType(Type type)
{
    m_type = type;
}

void VirtualInputDeviceTabletTool::setCapabilities(const QList<Capability> &capabilities)
{
    m_capabilities = capabilities;
}

quint64 VirtualInputDeviceTabletTool::serialId() const
{
    return m_serialId;
}

quint64 VirtualInputDeviceTabletTool::uniqueId() const
{
    return m_uniqueId;
}

VirtualInputDeviceTabletTool::Type VirtualInputDeviceTabletTool::type() const
{
    return m_type;
}

QList<VirtualInputDeviceTabletTool::Capability> VirtualInputDeviceTabletTool::capabilities() const
{
    return m_capabilities;
}

VirtualInputDevice::VirtualInputDevice(QObject *parent)
    : InputDevice(parent)
{
}

void VirtualInputDevice::setPointer(bool set)
{
    m_pointer = set;
}

void VirtualInputDevice::setKeyboard(bool set)
{
    m_keyboard = set;
}

void VirtualInputDevice::setTouch(bool set)
{
    m_touch = set;
}

void VirtualInputDevice::setLidSwitch(bool set)
{
    m_lidSwitch = set;
}

void VirtualInputDevice::setTabletPad(bool set)
{
    m_tabletPad = set;
}

void VirtualInputDevice::setTabletTool(bool set)
{
    m_tabletTool = set;
}

void VirtualInputDevice::setName(const QString &name)
{
    m_name = name;
}

void VirtualInputDevice::setGroup(uintptr_t group)
{
    m_group = reinterpret_cast<void *>(group);
}

QString VirtualInputDevice::name() const
{
    return m_name;
}

void *VirtualInputDevice::group() const
{
    return m_group;
}

bool VirtualInputDevice::isEnabled() const
{
    return true;
}

void VirtualInputDevice::setEnabled(bool enabled)
{
}

bool VirtualInputDevice::isKeyboard() const
{
    return m_keyboard;
}

bool VirtualInputDevice::isPointer() const
{
    return m_pointer;
}

bool VirtualInputDevice::isTouchpad() const
{
    return false;
}

bool VirtualInputDevice::isTouch() const
{
    return m_touch;
}

bool VirtualInputDevice::isTabletTool() const
{
    return m_tabletTool;
}

bool VirtualInputDevice::isTabletPad() const
{
    return m_tabletPad;
}

bool VirtualInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool VirtualInputDevice::isLidSwitch() const
{
    return m_lidSwitch;
}

XXColorManagerV4::XXColorManagerV4(::wl_registry *registry, uint32_t id, int version)
    : QtWayland::xx_color_manager_v4(registry, id, version)
{
}

XXColorManagerV4::~XXColorManagerV4()
{
    xx_color_manager_v4_destroy(object());
}

void keyboardKeyPressed(quint32 key, quint32 time)
{
    auto virtualKeyboard = static_cast<WaylandTestApplication *>(kwinApp())->virtualKeyboard();
    Q_EMIT virtualKeyboard->keyChanged(key, KeyboardKeyState::Pressed, std::chrono::milliseconds(time), virtualKeyboard);
}

void keyboardKeyReleased(quint32 key, quint32 time)
{
    auto virtualKeyboard = static_cast<WaylandTestApplication *>(kwinApp())->virtualKeyboard();
    Q_EMIT virtualKeyboard->keyChanged(key, KeyboardKeyState::Released, std::chrono::milliseconds(time), virtualKeyboard);
}

void pointerAxisHorizontal(qreal delta, quint32 time, qint32 discreteDelta, PointerAxisSource source)
{
    auto virtualPointer = static_cast<WaylandTestApplication *>(kwinApp())->virtualPointer();
    Q_EMIT virtualPointer->pointerAxisChanged(PointerAxis::Horizontal, delta, discreteDelta, source, false, std::chrono::milliseconds(time), virtualPointer);
    Q_EMIT virtualPointer->pointerFrame(virtualPointer);
}

void pointerAxisVertical(qreal delta, quint32 time, qint32 discreteDelta, PointerAxisSource source)
{
    auto virtualPointer = static_cast<WaylandTestApplication *>(kwinApp())->virtualPointer();
    Q_EMIT virtualPointer->pointerAxisChanged(PointerAxis::Vertical, delta, discreteDelta, source, false, std::chrono::milliseconds(time), virtualPointer);
    Q_EMIT virtualPointer->pointerFrame(virtualPointer);
}

void pointerButtonPressed(quint32 button, quint32 time)
{
    auto virtualPointer = static_cast<WaylandTestApplication *>(kwinApp())->virtualPointer();
    Q_EMIT virtualPointer->pointerButtonChanged(button, PointerButtonState::Pressed, std::chrono::milliseconds(time), virtualPointer);
    Q_EMIT virtualPointer->pointerFrame(virtualPointer);
}

void pointerButtonReleased(quint32 button, quint32 time)
{
    auto virtualPointer = static_cast<WaylandTestApplication *>(kwinApp())->virtualPointer();
    Q_EMIT virtualPointer->pointerButtonChanged(button, PointerButtonState::Released, std::chrono::milliseconds(time), virtualPointer);
    Q_EMIT virtualPointer->pointerFrame(virtualPointer);
}

void pointerMotion(const QPointF &position, quint32 time)
{
    auto virtualPointer = static_cast<WaylandTestApplication *>(kwinApp())->virtualPointer();
    Q_EMIT virtualPointer->pointerMotionAbsolute(position, std::chrono::milliseconds(time), virtualPointer);
    Q_EMIT virtualPointer->pointerFrame(virtualPointer);
}

void pointerMotionRelative(const QPointF &delta, quint32 time)
{
    auto virtualPointer = static_cast<WaylandTestApplication *>(kwinApp())->virtualPointer();
    Q_EMIT virtualPointer->pointerMotion(delta, delta, std::chrono::milliseconds(time), virtualPointer);
    Q_EMIT virtualPointer->pointerFrame(virtualPointer);
}

void touchCancel()
{
    auto virtualTouch = static_cast<WaylandTestApplication *>(kwinApp())->virtualTouch();
    Q_EMIT virtualTouch->touchCanceled(virtualTouch);
}

void touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    auto virtualTouch = static_cast<WaylandTestApplication *>(kwinApp())->virtualTouch();
    Q_EMIT virtualTouch->touchDown(id, pos, std::chrono::milliseconds(time), virtualTouch);
}

void touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    auto virtualTouch = static_cast<WaylandTestApplication *>(kwinApp())->virtualTouch();
    Q_EMIT virtualTouch->touchMotion(id, pos, std::chrono::milliseconds(time), virtualTouch);
}

void touchUp(qint32 id, quint32 time)
{
    auto virtualTouch = static_cast<WaylandTestApplication *>(kwinApp())->virtualTouch();
    Q_EMIT virtualTouch->touchUp(id, std::chrono::milliseconds(time), virtualTouch);
}

void tabletPadButtonPressed(quint32 button, quint32 time)
{
    auto virtualTabletPad = static_cast<WaylandTestApplication *>(kwinApp())->virtualTabletPad();
    Q_EMIT virtualTabletPad->tabletPadButtonEvent(button, true, std::chrono::milliseconds(time), virtualTabletPad);
}

void tabletPadButtonReleased(quint32 button, quint32 time)
{
    auto virtualTabletPad = static_cast<WaylandTestApplication *>(kwinApp())->virtualTabletPad();
    Q_EMIT virtualTabletPad->tabletPadButtonEvent(button, false, std::chrono::milliseconds(time), virtualTabletPad);
}

void tabletToolButtonPressed(quint32 button, quint32 time)
{
    auto tablet = static_cast<WaylandTestApplication *>(kwinApp())->virtualTablet();
    auto tool = static_cast<WaylandTestApplication *>(kwinApp())->virtualTabletTool();
    Q_EMIT tablet->tabletToolButtonEvent(button, true, tool, std::chrono::milliseconds(time), tablet);
}

void tabletToolButtonReleased(quint32 button, quint32 time)
{
    auto tablet = static_cast<WaylandTestApplication *>(kwinApp())->virtualTablet();
    auto tool = static_cast<WaylandTestApplication *>(kwinApp())->virtualTabletTool();
    Q_EMIT tablet->tabletToolButtonEvent(button, false, tool, std::chrono::milliseconds(time), tablet);
}

void tabletToolProximityEvent(const QPointF &pos, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipNear, qreal sliderPosition, quint32 time)
{
    auto tablet = static_cast<WaylandTestApplication *>(kwinApp())->virtualTablet();
    auto tool = static_cast<WaylandTestApplication *>(kwinApp())->virtualTabletTool();
    Q_EMIT tablet->tabletToolProximityEvent(pos, xTilt, yTilt, rotation, distance, tipNear, sliderPosition, tool, std::chrono::milliseconds(time), tablet);
}
}
}

#include "test_helpers.moc"
