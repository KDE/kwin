/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <config-kwin.h>
#include <kwin_export.h>

#if HAVE_MEMFD
#include "filedescriptor.h"
#else
#include <QTemporaryFile>
#include <memory>
#endif

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

    RamFile() = default;
    /**
     * Create a file of given size with given data.
     *
     * @note You should call seal() after copying the data into the file.
     *
     * @param name The file name, useful for debugging.
     * @param data The data to store in the file.
     * @param size The size of the file.
     * @param flags The flags to use when creating the file.
     */
    RamFile(const char *name, const void *inData, int size, Flags flags = {});

    RamFile(RamFile &&other) Q_DECL_NOEXCEPT;
    RamFile &operator=(RamFile &&other) Q_DECL_NOEXCEPT;

    /**
     * Destroys the file.
     */
    ~RamFile();

    /**
     * Whether this instance contains a valid file descriptor.
     */
    bool isValid() const;
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
    void cleanup();

    int m_size = 0;
    Flags m_flags = {};

#if HAVE_MEMFD
    KWin::FileDescriptor m_fd;
#else
    std::unique_ptr<QTemporaryFile> m_tmp;
#endif
};

} // namespace KWin
