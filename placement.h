/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>


Placement algorithms
Copyright (C) 1997 to 2002 Cristian Tibirna <tibirna@kde.org>
******************************************************************/
#ifndef KWIN_PLACEMENT_H
#define KWIN_PLACEMENT_H

#include <qpoint.h>
#include <qvaluelist.h>

namespace KWinInternal {

class Workspace;
class Client;

class PlacementPrivate;

class Placement
{
public:

    Placement(Workspace* w);
    ~Placement();

    void place(Client* c);

    void placeAtRandom            (Client* c);
    void placeCascaded            (Client* c, bool re_init = false);
    void placeSmart               (Client* c);
    void placeCentered    (Client* c);
    void placeZeroCornered(Client* c);

private:

    Placement();

    PlacementPrivate* d;

};

}

#endif
