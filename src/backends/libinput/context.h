/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <libinput.h>
#include <memory>

namespace KWin
{

class Session;
class Udev;

namespace LibInput
{

class Event;

class Context
{
public:
    Context(Session *session, std::unique_ptr<Udev> &&udev);
    ~Context();
    bool initialize();
    bool isValid() const
    {
        return m_libinput != nullptr;
    }
    bool isSuspended() const
    {
        return m_suspended;
    }

    Session *session() const;
    int fileDescriptor();
    void dispatch();
    void suspend();
    void resume();

    operator libinput *()
    {
        return m_libinput;
    }
    operator libinput *() const
    {
        return m_libinput;
    }

    /**
     * Gets the next event, if there is no new event @c nullptr is returned
     */
    std::unique_ptr<Event> event();

    static int openRestrictedCallback(const char *path, int flags, void *user_data);
    static void closeRestrictedCallBack(int fd, void *user_data);
    static const struct libinput_interface s_interface;

private:
    int openRestricted(const char *path, int flags);
    void closeRestricted(int fd);

    Session *m_session;
    struct libinput *m_libinput;
    bool m_suspended;
    std::unique_ptr<Udev> m_udev;
};

}
}
