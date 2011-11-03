/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Thomas Lübking <thomas.luebking@web.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef ANIDATA_H
#define ANIDATA_H

#include "kwinanimationeffect.h"
#include <QEasingCurve>
#include <QTime>
#include <netwm.h>

namespace KWin {

class AniData {
public:
    AniData();
    AniData(AnimationEffect::Attribute a, int meta, int ms, const FPx2 &to,
            QEasingCurve curve, int delay, const FPx2 &from, bool waitAtSource );
    AniData(const AniData &other);
    AniData(const QString &str);
    inline void addTime(int t) { time += t; }
    inline bool isOneDimensional() const {
        return from[0] == from[1] && to[0] == to[1];
    }
    static QList<AniData> list(const QString &str);
    QString toString() const;
    AnimationEffect::Attribute attribute;
    QEasingCurve curve;
    int customCurve;
    FPx2 from, to;
    int time, duration;
    uint meta;
    QTime startTime;
    NET::WindowTypeMask windowType;
    bool waitAtSource;
};

} // namespace

#endif // ANIDATA_H
