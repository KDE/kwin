/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ramfile.h"
#include "common.h" // for logging

#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

namespace KWin
{

std::optional<RamFile> RamFile::allocate(const char *name, int size, RamFile::Flags flags)
{
#if HAVE_MEMFD
    FileDescriptor fd(memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (!fd.isValid()) {
        qCWarning(KWIN_CORE).nospace() << name << ": Can't create memfd: " << strerror(errno);
        return std::nullopt;
    }
#else
    char filename[255];
    snprintf(filename, sizeof(filename), "/tmp/%s-XXXXXX", name);

    FileDescriptor fd(mkostemp(filename, O_CLOEXEC));
    if (!fd.isValid()) {
        qCWarning(KWIN_CORE, "mkostemp() failed: %s", strerror(errno));
        return std::nullopt;
    }

    if (unlink(filename) == -1) {
        qCWarning(KWIN_CORE, "unlink() for %s failed: %s", filename, strerror(errno));
    }
#endif

    if (ftruncate(fd.get(), size) < 0) {
        qCWarning(KWIN_CORE).nospace() << name << ": Failed to ftruncate memfd: " << strerror(errno);
        return std::nullopt;
    }

#if HAVE_MEMFD
    int seals = F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL;
    if (flags.testFlag(RamFile::Flag::SealWrite)) {
        seals |= F_SEAL_WRITE;
    }
    if (fcntl(fd.get(), F_ADD_SEALS, seals) != 0) {
        qCDebug(KWIN_CORE).nospace() << name << ": Failed to seal RamFile: " << strerror(errno);
    }
#endif

    return RamFile(std::move(fd), size);
}

std::optional<RamFile> RamFile::create(const char *name, const void *inData, int size, RamFile::Flags flags)
{
    auto file = allocate(name, size, flags);
    if (!file) {
        return std::nullopt;
    }

    void *data = mmap(nullptr, file->size(), PROT_READ | PROT_WRITE, MAP_SHARED, file->fd(), 0);
    if (data == MAP_FAILED) {
        qCWarning(KWIN_CORE).nospace() << name << ": mmap failed: " << strerror(errno);
        return std::nullopt;
    }

    memcpy(data, inData, file->size());

    munmap(data, file->size());

    return file;
}

RamFile::RamFile(FileDescriptor &&fd, int size)
    : m_fd(std::move(fd))
    , m_size(size)
{
}

RamFile::RamFile(RamFile &&other)
    : m_fd(std::exchange(other.m_fd, FileDescriptor{}))
    , m_size(std::exchange(other.m_size, 0))
{
}

RamFile &RamFile::operator=(RamFile &&other)
{
    m_size = std::exchange(other.m_size, 0);
    m_fd = std::exchange(other.m_fd, FileDescriptor{});
    return *this;
}

RamFile::Flags RamFile::effectiveFlags() const
{
    Flags flags = {};

    const int seals = fcntl(fd(), F_GET_SEALS);
    if (seals > 0) {
        if (seals & F_SEAL_WRITE) {
            flags.setFlag(Flag::SealWrite);
        }
    }

    return flags;
}

int RamFile::fd() const
{
    return m_fd.get();
}

int RamFile::size() const
{
    return m_size;
}

} // namespace KWin
