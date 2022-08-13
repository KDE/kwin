/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText:: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

namespace KWin
{

class Session;

class KWIN_EXPORT FileDescriptor
{
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int fd);
    FileDescriptor(FileDescriptor &&);
    FileDescriptor &operator=(FileDescriptor &&);
    ~FileDescriptor();

    bool isValid() const;
    int get() const;
    FileDescriptor duplicate() const;

private:
    int m_fd = -1;
};

class KWIN_EXPORT RestrictedFileDescriptor
{
public:
    RestrictedFileDescriptor() = default;
    explicit RestrictedFileDescriptor(Session *session, int fd);
    RestrictedFileDescriptor(RestrictedFileDescriptor &&fd);
    RestrictedFileDescriptor &operator=(RestrictedFileDescriptor &&);
    ~RestrictedFileDescriptor();

    bool isValid() const;
    int get() const;

private:
    Session *m_session = nullptr;
    int m_fd = -1;
};
}
