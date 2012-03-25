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

#include "kwinanimationeffect.h"
#include "anidata_p.h"

#include <QTime>
#include <QTimer>

namespace KWin {
struct AnimationEffectPrivate {
public:
    AnimationEffectPrivate() { m_animated = m_damageDirty = false; }
    AnimationEffect::AniMap m_animations;
    EffectWindowList m_zombies;
    bool m_animated, m_damageDirty, m_needSceneRepaint;
};
}

using namespace KWin;

AnimationEffect::AnimationEffect() : d_ptr(new AnimationEffectPrivate())
{
    Q_D(AnimationEffect);
    d->m_animated = false;
    /* this is the same as the QTimer::singleShot(0, SLOT(init())) kludge
     * defering the init and esp. the connection to the windowClosed slot */
    QMetaObject::invokeMethod( this, "init", Qt::QueuedConnection );
}

void AnimationEffect::init()
{
    /* by connecting the signal from a slot AFTER the inheriting class constructor had the chance to
     * connect it we can provide auto-referencing of animated and closed windows, since at the time
     * our slot will be called, the slot of the subclass has been (SIGNAL/SLOT connections are FIFO)
     * and has pot. started an animation so we have the window in our hash :) */
    connect ( effects,  SIGNAL(windowClosed(KWin::EffectWindow*)), SLOT(_windowClosed(KWin::EffectWindow*)) );
    connect ( effects,  SIGNAL(windowDeleted(KWin::EffectWindow*)), SLOT(_windowDeleted(KWin::EffectWindow*)) );
    connect ( effects,  SIGNAL(windowGeometryShapeChanged(KWin::EffectWindow*, const QRect&)),
                        SLOT(_expandedGeometryChanged(KWin::EffectWindow*, const QRect&)) );
    connect ( effects,  SIGNAL(windowPaddingChanged(KWin::EffectWindow*, const QRect&)),
                        SLOT(_expandedGeometryChanged(KWin::EffectWindow*, const QRect&)) );
}

bool AnimationEffect::isActive() const
{
    Q_D(const AnimationEffect);
    return !d->m_animations.isEmpty();
}


#define RELATIVE_XY(_FIELD_) const bool relative[2] = { metaData(Relative##_FIELD_##X, meta), metaData(Relative##_FIELD_##Y, meta) }

void AnimationEffect::animate( EffectWindow *w, Attribute a, uint meta, int ms, FPx2 to, QEasingCurve curve, int delay, FPx2 from )
{
    const bool waitAtSource = from.isValid();
    if (a < NonFloatBase) {
        if (a == Scale) {
            QRect area = effects->clientArea(ScreenArea , w);
            if (from.isValid()) {
                RELATIVE_XY(Source);
                from.set(   relative[0] ? from[0] * area.width() / w->width() : from[0],
                            relative[1] ? from[1] * area.height() / w->height() : from[1] );
            }
            if (to.isValid()) {
                RELATIVE_XY(Target);
                to.set( relative[0] ? to[0] * area.width() / w->width() : to[0],
                        relative[1] ? to[1] * area.height() / w->height() : to[1] );
            }
        } else if (a == Rotation) {
            if (!from.isValid()) {
                setMetaData( SourceAnchor, metaData(TargetAnchor, meta), meta );
                from.set(0.0,0.0);
            }
            if (!to.isValid()) {
                setMetaData( TargetAnchor, metaData(SourceAnchor, meta), meta );
                to.set(0.0,0.0);
            }
        }
        if (!from.isValid())
            from.set(1.0,1.0);
        if (!to.isValid())
            to.set(1.0,1.0);


    } else if (a == Position) {
        QRect area = effects->clientArea(ScreenArea , w);
        QPoint pt = w->geometry().bottomRight(); // cannot be < 0 ;-)
        if (from.isValid()) {
            RELATIVE_XY(Source);
            from.set( relative[0] ? area.x() + from[0] * area.width() : from[0],
                      relative[1] ? area.y() + from[1] * area.height() : from[1] );
        } else {
            from.set(pt.x(), pt.y());
            setMetaData( SourceAnchor, AnimationEffect::Bottom|AnimationEffect::Right, meta );
        }

        if (to.isValid()) {
            RELATIVE_XY(Target);
            to.set( relative[0] ? area.x() + to[0] * area.width() : to[0],
                    relative[1] ? area.y() + to[1] * area.height() : to[1] );
        } else {
            to.set(pt.x(), pt.y());
            setMetaData( TargetAnchor, AnimationEffect::Bottom|AnimationEffect::Right, meta );
        }


    } else if (a == Size) {
        QRect area = effects->clientArea(ScreenArea , w);
        if (from.isValid()) {
            RELATIVE_XY(Source);
            from.set( relative[0] ? from[0] * area.width() : from[0],
                      relative[1] ? from[1] * area.height() : from[1] );
        } else {
            from.set(w->width(), w->height());
        }

        if (to.isValid()) {
            RELATIVE_XY(Target);
            to.set( relative[0] ? to[0] * area.width() : to[0],
                    relative[1] ? to[1] * area.height() : to[1] );
        } else {
            from.set(w->width(), w->height());
        }


    } else if (a == Translation) {
        QRect area = w->rect();
        if (from.isValid()) {
            RELATIVE_XY(Source);
            from.set(   relative[0] ? from[0] * area.width() : from[0],
                        relative[1] ? from[1] * area.height() : from[1] );
        } else {
            from.set(0.0, 0.0);
        }

        if (to.isValid()) {
            RELATIVE_XY(Target);
            to.set( relative[0] ? to[0] * area.width() : to[0],
                    relative[1] ? to[1] * area.height() : to[1] );
        } else {
            to.set(0.0, 0.0);
        }
    }

    Q_D(AnimationEffect);
    AniMap::iterator it = d->m_animations.find(w);
    if (it == d->m_animations.end())
        it = d->m_animations.insert(w, QPair<QList<AniData>, QRect>(QList<AniData>(), QRect()));
    it->first.append(AniData(a, meta, ms, to, curve, delay, from, waitAtSource));
    it->second = QRect();

    if (delay > 0) {
        QTimer::singleShot(delay, this, SLOT(triggerRepaint()));
        if (waitAtSource)
            w->addLayerRepaint(0, 0, displayWidth(), displayHeight());
    }
    else {
        triggerRepaint();
    }
}

void AnimationEffect::prePaintScreen( ScreenPrePaintData& data, int time )
{
    Q_D(AnimationEffect);
    if (d->m_animations.isEmpty()) {
        effects->prePaintScreen(data, time);
        return;
    }

    AniMap::iterator entry = d->m_animations.begin(), mapEnd = d->m_animations.end();
    d->m_animated = false;
//     short int transformed = 0;
    while (entry != mapEnd) {
        bool invalidateLayerRect = false;
        QList<AniData>::iterator anim = entry->first.begin(), animEnd = entry->first.end();
        while (anim != animEnd) {
            if (QTime::currentTime() < anim->startTime) {
                if (!anim->waitAtSource) {
                    ++anim;
                    continue;
                }
            } else {
                anim->addTime(time);
            }

            if (anim->time < anim->duration) {
//                 if (anim->attribute != Brightness && anim->attribute != Saturation && anim->attribute != Opacity)
//                     transformed = true;
                d->m_animated = true;
                ++anim;
            } else {
                animationEnded(entry.key(), anim->attribute, anim->meta);
                anim = entry->first.erase(anim);
                invalidateLayerRect = d->m_damageDirty = true;
                animEnd = entry->first.end();
            }
        }
        if (entry->first.isEmpty()) {
            const int i = d->m_zombies.indexOf(entry.key());
            if ( i > -1 ) {
                d->m_zombies.removeAt( i );
                entry.key()->unrefWindow();
            }
            data.paint |= entry->second;
//             d->m_damageDirty = true; // TODO likely no longer required
            entry = d->m_animations.erase(entry);
            mapEnd = d->m_animations.end();
        } else {
            if (invalidateLayerRect)
                *const_cast<QRect*>(&(entry->second)) = QRect(); // invalidate
            ++entry;
        }
    }

    // NOTICE PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS and thus now no flag should be required
    // ... unless we start to get glitches ;-)
//     if ( transformed )
//         data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS; //PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    // janitorial...
    if ( !(d->m_animations.count() || d->m_zombies.isEmpty()) )
    {
        foreach ( EffectWindow *w, d->m_zombies )
            w->unrefWindow();
        d->m_zombies.clear();
    }

    effects->prePaintScreen(data, time);
}

void AnimationEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
{
    Q_D(AnimationEffect);
    if ( d->m_animated ) {
        AniMap::const_iterator entry = d->m_animations.constFind( w );
        if ( entry != d->m_animations.constEnd() ) {
            bool isUsed = false;
            for (QList<AniData>::const_iterator anim = entry->first.constBegin(); anim != entry->first.constEnd(); ++anim) {
                if (QTime::currentTime() < anim->startTime && !anim->waitAtSource)
                    continue;

                isUsed = true;
                if (anim->attribute == Opacity)
                    data.setTranslucent();
                else if (!(anim->attribute == Brightness || anim->attribute == Saturation)) {
                    data.setTransformed();
                    data.mask |= PAINT_WINDOW_TRANSFORMED;
                }
            }
            if ( isUsed ) {
                if ( w->isMinimized() )
                    w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
                else if ( w->isDeleted() )
                    w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DELETE );
                else if ( !w->isOnCurrentDesktop() )
                    w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
//                 if( !w->isPaintingEnabled() && !effects->activeFullScreenEffect() )
//                     effects->addLayerRepaint(w->expandedGeometry());
            }
        }
    }
    effects->prePaintWindow( w, data, time );
}

static inline float geometryCompensation(int flags, float v)
{
    if (flags & (AnimationEffect::Left|AnimationEffect::Top))
        return 0.0; // no compensation required
    if (flags & (AnimationEffect::Right|AnimationEffect::Bottom))
        return 1.0 - v; // full compensation
    return 0.5 * (1.0 - v); // half compensation
}


static int xCoord(const QRect &r, int flag) {
    if (flag & AnimationEffect::Left)
        return r.x();
    else if (flag & AnimationEffect::Right)
        return r.right();
    else
        return r.x() + r.width()/2;
}

static int yCoord(const QRect &r, int flag) {
    if (flag & AnimationEffect::Top)
        return r.y();
    else if (flag & AnimationEffect::Bottom)
        return r.bottom();
    else
        return r.y() + r.height()/2;
}

void AnimationEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
{
    Q_D(AnimationEffect);
    if ( d->m_animated ) {
        AniMap::const_iterator entry = d->m_animations.constFind( w );
        if ( entry != d->m_animations.constEnd() ) {
            for ( QList<AniData>::const_iterator anim = entry->first.constBegin(); anim != entry->first.constEnd(); ++anim ) {

                if (QTime::currentTime() < anim->startTime && !anim->waitAtSource)
                    continue;

                switch (anim->attribute) {
                case Opacity:
                    data.opacity *= interpolated(*anim); break;
                case Brightness:
                    data.brightness *= interpolated(*anim); break;
                case Saturation:
                    data.saturation *= interpolated(*anim); break;
                case Scale: {
                    const QSize sz = w->geometry().size();
                    float f1(1.0), f2(0.0);
                    if (anim->from[0] >= 0.0 && anim->to[0] >= 0.0) { // scale x
                        f1 = interpolated(*anim, 0);
                        f2 = geometryCompensation( anim->meta & AnimationEffect::Horizontal, f1 );
                        data.xTranslate += f2 * sz.width();
                        data.xScale *= f1;
                    }
                    if (anim->from[1] >= 0.0 && anim->to[1] >= 0.0) { // scale y
                        if (!anim->isOneDimensional()) {
                            f1 = interpolated(*anim, 1);
                            f2 = geometryCompensation( anim->meta & AnimationEffect::Vertical, f1 );
                        } else if ( ((anim->meta & AnimationEffect::Vertical)>>1) != (anim->meta & AnimationEffect::Horizontal) )
                            f2 = geometryCompensation( anim->meta & AnimationEffect::Vertical, f1 );
                        data.yTranslate += f2 * sz.height();
                        data.yScale *= f1;
                    }
                    break;
                }
                case Translation:
                    data.xTranslate += interpolated(*anim, 0);
                    data.yTranslate += interpolated(*anim, 1);
                    break;
                case Size: {
                    FPx2 dest = anim->from + progress(*anim) * (anim->to - anim->from);
                    const QSize sz = w->geometry().size();
                    float f;
                    if (anim->from[0] >= 0.0 && anim->to[0] >= 0.0) { // resize x
                        f = dest[0]/sz.width();
                        data.xTranslate += geometryCompensation( anim->meta & AnimationEffect::Horizontal, f ) * sz.width();
                        data.xScale *= f;
                    }
                    if (anim->from[1] >= 0.0 && anim->to[1] >= 0.0) { // resize y
                        f = dest[1]/sz.height();
                        data.yTranslate += geometryCompensation( anim->meta & AnimationEffect::Vertical, f ) * sz.height();
                        data.yScale *= f;
                    }
                    break;
                }
                case Position: {
                    const QRect geo = w->geometry();
                    const float prgrs = progress(*anim);
                    if ( anim->from[0] >= 0.0 && anim->to[0] >= 0.0 ) {
                        float dest = interpolated(*anim, 0);
                        const int x[2] = {  xCoord(geo, metaData(SourceAnchor, anim->meta)),
                                            xCoord(geo, metaData(TargetAnchor, anim->meta)) };
                        data.xTranslate += dest - (x[0] + prgrs*(x[1] - x[0]));
                    }
                    if ( anim->from[1] >= 0.0 && anim->to[1] >= 0.0 ) {
                        float dest = interpolated(*anim, 1);
                        const int y[2] = {  yCoord(geo, metaData(SourceAnchor, anim->meta)),
                                            yCoord(geo, metaData(TargetAnchor, anim->meta)) };
                        data.yTranslate += dest - (y[0] + prgrs*(y[1] - y[0]));
                    }
                    break;
                }
                case Rotation: {
                    RotationData rot;
                    rot.axis = (RotationData::RotationAxis)metaData(Axis, anim->meta);
                    const float prgrs = progress(*anim);
                    rot.angle = anim->from[0] + prgrs*(anim->to[0] - anim->from[0]);

                    const QRect geo = w->rect();
                    const uint  sAnchor = metaData(SourceAnchor, anim->meta),
                                tAnchor = metaData(TargetAnchor, anim->meta);
                    QPointF pt(xCoord(geo, sAnchor), yCoord(geo, sAnchor));

                    if (tAnchor != sAnchor) {
                        QPointF pt2(xCoord(geo, tAnchor), yCoord(geo, tAnchor));
                        pt += static_cast<qreal>(prgrs)*(pt2 - pt);
                    }

                    rot.xRotationPoint = pt.x();
                    rot.yRotationPoint = pt.y();
                    data.rotation = &rot;
                    break;
                }
                case Generic:
                    genericAnimation(w, data, progress(*anim), anim->meta);
                    break;
                default:
                    break;
                }
            }
        }
    }
    effects->paintWindow( w, mask, region, data );
}

void AnimationEffect::postPaintScreen()
{
    Q_D(AnimationEffect);
    if ( d->m_animated ) {
        if (d->m_damageDirty)
            updateLayerRepaints();
        if (d->m_needSceneRepaint) {
            effects->addRepaintFull();
        } else {
            AniMap::const_iterator it = d->m_animations.constBegin(), end = d->m_animations.constEnd();
            for (; it != end; ++it) {
                it.key()->addLayerRepaint(it->second);
            }
        }
    }
    effects->postPaintScreen();
}

float AnimationEffect::interpolated( const AniData &a, int i ) const
{
    if (QTime::currentTime() < a.startTime)
        return a.from[i];
    return a.from[i] + a.curve.valueForProgress( ((float)a.time)/a.duration )*(a.to[i] - a.from[i]);
}

float AnimationEffect::progress( const AniData &a ) const
{
    if (QTime::currentTime() < a.startTime)
        return 0.0;
    return a.curve.valueForProgress( ((float)a.time)/a.duration );
}


// TODO - get this out of the header - the functionpointer usage of QEasingCurve somehow sucks ;-)
// qreal AnimationEffect::qecGaussian(qreal progress) // exp(-5*(2*x-1)^2)
// {
//     progress = 2*progress - 1;
//     progress *= -5*progress;
//     return qExp(progress);
// }

int AnimationEffect::metaData( MetaType type, uint meta )
{
    switch (type) {
        case SourceAnchor:
            return ((meta>>5) & 0x1f);
        case TargetAnchor:
            return (meta& 0x1f);
        case RelativeSourceX:
        case RelativeSourceY:
        case RelativeTargetX:
        case RelativeTargetY: {
            const int shift = 10 + type - RelativeSourceX;
            return ((meta>>shift) & 1);
        }
        case Axis:
            return ((meta>>10) & 3);
        default:
            return 0;
    }
}

void AnimationEffect::setMetaData( MetaType type, uint value, uint &meta )
{
    switch (type) {
    case SourceAnchor:
        meta &= ~(0x1f<<5);
        meta |= ((value & 0x1f)<<5);
        break;
    case TargetAnchor:
        meta &= ~(0x1f);
        meta |= (value & 0x1f);
        break;
    case RelativeSourceX:
    case RelativeSourceY:
    case RelativeTargetX:
    case RelativeTargetY: {
        const int shift = 10 + type - RelativeSourceX;
        if (value)
            meta |= (1<<shift);
        else
            meta &= ~(1<<shift);
        break;
    }
    case Axis:
        meta &= ~(3<<10);
        meta |= ((value & 3)<<10);
        break;
    default:
        break;
    }
}

void AnimationEffect::triggerRepaint()
{
    Q_D(AnimationEffect);
    for (AniMap::const_iterator entry = d->m_animations.constBegin(), mapEnd = d->m_animations.constEnd(); entry != mapEnd; ++entry)
        *const_cast<QRect*>(&(entry->second)) = QRect();
    updateLayerRepaints();
    if (d->m_needSceneRepaint) {
        effects->addRepaintFull();
    } else {
        AniMap::const_iterator it = d->m_animations.constBegin(), end = d->m_animations.constEnd();
        for (; it != end; ++it)
            it.key()->addLayerRepaint(it->second);
    }
}

static float fixOvershoot(float f, const AniData &d, short int dir, float s = 1.1)
{
    switch(d.curve.type()) {
        case QEasingCurve::InOutElastic:
        case QEasingCurve::InOutBack:
            return f * s;
        case QEasingCurve::InElastic:
        case QEasingCurve::OutInElastic:
        case QEasingCurve::OutBack:
            return (dir&2) ? f * s : f;
        case QEasingCurve::OutElastic:
        case QEasingCurve::InBack:
            return (dir&1) ? f * s : f;
        default:
            return f;
    }
}

void AnimationEffect::updateLayerRepaints()
{
    Q_D(AnimationEffect);
    d->m_needSceneRepaint = false;
    for (AniMap::const_iterator entry = d->m_animations.constBegin(), mapEnd = d->m_animations.constEnd(); entry != mapEnd; ++entry) {
        if (!entry->second.isNull())
            continue;
        float f[2] = {1.0, 1.0};
        float t[2] = {0.0, 0.0};
        bool createRegion = false;
        QList<QRect> rects;
        QRect *layerRect = const_cast<QRect*>(&(entry->second));
        for (QList<AniData>::const_iterator anim = entry->first.constBegin(), animEnd = entry->first.constEnd(); anim != animEnd; ++anim) {
            if (QTime::currentTime() < anim->startTime)
                continue;
            switch (anim->attribute) {
                case Opacity:
                case Brightness:
                case Saturation:
                    createRegion = true;
                    break;
                case Rotation:
                    createRegion = false;
                    *layerRect = QRect(0, 0, displayWidth(), displayHeight());
                    goto region_creation; // sic! no need to do anything else
                case Generic:
                    d->m_needSceneRepaint = true; // we don't know whether this will change visual stacking order
                    return; // sic! no need to do anything else
                case Translation:
                case Position: {
                    createRegion = true;
                    QRect r(entry.key()->geometry());
                    int x[2] = {0,0};
                    int y[2] = {0,0};
                    if (anim->attribute == Translation) {
                        x[0] = anim->from[0];
                        x[1] = anim->to[0];
                        y[0] = anim->from[1];
                        y[1] = anim->to[1];
                    } else {
                        if ( anim->from[0] >= 0.0 && anim->to[0] >= 0.0 ) {
                            x[0] = anim->from[0] - xCoord(r, metaData(SourceAnchor, anim->meta));
                            x[1] = anim->to[0] - xCoord(r, metaData(TargetAnchor, anim->meta));
                        }
                        if ( anim->from[1] >= 0.0 && anim->to[1] >= 0.0 ) {
                            y[0] = anim->from[1] - yCoord(r, metaData(SourceAnchor, anim->meta));
                            y[1] = anim->to[1] - yCoord(r, metaData(TargetAnchor, anim->meta));
                        }
                    }
                    r = entry.key()->expandedGeometry();
                    rects << r.translated(x[0], y[0]) << r.translated(x[1], y[1]);
                    break;
                }
                case Size:
                case Scale: {
                    createRegion = true;
                    const QSize sz = entry.key()->geometry().size();
                    float fx = qMax(fixOvershoot(anim->from[0], *anim, 1), fixOvershoot(anim->to[0], *anim, 2));
//                     float fx = qMax(interpolated(*anim,0), anim->to[0]);
                    if (fx >= 0.0) {
                        if (anim->attribute == Size)
                            fx /= sz.width();
                        f[0] *= fx;
                        t[0] += geometryCompensation( anim->meta & AnimationEffect::Horizontal, fx ) * sz.width();
                    }
//                     float fy = qMax(interpolated(*anim,1), anim->to[1]);
                    float fy = qMax(fixOvershoot(anim->from[1], *anim, 1), fixOvershoot(anim->to[1], *anim, 2));
                    if (fy >= 0.0) {
                        if (anim->attribute == Size)
                            fy /= sz.height();
                        if (!anim->isOneDimensional()) {
                            f[1] *= fy;
                            t[1] += geometryCompensation( anim->meta & AnimationEffect::Vertical, fy ) * sz.height();
                        } else if ( ((anim->meta & AnimationEffect::Vertical)>>1) != (anim->meta & AnimationEffect::Horizontal) ) {
                            f[1] *= fx;
                            t[1] += geometryCompensation( anim->meta & AnimationEffect::Vertical, fx ) * sz.height();
                        }
                    }
                    break;
                }
            }
        }
region_creation:
        if (createRegion) {
            const QRect geo = entry.key()->expandedGeometry();
            if (rects.isEmpty())
                rects << geo;
            QList<QRect>::const_iterator r, rEnd = rects.constEnd();
            for ( r = rects.constBegin(); r != rEnd; ++r) { // transform
                const_cast<QRect*>(&(*r))->setSize(QSize(qRound(r->width()*f[0]), qRound(r->height()*f[1])));
                const_cast<QRect*>(&(*r))->translate(t[0], t[1]); // "const_cast" - don't do that at home, kids ;-)
            }
            QRect rect = rects.at(0);
            if (rects.count() > 1) {
                for ( r = rects.constBegin() + 1; r != rEnd; ++r) // unite
                    rect |= *r;
                const int dx = 110*(rect.width() - geo.width())/100 + 1 - rect.width() + geo.width();
                const int dy = 110*(rect.height() - geo.height())/100 + 1 - rect.height() + geo.height();
                rect.adjust(-dx,-dy,dx,dy); // fix pot. overshoot
            }
            *layerRect = rect;
        }
    }
    d->m_damageDirty = false;
}

void AnimationEffect::_expandedGeometryChanged(KWin::EffectWindow *w, const QRect &old)
{
    Q_D(AnimationEffect);
    AniMap::const_iterator entry = d->m_animations.constFind(w);
    if (entry != d->m_animations.constEnd()) {
        *const_cast<QRect*>(&(entry->second)) = QRect();
        updateLayerRepaints();
        if (!entry->second.isNull()) // actually got updated, ie. is in use - ensure it get's a repaint
            w->addLayerRepaint(entry->second);
    }
}

void AnimationEffect::_windowClosed( EffectWindow* w )
{
    Q_D(AnimationEffect);
    if (d->m_animations.contains(w) && !d->m_zombies.contains(w)) {
        w->refWindow();
        d->m_zombies << w;
    }
}

void AnimationEffect::_windowDeleted( EffectWindow* w )
{
    Q_D(AnimationEffect);
    d->m_zombies.removeAll( w ); // TODO this line is a workaround for a bug in KWin 4.8.0 & 4.8.1
    d->m_animations.remove( w );
}


#include "moc_kwinanimationeffect.cpp"
