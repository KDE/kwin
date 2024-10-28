/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/filedescriptor.h"

#include <optional>
#include <sys/socket.h>

namespace KWin
{

struct SocketPair
{
    FileDescriptor fds[2];

    static std::optional<SocketPair> create(int domain, int type, int protocol)
    {
        int fds[2];
        if (socketpair(domain, type, protocol, fds) < 0) {
            return std::nullopt;
        }
        return SocketPair{
            .fds = {
                FileDescriptor(fds[0]),
                FileDescriptor(fds[1]),
            },
        };
    }
};

} // namespace KWin
