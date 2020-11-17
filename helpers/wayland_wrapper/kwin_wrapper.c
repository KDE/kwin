/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * This tiny executable creates a socket, then starts kwin passing it the FD to the wayland socket.
 * The WAYLAND_DISPLAY environment variable gets set here and passed to all spawned kwin instances.
 * On any non-zero kwin exit kwin gets restarted.
 *
 * After restart kwin is relaunched but now with the KWIN_RESTART_COUNT env set to an incrementing counter
 *
 * It's somewhat  akin to systemd socket activation, but we also need the lock file, finding the next free socket
 * and so on, hence our own binary.
 *
 * Usage kwin_wayland_wrapper [argForKwin] [argForKwin] ...
 */

#include "wl-socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

char *old_wayland_env = NULL;

#define WAYLAND_ENV_NAME "WAYLAND_DISPLAY"

pid_t launch_kwin(struct wl_socket *socket, int argc, char **argv)
{
    printf("Launching kwin\n");
    pid_t pid = fork();
    if (pid == 0) { /* child process */
        char fdString[5]; // long enough string to contain what is probably a 1 digit number.
        snprintf(fdString, sizeof(fdString) - 1, "%d", wl_socket_get_fd(socket));

        char **args = calloc(argc + 6, sizeof(char *));
        uint pos = 0;
        args[pos++] = (char *)"kwin_wayland"; //process name is the first argument by convention
        args[pos++] = (char *)"--wayland_fd";
        args[pos++] = fdString;

        // for running in nested wayland, pass the original socket name
        if (old_wayland_env) {
            args[pos++] = (char *)"--wayland-display";
            args[pos++] = old_wayland_env;
        }

        //copy passed args
        for (int i = 0; i < argc; i++) {
            args[pos++] = argv[i];
        }

        args[pos++] = NULL;

        execvpe("kwin_wayland", args, environ);
        free(args);
        exit(127); /* if exec fails */
    } else {
        return pid;
    }
}

int main(int argc, char **argv)
{
    struct wl_socket *socket = wl_socket_create();
    if (!socket) {
        return -1;
    }

    // copy the old WAYLAND_DISPLAY as we are about to overwrite it and kwin may need it
    if (getenv(WAYLAND_ENV_NAME)) {
        old_wayland_env = strdup(getenv(WAYLAND_ENV_NAME));
    }

    setenv(WAYLAND_ENV_NAME, wl_socket_get_display_name(socket), 1);

    int crashCount = 0;
    while (crashCount < 10) {
        int status;

        if (crashCount > 0) {
            char restartCountEnv[3];
            snprintf(restartCountEnv, sizeof(restartCountEnv) - 1, "%d", crashCount);
            setenv("KWIN_RESTART_COUNT", restartCountEnv, 1);
        }

        // forward our own arguments, but drop the first, as that will be our own executable name
        pid_t pid = launch_kwin(socket, argc - 1, &argv[1]);
        if (pid < 0) {
            // failed to launch kwin, we can just quit immediately
            break;
        }

        waitpid(pid, &status, 0); /* wait for child */

        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            fprintf(stderr, "Kwin exited with code %d\n", exit_status);
            break;
        } else if (WIFSIGNALED(status)) {
            // we crashed! Let's go again!
            pid = 0;
            fprintf(stderr, "Compositor crashed, respawning\n");
        }
        crashCount++;
    }

    wl_socket_destroy(socket);
    free(old_wayland_env);
    return 0;
}
