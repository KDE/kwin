/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/filedescriptor.h"
#include <cstdint>

namespace KWin
{

struct DmaBufParams
{
    int planeCount = 0;
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;
};

struct DmaBufAttributes
{
    int planeCount = 0;
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;

    FileDescriptor fd[4];
    int offset[4] = {0, 0, 0, 0};
    int pitch[4] = {0, 0, 0, 0};
};

} // namespace KWin
