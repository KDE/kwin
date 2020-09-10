/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "screenlockerwatcher.h"
#include "wayland_server.h"
#include "workspace.h"
#include "qwayland-input-method-unstable-v1.h"
#include "virtualkeyboard.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/idleinhibit.h>
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
#include <KWayland/Client/xdgshell.h>
#include <KWayland/Client/xdgdecoration.h>
#include <KWayland/Client/outputmanagement.h>
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
    emit configureRequested(serial, QSize(width, height));
}

void LayerSurfaceV1::zwlr_layer_surface_v1_closed()
{
    emit closeRequested();
}

XdgShell::~XdgShell()
{
    destroy();
}

XdgSurface::XdgSurface(XdgShell *shell, Surface *surface, QObject *parent)
    : QObject(parent)
    , QtWayland::xdg_surface(shell->get_xdg_surface(*surface))
    , m_surface(surface)
{
}

XdgSurface::~XdgSurface()
{
    destroy();
}

Surface *XdgSurface::surface() const
{
    return m_surface;
}

void XdgSurface::xdg_surface_configure(uint32_t serial)
{
    emit configureRequested(serial);
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

    emit configureRequested(QSize(width, height), requestedStates);
}

void XdgToplevel::xdg_toplevel_close()
{
    emit closeRequested();
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
    emit configureRequested(QRect(x, y, width, height));
}

static struct {
    ConnectionThread *connection = nullptr;
    EventQueue *queue = nullptr;
    KWayland::Client::Compositor *compositor = nullptr;
    SubCompositor *subCompositor = nullptr;
    ServerSideDecorationManager *decoration = nullptr;
    ShadowManager *shadowManager = nullptr;
    KWayland::Client::XdgShell *xdgShellStable = nullptr;
    XdgShell *xdgShell = nullptr;
    ShmPool *shm = nullptr;
    Seat *seat = nullptr;
    PlasmaShell *plasmaShell = nullptr;
    PlasmaWindowManagement *windowManagement = nullptr;
    PointerConstraints *pointerConstraints = nullptr;
    Registry *registry = nullptr;
    OutputManagement* outputManagement = nullptr;
    QThread *thread = nullptr;
    QVector<Output*> outputs;
    IdleInhibitManager *idleInhibit = nullptr;
    AppMenuManager *appMenu = nullptr;
    XdgDecorationManager *xdgDecoration = nullptr;
    TextInputManager *textInputManager = nullptr;
    QtWayland::zwp_input_panel_v1 *inputPanelV1 = nullptr;
    MockInputMethod *inputMethodV1 = nullptr;
    QtWayland::zwp_input_method_context_v1 *inputMethodContextV1 = nullptr;
    LayerShellV1 *layerShellV1 = nullptr;
} s_waylandConnection;

class MockInputMethod : public QtWayland::zwp_input_method_v1
{
public:
    MockInputMethod(struct wl_registry *registry, int id, int version);
    ~MockInputMethod();

protected:
    void zwp_input_method_v1_activate(struct ::zwp_input_method_context_v1 *context) override;
    void zwp_input_method_v1_deactivate(struct ::zwp_input_method_context_v1 *context) override;

private:
    Surface *m_inputSurface = nullptr;
    QtWayland::zwp_input_panel_surface_v1 *m_inputMethodSurface = nullptr;
    AbstractClient *m_client = nullptr;
};

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
    m_client = Test::renderAndWaitForShown(m_inputSurface, QSize(1280, 400), Qt::blue);
}

void MockInputMethod::zwp_input_method_v1_deactivate(struct ::zwp_input_method_context_v1 *context)
{
    zwp_input_method_context_v1_destroy(context);

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
        auto output = registry->createOutput(name, version, s_waylandConnection.registry);
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
        if (interface == QByteArrayLiteral("xdg_wm_base")) {
            s_waylandConnection.xdgShell = new XdgShell();
            s_waylandConnection.xdgShell->init(*registry, name, version);
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
    s_waylandConnection.xdgShellStable = registry->createXdgShell(registry->interface(Registry::Interface::XdgShellStable).name, registry->interface(Registry::Interface::XdgShellStable).version);
    if (!s_waylandConnection.xdgShellStable->isValid()) {
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
    if (flags.testFlag(AdditionalWaylandInterface::OutputManagement)) {
        s_waylandConnection.outputManagement = registry->createOutputManagement(registry->interface(Registry::Interface::OutputManagement).name,
                                                                                registry->interface(Registry::Interface::OutputManagement).version);
        if (!s_waylandConnection.outputManagement->isValid()) {
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
    if (flags.testFlag(AdditionalWaylandInterface::IdleInhibition)) {
        s_waylandConnection.idleInhibit = registry->createIdleInhibitManager(registry->interface(Registry::Interface::IdleInhibitManagerUnstableV1).name,
                                                                            registry->interface(Registry::Interface::IdleInhibitManagerUnstableV1).version);
        if (!s_waylandConnection.idleInhibit->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::AppMenu)) {
        s_waylandConnection.appMenu = registry->createAppMenuManager(registry->interface(Registry::Interface::AppMenu).name, registry->interface(Registry::Interface::AppMenu).version);
        if (!s_waylandConnection.appMenu->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::XdgDecoration)) {
        s_waylandConnection.xdgDecoration = registry->createXdgDecorationManager(registry->interface(Registry::Interface::XdgDecorationUnstableV1).name, registry->interface(Registry::Interface::XdgDecorationUnstableV1).version);
        if (!s_waylandConnection.xdgDecoration->isValid()) {
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
    delete s_waylandConnection.xdgShellStable;
    s_waylandConnection.xdgShellStable = nullptr;
    delete s_waylandConnection.xdgShell;
    s_waylandConnection.xdgShell = nullptr;
    delete s_waylandConnection.shadowManager;
    s_waylandConnection.shadowManager = nullptr;
    delete s_waylandConnection.idleInhibit;
    s_waylandConnection.idleInhibit = nullptr;
    delete s_waylandConnection.shm;
    s_waylandConnection.shm = nullptr;
    delete s_waylandConnection.queue;
    s_waylandConnection.queue = nullptr;
    delete s_waylandConnection.registry;
    s_waylandConnection.registry = nullptr;
    delete s_waylandConnection.appMenu;
    s_waylandConnection.appMenu = nullptr;
    delete s_waylandConnection.xdgDecoration;
    s_waylandConnection.xdgDecoration = nullptr;
    delete s_waylandConnection.textInputManager;
    s_waylandConnection.textInputManager = nullptr;
    delete s_waylandConnection.inputPanelV1;
    s_waylandConnection.inputPanelV1 = nullptr;
    delete s_waylandConnection.layerShellV1;
    s_waylandConnection.layerShellV1 = nullptr;
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

IdleInhibitManager *waylandIdleInhibitManager()
{
    return s_waylandConnection.idleInhibit;
}

AppMenuManager* waylandAppMenuManager()
{
    return s_waylandConnection.appMenu;
}

XdgDecorationManager *xdgDecorationManager()
{
    return s_waylandConnection.xdgDecoration;
}

OutputManagement *waylandOutputManagement()
{
    return s_waylandConnection.outputManagement;
}

TextInputManager *waylandTextInputManager()
{
    return s_waylandConnection.textInputManager;
}

QVector<KWayland::Client::Output *> waylandOutputs()
{
    return s_waylandConnection.outputs;
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

void render(Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format)
{
    QImage img(size, format);
    img.fill(color);
    render(surface, img);
}

void render(Surface *surface, const QImage &img)
{
    surface->attachBuffer(s_waylandConnection.shm->createBuffer(img));
    surface->damage(QRect(QPoint(0, 0), img.size()));
    surface->commit(Surface::CommitFlag::None);
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

AbstractClient *renderAndWaitForShown(Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format, int timeout)
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

Surface *createSurface(QObject *parent)
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

SubSurface *createSubSurface(Surface *surface, Surface *parentSurface, QObject *parent)
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

LayerSurfaceV1 *createLayerSurfaceV1(Surface *surface, const QString &scope, Output *output, LayerShellV1::layer layer)
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

XdgShellSurface *createXdgShellStableSurface(Surface *surface, QObject *parent, CreationSetup creationSetup)
{
    if (!s_waylandConnection.xdgShellStable) {
        return nullptr;
    }
    auto s = s_waylandConnection.xdgShellStable->createSurface(surface, parent);
    if (!s->isValid()) {
        delete s;
        return nullptr;
    }
    if (creationSetup == CreationSetup::CreateAndConfigure) {
        initXdgShellSurface(surface, s);
    }
    return s;
}

QtWayland::zwp_input_panel_surface_v1 *createInputPanelSurfaceV1(Surface *surface, Output *output)
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

XdgShellPopup *createXdgShellStablePopup(Surface *surface, XdgShellSurface *parentSurface, const KWayland::Client::XdgPositioner &positioner, QObject *parent, CreationSetup creationSetup)
{
    if (!s_waylandConnection.xdgShellStable) {
        return nullptr;
    }
    auto s = s_waylandConnection.xdgShellStable->createPopup(surface, parentSurface, positioner, parent);
    if (!s->isValid()) {
        delete s;
        return nullptr;
    }
    if (creationSetup == CreationSetup::CreateAndConfigure) {
        initXdgShellPopup(surface, s);
    }
    return s;
}

void initXdgShellSurface(KWayland::Client::Surface *surface, KWayland::Client::XdgShellSurface *shellSurface)
{
    //wait for configure
    QSignalSpy configureRequestedSpy(shellSurface, &KWayland::Client::XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(configureRequestedSpy.wait());
    shellSurface->ackConfigure(configureRequestedSpy.last()[2].toInt());
}

void initXdgShellPopup(KWayland::Client::Surface *surface, KWayland::Client::XdgShellPopup *shellPopup)
{
    //wait for configure
    QSignalSpy configureRequestedSpy(shellPopup, &KWayland::Client::XdgShellPopup::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(configureRequestedSpy.wait());
    shellPopup->ackConfigure(configureRequestedSpy.last()[1].toInt());
}

static void waitForConfigured(XdgSurface *shellSurface)
{
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface, &XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.isValid());

    shellSurface->surface()->commit(Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->ack_configure(surfaceConfigureRequestedSpy.last().first().toUInt());
}

XdgToplevel *createXdgToplevelSurface(Surface *surface, QObject *parent, CreationSetup configureMode)
{
    XdgShell *shell = s_waylandConnection.xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_toplevel surface because xdg_wm_base global is not bound";
        return nullptr;
    }

    XdgSurface *xdgSurface = new XdgSurface(shell, surface, parent);
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

XdgPopup *createXdgPopupSurface(Surface *surface, XdgSurface *parentSurface, XdgPositioner *positioner,
                                QObject *parent, CreationSetup configureMode)
{
    XdgShell *shell = s_waylandConnection.xdgShell;

    if (!shell) {
        qWarning() << "Could not create an xdg_popup surface because xdg_wm_base global is not bound";
        return nullptr;
    }

    XdgSurface *xdgSurface = new XdgSurface(shell, surface, parent);
    XdgPopup *xdgPopup = new XdgPopup(xdgSurface, parentSurface, positioner, parent);

    if (configureMode == CreationSetup::CreateAndConfigure) {
        waitForConfigured(xdgSurface);
    }

    return xdgPopup;
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

}
}
