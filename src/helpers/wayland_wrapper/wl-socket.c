/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2008 Kristian Høgsberg

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/* This is the size of the char array in struct sock_addr_un.
 * No Wayland socket can be created with a path longer than this,
 * including the null terminator.
 */
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#define LOCK_SUFFIX ".lock"
#define LOCK_SUFFIXLEN 5

struct wl_socket {
    int fd;
    int fd_lock;
    struct sockaddr_un addr;
    char lock_addr[UNIX_PATH_MAX + LOCK_SUFFIXLEN];
    char display_name[16];
};

static struct wl_socket *wl_socket_alloc(void)
{
    struct wl_socket *s;

    s = malloc(sizeof *s);
    if (!s)
        return NULL;

    s->fd = -1;
    s->fd_lock = -1;

    return s;
}

static int wl_socket_lock(struct wl_socket *socket)
{
    struct stat socket_stat;

    snprintf(socket->lock_addr, sizeof socket->lock_addr, "%s%s", socket->addr.sun_path, LOCK_SUFFIX);

    socket->fd_lock = open(socket->lock_addr, O_CREAT | O_CLOEXEC | O_RDWR, (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));

    if (socket->fd_lock < 0) {
        printf("unable to open lockfile %s check permissions\n", socket->lock_addr);
        goto err;
    }

    if (flock(socket->fd_lock, LOCK_EX | LOCK_NB) < 0) {
        printf("unable to lock lockfile %s, maybe another compositor is running\n", socket->lock_addr);
        goto err_fd;
    }

    if (lstat(socket->addr.sun_path, &socket_stat) < 0) {
        if (errno != ENOENT) {
            printf("did not manage to stat file %s\n", socket->addr.sun_path);
            goto err_fd;
        }
    } else if (socket_stat.st_mode & S_IWUSR || socket_stat.st_mode & S_IWGRP) {
        unlink(socket->addr.sun_path);
    }

    return 0;
err_fd:
    close(socket->fd_lock);
    socket->fd_lock = -1;
err:
    *socket->lock_addr = 0;
    /* we did not set this value here, but without lock the
     * socket won't be created anyway. This prevents the
     * wl_socket_destroy from unlinking already existing socket
     * created by other compositor */
    *socket->addr.sun_path = 0;

    return -1;
}

void wl_socket_destroy(struct wl_socket *s)
{
    if (s->addr.sun_path[0])
        unlink(s->addr.sun_path);
    if (s->fd >= 0)
        close(s->fd);
    if (s->lock_addr[0])
        unlink(s->lock_addr);
    if (s->fd_lock >= 0)
        close(s->fd_lock);

    free(s);
}

const char *wl_socket_get_display_name(struct wl_socket *s)
{
    return s->display_name;
}

int wl_socket_get_fd(struct wl_socket *s)
{
    return s->fd;
}

struct wl_socket *wl_socket_create()
{
    struct wl_socket *s;
    int displayno = 0;
    int name_size;

    /* A reasonable number of maximum default sockets. If
     * you need more than this, use the explicit add_socket API. */
    const int MAX_DISPLAYNO = 32;
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        printf("XDG_RUNTIME_DIR not set");
        return NULL;
    }

    s = wl_socket_alloc();
    if (s == NULL)
        return NULL;

    do {
        snprintf(s->display_name, sizeof s->display_name, "wayland-%d", displayno);
        s->addr.sun_family = AF_LOCAL;
        name_size = snprintf(s->addr.sun_path, sizeof s->addr.sun_path, "%s/%s", runtime_dir, s->display_name) + 1;
        assert(name_size > 0);

        if (name_size > (int)sizeof s->addr.sun_path) {
            goto fail;
        }

        if (wl_socket_lock(s) < 0)
            continue;

        s->fd = socket(PF_LOCAL, SOCK_STREAM, 0);

        int size = SUN_LEN(&s->addr);
        int ret = bind(s->fd, (struct sockaddr*)&s->addr, size);
        if (ret < 0) {
            goto fail;
        }
        ret = listen(s->fd, 128);
        if (ret < 0) {
            goto fail;
        }
        return s;
    } while (displayno++ < MAX_DISPLAYNO);

fail:
    wl_socket_destroy(s);
    return NULL;
}
