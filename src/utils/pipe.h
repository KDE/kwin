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
    FileDescriptor readEndpoint;
    FileDescriptor writeEndpoint;

    static std::optional<Pipe> create(int flags)
    {
        int fds[2];
        if (pipe2(fds, flags) != 0) {
            return std::nullopt;
        }
        return Pipe{
            .readEndpoint = FileDescriptor(fds[0]),
            .writeEndpoint = FileDescriptor(fds[1]),
        };
    }
};

} // namespace KWin
