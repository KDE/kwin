/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef MAIN_H
#define MAIN_H

#include <kapplication.h>
#include "workspace.h"

class Application : public  KApplication
{
public:
    Application();
    ~Application();

protected:
    bool x11EventFilter( XEvent * );
};


#endif
