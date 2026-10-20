#pragma once

#include "config-kwin.h"

#include "utils/filedescriptor.h"

#include <QDebug>

#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace KWin
{

struct AnonymousFile
{
    static FileDescriptor allocate(const char *name, int sizeInBytes)
    {
#if HAVE_MEMFD
        FileDescriptor fd(memfd_create(name, MFD_CLOEXEC));
        if (!fd.isValid()) {
            qWarning("memfd_create() for %s failed: %s", name, strerror(errno));
            return FileDescriptor();
        }
#else
        char filename[255];
        snprintf(filename, sizeof(filename), "/tmp/%s-XXXXXX", name);

        FileDescriptor fd(mkostemp(filename, O_CLOEXEC));
        if (!fd.isValid()) {
            qWarning("mkostemp() failed: %s", strerror(errno));
            return FileDescriptor();
        }

        if (unlink(filename) == -1) {
            qWarning("unlink() for %s failed: %s", filename, strerror(errno));
        }
#endif

        if (ftruncate(fd.get(), sizeInBytes) < 0) {
            qWarning("Failed to resize %s anonymous file: %s", name, strerror(errno));
            return FileDescriptor();
        }

        return fd;
    }
};

}
