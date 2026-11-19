/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/memorymap.h"

#include <QThreadPool>

namespace KWin
{

void asyncMunmap(void *addr, size_t length)
{
    QThreadPool::globalInstance()->start([addr, length]() {
        munmap(addr, length);
    });
}

}
