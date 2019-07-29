/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
