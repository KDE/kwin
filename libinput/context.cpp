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
#include "context.h"
#include "events.h"
#include "libinput_logging.h"
#include "../logind.h"
#include "../udev.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>

namespace KWin
{
namespace LibInput
{

static void libinputLogHandler(libinput *libinput, libinput_log_priority priority, const char *format, va_list args)
{
    Q_UNUSED(libinput)
    char buf[1024];
    if (std::vsnprintf(buf, 1023, format, args) <= 0) {
        return;
    }
    switch (priority) {
    case LIBINPUT_LOG_PRIORITY_DEBUG:
        qCDebug(KWIN_LIBINPUT) << "Libinput:" << buf;
        break;
    case LIBINPUT_LOG_PRIORITY_INFO:
        qCInfo(KWIN_LIBINPUT) << "Libinput:" << buf;
        break;
    case LIBINPUT_LOG_PRIORITY_ERROR:
    default:
        qCCritical(KWIN_LIBINPUT) << "Libinput:" << buf;
        break;
    }
}

Context::Context(const Udev &udev)
    : m_libinput(libinput_udev_create_context(&Context::s_interface, this, udev))
    , m_suspended(false)
{
    libinput_log_set_priority(m_libinput, LIBINPUT_LOG_PRIORITY_DEBUG);
    libinput_log_set_handler(m_libinput, &libinputLogHandler);
}

Context::~Context()
{
    if (m_libinput) {
        libinput_unref(m_libinput);
    }
}

bool Context::assignSeat(const char *seat)
{
    if (!isValid()) {
        return false;
    }
    return libinput_udev_assign_seat(m_libinput, seat) == 0;
}

int Context::fileDescriptor()
{
    if (!isValid()) {
        return -1;
    }
    return libinput_get_fd(m_libinput);
}

void Context::dispatch()
{
    libinput_dispatch(m_libinput);
}

const struct libinput_interface Context::s_interface = {
    Context::openRestrictedCallback,
    Context::closeRestrictedCallBack
};

int Context::openRestrictedCallback(const char *path, int flags, void *user_data)
{
    return ((Context*)user_data)->openRestricted(path, flags);
}

void Context::closeRestrictedCallBack(int fd, void *user_data)
{
    ((Context*)user_data)->closeRestricted(fd);
}

int Context::openRestricted(const char *path, int flags)
{
    LogindIntegration *logind = LogindIntegration::self();
    Q_ASSERT(logind);
    int fd = logind->takeDevice(path);
    if (fd < 0) {
        // failed
        return fd;
    }
    // adjust flags - based on Weston (logind-util.c)
    int fl = fcntl(fd, F_GETFL);
    auto errorHandling = [fd, this]() {
        close(fd);
        closeRestricted(fd);
    };
    if (fl < 0) {
        errorHandling();
        return -1;
    }

    if (flags & O_NONBLOCK) {
            fl |= O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, fl) < 0) {
        errorHandling();
        return -1;
    }

    fl = fcntl(fd, F_GETFD);
    if (fl < 0) {
        errorHandling();
        return -1;
    }

    if (!(flags & O_CLOEXEC)) {
            fl &= ~FD_CLOEXEC;
    }

    if (fcntl(fd, F_SETFD, fl) < 0) {
        errorHandling();
        return -1;
    }
    return fd;
}

void Context::closeRestricted(int fd)
{
    LogindIntegration *logind = LogindIntegration::self();
    Q_ASSERT(logind);
    logind->releaseDevice(fd);
}

Event *Context::event()
{
    return Event::create(libinput_get_event(m_libinput));
}

void Context::suspend()
{
    if (m_suspended) {
        return;
    }
    libinput_suspend(m_libinput);
    m_suspended = true;
}

void Context::resume()
{
    if (!m_suspended) {
        return;
    }
    libinput_resume(m_libinput);
    m_suspended = false;
}

}
}
