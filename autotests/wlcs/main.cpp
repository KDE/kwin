#include <kwin_export.h>

#include "wlcs.h"

extern "C" {

::WlcsDisplayServer* createServer(int argc, const char** argv)
{
    return new KWin::WlcsServer(argc, argv);
}

void destroyServer(::WlcsDisplayServer* server)
{
    delete static_cast<KWin::WlcsServer*>(server);
}

// this external symbol is what's used by wlcs to do the thing
extern const KWIN_EXPORT ::WlcsServerIntegration wlcs_server_integration
{
    1,
    &createServer,
    &destroyServer,
};

}
