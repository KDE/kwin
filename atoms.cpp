#include <qapplication.h>
#include "atoms.h"

Atoms::Atoms()
{

    //TODO use XInternAtoms instead to avoid roundtrips
    wm_protocols = XInternAtom(qt_xdisplay(), "WM_PROTOCOLS", FALSE);
    wm_delete_window = XInternAtom(qt_xdisplay(), "WM_DELETE_WINDOW", FALSE);
    wm_take_focus = XInternAtom(qt_xdisplay(), "WM_TAKE_FOCUS", FALSE);
    
     // compatibility
    kwm_win_icon = XInternAtom(qt_xdisplay(), "KWM_WIN_ICON", FALSE);

}
