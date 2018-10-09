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

#include "anidata_p.h"

#include "logging_p.h"

QDebug operator<<(QDebug dbg, const KWin::AniData &a)
{
    dbg.nospace() << a.debugInfo();
    return dbg.space();
}

using namespace KWin;
static const int Gaussian = 46;

FullScreenEffectLock::FullScreenEffectLock(Effect *effect)
{
    effects->setActiveFullScreenEffect(effect);
}

FullScreenEffectLock::~FullScreenEffectLock()
{
    effects->setActiveFullScreenEffect(nullptr);
}

AniData::AniData()
 : attribute(AnimationEffect::Opacity)
 , customCurve(0) // Linear
 , time(0)
 , duration(0)
 , meta(0)
 , startTime(0)
 , windowType((NET::WindowTypeMask)0)
 , waitAtSource(false)
 , keepAtTarget(false)
{
}

AniData::AniData(AnimationEffect::Attribute a, int meta_, int ms, const FPx2 &to_,
                 QEasingCurve curve_, int delay, const FPx2 &from_, bool waitAtSource_, bool keepAtTarget_,
                 FullScreenEffectLockPtr fullScreenEffectLock_)
 : attribute(a)
 , curve(curve_)
 , from(from_)
 , to(to_)
 , time(0)
 , duration(ms)
 , meta(meta_)
 , startTime(AnimationEffect::clock() + delay)
 , windowType((NET::WindowTypeMask)0)
 , fullScreenEffectLock(fullScreenEffectLock_)
 , waitAtSource(waitAtSource_)
 , keepAtTarget(keepAtTarget_)
{
}

static FPx2 fpx2(const QString &s, AnimationEffect::Attribute a)
{
    bool ok; float f1, f2;
    const QVector<QStringRef> floats = s.splitRef(u',');
    f1 = floats.at(0).toFloat(&ok);
    if (!ok || (f1 < 0.0 && !(  a == AnimationEffect::Position ||
                                a == AnimationEffect::Translation ||
                                a == AnimationEffect::Size ||
                                a == AnimationEffect::Rotation)) ) {
        if (ok)
            qCDebug(LIBKWINEFFECTS) << "Invalid value (must not be negative)" << s;
        return FPx2();
    }

    bool forced_align = (floats.count() < 2);
    if (forced_align)
        f2 = f1;
    else {
        f2 = floats.at(1).toFloat(&ok);
        if ( (forced_align = !ok || (f2 < 0.0 && !( a == AnimationEffect::Position ||
                                                    a == AnimationEffect::Translation ||
                                                    a == AnimationEffect::Size ||
                                                    a == AnimationEffect::Rotation))) )
            f2 = f1;
    }
    if ( forced_align && a >= AnimationEffect::NonFloatBase )
        qCDebug(LIBKWINEFFECTS) << "Generic Animations, WARNING: had to align second dimension of non-onedimensional attribute" << a;
    return FPx2(f1, f2);
}

AniData::AniData(const QString &str) // format: WindowMask:Attribute:Meta:Duration:To:Shape:Delay:From
 : customCurve(0) // Linear
 , time(0)
 , duration(1) // invalidate
{
    const QVector<QStringRef> animation = str.splitRef(u':');
    if (animation.count() < 5)
        return; // at least window type, attribute, metadata, time and target is required

    windowType = (NET::WindowTypeMask)animation.at(0).toUInt();

    if (animation.at(1) == QLatin1String("Opacity"))           attribute = AnimationEffect::Opacity;
    else if (animation.at(1) == QLatin1String("Brightness"))   attribute = AnimationEffect::Brightness;
    else if (animation.at(1) == QLatin1String("Saturation"))   attribute = AnimationEffect::Saturation;
    else if (animation.at(1) == QLatin1String("Scale"))        attribute = AnimationEffect::Scale;
    else if (animation.at(1) == QLatin1String("Translation"))  attribute = AnimationEffect::Translation;
    else if (animation.at(1) == QLatin1String("Rotation"))     attribute = AnimationEffect::Rotation;
    else if (animation.at(1) == QLatin1String("Position"))     attribute = AnimationEffect::Position;
    else if (animation.at(1) == QLatin1String("Size"))         attribute = AnimationEffect::Size;
    else if (animation.at(1) == QLatin1String("Clip"))         attribute = AnimationEffect::Clip;
    else {
        qCDebug(LIBKWINEFFECTS) << "Invalid attribute" << animation.at(1);
        return;
    }

    meta = animation.at(2).toUInt();

    bool ok;
    duration = animation.at(3).toInt(&ok);
    if (!ok || duration < 0) {
        qCDebug(LIBKWINEFFECTS) << "Invalid duration" << animation.at(3);
        duration = 0;
        return;
    }

    to = fpx2(animation.at(4).toString(), attribute);

    if (animation.count() > 5) {
        customCurve = animation.at(5).toInt();
        if (customCurve < QEasingCurve::Custom)
            curve.setType((QEasingCurve::Type)customCurve);
        else if (customCurve == Gaussian)
            curve.setCustomType(AnimationEffect::qecGaussian);
        else
            qCDebug(LIBKWINEFFECTS) << "Unknown curve type" << customCurve; // remains default, ie. linear

        if (animation.count() > 6) {
            int t = animation.at(6).toInt();
            if (t < 0)
                qCDebug(LIBKWINEFFECTS) << "Delay can not be negative" << animation.at(6);
            else
                time = t;

            if (animation.count() > 7)
                from = fpx2(animation.at(7).toString(), attribute);
        }
    }
    if (!(from.isValid() || to.isValid())) {
        duration = -1; // invalidate
        return;
    }
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

QList<AniData> AniData::list(const QString &str)
{
    QList<AniData> newList;
    foreach (const QString &astr, str.split(u';', QString::SkipEmptyParts)) {
        newList << AniData(astr);
        if (newList.last().duration < 0)
            newList.removeLast();
    }
    return newList;
}

QString AniData::toString() const
{
    QString ret =   QString::number((uint)windowType) + QLatin1Char(':') + attributeString(attribute) + QLatin1Char(':') +
                    QString::number(meta) + QLatin1Char(':') + QString::number(duration) + QLatin1Char(':') +
                    to.toString() + QLatin1Char(':') + QString::number(customCurve) + QLatin1Char(':') +
                    QString::number(time) + QLatin1Char(':') + from.toString();
    return ret;
}

QString AniData::debugInfo() const
{
    return QLatin1String("Animation: ") + attributeString(attribute) +
           QLatin1String("\n     From: ") + from.toString() +
           QLatin1String("\n       To: ") + to.toString() +
           QLatin1String("\n  Started: ") + QString::number(AnimationEffect::clock() - startTime) + QLatin1String("ms ago\n") +
           QLatin1String(  " Duration: ") + QString::number(duration) + QLatin1String("ms\n") +
           QLatin1String(  "   Passed: ") + QString::number(time) + QLatin1String("ms\n") +
           QLatin1String(  " Applying: ") + QString::number(windowType) + QLatin1Char('\n');
}
