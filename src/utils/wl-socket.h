/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocate and create a socket
 * It is bound and accepted
 */
struct wl_socket *wl_socket_create();

/**
 * Returns the file descriptor for the socket
 */
int wl_socket_get_fd(struct wl_socket *);

/**
 * Returns the name of the socket, i.e "wayland-0"
 */
char *wl_socket_get_display_name(struct wl_socket *);

/**
 * Cleanup resources and close the FD
 */
void wl_socket_destroy(struct wl_socket *socket);

#ifdef __cplusplus
}
#endif
