/*
   SPDX-FileCopyrightText: 2021 Tobias C. Berner <tcberner@FreeBSD.org>

   SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "executable_path.h"

QString executablePathFromPid(pid_t pid)
{
    const int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, static_cast<int>(pid)};
    char buf[MAXPATHLEN];
    size_t cb = sizeof(buf);
    if (sysctl(mib, 4, buf, &cb, nullptr, 0) == 0) {
        return QString::fromLocal8Bit(realpath(buf, nullptr));
    }
    return QString();
}
