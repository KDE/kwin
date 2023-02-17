/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ramfile.h"
#include "common.h" // for logging

#include <QScopeGuard>

#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

namespace KWin
{

RamFile::RamFile(const char *name, const void *inData, int size, RamFile::Flags flags)
    : m_size(size)
    , m_flags(flags)
{
    auto guard = qScopeGuard([this] {
        cleanup();
    });

#if HAVE_MEMFD
    m_fd = FileDescriptor(memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (!m_fd.isValid()) {
        qCWarning(KWIN_CORE).nospace() << name << ": Can't create memfd: " << strerror(errno);
        return;
    }

    if (ftruncate(m_fd.get(), size) < 0) {
        qCWarning(KWIN_CORE).nospace() << name << ": Failed to ftruncate memfd: " << strerror(errno);
        return;
    }

    void *data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd.get(), 0);
    if (data == MAP_FAILED) {
        qCWarning(KWIN_CORE).nospace() << name << ": mmap failed: " << strerror(errno);
        return;
    }
#else
    m_tmp = std::make_unique<QTemporaryFile>();
    if (!m_tmp->open()) {
        qCWarning(KWIN_CORE).nospace() << name << ": Can't open temporary file";
        return;
    }

    if (unlink(m_tmp->fileName().toUtf8().constData()) != 0) {
        qCWarning(KWIN_CORE).nospace() << name << ": Failed to remove temporary file from filesystem: " << strerror(errno);
    }

    if (!m_tmp->resize(size)) {
        qCWarning(KWIN_CORE).nospace() << name << ": Failed to resize temporary file";
        return;
    }

    uchar *data = m_tmp->map(0, size);
    if (!data) {
        qCWarning(KWIN_CORE).nospace() << name << ": map failed";
        return;
    }
#endif

    memcpy(data, inData, size);

#if HAVE_MEMFD
    munmap(data, size);
#else
    m_tmp->unmap(data);
#endif

    int seals = F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL;
    if (flags.testFlag(RamFile::Flag::SealWrite)) {
        seals |= F_SEAL_WRITE;
    }
    // This can fail for QTemporaryFile based on the underlying file system.
    if (fcntl(fd(), F_ADD_SEALS, seals) != 0) {
        qCDebug(KWIN_CORE).nospace() << name << ": Failed to seal RamFile: " << strerror(errno);
    }

    guard.dismiss();
}

RamFile::RamFile(RamFile &&other) Q_DECL_NOEXCEPT
    : m_size(std::exchange(other.m_size, 0))
    , m_flags(std::exchange(other.m_flags, RamFile::Flags{}))
#if HAVE_MEMFD
    , m_fd(std::exchange(other.m_fd, KWin::FileDescriptor{}))
#else
    , m_tmp(std::exchange(other.m_tmp, {}))
#endif
{
}

RamFile &RamFile::operator=(RamFile &&other) Q_DECL_NOEXCEPT
{
    cleanup();
    m_size = std::exchange(other.m_size, 0);
    m_flags = std::exchange(other.m_flags, RamFile::Flags{});
#if HAVE_MEMFD
    m_fd = std::exchange(other.m_fd, KWin::FileDescriptor{});
#else
    m_tmp = std::exchange(other.m_tmp, {});
#endif
    return *this;
}

RamFile::~RamFile()
{
    cleanup();
}

void RamFile::cleanup()
{
#if HAVE_MEMFD
    m_fd = KWin::FileDescriptor();
#else
    m_tmp.reset();
#endif
}

bool RamFile::isValid() const
{
    return fd() != -1;
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
#if HAVE_MEMFD
    return m_fd.get();
#else
    return m_tmp->handle();
#endif
}

int RamFile::size() const
{
    return m_size;
}

} // namespace KWin
