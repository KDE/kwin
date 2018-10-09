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
#include <netwm.h>

namespace KWin {

/**
 * Wraps effects->setActiveFullScreenEffect for the duration of it's lifespan
 */
class FullScreenEffectLock
{
public:
    FullScreenEffectLock(Effect *effect);
    ~FullScreenEffectLock();
private:
    Q_DISABLE_COPY(FullScreenEffectLock)
    void *d; //unused currently
};
typedef QSharedPointer<FullScreenEffectLock> FullScreenEffectLockPtr;

class KWINEFFECTS_EXPORT AniData {
public:
    AniData();
    AniData(AnimationEffect::Attribute a, int meta, int ms, const FPx2 &to,
            QEasingCurve curve, int delay, const FPx2 &from, bool waitAtSource, bool keepAtTarget = false, FullScreenEffectLockPtr=FullScreenEffectLockPtr());
    explicit AniData(const QString &str);
    inline void addTime(int t) { time += t; }
    inline bool isOneDimensional() const {
        return from[0] == from[1] && to[0] == to[1];
    }

    quint64 id{0};
    static QList<AniData> list(const QString &str);
    QString toString() const;
    QString debugInfo() const;
    AnimationEffect::Attribute attribute;
    QEasingCurve curve;
    int customCurve;
    FPx2 from, to;
    int time, duration;
    uint meta;
    qint64 startTime;
    NET::WindowTypeMask windowType;
    QSharedPointer<FullScreenEffectLock> fullScreenEffectLock;
    bool waitAtSource, keepAtTarget;
};

} // namespace

QDebug operator<<(QDebug dbg, const KWin::AniData &a);

#endif // ANIDATA_H
