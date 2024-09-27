/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Thomas Lübking <thomas.luebking@web.de>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/animationeffect.h"
#include "effect/anidata_p.h"
#include "effect/effecthandler.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "scene/windowitem.h"

#include <QDateTime>
#include <QTimer>
#include <QVector3D>
#include <QtDebug>

namespace KWin
{

QDebug operator<<(QDebug dbg, const KWin::FPx2 &fpx2)
{
    dbg.nospace() << fpx2[0] << "," << fpx2[1] << QString(fpx2.isValid() ? QStringLiteral(" (valid)") : QStringLiteral(" (invalid)"));
    return dbg.space();
}

QElapsedTimer AnimationEffect::s_clock;

class AnimationEffectPrivate
{
public:
    AnimationEffectPrivate()
    {
        m_animationsTouched = m_isInitialized = false;
        m_justEndedAnimation = 0;
    }
    AnimationEffect::AniMap m_animations;
    static quint64 m_animCounter;
    quint64 m_justEndedAnimation; // protect against cancel
    std::weak_ptr<FullScreenEffectLock> m_fullScreenEffectLock;
    bool m_needSceneRepaint, m_animationsTouched, m_isInitialized;
};

quint64 AnimationEffectPrivate::m_animCounter = 0;

AnimationEffect::AnimationEffect()
    : CrossFadeEffect()
    , d(std::make_unique<AnimationEffectPrivate>())
{
    if (!s_clock.isValid()) {
        s_clock.start();
    }
    /* this is the same as the QTimer::singleShot(0, SLOT(init())) kludge
     * defering the init and esp. the connection to the windowClosed slot */
    QMetaObject::invokeMethod(this, &AnimationEffect::init, Qt::QueuedConnection);
}

AnimationEffect::~AnimationEffect()
{
    if (d->m_isInitialized) {
        disconnect(effects, &EffectsHandler::windowDeleted, this, &AnimationEffect::_windowDeleted);
    }
    d->m_animations.clear();
}

void AnimationEffect::init()
{
    if (d->m_isInitialized) {
        return; // not more than once, please
    }
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
    return !d->m_animations.empty() && !effects->isScreenLocked();
}

#define RELATIVE_XY(_FIELD_) const bool relative[2] = {static_cast<bool>(metaData(Relative##_FIELD_##X, meta)), \
                                                       static_cast<bool>(metaData(Relative##_FIELD_##Y, meta))}

void AnimationEffect::validate(Attribute a, uint &meta, FPx2 *from, FPx2 *to, const EffectWindow *w) const
{
    if (a < NonFloatBase) {
        if (a == Scale) {
            QRectF area = effects->clientArea(ScreenArea, w);
            if (from && from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? (*from)[0] * area.width() / w->width() : (*from)[0],
                          relative[1] ? (*from)[1] * area.height() / w->height() : (*from)[1]);
            }
            if (to && to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? (*to)[0] * area.width() / w->width() : (*to)[0],
                        relative[1] ? (*to)[1] * area.height() / w->height() : (*to)[1]);
            }
        } else if (a == Rotation) {
            if (from && !from->isValid()) {
                setMetaData(SourceAnchor, metaData(TargetAnchor, meta), meta);
                from->set(0.0, 0.0);
            }
            if (to && !to->isValid()) {
                setMetaData(TargetAnchor, metaData(SourceAnchor, meta), meta);
                to->set(0.0, 0.0);
            }
        }
        if (from && !from->isValid()) {
            from->set(1.0, 1.0);
        }
        if (to && !to->isValid()) {
            to->set(1.0, 1.0);
        }

    } else if (a == Position) {
        QRectF area = effects->clientArea(ScreenArea, w);
        QPointF pt = w->frameGeometry().bottomRight(); // cannot be < 0 ;-)
        if (from) {
            if (from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? area.x() + (*from)[0] * area.width() : (*from)[0],
                          relative[1] ? area.y() + (*from)[1] * area.height() : (*from)[1]);
            } else {
                from->set(pt.x(), pt.y());
                setMetaData(SourceAnchor, AnimationEffect::Bottom | AnimationEffect::Right, meta);
            }
        }

        if (to) {
            if (to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? area.x() + (*to)[0] * area.width() : (*to)[0],
                        relative[1] ? area.y() + (*to)[1] * area.height() : (*to)[1]);
            } else {
                to->set(pt.x(), pt.y());
                setMetaData(TargetAnchor, AnimationEffect::Bottom | AnimationEffect::Right, meta);
            }
        }

    } else if (a == Size) {
        QRectF area = effects->clientArea(ScreenArea, w);
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
        QRect area = w->rect().toRect();
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
            from->set(1.0, 1.0);
            setMetaData(SourceAnchor, metaData(TargetAnchor, meta), meta);
        }
        if (to && !to->isValid()) {
            to->set(1.0, 1.0);
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

quint64 AnimationEffect::p_animate(EffectWindow *w, Attribute a, uint meta, int ms, FPx2 to, const QEasingCurve &curve, int delay, FPx2 from, bool keepAtTarget, bool fullScreenEffect, bool keepAlive, GLShader *shader)
{
    const bool waitAtSource = from.isValid();
    validate(a, meta, &from, &to, w);

    if (!d->m_isInitialized) {
        init(); // needs to ensure the window gets removed if deleted in the same event cycle
    }
    AniMap::iterator it = d->m_animations.find(w);
    if (it == d->m_animations.end()) {
        connect(w, &EffectWindow::windowExpandedGeometryChanged,
                this, &AnimationEffect::_windowExpandedGeometryChanged);
        it = d->m_animations.emplace(std::make_pair(w, std::pair<std::vector<AniData>, QRect>{})).first;
    }
    auto &[animations, rect] = it->second;

    std::shared_ptr<FullScreenEffectLock> fullscreen;
    if (fullScreenEffect) {
        fullscreen = d->m_fullScreenEffectLock.lock();
        if (!fullscreen) {
            fullscreen = std::make_shared<FullScreenEffectLock>(this);
            d->m_fullScreenEffectLock = fullscreen;
        }
    }

    if (a == CrossFadePrevious) {
        CrossFadeEffect::redirect(w);
    }

    animations.push_back(AniData(
        a, // Attribute
        meta, // Metadata
        to, // Target
        delay, // Delay
        from, // Source
        waitAtSource, // Whether the animation should be kept at source
        fullscreen, // Full screen effect lock
        keepAlive, // Keep alive flag
        shader));

    const quint64 ret_id = ++d->m_animCounter;
    AniData &animation = animations.back();
    animation.id = ret_id;

    animation.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_MINIMIZE | EffectWindow::PAINT_DISABLED_BY_DESKTOP | EffectWindow::PAINT_DISABLED);
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration(std::chrono::milliseconds(ms));
    animation.timeLine.setEasingCurve(curve);
    animation.timeLine.setSourceRedirectMode(TimeLine::RedirectMode::Strict);
    animation.timeLine.setTargetRedirectMode(TimeLine::RedirectMode::Relaxed);
    animation.itemEffect = ItemEffect(w->windowItem());

    animation.terminationFlags = TerminateAtSource;
    if (!keepAtTarget) {
        animation.terminationFlags |= TerminateAtTarget;
    }

    rect = QRect();

    d->m_animationsTouched = true;

    if (delay > 0) {
        QTimer::singleShot(delay, this, &AnimationEffect::triggerRepaint);
        const QSize &s = effects->virtualScreenSize();
        if (waitAtSource) {
            w->addLayerRepaint(0, 0, s.width(), s.height());
        }
    } else {
        triggerRepaint();
    }
    if (shader) {
        CrossFadeEffect::redirect(w);
    }
    return ret_id;
}

bool AnimationEffect::retarget(quint64 animationId, FPx2 newTarget, int newRemainingTime)
{
    if (animationId == d->m_justEndedAnimation) {
        return false; // this is just ending, do not try to retarget it
    }
    for (auto &[window, pair] : d->m_animations) {
        auto &[animations, rect] = pair;
        const auto anim = std::ranges::find_if(animations, [animationId](const auto &anim) {
            return anim.id == animationId;
        });
        if (anim != animations.end()) {
            anim->from.set(interpolated(*anim, 0), interpolated(*anim, 1));
            validate(anim->attribute, anim->meta, nullptr, &newTarget, window);
            anim->to.set(newTarget[0], newTarget[1]);

            anim->timeLine.setDirection(TimeLine::Forward);
            anim->timeLine.setDuration(std::chrono::milliseconds(newRemainingTime));
            anim->timeLine.reset();

            if (anim->attribute == CrossFadePrevious) {
                CrossFadeEffect::redirect(window);
            }
            return true;
        }
    }
    return false;
}

bool AnimationEffect::freezeInTime(quint64 animationId, qint64 frozenTime)
{
    if (animationId == d->m_justEndedAnimation) {
        return false; // this is just ending, do not try to retarget it
    }
    for (auto &[window, pair] : d->m_animations) {
        auto &[animations, rect] = pair;
        const auto anim = std::ranges::find_if(animations, [animationId](const auto &anim) {
            return anim.id == animationId;
        });
        if (anim != animations.end()) {
            if (frozenTime >= 0) {
                anim->timeLine.setElapsed(std::chrono::milliseconds(frozenTime));
            }
            anim->frozenTime = frozenTime;
            return true;
        }
    }
    return false;
}

bool AnimationEffect::redirect(quint64 animationId, Direction direction, TerminationFlags terminationFlags)
{
    if (animationId == d->m_justEndedAnimation) {
        return false;
    }
    for (auto &[window, pair] : d->m_animations) {
        auto &[animations, rect] = pair;
        const auto anim = std::ranges::find_if(animations, [animationId](const auto &anim) {
            return anim.id == animationId;
        });
        if (anim != animations.end()) {
            switch (direction) {
            case Backward:
                anim->timeLine.setDirection(TimeLine::Backward);
                break;
            case Forward:
                anim->timeLine.setDirection(TimeLine::Forward);
                break;
            }
            anim->terminationFlags = terminationFlags & ~TerminateAtTarget;
            return true;
        }
    }
    return false;
}

bool AnimationEffect::complete(quint64 animationId)
{
    if (animationId == d->m_justEndedAnimation) {
        return false;
    }
    for (auto &[window, pair] : d->m_animations) {
        auto &[animations, rect] = pair;
        const auto anim = std::ranges::find_if(animations, [animationId](const auto &anim) {
            return anim.id == animationId;
        });
        if (anim != animations.end()) {
            anim->timeLine.setElapsed(anim->timeLine.duration());
            unredirect(window);
            return true;
        }
    }
    return false;
}

bool AnimationEffect::cancel(quint64 animationId)
{
    if (animationId == d->m_justEndedAnimation) {
        return true; // this is just ending, do not try to cancel it but fake success
    }
    for (auto &[window, pair] : d->m_animations) {
        auto &[animations, rect] = pair;
        const auto anim = std::ranges::find_if(animations, [animationId](const auto &anim) {
            return anim.id == animationId;
        });
        if (anim != animations.end()) {
            EffectWindowDeletedRef ref = std::move(anim->deletedRef); // delete window once we're done updating m_animations
            if (std::ranges::none_of(animations, [animationId](const auto &anim) {
                return anim.id != animationId && (anim.shader || anim.attribute == AnimationEffect::CrossFadePrevious);
            })) {
                unredirect(window);
            }
            animations.erase(anim);
            if (animations.empty()) { // no other animations on the window, release it.
                disconnect(window, &EffectWindow::windowExpandedGeometryChanged,
                           this, &AnimationEffect::_windowExpandedGeometryChanged);
                d->m_animations.erase(window);
            }
            d->m_animationsTouched = true; // could be called from animationEnded
            return true;
        }
    }
    return false;
}

void AnimationEffect::animationEnded(EffectWindow *w, Attribute a, uint meta)
{
}

void AnimationEffect::genericAnimation(EffectWindow *w, WindowPaintData &data, float progress, uint meta)
{
}

static qreal xCoord(const QRectF &r, int flag)
{
    if (flag & AnimationEffect::Left) {
        return r.x();
    } else if (flag & AnimationEffect::Right) {
        return r.right();
    } else {
        return r.x() + r.width() / 2;
    }
}

static qreal yCoord(const QRectF &r, int flag)
{
    if (flag & AnimationEffect::Top) {
        return r.y();
    } else if (flag & AnimationEffect::Bottom) {
        return r.bottom();
    } else {
        return r.y() + r.height() / 2;
    }
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
    const QRect center = geo.adjusted(clip.width() / 2, clip.height() / 2,
                                      -(clip.width() + 1) / 2, -(clip.height() + 1) / 2);
    const qreal x[2] = {xCoord(center, metaData(SourceAnchor, anim.meta)),
                        xCoord(center, metaData(TargetAnchor, anim.meta))};
    const qreal y[2] = {yCoord(center, metaData(SourceAnchor, anim.meta)),
                        yCoord(center, metaData(TargetAnchor, anim.meta))};
    const QPoint d(x[0] + ratio[0] * (x[1] - x[0]), y[0] + ratio[1] * (y[1] - y[0]));
    clip.moveTopLeft(QPoint(d.x() - clip.width() / 2, d.y() - clip.height() / 2));
    return clip;
}

void AnimationEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto entry = d->m_animations.find(w);
    if (entry != d->m_animations.end()) {
        auto &[window, pair] = *entry;
        auto &[list, rect] = pair;
        for (auto &anim : list) {
            if (anim.startTime > clock() && !anim.waitAtSource) {
                continue;
            }

            if (anim.frozenTime < 0) {
                anim.timeLine.advance(presentTime);
            }

            if (anim.attribute == Opacity || anim.attribute == CrossFadePrevious) {
                data.setTranslucent();
            } else if (!(anim.attribute == Brightness || anim.attribute == Saturation)) {
                data.setTransformed();
            }
        }
    }
    effects->prePaintWindow(w, data, presentTime);
}

static inline float geometryCompensation(int flags, float v)
{
    if (flags & (AnimationEffect::Left | AnimationEffect::Top)) {
        return 0.0; // no compensation required
    }
    if (flags & (AnimationEffect::Right | AnimationEffect::Bottom)) {
        return 1.0 - v; // full compensation
    }
    return 0.5 * (1.0 - v); // half compensation
}

void AnimationEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    auto it = d->m_animations.find(w);
    if (it == d->m_animations.end()) {
        effects->paintWindow(renderTarget, viewport, w, mask, region, data);
        return;
    }
    auto &[window, pair] = *it;
    auto &[list, rect] = pair;
    for (auto &anim : list) {
        if (anim.startTime > clock() && !anim.waitAtSource) {
            continue;
        }

        switch (anim.attribute) {
        case Opacity:
            data.multiplyOpacity(interpolated(anim));
            break;
        case Brightness:
            data.multiplyBrightness(interpolated(anim));
            break;
        case Saturation:
            data.multiplySaturation(interpolated(anim));
            break;
        case Scale: {
            const QSizeF sz = w->frameGeometry().size();
            float f1(1.0), f2(0.0);
            if (anim.from[0] >= 0.0 && anim.to[0] >= 0.0) { // scale x
                f1 = interpolated(anim, 0);
                f2 = geometryCompensation(anim.meta & AnimationEffect::Horizontal, f1);
                data.translate(f2 * sz.width());
                data.setXScale(data.xScale() * f1);
            }
            if (anim.from[1] >= 0.0 && anim.to[1] >= 0.0) { // scale y
                if (!anim.isOneDimensional()) {
                    f1 = interpolated(anim, 1);
                    f2 = geometryCompensation(anim.meta & AnimationEffect::Vertical, f1);
                } else if (((anim.meta & AnimationEffect::Vertical) >> 1) != (anim.meta & AnimationEffect::Horizontal)) {
                    f2 = geometryCompensation(anim.meta & AnimationEffect::Vertical, f1);
                }
                data.translate(0.0, f2 * sz.height());
                data.setYScale(data.yScale() * f1);
            }
            break;
        }
        case Clip:
            region = clipRect(w->expandedGeometry().toAlignedRect(), anim);
            break;
        case Translation:
            data += QPointF(interpolated(anim, 0), interpolated(anim, 1));
            break;
        case Size: {
            FPx2 dest = anim.from + progress(anim) * (anim.to - anim.from);
            const QSizeF sz = w->frameGeometry().size();
            float f;
            if (anim.from[0] >= 0.0 && anim.to[0] >= 0.0) { // resize x
                f = dest[0] / sz.width();
                data.translate(geometryCompensation(anim.meta & AnimationEffect::Horizontal, f) * sz.width());
                data.setXScale(data.xScale() * f);
            }
            if (anim.from[1] >= 0.0 && anim.to[1] >= 0.0) { // resize y
                f = dest[1] / sz.height();
                data.translate(0.0, geometryCompensation(anim.meta & AnimationEffect::Vertical, f) * sz.height());
                data.setYScale(data.yScale() * f);
            }
            break;
        }
        case Position: {
            const QRectF geo = w->frameGeometry();
            const float prgrs = progress(anim);
            if (anim.from[0] >= 0.0 && anim.to[0] >= 0.0) {
                float dest = interpolated(anim, 0);
                const qreal x[2] = {xCoord(geo, metaData(SourceAnchor, anim.meta)),
                                    xCoord(geo, metaData(TargetAnchor, anim.meta))};
                data.translate(dest - (x[0] + prgrs * (x[1] - x[0])));
            }
            if (anim.from[1] >= 0.0 && anim.to[1] >= 0.0) {
                float dest = interpolated(anim, 1);
                const qreal y[2] = {yCoord(geo, metaData(SourceAnchor, anim.meta)),
                                    yCoord(geo, metaData(TargetAnchor, anim.meta))};
                data.translate(0.0, dest - (y[0] + prgrs * (y[1] - y[0])));
            }
            break;
        }
        case Rotation: {
            data.setRotationAxis((Qt::Axis)metaData(Axis, anim.meta));
            const float prgrs = progress(anim);
            data.setRotationAngle(anim.from[0] + prgrs * (anim.to[0] - anim.from[0]));

            const QRect geo = w->rect().toRect();
            const uint sAnchor = metaData(SourceAnchor, anim.meta),
                       tAnchor = metaData(TargetAnchor, anim.meta);
            QPointF pt(xCoord(geo, sAnchor), yCoord(geo, sAnchor));

            if (tAnchor != sAnchor) {
                QPointF pt2(xCoord(geo, tAnchor), yCoord(geo, tAnchor));
                pt += static_cast<qreal>(prgrs) * (pt2 - pt);
            }
            data.setRotationOrigin(QVector3D(pt));
            break;
        }
        case Generic:
            genericAnimation(w, data, progress(anim), anim.meta);
            break;
        case CrossFadePrevious:
            data.setCrossFadeProgress(progress(anim));
            break;
        case Shader:
            if (anim.shader && anim.shader->isValid()) {
                ShaderBinder binder{anim.shader};
                anim.shader->setUniform("animationProgress", progress(anim));
                setShader(w, anim.shader);
            }
            break;
        case ShaderUniform:
            if (anim.shader && anim.shader->isValid()) {
                ShaderBinder binder{anim.shader};
                anim.shader->setUniform("animationProgress", progress(anim));
                anim.shader->setUniform(anim.meta, interpolated(anim));
                setShader(w, anim.shader);
            }
            break;
        default:
            break;
        }
    }
    effects->paintWindow(renderTarget, viewport, w, mask, region, data);
}

void AnimationEffect::postPaintScreen()
{
    d->m_animationsTouched = false;
    bool damageDirty = false;
    std::vector<EffectWindowDeletedRef> zombies;

    for (auto entry = d->m_animations.begin(); entry != d->m_animations.end();) {
        bool invalidateLayerRect = false;
        size_t animCounter = 0;
        EffectWindow *const window = entry->first;
        for (auto anim = entry->second.first.begin(); anim != entry->second.first.end();) {
            if (anim->isActive() || (anim->startTime > clock() && !anim->waitAtSource)) {
                ++anim;
                ++animCounter;
                continue;
            }
            d->m_justEndedAnimation = anim->id;
            if (std::ranges::none_of(entry->second.first, [anim](const auto &other) {
                return anim->id != other.id && (other.shader || other.attribute == AnimationEffect::CrossFadePrevious);
            })) {
                unredirect(window);
            }
            animationEnded(window, anim->attribute, anim->meta);
            d->m_justEndedAnimation = 0;
            // NOTICE animationEnded is an external call and might have called "::animate"
            // as a result our iterators could now point random junk on the heap
            // so we've to restore the former states, ie. find our window list and animation
            if (d->m_animationsTouched) {
                d->m_animationsTouched = false;
                entry = std::ranges::find_if(d->m_animations, [window](const auto &pair) {
                    return pair.first == window;
                });
                Q_ASSERT(entry != d->m_animations.end()); // usercode should not delete animations from animationEnded (not even possible atm.)
                Q_ASSERT(animCounter < entry->second.first.size());
                anim = entry->second.first.begin() + animCounter;
            }
            // If it's a closed window, keep it alive for a little bit longer until we're done
            // updating m_animations. Otherwise our windowDeleted slot can access m_animations
            // while we still modify it.
            if (!anim->deletedRef.isNull()) {
                zombies.emplace_back(std::move(anim->deletedRef));
            }
            anim = entry->second.first.erase(anim);
            invalidateLayerRect = damageDirty = true;
        }
        if (entry->second.first.empty()) {
            disconnect(window, &EffectWindow::windowExpandedGeometryChanged,
                       this, &AnimationEffect::_windowExpandedGeometryChanged);
            effects->addRepaint(entry->second.second);
            entry = d->m_animations.erase(entry);
        } else {
            if (invalidateLayerRect) {
                entry->second.second = QRect(); // invalidate
            }
            ++entry;
        }
    }

    if (damageDirty) {
        updateLayerRepaints();
    }
    if (d->m_needSceneRepaint) {
        effects->addRepaintFull();
    } else {
        for (const auto &[window, pair] : d->m_animations) {
            const auto &[data, rect] = pair;
            for (const auto &anim : data) {
                if (anim.startTime > clock()) {
                    continue;
                }
                if (!anim.timeLine.done()) {
                    window->addLayerRepaint(rect);
                    break;
                }
            }
        }
    }

    effects->postPaintScreen();
}

float AnimationEffect::interpolated(const AniData &a, int i) const
{
    return a.from[i] + a.timeLine.value() * (a.to[i] - a.from[i]);
}

float AnimationEffect::progress(const AniData &a) const
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

int AnimationEffect::metaData(MetaType type, uint meta)
{
    switch (type) {
    case SourceAnchor:
        return ((meta >> 5) & 0x1f);
    case TargetAnchor:
        return (meta & 0x1f);
    case RelativeSourceX:
    case RelativeSourceY:
    case RelativeTargetX:
    case RelativeTargetY: {
        const int shift = 10 + type - RelativeSourceX;
        return ((meta >> shift) & 1);
    }
    case Axis:
        return ((meta >> 10) & 3);
    default:
        return 0;
    }
}

void AnimationEffect::setMetaData(MetaType type, uint value, uint &meta)
{
    switch (type) {
    case SourceAnchor:
        meta &= ~(0x1f << 5);
        meta |= ((value & 0x1f) << 5);
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
        if (value) {
            meta |= (1 << shift);
        } else {
            meta &= ~(1 << shift);
        }
        break;
    }
    case Axis:
        meta &= ~(3 << 10);
        meta |= ((value & 3) << 10);
        break;
    default:
        break;
    }
}

void AnimationEffect::triggerRepaint()
{
    for (auto &[window, pair] : d->m_animations) {
        pair.second = QRect();
    }
    updateLayerRepaints();
    if (d->m_needSceneRepaint) {
        effects->addRepaintFull();
    } else {
        for (const auto &[window, pair] : d->m_animations) {
            window->addLayerRepaint(pair.second);
        }
    }
}

static float fixOvershoot(float f, const AniData &d, short int dir, float s = 1.1)
{
    switch (d.timeLine.easingCurve().type()) {
    case QEasingCurve::InOutElastic:
    case QEasingCurve::InOutBack:
        return f * s;
    case QEasingCurve::InElastic:
    case QEasingCurve::OutInElastic:
    case QEasingCurve::OutBack:
        return (dir & 2) ? f * s : f;
    case QEasingCurve::OutElastic:
    case QEasingCurve::InBack:
        return (dir & 1) ? f * s : f;
    default:
        return f;
    }
}

void AnimationEffect::updateLayerRepaints()
{
    d->m_needSceneRepaint = false;
    for (auto &[window, pair] : d->m_animations) {
        auto &[data, rect] = pair;
        if (!rect.isNull()) {
            continue;
        }
        float f[2] = {1.0, 1.0};
        float t[2] = {0.0, 0.0};
        bool createRegion = false;
        QList<QRect> rects;
        for (auto &anim : data) {
            if (anim.startTime > clock()) {
                continue;
            }
            switch (anim.attribute) {
            case Opacity:
            case Brightness:
            case Saturation:
            case CrossFadePrevious:
            case Shader:
            case ShaderUniform:
                createRegion = true;
                break;
            case Rotation:
                createRegion = false;
                rect = QRect(QPoint(0, 0), effects->virtualScreenSize());
                break; // sic! no need to do anything else
            case Generic:
                d->m_needSceneRepaint = true; // we don't know whether this will change visual stacking order
                return; // sic! no need to do anything else
            case Translation:
            case Position: {
                createRegion = true;
                QRect r(window->frameGeometry().toRect());
                int x[2] = {0, 0};
                int y[2] = {0, 0};
                if (anim.attribute == Translation) {
                    x[0] = anim.from[0];
                    x[1] = anim.to[0];
                    y[0] = anim.from[1];
                    y[1] = anim.to[1];
                } else {
                    if (anim.from[0] >= 0.0 && anim.to[0] >= 0.0) {
                        x[0] = anim.from[0] - xCoord(r, metaData(SourceAnchor, anim.meta));
                        x[1] = anim.to[0] - xCoord(r, metaData(TargetAnchor, anim.meta));
                    }
                    if (anim.from[1] >= 0.0 && anim.to[1] >= 0.0) {
                        y[0] = anim.from[1] - yCoord(r, metaData(SourceAnchor, anim.meta));
                        y[1] = anim.to[1] - yCoord(r, metaData(TargetAnchor, anim.meta));
                    }
                }
                r = window->expandedGeometry().toRect();
                rects.push_back(r.translated(x[0], y[0]));
                rects.push_back(r.translated(x[1], y[1]));
                break;
            }
            case Clip:
                createRegion = true;
                break;
            case Size:
            case Scale: {
                createRegion = true;
                const QSize sz = window->frameGeometry().size().toSize();
                float fx = std::max(fixOvershoot(anim.from[0], anim, 1), fixOvershoot(anim.to[0], anim, 2));
                //                     float fx = std::max(interpolated(*anim,0), anim.to[0]);
                if (fx >= 0.0) {
                    if (anim.attribute == Size) {
                        fx /= sz.width();
                    }
                    f[0] *= fx;
                    t[0] += geometryCompensation(anim.meta & AnimationEffect::Horizontal, fx) * sz.width();
                }
                //                     float fy = std::max(interpolated(*anim,1), anim.to[1]);
                float fy = std::max(fixOvershoot(anim.from[1], anim, 1), fixOvershoot(anim.to[1], anim, 2));
                if (fy >= 0.0) {
                    if (anim.attribute == Size) {
                        fy /= sz.height();
                    }
                    if (!anim.isOneDimensional()) {
                        f[1] *= fy;
                        t[1] += geometryCompensation(anim.meta & AnimationEffect::Vertical, fy) * sz.height();
                    } else if (((anim.meta & AnimationEffect::Vertical) >> 1) != (anim.meta & AnimationEffect::Horizontal)) {
                        f[1] *= fx;
                        t[1] += geometryCompensation(anim.meta & AnimationEffect::Vertical, fx) * sz.height();
                    }
                }
                break;
            }
            }
        }
        if (createRegion) {
            const QRect geo = window->expandedGeometry().toRect();
            if (rects.empty()) {
                rects.push_back(geo);
            }
            for (auto &r : rects) { // transform
                r.setSize(QSize(std::round(r.width() * f[0]), std::round(r.height() * f[1])));
                r.translate(t[0], t[1]); // "const_cast" - don't do that at home, kids ;-)
            }
            rect = rects.at(0);
            if (rects.count() > 1) {
                for (const auto &r : rects | std::views::drop(1)) { // unite
                    rect |= r;
                }
                const int dx = 110 * (rect.width() - geo.width()) / 100 + 1 - rect.width() + geo.width();
                const int dy = 110 * (rect.height() - geo.height()) / 100 + 1 - rect.height() + geo.height();
                rect.adjust(-dx, -dy, dx, dy); // fix pot. overshoot
            }
        }
    }
}

void AnimationEffect::_windowExpandedGeometryChanged(KWin::EffectWindow *w)
{
    const auto entry = d->m_animations.find(w);
    if (entry != d->m_animations.end()) {
        auto &[data, rect] = entry->second;
        rect = QRect();
        updateLayerRepaints();
        if (!rect.isNull()) { // actually got updated, ie. is in use - ensure it get's a repaint
            w->addLayerRepaint(rect);
        }
    }
}

void AnimationEffect::_windowClosed(EffectWindow *w)
{
    auto it = d->m_animations.find(w);
    if (it == d->m_animations.end()) {
        return;
    }
    auto &[animations, rect] = it->second;
    for (auto &animation : animations) {
        if (animation.keepAlive) {
            animation.deletedRef = EffectWindowDeletedRef(w);
        }
    }
}

void AnimationEffect::_windowDeleted(EffectWindow *w)
{
    d->m_animations.erase(w);
}

QString AnimationEffect::debug(const QString &parameter) const
{
    QString dbg;
    if (d->m_animations.empty()) {
        dbg = QStringLiteral("No window is animated");
    } else {
        for (const auto &[window, pair] : d->m_animations) {
            const auto &[data, rect] = pair;
            QString caption = window->isDeleted() ? QStringLiteral("[Deleted]") : window->caption();
            if (caption.isEmpty()) {
                caption = QStringLiteral("[Untitled]");
            }
            dbg += QLatin1String("Animating window: ") + caption + QLatin1Char('\n');
            for (const auto &anim : data) {
                dbg += anim.debugInfo();
            }
        }
    }
    return dbg;
}

const AnimationEffect::AniMap &AnimationEffect::state() const
{
    return d->m_animations;
}

} // namespace KWin

#include "moc_animationeffect.cpp"
