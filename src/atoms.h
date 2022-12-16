/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/xcbutils.h"

namespace KWin
{

class KWIN_EXPORT Atoms
{
public:
    Atoms();

    Xcb::Atom activities;

    Xcb::Atom wm_protocols;
    Xcb::Atom wm_delete_window;
    Xcb::Atom wm_take_focus;
    Xcb::Atom wm_change_state;
    Xcb::Atom wm_client_leader;
    Xcb::Atom wm_window_role;
    Xcb::Atom wm_state;
    Xcb::Atom sm_client_id;

    Xcb::Atom motif_wm_hints;
    Xcb::Atom net_wm_context_help;
    Xcb::Atom net_wm_ping;
    Xcb::Atom net_wm_user_time;
    Xcb::Atom kde_net_wm_user_creation_time;
    Xcb::Atom net_wm_take_activity;
    Xcb::Atom net_wm_window_opacity;
    Xcb::Atom xdnd_selection;
    Xcb::Atom xdnd_aware;
    Xcb::Atom xdnd_enter;
    Xcb::Atom xdnd_type_list;
    Xcb::Atom xdnd_position;
    Xcb::Atom xdnd_status;
    Xcb::Atom xdnd_action_copy;
    Xcb::Atom xdnd_action_move;
    Xcb::Atom xdnd_action_ask;
    Xcb::Atom xdnd_drop;
    Xcb::Atom xdnd_leave;
    Xcb::Atom xdnd_finished;
    Xcb::Atom net_frame_extents;
    Xcb::Atom kde_net_wm_frame_strut;
    Xcb::Atom net_wm_sync_request_counter;
    Xcb::Atom net_wm_sync_request;
    Xcb::Atom kde_net_wm_shadow;
    Xcb::Atom kde_first_in_window_list;
    Xcb::Atom kde_color_sheme;
    Xcb::Atom kde_skip_close_animation;
    Xcb::Atom kde_screen_edge_show;
    Xcb::Atom utf8_string;
    Xcb::Atom text;
    Xcb::Atom uri_list;
    Xcb::Atom netscape_url;
    Xcb::Atom moz_url;
    Xcb::Atom wl_surface_id;
    Xcb::Atom wl_surface_serial;
    Xcb::Atom kde_net_wm_appmenu_service_name;
    Xcb::Atom kde_net_wm_appmenu_object_path;
    Xcb::Atom clipboard;
    Xcb::Atom timestamp;
    Xcb::Atom targets;
    Xcb::Atom delete_atom;
    Xcb::Atom incr;
    Xcb::Atom wl_selection;
    Xcb::Atom primary;
    Xcb::Atom edid;

    /**
     * @internal
     */
    void retrieveHelpers();

private:
    // helper atoms we need to resolve to "announce" support (see #172028)
    Xcb::Atom m_dtSmWindowInfo;
    Xcb::Atom m_motifSupport;
    bool m_helpersRetrieved;
};

extern KWIN_EXPORT Atoms *atoms;

} // namespace
