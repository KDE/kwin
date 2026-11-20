/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/memorymap.h"

#include <QThreadPool>

namespace KWin
{

namespace
{

class UnmapThreadPool : public QThreadPool
{
public:
    UnmapThreadPool()
    {
        setMaxThreadCount(1);
        setObjectName(QStringLiteral("KWin munmap"));
    }
};

}

Q_GLOBAL_STATIC(UnmapThreadPool, s_unmapThreadPool)

void asyncMunmap(void *addr, size_t length, FileDescriptor &&fd)
{
    if (s_unmapThreadPool.isDestroyed()) {
        munmap(addr, length);
        return;
    }
    s_unmapThreadPool->start([addr, length, fd = std::move(fd)]() mutable {
        munmap(addr, length);
        fd.reset();
    });
}

}
