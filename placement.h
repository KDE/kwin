/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>


Placement algorithms
Copyright (C) 1997 to 2002 Cristian Tibirna <tibirna@kde.org>
******************************************************************/

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
    void placeStupidlyCentered    (Client* c);
    void placeStupidlyZeroCornered(Client* c);

private:

    Placement();

    PlacementPrivate* d;

};

};
