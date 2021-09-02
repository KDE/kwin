#include "wlcs.h"

#include <KWaylandServer/compositor_interface.h>
#include <KWaylandServer/surface_interface.h>

#include "abstract_client.h"
#include "abstract_output.h"
#include "platform.h"
#include "wayland_server.h"

#include "wayland-client.h"
#include "wayland-client-protocol.h"
#include "wayland-server.h"
#include "wayland-server-protocol.h"
#include "wayland-server-core.h"

#include "wlcs_pointer.h"
#include "wlcs_touch.h"

static const char* s_socketName = "kwin-wlcs-test-0";

void ApplicationThread::run()
{
    using namespace KWin;

    qDebug() << __PRETTY_FUNCTION__ << "Starting WlcsServer thread...";

    app.reset
        ( new WaylandTestApplication
            ( WaylandTestApplication::OperationModeWaylandOnly
            , argc
            , const_cast<char**>(argv)
            )
        );

    qDebug() << __PRETTY_FUNCTION__ << "Initting Wayland Server...";

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    Q_ASSERT(applicationStartedSpy.isValid());
    Q_ASSERT(waylandServer()->init(s_socketName));

    qDebug() << __PRETTY_FUNCTION__ << "Setting up output size...";

    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));

    qDebug() << __PRETTY_FUNCTION__ << "Setting up virtual outputs";

    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    qDebug() << __PRETTY_FUNCTION__ << "Starting KWin App...";

    kwinApp()->start();

    qDebug() << __PRETTY_FUNCTION__ << "Waiting On Started...";

    Q_ASSERT(applicationStartedSpy.wait(6 * 1000));

    qDebug() << __PRETTY_FUNCTION__ << "Started!";

    qDebug() << __PRETTY_FUNCTION__ << "Sanity checking outputs...";

    const auto outputs = kwinApp()->platform()->enabledOutputs();
    Q_ASSERT(outputs.count() == 2);
    Q_ASSERT(outputs[0]->geometry() == QRect(0, 0, 1280, 1024));
    Q_ASSERT(outputs[1]->geometry() == QRect(1280, 0, 1280, 1024));

    qDebug() << __PRETTY_FUNCTION__ << "Initialising Workspace...";

    Test::initWaylandWorkspace();

    cond.wakeAll();

    qDebug() << __PRETTY_FUNCTION__ << "Thread initialisation done, starting event loop...";

    app->exec();

    qDebug() << __PRETTY_FUNCTION__ << "Event loop finished!";
}

namespace KWin
{

void thunk_startWlcs(WlcsDisplayServer* server)
{
    kwin(server)->start();
}

void thunk_stopWlcs(WlcsDisplayServer* server)
{
    kwin(server)->stop();
}

int thunk_createClientSocket(WlcsDisplayServer* server)
{
    return kwin(server)->createClientSocket();
}

void thunk_positionWindowAbsolute(WlcsDisplayServer* server, wl_display* client, wl_surface* surface, int x, int y)
{
    kwin(server)->positionWindowAbsolute(client, surface, x, y);
}

::WlcsPointer* thunk_createPointer(WlcsDisplayServer* server)
{
    return kwin(server)->createPointer();
}

::WlcsTouch* thunk_createTouch(WlcsDisplayServer* server)
{
    return kwin(server)->createTouch();
}

const WlcsIntegrationDescriptor* thunk_getDescriptor(const WlcsDisplayServer* server)
{
    return kwin(server)->integrationDescriptor();
}

#define unimplemented ({ qFatal("unimplemented"); nullptr; })

WlcsServer::WlcsServer(int argc, const char** argv)
    : ::WlcsDisplayServer
        { .version = 3
        , .start = &thunk_startWlcs
        , .stop = &thunk_stopWlcs
        , .create_client_socket = &thunk_createClientSocket
        , .position_window_absolute = &thunk_positionWindowAbsolute
        , .create_pointer = &thunk_createPointer
        , .create_touch = &thunk_createTouch
        , .get_descriptor = &thunk_getDescriptor
        , .start_on_this_thread = nullptr
        }
    , thread
        ( new ApplicationThread(argc, argv)
        )
    , argc
        ( argc
        )
    , argv
        ( argv
        )
{
}

void WlcsServer::start()
{
    qDebug() << __PRETTY_FUNCTION__ << "Starting WlcsServer...";

    thread->start();

    QMutex mtx;
    QMutexLocker mtxLocker(&mtx);

    thread->cond.wait(&mtx);

    qDebug() << __PRETTY_FUNCTION__ << "Started WlcsServer...";
}

void WlcsServer::stop()
{
    qDebug() << __PRETTY_FUNCTION__ << "Stopping WlcsServer...";

    thread->stop();
    thread->wait();

    qDebug() << __PRETTY_FUNCTION__ << "Stopped WlcsServer...";
}

int WlcsServer::createClientSocket()
{
    qDebug() << __PRETTY_FUNCTION__ << "Creating socket connection...";
    auto conn = WaylandServer::self()->createConnection();
    conns[conn.fd] = conn.connection;

    qDebug() << __PRETTY_FUNCTION__ << "Returning socket connection...";
    return conn.fd;
}

void WlcsServer::positionWindowAbsolute(wl_display* client, wl_surface* surface, int x, int y)
{
    qDebug() << __PRETTY_FUNCTION__ << "Positioning window absolutely...";

    Q_UNUSED(client)

    const auto fd = wl_display_get_fd(client);
    const auto id = wl_proxy_get_id(reinterpret_cast<wl_proxy*>(surface));

    auto surf = KWaylandServer::SurfaceInterface::get(id, conns[fd]);
    Q_ASSERT(surf);

    auto it = waylandServer()->findClient(surf);
    Q_ASSERT(it);

    it->move(QPoint(x, y));
}

::WlcsPointer* WlcsServer::createPointer()
{
    qDebug() << __PRETTY_FUNCTION__ << "Creating pointer...";
    return new WlcsPointer();
}

::WlcsTouch* WlcsServer::createTouch()
{
    qDebug() << __PRETTY_FUNCTION__ << "Creating touch...";
    return new WlcsTouch();
}

// TODO: figure out how to retrieve this list dynamically at runtime
constexpr WlcsExtensionDescriptor interfaces[] = {
    {"wl_compositor", 4},
    {"zwp_tablet_manager_v2", 1},
    {"zwp_keyboard_shortcuts_inhibit_manager_v1", 1},
    {"xdg_wm_base", 3},
    {"zwlr_layer_shell_v1", 3},
    {"zxdg_decoration_manager_v1", 1},
    {"wp_viewporter", 1},
    {"wl_shm", 1},
    {"wl_seat", 7},
    {"zwp_pointer_gestures_v1", 2},
    {"zwp_pointer_constraints_v1", 1},
    {"wl_data_device_manager", 3},
    {"zwlr_data_control_manager_v1", 2},
    {"zwp_primary_selection_device_manager_v1", 1},
    {"org_kde_kwin_idle", 1},
    {"zwp_idle_inhibit_manager_v1", 1},
    {"org_kde_plasma_shell", 6},
    {"org_kde_kwin_appmenu_manager", 1},
    {"org_kde_kwin_server_decoration_palette_manager", 1},
    {"org_kde_plasma_virtual_desktop_management", 2},
    {"org_kde_kwin_shadow_manager", 2},
    {"org_kde_kwin_dpms_manager", 1},
    {"org_kde_kwin_server_decoration_manager", 1},
    {"org_kde_kwin_outputmanagement", 4},
    {"zxdg_output_manager_v1", 3},
    {"wl_subcompositor", 1},
    {"zxdg_exporter_v2", 1},
    {"zxdg_importer_v2", 1},
    {"xdg_activation_v1", 1},
    {"zwp_relative_pointer_manager_v1", 1},
    {"wl_drm", 2},
    {"zwp_linux_dmabuf_v1", 3},
    {"org_kde_kwin_outputdevice", 4},
    {"wl_output", 3},
    {"zwp_text_input_manager_v2", 1},
    {"zwp_text_input_manager_v3", 1},
    {"org_kde_kwin_blur_manager", 1},
    {"org_kde_kwin_contrast_manager", 2},
    {"org_kde_kwin_slide_manager", 1},
    {"org_kde_kwin_outputdevice", 4},
    {"wl_output", 3},
};

const WlcsIntegrationDescriptor descriptor
    { .version = 1
    , .num_extensions = sizeof(interfaces) / sizeof(interfaces[0])
    , .supported_extensions = interfaces
    };

const WlcsIntegrationDescriptor* WlcsServer::integrationDescriptor() const
{
    return &descriptor;
}

};
