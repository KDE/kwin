/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <sys/mman.h>
#include <utility>

namespace KWin
{

class MemoryMap
{
public:
    MemoryMap()
        : m_data(MAP_FAILED)
        , m_size(0)
    {
    }

    MemoryMap(int size, int prot, int flags, int fd, off_t offset)
        : m_data(mmap(nullptr, size, prot, flags, fd, offset))
        , m_size(size)
    {
    }

    MemoryMap(MemoryMap &&other)
        : m_data(std::exchange(other.m_data, MAP_FAILED))
        , m_size(std::exchange(other.m_size, 0))
    {
    }

    ~MemoryMap()
    {
        if (m_data != MAP_FAILED) {
            munmap(m_data, m_size);
        }
    }

    MemoryMap &operator=(MemoryMap &&other)
    {
        if (m_data != MAP_FAILED) {
            munmap(m_data, m_size);
        }
        m_data = std::exchange(other.m_data, MAP_FAILED);
        m_size = std::exchange(other.m_size, 0);
        return *this;
    }

    inline bool isValid() const
    {
        return m_data != MAP_FAILED;
    }

    inline void *data() const
    {
        return m_data;
    }

    inline int size() const
    {
        return m_size;
    }

private:
    void *m_data;
    int m_size;
};

} // namespace KWin
