/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

#include "filedescriptor.h"

#include <QFlag>

class QByteArray;

namespace KWin
{

/**
 * @brief Creates a file in memory.
 *
 * This is useful for passing larger data to clients,
 * for example the xkeymap.
 *
 * If memfd is supported, it is used, otherwise
 * a temporary file is created.
 *
 * @note It is advisable not to send the same file
 * descriptor out to multiple clients unless it
 * is sealed for writing. Check which flags actually
 * apply before handing out the file descriptor.
 *
 * @sa effectiveFlags()
 */
class KWIN_EXPORT RamFile
{
public:
    /**
     * Flags to use when creating the file.
     *
     * @note Check with effectiveFlags() which flags
     * actually apply after the file was created.
     */
    enum class Flag {
        SealWrite = 1 << 0, ///< Seal the file descriptor for writing.
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    RamFile(RamFile &&other);
    RamFile &operator=(RamFile &&other);

    static std::optional<RamFile> allocate(const char *name, int size, Flags flags = {});
    static std::optional<RamFile> create(const char *name, const void *inData, int size, Flags flags = {});

    /**
     * The flags that are effectively applied.
     *
     * For instance, even though SealWrite was passed in the constructor,
     * it might not be supported.
     */
    Flags effectiveFlags() const;

    /**
     * The underlying file descriptor
     *
     * @return The fd, or -1 if there is none.
     */
    int fd() const;
    /**
     * The size of the file
     */
    int size() const;

private:
    RamFile(FileDescriptor &&fd, int size);

    FileDescriptor m_fd;
    int m_size = 0;
};

} // namespace KWin
