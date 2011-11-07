/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_ATOMS_H
#define KWIN_ATOMS_H

#include <QApplication>
#include <X11/Xlib.h>

namespace KWin
{

class Atoms
{
public:
    Atoms();

    Atom kwin_running;
    Atom activities;

    Atom wm_protocols;
    Atom wm_delete_window;
    Atom wm_take_focus;
    Atom wm_change_state;
    Atom wm_client_leader;
    Atom wm_window_role;
    Atom wm_state;
    Atom sm_client_id;

    Atom motif_wm_hints;
    Atom net_wm_context_help;
    Atom net_wm_ping;
    Atom kde_wm_change_state;
    Atom net_wm_user_time;
    Atom kde_net_wm_user_creation_time;
    Atom kde_system_tray_embedding;
    Atom net_wm_take_activity;
    Atom net_wm_window_opacity;
    Atom xdnd_aware;
    Atom xdnd_position;
    Atom net_frame_extents;
    Atom kde_net_wm_frame_strut;
    Atom net_wm_sync_request_counter;
    Atom net_wm_sync_request;
    Atom kde_net_wm_block_compositing;
    Atom kde_net_wm_shadow;
    Atom kde_net_wm_opaque_region;
    Atom kde_net_wm_tab_group;
};


extern Atoms* atoms;

} // namespace

#endif
