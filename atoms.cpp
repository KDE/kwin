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
    kwm_running = XInternAtom(qt_xdisplay(), "KWM_RUNNING", FALSE);


    net_number_of_desktops = XInternAtom(qt_xdisplay(), "_NET_NUMBER_OF_DESKTOPS", False);
    net_current_desktop = XInternAtom(qt_xdisplay(), "_NET_CURRENT_DESKTOP", False);
    net_active_window = XInternAtom(qt_xdisplay(), "_NET_ACTIVE_WINDOW", False);

    net_client_list = XInternAtom(qt_xdisplay(), "_NET_CLIENT_LIST", False);
    net_client_list_stacking = XInternAtom(qt_xdisplay(), "_NET_CLIENT_LIST_STACKIN", False);
    net_kde_docking_windows = XInternAtom(qt_xdisplay(), "_NET_KDE_DOCKING_WINDOWS", False);
}
