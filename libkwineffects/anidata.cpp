/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Thomas Lübking <thomas.luebking@web.de>
Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

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

#include "anidata_p.h"

#include "logging_p.h"

QDebug operator<<(QDebug dbg, const KWin::AniData &a)
{
    dbg.nospace() << a.debugInfo();
    return dbg.space();
}

using namespace KWin;

FullScreenEffectLock::FullScreenEffectLock(Effect *effect)
{
    effects->setActiveFullScreenEffect(effect);
}

FullScreenEffectLock::~FullScreenEffectLock()
{
    effects->setActiveFullScreenEffect(nullptr);
}

KeepAliveLock::KeepAliveLock(EffectWindow *w)
    : m_window(w)
{
    m_window->refWindow();
}

KeepAliveLock::~KeepAliveLock()
{
    m_window->unrefWindow();
}

PreviousWindowPixmapLock::PreviousWindowPixmapLock(EffectWindow *w)
    : m_window(w)
{
    m_window->referencePreviousWindowPixmap();
}

PreviousWindowPixmapLock::~PreviousWindowPixmapLock()
{
    m_window->unreferencePreviousWindowPixmap();

    // Add synthetic repaint to prevent glitches after cross-fading
    // translucent windows.
    effects->addRepaint(m_window->expandedGeometry());
}

AniData::AniData()
 : attribute(AnimationEffect::Opacity)
 , customCurve(0) // Linear
 , time(0)
 , duration(0)
 , meta(0)
 , startTime(0)
 , waitAtSource(false)
 , keepAtTarget(false)
 , keepAlive(true)
{
}

AniData::AniData(AnimationEffect::Attribute a, int meta_, int ms, const FPx2 &to_,
                 QEasingCurve curve_, int delay, const FPx2 &from_, bool waitAtSource_, bool keepAtTarget_,
                 FullScreenEffectLockPtr fullScreenEffectLock_, bool keepAlive,
                 PreviousWindowPixmapLockPtr previousWindowPixmapLock_)
 : attribute(a)
 , curve(curve_)
 , from(from_)
 , to(to_)
 , time(0)
 , duration(ms)
 , meta(meta_)
 , startTime(AnimationEffect::clock() + delay)
 , fullScreenEffectLock(fullScreenEffectLock_)
 , waitAtSource(waitAtSource_)
 , keepAtTarget(keepAtTarget_)
 , keepAlive(keepAlive)
 , previousWindowPixmapLock(previousWindowPixmapLock_)
{
}

static QString attributeString(KWin::AnimationEffect::Attribute attribute)
{
    switch (attribute) {
    case KWin::AnimationEffect::Opacity:      return QStringLiteral("Opacity");
    case KWin::AnimationEffect::Brightness:   return QStringLiteral("Brightness");
    case KWin::AnimationEffect::Saturation:   return QStringLiteral("Saturation");
    case KWin::AnimationEffect::Scale:        return QStringLiteral("Scale");
    case KWin::AnimationEffect::Translation:  return QStringLiteral("Translation");
    case KWin::AnimationEffect::Rotation:     return QStringLiteral("Rotation");
    case KWin::AnimationEffect::Position:     return QStringLiteral("Position");
    case KWin::AnimationEffect::Size:         return QStringLiteral("Size");
    case KWin::AnimationEffect::Clip:         return QStringLiteral("Clip");
    default:                                  return QStringLiteral(" ");
    }
}

QString AniData::debugInfo() const
{
    return QLatin1String("Animation: ") + attributeString(attribute) +
           QLatin1String("\n     From: ") + from.toString() +
           QLatin1String("\n       To: ") + to.toString() +
           QLatin1String("\n  Started: ") + QString::number(AnimationEffect::clock() - startTime) + QLatin1String("ms ago\n") +
           QLatin1String(  " Duration: ") + QString::number(duration) + QLatin1String("ms\n") +
           QLatin1String(  "   Passed: ") + QString::number(time) + QLatin1String("ms\n");
}
