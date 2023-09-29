/*
    SPDX-FileCopyrightText: 2021 Tobias C. Berner <tcberner@FreeBSD.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "executable_path.h"

#include <QFileInfo>

QString executablePathFromPid(pid_t pid)
{
    return QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
}
