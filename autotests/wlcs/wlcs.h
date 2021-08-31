#pragma once

#include <qnamespace.h>
#include <wlcs/display_server.h>
#include "kwin_wayland_test.h"

namespace KWaylandServer
{

class ClientConnection;

};

namespace KWin
{

struct WlcsServer : public ::WlcsDisplayServer
{
    explicit WlcsServer(int argc, const char** argv);

    QScopedPointer<WaylandTestApplication> app;

    int argc;
    const char** argv;

    QScopedPointer<QThread> thread;

    QMap<int, KWaylandServer::ClientConnection*> conns;

    // lifespan management
    void start();
    void stop();
    int createClientSocket();

    // positioning windows
    void positionWindowAbsolute(wl_display* client, wl_surface* surface, int x, int y);

    // input
    ::WlcsPointer* createPointer();
    ::WlcsTouch* createTouch();

    // description of our thing
    const WlcsIntegrationDescriptor* integrationDescriptor() const;
};

inline WlcsServer* kwin(WlcsDisplayServer* it)
{
    return static_cast<WlcsServer*>(it);
}

inline const WlcsServer* kwin(const WlcsDisplayServer* it)
{
    return static_cast<const WlcsServer*>(it);
}

};
