/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cstdint>

namespace KWin
{

struct DmaBufAttributes
{
    int planeCount = 0;
    int width = 0;
    int height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;

    int fd[4] = {-1, -1, -1, -1};
    int offset[4] = {0, 0, 0, 0};
    int pitch[4] = {0, 0, 0, 0};
};

} // namespace KWin
