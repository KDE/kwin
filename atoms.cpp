/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#include <qapplication.h>
#include "atoms.h"

Atoms::Atoms()
{

    const int max = 20;
    Atom* atoms[max];
    char* names[max];
    Atom atoms_return[max];
    int n = 0;

    atoms[n] = &kwin_running;
    names[n++] = (char *) "KWIN_RUNNING";

    atoms[n] = &wm_protocols;
    names[n++] = (char *) "WM_PROTOCOLS";

    atoms[n] = &wm_delete_window;
    names[n++] = (char *) "WM_DELETE_WINDOW";

    atoms[n] = &wm_take_focus;
    names[n++] = (char *) "WM_TAKE_FOCUS";

    atoms[n] = &wm_change_state;
    names[n++] = (char *) "WM_CHANGE_STATE";

    atoms[n] = &wm_client_leader;
    names[n++] = (char *) "WM_CLIENT_LEADER";

    atoms[n] = &wm_save_yourself;
    names[n++] = (char *) "WM_SAVE_YOURSELF";

    atoms[n] = &motif_wm_hints;
    names[n++] = (char *) "_MOTIF_WM_HINTS";

    atoms[n] = &net_wm_context_help;
    names[n++] = (char *) "_NET_WM_CONTEXT_HELP";

    atoms[n] = &kde_wm_change_state;
    names[n++] = (char *) "_KDE_WM_CHANGE_STATE";

    Atom fake;
    atoms[n] = &fake;
    names[n++] = (char *) "_DT_SM_WINDOW_INFO";

    XInternAtoms( qt_xdisplay(), names, n, FALSE, atoms_return );
    for (int i = 0; i < n; i++ )
	*atoms[i] = atoms_return[i];


}
