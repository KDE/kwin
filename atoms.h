/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef ATOMS_H
#define ATOMS_H
#include <X11/Xlib.h>

class Atoms {
public:
    Atoms();

    Atom kwin_running;

    Atom wm_protocols;
    Atom wm_delete_window;
    Atom wm_take_focus;
    Atom wm_change_state;
    Atom wm_client_leader;
    Atom wm_save_yourself;

    Atom motif_wm_hints;
    Atom net_wm_context_help;
    Atom kde_wm_change_state;

};


extern Atoms* atoms;

#endif
