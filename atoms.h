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

    Atom wm_protocols;
    Atom wm_delete_window;
    Atom wm_take_focus;
    Atom wm_change_state;
    Atom kwm_win_icon; // compatibility
    Atom kwm_command;  // compatibility
    Atom kwm_running;

    Atom net_number_of_desktops;
    Atom net_current_desktop;
    Atom net_active_window;
    Atom net_client_list;
    Atom net_client_list_stacking;
    Atom net_wm_context_help;

    Atom net_kde_docking_windows;

};


extern Atoms* atoms;

#endif
