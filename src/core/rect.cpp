/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/rect.h"

#include <QDebugStateSaver>

QDebug operator<<(QDebug dbg, const KWin::Rect &rect)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "KWin::Rect(" << rect.x() << "," << rect.y() << " " << rect.width() << "x" << rect.height() << ")";
    return dbg;
}

QDebug operator<<(QDebug dbg, const KWin::RectF &rect)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "KWin::RectF(" << rect.x() << "," << rect.y() << " " << rect.width() << "x" << rect.height() << ")";
    return dbg;
}

QDataStream &operator<<(QDataStream &stream, const KWin::Rect &rect)
{
    stream << int(rect.left()) << int(rect.top()) << int(rect.right()) << int(rect.bottom());
    return stream;
}

QDataStream &operator>>(QDataStream &stream, KWin::Rect &rect)
{
    int left;
    int top;
    int right;
    int bottom;
    stream >> left >> top >> right >> bottom;
    rect.setCoords(left, top, right, bottom);
    return stream;
}

QDataStream &operator<<(QDataStream &stream, const KWin::RectF &rect)
{
    stream << double(rect.left()) << double(rect.top()) << double(rect.right()) << double(rect.bottom());
    return stream;
}

QDataStream &operator>>(QDataStream &stream, KWin::RectF &rect)
{
    double left;
    double top;
    double right;
    double bottom;
    stream >> left >> top >> right >> bottom;
    rect.setCoords(left, top, right, bottom);
    return stream;
}
