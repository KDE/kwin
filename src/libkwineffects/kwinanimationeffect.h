/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Thomas Lübking <thomas.luebking@web.de>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QEasingCurve>
#include <QElapsedTimer>
#include <QtMath>
#include <kwinoffscreeneffect.h>
#include <kwineffects_export.h>

namespace KWin
{

class KWINEFFECTS_EXPORT FPx2
{
public:
    FPx2()
    {
        f[0] = f[1] = 0.0;
        valid = false;
    }
    explicit FPx2(float v)
    {
        f[0] = f[1] = v;
        valid = true;
    }
    FPx2(float v1, float v2)
    {
        f[0] = v1;
        f[1] = v2;
        valid = true;
    }
    FPx2(const FPx2 &other)
    {
        f[0] = other.f[0];
        f[1] = other.f[1];
        valid = other.valid;
    }
    explicit FPx2(const QPoint &other)
    {
        f[0] = other.x();
        f[1] = other.y();
        valid = true;
    }
    explicit FPx2(const QPointF &other)
    {
        f[0] = other.x();
        f[1] = other.y();
        valid = true;
    }
    explicit FPx2(const QSize &other)
    {
        f[0] = other.width();
        f[1] = other.height();
        valid = true;
    }
    explicit FPx2(const QSizeF &other)
    {
        f[0] = other.width();
        f[1] = other.height();
        valid = true;
    }
    inline void invalidate()
    {
        valid = false;
    }
    inline bool isValid() const
    {
        return valid;
    }
    inline float operator[](int n) const
    {
        return f[n];
    }
    inline QString toString() const
    {
        QString ret;
        if (valid) {
            ret = QString::number(f[0]) + QLatin1Char(',') + QString::number(f[1]);
        } else {
            ret = QString();
        }
        return ret;
    }

    inline FPx2 &operator=(const FPx2 &other)
    {
        f[0] = other.f[0];
        f[1] = other.f[1];
        valid = other.valid;
        return *this;
    }
    inline FPx2 &operator+=(const FPx2 &other)
    {
        f[0] += other[0];
        f[1] += other[1];
        return *this;
    }
    inline FPx2 &operator-=(const FPx2 &other)
    {
        f[0] -= other[0];
        f[1] -= other[1];
        return *this;
    }
    inline FPx2 &operator*=(float fl)
    {
        f[0] *= fl;
        f[1] *= fl;
        return *this;
    }
    inline FPx2 &operator/=(float fl)
    {
        f[0] /= fl;
        f[1] /= fl;
        return *this;
    }

    friend inline bool operator==(const FPx2 &f1, const FPx2 &f2)
    {
        return f1[0] == f2[0] && f1[1] == f2[1];
    }
    friend inline bool operator!=(const FPx2 &f1, const FPx2 &f2)
    {
        return f1[0] != f2[0] || f1[1] != f2[1];
    }
    friend inline const FPx2 operator+(const FPx2 &f1, const FPx2 &f2)
    {
        return FPx2(f1[0] + f2[0], f1[1] + f2[1]);
    }
    friend inline const FPx2 operator-(const FPx2 &f1, const FPx2 &f2)
    {
        return FPx2(f1[0] - f2[0], f1[1] - f2[1]);
    }
    friend inline const FPx2 operator*(const FPx2 &f, float fl)
    {
        return FPx2(f[0] * fl, f[1] * fl);
    }
    friend inline const FPx2 operator*(float fl, const FPx2 &f)
    {
        return FPx2(f[0] * fl, f[1] * fl);
    }
    friend inline const FPx2 operator-(const FPx2 &f)
    {
        return FPx2(-f[0], -f[1]);
    }
    friend inline const FPx2 operator/(const FPx2 &f, float fl)
    {
        return FPx2(f[0] / fl, f[1] / fl);
    }

    inline void set(float v)
    {
        f[0] = v;
        valid = true;
    }
    inline void set(float v1, float v2)
    {
        f[0] = v1;
        f[1] = v2;
        valid = true;
    }

private:
    float f[2];
    bool valid;
};

class AniData;
class AnimationEffectPrivate;

/**
 * Base class for animation effects.
 *
 * AnimationEffect serves as a base class for animation effects. It makes easier
 * implementing animated transitions, without having to worry about low-level
 * specific stuff, e.g. referencing and unreferencing deleted windows, scheduling
 * repaints for the next frame, etc.
 *
 * Each animation animates one specific attribute, e.g. size, position, scale, etc.
 * You can provide your own implementation of the Generic attribute if none of the
 * standard attributes(e.g. size, position, etc) satisfy your requirements.
 *
 * @since 4.8
 */
class KWINEFFECTS_EXPORT AnimationEffect : public CrossFadeEffect
{
    Q_OBJECT

public:
    enum Anchor { Left = 1 << 0,
                  Top = 1 << 1,
                  Right = 1 << 2,
                  Bottom = 1 << 3,
                  Horizontal = Left | Right,
                  Vertical = Top | Bottom,
                  Mouse = 1 << 4 };
    Q_ENUM(Anchor)

    enum Attribute {
        Opacity = 0,
        Brightness,
        Saturation,
        Scale,
        Rotation,
        Position,
        Size,
        Translation,
        Clip,
        Generic,
        CrossFadePrevious,
        /**
         * Performs an animation with a provided shader.
         * The float uniform @c animationProgress is set to the current progress of the animation.
         **/
        Shader,
        /**
         * Like Shader, but additionally allows to animate a float uniform passed to the shader.
         * The uniform location must be provided as metadata.
         **/
        ShaderUniform,
        NonFloatBase = Position
    };
    Q_ENUM(Attribute)

    enum MetaType { SourceAnchor,
                    TargetAnchor,
                    RelativeSourceX,
                    RelativeSourceY,
                    RelativeTargetX,
                    RelativeTargetY,
                    Axis };
    Q_ENUM(MetaType)

    /**
     * This enum type is used to specify the direction of the animation.
     *
     * @since 5.15
     */
    enum Direction {
        Forward, ///< The animation goes from source to target.
        Backward ///< The animation goes from target to source.
    };
    Q_ENUM(Direction)

    /**
     * This enum type is used to specify when the animation should be terminated.
     *
     * @since 5.15
     */
    enum TerminationFlag {
        /**
         * Don't terminate the animation when it reaches source or target position.
         */
        DontTerminate = 0x00,
        /**
         * Terminate the animation when it reaches the source position. An animation
         * can reach the source position if its direction was changed to go backward
         * (from target to source).
         */
        TerminateAtSource = 0x01,
        /**
         * Terminate the animation when it reaches the target position. If this flag
         * is not set, then the animation will be persistent.
         */
        TerminateAtTarget = 0x02
    };
    Q_DECLARE_FLAGS(TerminationFlags, TerminationFlag)
    Q_FLAG(TerminationFlags)

    /**
     * Constructs AnimationEffect.
     *
     * Whenever you intend to connect to the EffectsHandler::windowClosed() signal,
     * do so when reimplementing the constructor. Do not add private slots named
     * _windowClosed or _windowDeleted! The AnimationEffect connects them right after
     * the construction.
     *
     * If you shadow the _windowDeleted slot (it doesn't matter that it's a private
     * slot), this will lead to segfaults.
     *
     * If you shadow _windowClosed or connect your slot to EffectsHandler::windowClosed()
     * after _windowClosed was connected, animations for closing windows will fail.
     */
    AnimationEffect();
    ~AnimationEffect() override;

    bool isActive() const override;

    /**
     * Gets stored metadata.
     *
     * Metadata can be used to store some extra information, for example rotation axis,
     * etc. The first 24 bits are reserved for the AnimationEffect class, you can use
     * the last 8 bits for custom hints. In case when you transform a Generic attribute,
     * all 32 bits are yours and you can use them as you want and read them in your
     * genericAnimation() implementation.
     *
     * @param type The type of the metadata.
     * @param meta Where the metadata is stored.
     * @returns Stored metadata.
     * @since 4.8
     */
    static int metaData(MetaType type, uint meta);

    /**
     * Sets metadata.
     *
     * @param type The type of the metadata.
     * @param value The data to be stored.
     * @param meta Where the metadata will be stored.
     * @since 4.8
     */
    static void setMetaData(MetaType type, uint value, uint &meta);

    // Reimplemented from KWin::Effect.
    QString debug(const QString &parameter) const override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void postPaintScreen() override;

    /**
     * Gaussian (bumper) animation curve for QEasingCurve.
     *
     * @since 4.8
     */
    static qreal qecGaussian(qreal progress)
    {
        progress = 2 * progress - 1;
        progress *= -5 * progress;
        return qExp(progress);
    }

    /**
     * @since 4.8
     */
    static inline qint64 clock()
    {
        return s_clock.elapsed();
    }

protected:
    /**
     * Starts an animated transition of any supported attribute.
     *
     * @param w The animated window.
     * @param a The animated attribute.
     * @param meta Basically a wildcard to carry various extra information, e.g.
     *   the anchor, relativity or rotation axis. You will probably use it when
     *   performing Generic animations.
     * @param ms How long the transition will last.
     * @param to The target value. FPx2 is an agnostic two component float type
     *   (like QPointF or QSizeF, but without requiring to be either and supporting
     *   an invalid state).
     * @param curve How the animation progresses, e.g. Linear progresses constantly
     *   while Exponential start slow and becomes very fast in the end.
     * @param delay When the animation will start compared to "now" (the window will
     *   remain at the "from" position until then).
     * @param from The starting value, the default is invalid, ie. the attribute for
     *   the window is not transformed in the beginning.
     * @param fullScreen Sets this effect as the active full screen effect for the
     *   duration of the animation.
     * @param keepAlive Whether closed windows should be kept alive during animation.
     * @param shader Optional shader to use to render the window.
     * @returns An ID that you can use to cancel a running animation.
     * @since 4.8
     */
    quint64 animate(EffectWindow *w, Attribute a, uint meta, int ms, const FPx2 &to, const QEasingCurve &curve = QEasingCurve(), int delay = 0, const FPx2 &from = FPx2(), bool fullScreen = false, bool keepAlive = true, GLShader *shader = nullptr)
    {
        return p_animate(w, a, meta, ms, to, curve, delay, from, false, fullScreen, keepAlive, shader);
    }

    /**
     * Starts a persistent animated transition of any supported attribute.
     *
     * This method is equal to animate() with one important difference:
     * the target value for the attribute is kept until you call cancel().
     *
     * @param w The animated window.
     * @param a The animated attribute.
     * @param meta Basically a wildcard to carry various extra information, e.g.
     *   the anchor, relativity or rotation axis. You will probably use it when
     *   performing Generic animations.
     * @param ms How long the transition will last.
     * @param to The target value. FPx2 is an agnostic two component float type
     *   (like QPointF or QSizeF, but without requiring to be either and supporting
     *   an invalid state).
     * @param curve How the animation progresses, e.g. Linear progresses constantly
     *   while Exponential start slow and becomes very fast in the end.
     * @param delay When the animation will start compared to "now" (the window will
     *   remain at the "from" position until then).
     * @param from The starting value, the default is invalid, ie. the attribute for
     *   the window is not transformed in the beginning.
     * @param fullScreen Sets this effect as the active full screen effect for the
     *   duration of the animation.
     * @param keepAlive Whether closed windows should be kept alive during animation.
     * @param shader Optional shader to use to render the window.
     * @returns An ID that you need to use to cancel this manipulation.
     * @since 4.11
     */
    quint64 set(EffectWindow *w, Attribute a, uint meta, int ms, const FPx2 &to, const QEasingCurve &curve = QEasingCurve(), int delay = 0, const FPx2 &from = FPx2(), bool fullScreen = false, bool keepAlive = true, GLShader *shader = nullptr)
    {
        return p_animate(w, a, meta, ms, to, curve, delay, from, true, fullScreen, keepAlive, shader);
    }

    /**
     * Changes the target (but not type or curve) of a running animation.
     *
     * Please use cancel() to cancel an animation rather than altering it.
     *
     * @param animationId The id of the animation to be retargetted.
     * @param newTarget The new target.
     * @param newRemainingTime The new duration of the transition. By default (-1),
     *   the remaining time remains unchanged.
     * @returns @c true if the animation was retargetted successfully, @c false otherwise.
     * @note You can NOT retarget an animation that just has just ended!
     * @since 5.6
     */
    bool retarget(quint64 animationId, FPx2 newTarget, int newRemainingTime = -1);

    bool freezeInTime(quint64 animationId, qint64 frozenTime);

    /**
     * Changes the direction of the animation.
     *
     * @param animationId The id of the animation.
     * @param direction The new direction of the animation.
     * @param terminationFlags Whether the animation should be terminated when it
     *   reaches the source position after its direction was changed to go backward.
     *   Currently, TerminationFlag::TerminateAtTarget has no effect.
     * @returns @c true if the direction of the animation was changed successfully,
     *   otherwise @c false.
     * @since 5.15
     */
    bool redirect(quint64 animationId,
                  Direction direction,
                  TerminationFlags terminationFlags = TerminateAtSource);

    /**
     * Fast-forwards the animation to the target position.
     *
     * @param animationId The id of the animation.
     * @returns @c true if the animation was fast-forwarded successfully, otherwise
     *   @c false.
     * @since 5.15
     */
    bool complete(quint64 animationId);

    /**
     * Called whenever an animation ends.
     *
     * You can reimplement this method to keep a constant transformation for the window
     * (i.e. keep it at some opacity or position) or to start another animation.
     *
     * @param w The animated window.
     * @param a The animated attribute.
     * @param meta Originally supplied metadata to animate() or set().
     * @since 4.8
     */
    virtual void animationEnded(EffectWindow *w, Attribute a, uint meta);

    /**
     * Cancels a running animation.
     *
     * @param animationId The id of the animation.
     * @returns @c true if the animation was found (and canceled), @c false otherwise.
     * @note There is NO animated reset of the original value. You'll have to provide
     *   that with a second animation.
     * @note This will eventually release a Deleted window as well.
     * @note If you intend to run another animation on the (Deleted) window, you have
     *   to do that before cancelling the old animation (to keep the window around).
     * @since 4.11
     */
    bool cancel(quint64 animationId);

    /**
     * Called whenever animation that transforms Generic attribute needs to be painted.
     *
     * You should reimplement this method if you transform Generic attribute. @p meta
     * can be used to support more than one additional animations.
     *
     * @param w The animated window.
     * @param data The paint data.
     * @param progress Current progress value.
     * @param meta The metadata.
     * @since 4.8
     */
    virtual void genericAnimation(EffectWindow *w, WindowPaintData &data, float progress, uint meta);

    /**
     * @internal
     */
    typedef QMap<EffectWindow *, QPair<QList<AniData>, QRect>> AniMap;

    /**
     * @internal
     */
    AniMap state() const;

private:
    quint64 p_animate(EffectWindow *w, Attribute a, uint meta, int ms, FPx2 to, const QEasingCurve &curve, int delay, FPx2 from, bool keepAtTarget, bool fullScreenEffect, bool keepAlive, GLShader *shader);
    QRect clipRect(const QRect &windowRect, const AniData &) const;
    float interpolated(const AniData &, int i = 0) const;
    float progress(const AniData &) const;
    void disconnectGeometryChanges();
    void updateLayerRepaints();
    void validate(Attribute a, uint &meta, FPx2 *from, FPx2 *to, const EffectWindow *w) const;

private Q_SLOTS:
    void init();
    void triggerRepaint();
    void _windowClosed(KWin::EffectWindow *w);
    void _windowDeleted(KWin::EffectWindow *w);
    void _windowExpandedGeometryChanged(KWin::EffectWindow *w);

private:
    static QElapsedTimer s_clock;
    const std::unique_ptr<AnimationEffectPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AnimationEffect)
    Q_DISABLE_COPY(AnimationEffect)
};

} // namespace

Q_DECLARE_METATYPE(KWin::FPx2)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::AnimationEffect::TerminationFlags)
