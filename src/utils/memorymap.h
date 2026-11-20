/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/filedescriptor.h"

#include <kwin_export.h>

#include <sys/mman.h>
#include <utility>

namespace KWin
{

/*!
 * Asynchronously unmaps a part of the address space starting at \a addr and continuing for
 * \a length bytes.
 *
 * Plain munmap() can take a significant amount of time to complete sometimes, for example, it
 * can take 10ms or so. This function schedules munmap() to be called from a worker thread
 * to keep the caller thread unblocked.
 *
 * If \a fd is valid, it is closed on the worker thread after munmap().
 */
KWIN_EXPORT void asyncMunmap(void *addr, size_t length, FileDescriptor &&fd = FileDescriptor());

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

    /*!
     * Keeps a duplicate of \a fd and closes it after unmapping. If the mapping holds the last
     * reference to a shared memory file, the pages are then freed by close(), outside the
     * mmap_lock that munmap() holds.
     */
    MemoryMap(int size, int prot, int flags, const FileDescriptor &fd, off_t offset)
        : m_data(mmap(nullptr, size, prot, flags, fd.get(), offset))
        , m_size(size)
        , m_fd(fd.duplicate())
    {
    }

    MemoryMap(MemoryMap &&other)
        : m_data(std::exchange(other.m_data, MAP_FAILED))
        , m_size(std::exchange(other.m_size, 0))
        , m_fd(std::move(other.m_fd))
    {
    }

    ~MemoryMap()
    {
        if (m_data != MAP_FAILED) {
            asyncMunmap(m_data, m_size, std::move(m_fd));
        }
    }

    MemoryMap &operator=(MemoryMap &&other)
    {
        if (m_data != MAP_FAILED) {
            asyncMunmap(m_data, m_size, std::move(m_fd));
        }
        m_data = std::exchange(other.m_data, MAP_FAILED);
        m_size = std::exchange(other.m_size, 0);
        m_fd = std::move(other.m_fd);
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
    FileDescriptor m_fd;
};

} // namespace KWin
