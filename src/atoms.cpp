/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "atoms.h"

namespace KWin
{

Atoms::Atoms()
    : activities(QByteArrayLiteral("_KDE_NET_WM_ACTIVITIES"))
    , wm_protocols(QByteArrayLiteral("WM_PROTOCOLS"))
    , wm_delete_window(QByteArrayLiteral("WM_DELETE_WINDOW"))
    , wm_take_focus(QByteArrayLiteral("WM_TAKE_FOCUS"))
    , wm_change_state(QByteArrayLiteral("WM_CHANGE_STATE"))
    , wm_client_leader(QByteArrayLiteral("WM_CLIENT_LEADER"))
    , wm_window_role(QByteArrayLiteral("WM_WINDOW_ROLE"))
    , wm_state(QByteArrayLiteral("WM_STATE"))
    , sm_client_id(QByteArrayLiteral("SM_CLIENT_ID"))
    , motif_wm_hints(QByteArrayLiteral("_MOTIF_WM_HINTS"))
    , net_wm_context_help(QByteArrayLiteral("_NET_WM_CONTEXT_HELP"))
    , net_wm_ping(QByteArrayLiteral("_NET_WM_PING"))
    , net_wm_user_time(QByteArrayLiteral("_NET_WM_USER_TIME"))
    , kde_net_wm_user_creation_time(QByteArrayLiteral("_KDE_NET_WM_USER_CREATION_TIME"))
    , net_wm_take_activity(QByteArrayLiteral("_NET_WM_TAKE_ACTIVITY"))
    , net_wm_window_opacity(QByteArrayLiteral("_NET_WM_WINDOW_OPACITY"))
    , xdnd_selection(QByteArrayLiteral("XdndSelection"))
    , xdnd_aware(QByteArrayLiteral("XdndAware"))
    , xdnd_enter(QByteArrayLiteral("XdndEnter"))
    , xdnd_type_list(QByteArrayLiteral("XdndTypeList"))
    , xdnd_position(QByteArrayLiteral("XdndPosition"))
    , xdnd_status(QByteArrayLiteral("XdndStatus"))
    , xdnd_action_copy(QByteArrayLiteral("XdndActionCopy"))
    , xdnd_action_move(QByteArrayLiteral("XdndActionMove"))
    , xdnd_action_ask(QByteArrayLiteral("XdndActionAsk"))
    , xdnd_drop(QByteArrayLiteral("XdndDrop"))
    , xdnd_leave(QByteArrayLiteral("XdndLeave"))
    , xdnd_finished(QByteArrayLiteral("XdndFinished"))
    , net_frame_extents(QByteArrayLiteral("_NET_FRAME_EXTENTS"))
    , kde_net_wm_frame_strut(QByteArrayLiteral("_KDE_NET_WM_FRAME_STRUT"))
    , net_wm_sync_request_counter(QByteArrayLiteral("_NET_WM_SYNC_REQUEST_COUNTER"))
    , net_wm_sync_request(QByteArrayLiteral("_NET_WM_SYNC_REQUEST"))
    , kde_net_wm_shadow(QByteArrayLiteral("_KDE_NET_WM_SHADOW"))
    , kde_color_sheme(QByteArrayLiteral("_KDE_NET_WM_COLOR_SCHEME"))
    , kde_skip_close_animation(QByteArrayLiteral("_KDE_NET_WM_SKIP_CLOSE_ANIMATION"))
    , kde_screen_edge_show(QByteArrayLiteral("_KDE_NET_WM_SCREEN_EDGE_SHOW"))
    , utf8_string(QByteArrayLiteral("UTF8_STRING"))
    , text(QByteArrayLiteral("TEXT"))
    , uri_list(QByteArrayLiteral("text/uri-list"))
    , netscape_url(QByteArrayLiteral("_NETSCAPE_URL"))
    , moz_url(QByteArrayLiteral("text/x-moz-url"))
    , wl_surface_serial(QByteArrayLiteral("WL_SURFACE_SERIAL"))
    , kde_net_wm_appmenu_service_name(QByteArrayLiteral("_KDE_NET_WM_APPMENU_SERVICE_NAME"))
    , kde_net_wm_appmenu_object_path(QByteArrayLiteral("_KDE_NET_WM_APPMENU_OBJECT_PATH"))
    , clipboard(QByteArrayLiteral("CLIPBOARD"))
    , timestamp(QByteArrayLiteral("TIMESTAMP"))
    , targets(QByteArrayLiteral("TARGETS"))
    , delete_atom(QByteArrayLiteral("DELETE"))
    , incr(QByteArrayLiteral("INCR"))
    , wl_selection(QByteArrayLiteral("WL_SELECTION"))
    , primary(QByteArrayLiteral("PRIMARY"))
    , edid(QByteArrayLiteral("EDID"))
    , xwayland_allow_commits(QByteArrayLiteral("_XWAYLAND_ALLOW_COMMITS"))
    , m_dtSmWindowInfo(QByteArrayLiteral("_DT_SM_WINDOW_INFO"))
    , m_motifSupport(QByteArrayLiteral("_MOTIF_WM_INFO"))
    , m_helpersRetrieved(false)
{
}

void Atoms::retrieveHelpers()
{
    if (m_helpersRetrieved) {
        return;
    }
    // just retrieve the atoms once, all others are retrieved when being accessed
    m_dtSmWindowInfo.getReply();
    m_motifSupport.getReply();
    m_helpersRetrieved = true;
}

} // namespace
