/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/box.h"

#include <QDebugStateSaver>

QDebug operator<<(QDebug dbg, const KWin::Box &box)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "KWin::Box(" << box.x() << "," << box.y() << " " << box.width() << "x" << box.height() << ")";
    return dbg;
}

QDebug operator<<(QDebug dbg, const KWin::BoxF &box)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "KWin::BoxF(" << box.x() << "," << box.y() << " " << box.width() << "x" << box.height() << ")";
    return dbg;
}
