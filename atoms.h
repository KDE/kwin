#ifndef ATOMS_H
#define ATOMS_H
#include <X11/Xlib.h>

class Atoms {
public:
    Atoms();

    Atom wm_protocols;
    Atom wm_delete_window;
    Atom wm_take_focus;
    Atom kwm_win_icon; // compatibility
    
    Atom net_number_of_desktops;
    Atom net_current_desktop;
    Atom net_active_window;
    Atom net_client_list;
    Atom net_client_list_stacking;

};


extern Atoms* atoms;

#endif
