#ifndef MAIN_H
#define MAIN_H

#include <kapp.h>
#include "workspace.h"

typedef QValueList<Workspace*> WorkspaceList;
class Application : public  KApplication
{
public:
    Application();
    ~Application();

protected:
    bool x11EventFilter( XEvent * );

private:
    WorkspaceList workspaces;
};



#endif
