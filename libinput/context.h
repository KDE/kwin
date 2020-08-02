/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_LIBINPUT_CONTEXT_H
#define KWIN_LIBINPUT_CONTEXT_H

#include <libinput.h>

namespace KWin
{

class Udev;

namespace LibInput
{

class Event;

class Context
{
public:
    Context(const Udev &udev);
    ~Context();
    bool assignSeat(const char *seat);
    bool isValid() const {
        return m_libinput != nullptr;
    }
    bool isSuspended() const {
        return m_suspended;
    }

    int fileDescriptor();
    void dispatch();
    void suspend();
    void resume();

    operator libinput*() {
        return m_libinput;
    }
    operator libinput*() const {
        return m_libinput;
    }

    /**
     * Gets the next event, if there is no new event @c null is returned.
     * The caller takes ownership of the returned pointer.
     */
    Event *event();

    static int openRestrictedCallback(const char *path, int flags, void *user_data);
    static void closeRestrictedCallBack(int fd, void *user_data);
    static const struct libinput_interface s_interface;

private:
    int openRestricted(const char *path, int flags);
    void closeRestricted(int fd);
    struct libinput *m_libinput;
    bool m_suspended;
};

}
}

#endif
