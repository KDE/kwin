/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText:: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "filedescriptor.h"
#include "session.h"

#include <unistd.h>
#include <utility>

namespace KWin
{

FileDescriptor::FileDescriptor(int fd)
    : m_fd(fd)
{
}

FileDescriptor::FileDescriptor(FileDescriptor &&other)
    : m_fd(std::exchange(other.m_fd, -1))
{
}

FileDescriptor &FileDescriptor::operator=(FileDescriptor &&other)
{
    if (m_fd != -1) {
        ::close(m_fd);
    }
    m_fd = std::exchange(other.m_fd, -1);
    return *this;
}

FileDescriptor::~FileDescriptor()
{
    if (m_fd != -1) {
        ::close(m_fd);
    }
}

bool FileDescriptor::isValid() const
{
    return m_fd != -1;
}

int FileDescriptor::get() const
{
    return m_fd;
}

FileDescriptor FileDescriptor::duplicate() const
{
    if (m_fd != -1) {
        return FileDescriptor{dup(m_fd)};
    } else {
        return {};
    }
}

RestrictedFileDescriptor::RestrictedFileDescriptor(Session *session, int fd)
    : m_session(session)
    , m_fd(fd)
{
}

RestrictedFileDescriptor::RestrictedFileDescriptor(RestrictedFileDescriptor &&other)
    : m_session(other.m_session)
    , m_fd(std::exchange(other.m_fd, -1))
{
}

RestrictedFileDescriptor &RestrictedFileDescriptor::operator=(RestrictedFileDescriptor &&other)
{
    if (m_fd != -1) {
        m_session->closeRestricted(m_fd);
    }
    m_fd = std::exchange(other.m_fd, -1);
    m_session = other.m_session;
    return *this;
}

RestrictedFileDescriptor::~RestrictedFileDescriptor()
{
    if (m_fd != -1) {
        m_session->closeRestricted(m_fd);
    }
}

bool RestrictedFileDescriptor::isValid() const
{
    return m_fd != -1;
}

int RestrictedFileDescriptor::get() const
{
    return m_fd;
}
}
