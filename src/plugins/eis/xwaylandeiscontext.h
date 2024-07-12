#pragma once

#include <eiscontext.h>

namespace KWin
{

class XWaylandEisContext : public EisContext
{
public:
    XWaylandEisContext(EisBackend *backend);

    const QByteArray socketName;

private:
    void connectionRequested(eis_client *client) override;
};
}
