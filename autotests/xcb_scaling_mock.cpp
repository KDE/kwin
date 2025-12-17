/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <utils/xcbutils.h>

namespace KWin
{

uint32_t Xcb::toXNative(qreal value)
{
    return value;
}

Rect Xcb::toXNative(const RectF &rect)
{
    return rect.toRect();
}

qreal Xcb::fromXNative(int value)
{
    return value;
}

QSizeF Xcb::fromXNative(const QSize &value)
{
    return value;
}

RectF Xcb::nativeFloor(const RectF &value)
{
    return value;
}
}

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
