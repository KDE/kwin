/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/filedescriptor.h"

#include <optional>
#include <unistd.h>

namespace KWin
{

struct Pipe
{
    FileDescriptor fds[2];

    static std::optional<Pipe> create(int flags)
    {
        int fds[2];
        if (pipe2(fds, flags) != 0) {
            return std::nullopt;
        }
        return Pipe{
            .fds = {
                FileDescriptor(fds[0]),
                FileDescriptor(fds[1]),
            },
        };
    }
};

} // namespace KWin
