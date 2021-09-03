#pragma once

#include <QThread>
#include <QWaitCondition>

#include <wlcs/display_server.h>
#include "kwin_wayland_test.h"

namespace KWaylandServer
{

class ClientConnection;

};

// this isn't a qthread because using a qthread will set what
// qt considers the main thread to the wrong one
class ApplicationThread
{

    QScopedPointer<KWin::WaylandTestApplication> app;

    int argc;
    const char** argv;

    std::thread thread;

    void run();

public:

    QWaitCondition cond;

    ApplicationThread(int argc, const char** argv)
        : app(nullptr), argc(argc), argv(argv)
    {

    }

    void start()
    {
        thread = std::thread(&ApplicationThread::run, this);
    }
    void stop()
    {
        app->quit();
    }
    void wait()
    {
        thread.join();
    }
    void resetApp()
    {
        app.reset(nullptr);
    }

};

namespace KWin
{

struct WlcsServer : public ::WlcsDisplayServer
{
    explicit WlcsServer(int argc, const char** argv);

    QScopedPointer<ApplicationThread> thread;

    int argc;
    const char** argv;

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
