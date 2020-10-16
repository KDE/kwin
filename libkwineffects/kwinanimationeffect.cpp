/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Thomas Lübking <thomas.luebking@web.de>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinanimationeffect.h"
#include "anidata_p.h"

#include <QDateTime>
#include <QTimer>
#include <QtDebug>
#include <QVector3D>

namespace KWin
{

QDebug operator<<(QDebug dbg, const KWin::FPx2 &fpx2)
{
    dbg.nospace() << fpx2[0] << "," << fpx2[1] << QString(fpx2.isValid() ? QStringLiteral(" (valid)") : QStringLiteral(" (invalid)"));
    return dbg.space();
}

QElapsedTimer AnimationEffect::s_clock;

class AnimationEffectPrivate {
public:
    AnimationEffectPrivate()
    {
        m_animated = m_damageDirty = m_animationsTouched = m_isInitialized = false;
        m_justEndedAnimation = 0;
    }
    AnimationEffect::AniMap m_animations;
    static quint64 m_animCounter;
    quint64 m_justEndedAnimation; // protect against cancel
    QWeakPointer<FullScreenEffectLock> m_fullScreenEffectLock;
    bool m_animated, m_damageDirty, m_needSceneRepaint, m_animationsTouched, m_isInitialized;
};

quint64 AnimationEffectPrivate::m_animCounter = 0;

AnimationEffect::AnimationEffect() : d_ptr(new AnimationEffectPrivate())
{
    Q_D(AnimationEffect);
    d->m_animated = false;
    if (!s_clock.isValid())
        s_clock.start();
    /* this is the same as the QTimer::singleShot(0, SLOT(init())) kludge
     * defering the init and esp. the connection to the windowClosed slot */
    QMetaObject::invokeMethod( this, "init", Qt::QueuedConnection );
}

AnimationEffect::~AnimationEffect()
{
    delete d_ptr;
}

void AnimationEffect::init()
{
    Q_D(AnimationEffect);
    if (d->m_isInitialized)
        return; // not more than once, please
    d->m_isInitialized = true;
    /* by connecting the signal from a slot AFTER the inheriting class constructor had the chance to
     * connect it we can provide auto-referencing of animated and closed windows, since at the time
     * our slot will be called, the slot of the subclass has been (SIGNAL/SLOT connections are FIFO)
     * and has pot. started an animation so we have the window in our hash :) */
    connect(effects, &EffectsHandler::windowClosed, this, &AnimationEffect::_windowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &AnimationEffect::_windowDeleted);
}

bool AnimationEffect::isActive() const
{
    Q_D(const AnimationEffect);
    return !d->m_animations.isEmpty() && !effects->isScreenLocked();
}


#define RELATIVE_XY(_FIELD_) const bool relative[2] = { static_cast<bool>(metaData(Relative##_FIELD_##X, meta)), \
                                                        static_cast<bool>(metaData(Relative##_FIELD_##Y, meta)) }

void AnimationEffect::validate(Attribute a, uint &meta, FPx2 *from, FPx2 *to, const EffectWindow *w) const
{
    if (a < NonFloatBase) {
        if (a == Scale) {
            QRect area = effects->clientArea(ScreenArea , w);
            if (from && from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? (*from)[0] * area.width() / w->width() : (*from)[0],
                          relative[1] ? (*from)[1] * area.height() / w->height() : (*from)[1]);
            }
            if (to && to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? (*to)[0] * area.width() / w->width() : (*to)[0],
                        relative[1] ? (*to)[1] * area.height() / w->height() : (*to)[1] );
            }
        } else if (a == Rotation) {
            if (from && !from->isValid()) {
                setMetaData(SourceAnchor, metaData(TargetAnchor, meta), meta);
                from->set(0.0,0.0);
            }
            if (to && !to->isValid()) {
                setMetaData(TargetAnchor, metaData(SourceAnchor, meta), meta);
                to->set(0.0,0.0);
            }
        }
        if (from && !from->isValid())
            from->set(1.0,1.0);
        if (to && !to->isValid())
            to->set(1.0,1.0);


    } else if (a == Position) {
        QRect area = effects->clientArea(ScreenArea , w);
        QPoint pt = w->geometry().bottomRight(); // cannot be < 0 ;-)
        if (from) {
            if (from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? area.x() + (*from)[0] * area.width() : (*from)[0],
                        relative[1] ? area.y() + (*from)[1] * area.height() : (*from)[1]);
            } else {
                from->set(pt.x(), pt.y());
                setMetaData(SourceAnchor, AnimationEffect::Bottom|AnimationEffect::Right, meta);
            }
        }

        if (to) {
            if (to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? area.x() + (*to)[0] * area.width() : (*to)[0],
                        relative[1] ? area.y() + (*to)[1] * area.height() : (*to)[1]);
            } else {
                to->set(pt.x(), pt.y());
                setMetaData( TargetAnchor, AnimationEffect::Bottom|AnimationEffect::Right, meta );
            }
        }


    } else if (a == Size) {
        QRect area = effects->clientArea(ScreenArea , w);
        if (from) {
            if (from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? (*from)[0] * area.width() : (*from)[0],
                          relative[1] ? (*from)[1] * area.height() : (*from)[1]);
            } else {
                from->set(w->width(), w->height());
            }
        }

        if (to) {
            if (to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? (*to)[0] * area.width() : (*to)[0],
                        relative[1] ? (*to)[1] * area.height() : (*to)[1]);
            } else {
                to->set(w->width(), w->height());
            }
        }

    } else if (a == Translation) {
        QRect area = w->rect();
        if (from) {
            if (from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? (*from)[0] * area.width() : (*from)[0],
                          relative[1] ? (*from)[1] * area.height() : (*from)[1]);
            } else {
                from->set(0.0, 0.0);
            }
        }

        if (to) {
            if (to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? (*to)[0] * area.width() : (*to)[0],
                        relative[1] ? (*to)[1] * area.height() : (*to)[1]);
            } else {
                to->set(0.0, 0.0);
            }
        }

    } else if (a == Clip) {
        if (from && !from->isValid()) {
            from->set(1.0,1.0);
            setMetaData(SourceAnchor, metaData(TargetAnchor, meta), meta);
        }
        if (to && !to->isValid()) {
            to->set(1.0,1.0);
            setMetaData(TargetAnchor, metaData(SourceAnchor, meta), meta);
        }

    } else if (a == CrossFadePrevious) {
        if (from && !from->isValid()) {
            from->set(0.0);
        }
        if (to && !to->isValid()) {
            to->set(1.0);
        }
    }
}

quint64 AnimationEffect::p_animate( EffectWindow *w, Attribute a, uint meta, int ms, FPx2 to, const QEasingCurve &curve, int delay, FPx2 from, bool keepAtTarget, bool fullScreenEffect, bool keepAlive)
{
    const bool waitAtSource = from.isValid();
    validate(a, meta, &from, &to, w);

    Q_D(AnimationEffect);
    if (!d->m_isInitialized)
        init(); // needs to ensure the window gets removed if deleted in the same event cycle
    if (d->m_animations.isEmpty()) {
        connect(effects, &EffectsHandler::windowGeometryShapeChanged,
            this, &AnimationEffect::_expandedGeometryChanged);
        connect(effects, &EffectsHandler::windowStepUserMovedResized,
            this, &AnimationEffect::_expandedGeometryChanged);
        connect(effects, &EffectsHandler::windowPaddingChanged,
            this, &AnimationEffect::_expandedGeometryChanged);
    }
    AniMap::iterator it = d->m_animations.find(w);
    if (it == d->m_animations.end())
        it = d->m_animations.insert(w, QPair<QList<AniData>, QRect>(QList<AniData>(), QRect()));

    FullScreenEffectLockPtr fullscreen;
    if (fullScreenEffect) {
        if (d->m_fullScreenEffectLock.isNull()) {
            fullscreen = FullScreenEffectLockPtr::create(this);
            d->m_fullScreenEffectLock = fullscreen.toWeakRef();
        } else {
            fullscreen = d->m_fullScreenEffectLock.toStrongRef();
        }
    }

    PreviousWindowPixmapLockPtr previousPixmap;
    if (a == CrossFadePrevious) {
        previousPixmap = PreviousWindowPixmapLockPtr::create(w);
    }

    it->first.append(AniData(
        a,              // Attribute
        meta,           // Metadata
        to,             // Target
        delay,          // Delay
        from,           // Source
        waitAtSource,   // Whether the animation should be kept at source
        fullscreen,     // Full screen effect lock
        keepAlive,      // Keep alive flag
        previousPixmap  // Previous window pixmap lock
    ));

    const quint64 ret_id = ++d->m_animCounter;
    AniData &animation = it->first.last();
    animation.id = ret_id;

    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration(std::chrono::milliseconds(ms));
    animation.timeLine.setEasingCurve(curve);
    animation.timeLine.setSourceRedirectMode(TimeLine::RedirectMode::Strict);
    animation.timeLine.setTargetRedirectMode(TimeLine::RedirectMode::Relaxed);

    animation.terminationFlags = TerminateAtSource;
    if (!keepAtTarget) {
        animation.terminationFlags |= TerminateAtTarget;
    }

    it->second = QRect();

    d->m_animationsTouched = true;

    if (delay > 0) {
        QTimer::singleShot(delay, this, &AnimationEffect::triggerRepaint);
        const QSize &s = effects->virtualScreenSize();
        if (waitAtSource)
            w->addLayerRepaint(0, 0, s.width(), s.height());
    }
    else {
        triggerRepaint();
    }
    return ret_id;
}

bool AnimationEffect::retarget(quint64 animationId, FPx2 newTarget, int newRemainingTime)
{
    Q_D(AnimationEffect);
    if (animationId == d->m_justEndedAnimation)
        return false; // this is just ending, do not try to retarget it
    for (AniMap::iterator entry = d->m_animations.begin(),
                         mapEnd = d->m_animations.end(); entry != mapEnd; ++entry) {
        for (QList<AniData>::iterator anim = entry->first.begin(),
                                   animEnd = entry->first.end(); anim != animEnd; ++anim) {
            if (anim->id == animationId) {
                anim->from.set(interpolated(*anim, 0), interpolated(*anim, 1));
                validate(anim->attribute, anim->meta, nullptr, &newTarget, entry.key());
                anim->to.set(newTarget[0], newTarget[1]);

                anim->timeLine.setDirection(TimeLine::Forward);
                anim->timeLine.setDuration(std::chrono::milliseconds(newRemainingTime));
                anim->timeLine.reset();

                return true;
            }
        }
    }
    return false; // no animation found
}

bool AnimationEffect::redirect(quint64 animationId, Direction direction, TerminationFlags terminationFlags)
{
    Q_D(AnimationEffect);

    if (animationId == d->m_justEndedAnimation) {
        return false;
    }

    for (auto entryIt = d->m_animations.begin(); entryIt != d->m_animations.end(); ++entryIt) {
        auto animIt = std::find_if(entryIt->first.begin(), entryIt->first.end(),
            [animationId] (AniData &anim) {
                return anim.id == animationId;
            }
        );
        if (animIt == entryIt->first.end()) {
            continue;
        }

        switch (direction) {
        case Backward:
            animIt->timeLine.setDirection(TimeLine::Backward);
            break;

        case Forward:
            animIt->timeLine.setDirection(TimeLine::Forward);
            break;
        }

        animIt->terminationFlags = terminationFlags & ~TerminateAtTarget;

        return true;
    }

    return false;
}

bool AnimationEffect::complete(quint64 animationId)
{
    Q_D(AnimationEffect);

    if (animationId == d->m_justEndedAnimation) {
        return false;
    }

    for (auto entryIt = d->m_animations.begin(); entryIt != d->m_animations.end(); ++entryIt) {
        auto animIt = std::find_if(entryIt->first.begin(), entryIt->first.end(),
            [animationId] (AniData &anim) {
                return anim.id == animationId;
            }
        );
        if (animIt == entryIt->first.end()) {
            continue;
        }

        animIt->timeLine.setElapsed(animIt->timeLine.duration());

        return true;
    }

    return false;
}

bool AnimationEffect::cancel(quint64 animationId)
{
    Q_D(AnimationEffect);
    if (animationId == d->m_justEndedAnimation)
        return true; // this is just ending, do not try to cancel it but fake success
    for (AniMap::iterator entry = d->m_animations.begin(), mapEnd = d->m_animations.end(); entry != mapEnd; ++entry) {
        for (QList<AniData>::iterator anim = entry->first.begin(), animEnd = entry->first.end(); anim != animEnd; ++anim) {
            if (anim->id == animationId) {
                entry->first.erase(anim); // remove the animation
                if (entry->first.isEmpty()) { // no other animations on the window, release it.
                    d->m_animations.erase(entry);
                }
                if (d->m_animations.isEmpty())
                    disconnectGeometryChanges();
                d->m_animationsTouched = true; // could be called from animationEnded
                return true;
            }
        }
    }
    return false;
}

void AnimationEffect::prePaintScreen( ScreenPrePaintData& data, int time )
{
    Q_D(AnimationEffect);
    if (d->m_animations.isEmpty()) {
        effects->prePaintScreen(data, time);
        return;
    }

    d->m_animationsTouched = false;
    AniMap::iterator entry = d->m_animations.begin(), mapEnd = d->m_animations.end();
    d->m_animated = false;
//     short int transformed = 0;
    while (entry != mapEnd) {
        bool invalidateLayerRect = false;
        QList<AniData>::iterator anim = entry->first.begin(), animEnd = entry->first.end();
        int animCounter = 0;
        while (anim != animEnd) {
            if (anim->startTime > clock()) {
                if (!anim->waitAtSource) {
                    ++anim;
                    ++animCounter;
                    continue;
                }
            } else {
                anim->timeLine.update(std::chrono::milliseconds(time));
            }

            if (anim->isActive()) {
//                 if (anim->attribute != Brightness && anim->attribute != Saturation && anim->attribute != Opacity)
//                     transformed = true;
                d->m_animated = true;
                ++anim;
                ++animCounter;
            } else {
                EffectWindow *oldW = entry.key();
                d->m_justEndedAnimation = anim->id;
                animationEnded(oldW, anim->attribute, anim->meta);
                d->m_justEndedAnimation = 0;
                // NOTICE animationEnded is an external call and might have called "::animate"
                // as a result our iterators could now point random junk on the heap
                // so we've to restore the former states, ie. find our window list and animation
                if (d->m_animationsTouched) {
                    d->m_animationsTouched = false;
                    entry = d->m_animations.begin(), mapEnd = d->m_animations.end();
                    while (entry.key() != oldW && entry != mapEnd)
                        ++entry;
                    Q_ASSERT(entry != mapEnd); // usercode should not delete animations from animationEnded (not even possible atm.)
                    anim = entry->first.begin(), animEnd = entry->first.end();
                    Q_ASSERT(animCounter < entry->first.count());
                    for (int i = 0; i < animCounter; ++i)
                        ++anim;
                }
                anim = entry->first.erase(anim);
                invalidateLayerRect = d->m_damageDirty = true;
                animEnd = entry->first.end();
            }
        }
        if (entry->first.isEmpty()) {
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

    // janitorial...
    if (d->m_animations.isEmpty()) {
        disconnectGeometryChanges();
    }

    effects->prePaintScreen(data, time);
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

QRect AnimationEffect::clipRect(const QRect &geo, const AniData &anim) const
{
    QRect clip = geo;
    FPx2 ratio = anim.from + progress(anim) * (anim.to - anim.from);
    if (anim.from[0] < 1.0 || anim.to[0] < 1.0) {
        clip.setWidth(clip.width() * ratio[0]);
    }
    if (anim.from[1] < 1.0 || anim.to[1] < 1.0) {
        clip.setHeight(clip.height() * ratio[1]);
    }
    const QRect center = geo.adjusted(clip.width()/2, clip.height()/2,
                                        -(clip.width()+1)/2, -(clip.height()+1)/2 );
    const int x[2] = {  xCoord(center, metaData(SourceAnchor, anim.meta)),
                        xCoord(center, metaData(TargetAnchor, anim.meta)) };
    const int y[2] = {  yCoord(center, metaData(SourceAnchor, anim.meta)),
                        yCoord(center, metaData(TargetAnchor, anim.meta)) };
    const QPoint d(x[0] + ratio[0]*(x[1]-x[0]), y[0] + ratio[1]*(y[1]-y[0]));
    clip.moveTopLeft(QPoint(d.x() - clip.width()/2, d.y() - clip.height()/2));
    return clip;
}

void AnimationEffect::clipWindow(const EffectWindow *w, const AniData &anim, WindowQuadList &quads) const
{
    return;
    const QRect geo = w->expandedGeometry();
    QRect clip = AnimationEffect::clipRect(geo, anim);
    WindowQuadList filtered;
    if (clip.left() != geo.left()) {
        quads = quads.splitAtX(clip.left());
        foreach (const WindowQuad &quad, quads) {
            if (quad.right() >= clip.left())
                filtered << quad;
        }
        quads = filtered;
        filtered.clear();
    }
    if (clip.right() != geo.right()) {
        quads = quads.splitAtX(clip.left());
        foreach (const WindowQuad &quad, quads) {
            if (quad.right() <= clip.right())
                filtered << quad;
        }
        quads = filtered;
        filtered.clear();
    }
    if (clip.top() != geo.top()) {
        quads = quads.splitAtY(clip.top());
        foreach (const WindowQuad &quad, quads) {
            if (quad.top() >= clip.top())
                filtered << quad;
        }
        quads = filtered;
        filtered.clear();
    }
    if (clip.bottom() != geo.bottom()) {
        quads = quads.splitAtY(clip.bottom());
        foreach (const WindowQuad &quad, quads) {
            if (quad.bottom() <= clip.bottom())
                filtered << quad;
        }
        quads = filtered;
    }
}

void AnimationEffect::disconnectGeometryChanges()
{
    disconnect(effects, &EffectsHandler::windowGeometryShapeChanged,
        this, &AnimationEffect::_expandedGeometryChanged);
    disconnect(effects, &EffectsHandler::windowStepUserMovedResized,
        this, &AnimationEffect::_expandedGeometryChanged);
    disconnect(effects, &EffectsHandler::windowPaddingChanged,
        this, &AnimationEffect::_expandedGeometryChanged);
}


void AnimationEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
{
    Q_D(AnimationEffect);
    if ( d->m_animated ) {
        AniMap::const_iterator entry = d->m_animations.constFind( w );
        if ( entry != d->m_animations.constEnd() ) {
            bool isUsed = false;
            bool paintDeleted = false;
            for (QList<AniData>::const_iterator anim = entry->first.constBegin(); anim != entry->first.constEnd(); ++anim) {
                if (anim->startTime > clock() && !anim->waitAtSource)
                    continue;

                isUsed = true;
                if (anim->attribute == Opacity || anim->attribute == CrossFadePrevious)
                    data.setTranslucent();
                else if (!(anim->attribute == Brightness || anim->attribute == Saturation)) {
                    data.setTransformed();
                    if (anim->attribute == Clip)
                        clipWindow(w, *anim, data.quads);
                }

                paintDeleted |= anim->keepAlive;
            }
            if ( isUsed ) {
                if ( w->isMinimized() )
                    w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
                else if ( w->isDeleted() && paintDeleted )
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

void AnimationEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
{
    Q_D(AnimationEffect);
    if ( d->m_animated ) {
        AniMap::const_iterator entry = d->m_animations.constFind( w );
        if ( entry != d->m_animations.constEnd() ) {
            for ( QList<AniData>::const_iterator anim = entry->first.constBegin(); anim != entry->first.constEnd(); ++anim ) {

                if (anim->startTime > clock() && !anim->waitAtSource)
                    continue;

                switch (anim->attribute) {
                case Opacity:
                    data.multiplyOpacity(interpolated(*anim)); break;
                case Brightness:
                    data.multiplyBrightness(interpolated(*anim)); break;
                case Saturation:
                    data.multiplySaturation(interpolated(*anim)); break;
                case Scale: {
                    const QSize sz = w->geometry().size();
                    float f1(1.0), f2(0.0);
                    if (anim->from[0] >= 0.0 && anim->to[0] >= 0.0) { // scale x
                        f1 = interpolated(*anim, 0);
                        f2 = geometryCompensation( anim->meta & AnimationEffect::Horizontal, f1 );
                        data.translate(f2 * sz.width());
                        data.setXScale(data.xScale() * f1);
                    }
                    if (anim->from[1] >= 0.0 && anim->to[1] >= 0.0) { // scale y
                        if (!anim->isOneDimensional()) {
                            f1 = interpolated(*anim, 1);
                            f2 = geometryCompensation( anim->meta & AnimationEffect::Vertical, f1 );
                        }
                        else if ( ((anim->meta & AnimationEffect::Vertical)>>1) != (anim->meta & AnimationEffect::Horizontal) )
                            f2 = geometryCompensation( anim->meta & AnimationEffect::Vertical, f1 );
                        data.translate(0.0, f2 * sz.height());
                        data.setYScale(data.yScale() * f1);
                    }
                    break;
                }
                case Clip:
                    region = clipRect(w->expandedGeometry(), *anim);
                    break;
                case Translation:
                    data += QPointF(interpolated(*anim, 0), interpolated(*anim, 1));
                    break;
                case Size: {
                    FPx2 dest = anim->from + progress(*anim) * (anim->to - anim->from);
                    const QSize sz = w->geometry().size();
                    float f;
                    if (anim->from[0] >= 0.0 && anim->to[0] >= 0.0) { // resize x
                        f = dest[0]/sz.width();
                        data.translate(geometryCompensation( anim->meta & AnimationEffect::Horizontal, f ) * sz.width());
                        data.setXScale(data.xScale() * f);
                    }
                    if (anim->from[1] >= 0.0 && anim->to[1] >= 0.0) { // resize y
                        f = dest[1]/sz.height();
                        data.translate(0.0, geometryCompensation( anim->meta & AnimationEffect::Vertical, f ) * sz.height());
                        data.setYScale(data.yScale() * f);
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
                        data.translate(dest - (x[0] + prgrs*(x[1] - x[0])));
                    }
                    if ( anim->from[1] >= 0.0 && anim->to[1] >= 0.0 ) {
                        float dest = interpolated(*anim, 1);
                        const int y[2] = {  yCoord(geo, metaData(SourceAnchor, anim->meta)),
                                            yCoord(geo, metaData(TargetAnchor, anim->meta)) };
                        data.translate(0.0, dest - (y[0] + prgrs*(y[1] - y[0])));
                    }
                    break;
                }
                case Rotation: {
                    data.setRotationAxis((Qt::Axis)metaData(Axis, anim->meta));
                    const float prgrs = progress(*anim);
                    data.setRotationAngle(anim->from[0] + prgrs*(anim->to[0] - anim->from[0]));

                    const QRect geo = w->rect();
                    const uint  sAnchor = metaData(SourceAnchor, anim->meta),
                                tAnchor = metaData(TargetAnchor, anim->meta);
                    QPointF pt(xCoord(geo, sAnchor), yCoord(geo, sAnchor));

                    if (tAnchor != sAnchor) {
                        QPointF pt2(xCoord(geo, tAnchor), yCoord(geo, tAnchor));
                        pt += static_cast<qreal>(prgrs)*(pt2 - pt);
                    }
                    data.setRotationOrigin(QVector3D(pt));
                    break;
                }
                case Generic:
                    genericAnimation(w, data, progress(*anim), anim->meta);
                    break;
                case CrossFadePrevious:
                    data.setCrossFadeProgress(progress(*anim));
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
                bool addRepaint = false;
                QList<AniData>::const_iterator anim = it->first.constBegin();
                for (; anim != it->first.constEnd(); ++anim) {
                    if (anim->startTime > clock())
                        continue;
                    if (!anim->timeLine.done()) {
                        addRepaint = true;
                        break;
                    }
                }
                if (addRepaint) {
                    it.key()->addLayerRepaint(it->second);
                }
            }
        }
    }
    effects->postPaintScreen();
}

float AnimationEffect::interpolated( const AniData &a, int i ) const
{
    if (a.startTime > clock())
        return a.from[i];
    if (!a.timeLine.done())
        return a.from[i] + a.timeLine.value() * (a.to[i] - a.from[i]);
    return a.to[i]; // we're done and "waiting" at the target value
}

float AnimationEffect::progress( const AniData &a ) const
{
    return a.startTime < clock() ? a.timeLine.value() : 0.0;
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
        for (; it != end; ++it) {
            it.key()->addLayerRepaint(it->second);
        }
    }
}

static float fixOvershoot(float f, const AniData &d, short int dir, float s = 1.1)
{
    switch(d.timeLine.easingCurve().type()) {
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
            if (anim->startTime > clock())
                continue;
            switch (anim->attribute) {
                case Opacity:
                case Brightness:
                case Saturation:
                case CrossFadePrevious:
                    createRegion = true;
                    break;
                case Rotation:
                    createRegion = false;
                    *layerRect = QRect(QPoint(0, 0), effects->virtualScreenSize());
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
                case Clip:
                    createRegion = true;
                    break;
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
    Q_UNUSED(old)
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

    auto it = d->m_animations.find(w);
    if (it == d->m_animations.end()) {
        return;
    }

    KeepAliveLockPtr keepAliveLock;

    QList<AniData> &animations = (*it).first;
    for (auto animationIt = animations.begin();
            animationIt != animations.end();
            ++animationIt) {
        if (!(*animationIt).keepAlive) {
            continue;
        }

        if (keepAliveLock.isNull()) {
            keepAliveLock = KeepAliveLockPtr::create(w);
        }

        (*animationIt).keepAliveLock = keepAliveLock;
    }
}

void AnimationEffect::_windowDeleted( EffectWindow* w )
{
    Q_D(AnimationEffect);
    d->m_animations.remove( w );
}


QString AnimationEffect::debug(const QString &/*parameter*/) const
{
    Q_D(const AnimationEffect);
    QString dbg;
    if (d->m_animations.isEmpty())
        dbg = QStringLiteral("No window is animated");
    else {
        AniMap::const_iterator entry = d->m_animations.constBegin(), mapEnd = d->m_animations.constEnd();
        for (; entry != mapEnd; ++entry) {
            QString caption = entry.key()->isDeleted() ? QStringLiteral("[Deleted]") : entry.key()->caption();
            if (caption.isEmpty())
                caption = QStringLiteral("[Untitled]");
            dbg += QLatin1String("Animating window: ") + caption + QLatin1Char('\n');
            QList<AniData>::const_iterator anim = entry->first.constBegin(), animEnd = entry->first.constEnd();
            for (; anim != animEnd; ++anim)
                dbg += anim->debugInfo();
        }
    }
    return dbg;
}

AnimationEffect::AniMap AnimationEffect::state() const
{
    Q_D(const AnimationEffect);
    return d->m_animations;
}

} // namespace KWin

#include "moc_kwinanimationeffect.cpp"
