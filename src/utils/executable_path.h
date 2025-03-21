/*
    SPDX-FileCopyrightText: 2021 Tobias C. Berner <tcberner@FreeBSD.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QString>

KWIN_EXPORT QString executablePathFromPid(pid_t);
