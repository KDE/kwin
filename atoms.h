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

};


extern Atoms* atoms;

#endif
