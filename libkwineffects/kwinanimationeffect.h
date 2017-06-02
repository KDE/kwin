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

#ifndef ANIMATION_EFFECT_H
#define ANIMATION_EFFECT_H

#include <QEasingCurve>
#include <QElapsedTimer>
#include <qmath.h>
#include <kwineffects.h>
#include <kwineffects_export.h>


namespace KWin
{

class KWINEFFECTS_EXPORT FPx2 {
public:
    FPx2() { f[0] = f[1] = 0.0; valid = false; }
    explicit FPx2(float v) { f[0] = f[1] = v; valid = true; }
    FPx2(float v1, float v2) { f[0] = v1; f[1] = v2; valid = true; }
    FPx2(const FPx2 &other) { f[0] = other.f[0]; f[1] = other.f[1]; valid = other.valid; }
    explicit FPx2(const QPoint &other) { f[0] = other.x(); f[1] = other.y(); valid = true; }
    explicit FPx2(const QPointF &other) { f[0] = other.x(); f[1] = other.y(); valid = true; }
    explicit FPx2(const QSize &other) { f[0] = other.width(); f[1] = other.height(); valid = true; }
    explicit FPx2(const QSizeF &other) { f[0] = other.width(); f[1] = other.height(); valid = true; }
    inline void invalidate() { valid = false; }
    inline bool isValid() const { return valid; }
    inline float operator[](int n) const { return f[n]; }
    inline QString toString() const {
        QString ret;
        if (valid)
            ret = QString::number(f[0]) + QLatin1Char(',') + QString::number(f[1]);
        else
            ret = QString();
        return ret;
    }


    inline FPx2 &operator+=(const FPx2 &other)
        { f[0] += other[0]; f[1] += other[1]; return *this; }
    inline FPx2 &operator-=(const FPx2 &other)
        { f[0] -= other[0]; f[1] -= other[1]; return *this; }
    inline FPx2 &operator*=(float fl)
        { f[0] *= fl; f[1] *= fl; return *this; }
    inline FPx2 &operator/=(float fl)
        { f[0] /= fl; f[1] /= fl; return *this; }

    friend inline bool operator==(const FPx2 &f1, const FPx2 &f2)
        { return f1[0] == f2[0] && f1[1] == f2[1]; }
    friend inline bool operator!=(const FPx2 &f1, const FPx2 &f2)
        { return f1[0] != f2[0] || f1[1] != f2[1]; }
    friend inline const FPx2 operator+(const FPx2 &f1, const FPx2 &f2)
        { return FPx2( f1[0] + f2[0], f1[1] + f2[1] ); }
    friend inline const FPx2 operator-(const FPx2 &f1, const FPx2 &f2)
        { return FPx2( f1[0] - f2[0], f1[1] - f2[1] ); }
    friend inline const FPx2 operator*(const FPx2 &f, float fl)
        { return FPx2( f[0] * fl, f[1] * fl ); }
    friend inline const FPx2 operator*(float fl, const FPx2 &f)
        { return FPx2( f[0] * fl, f[1] *fl ); }
    friend inline const FPx2 operator-(const FPx2 &f)
        { return FPx2( -f[0], -f[1] ); }
    friend inline const FPx2 operator/(const FPx2 &f, float fl)
        { return FPx2( f[0] / fl, f[1] / fl ); }

    inline void set(float v) { f[0] = v; valid = true; }
    inline void set(float v1, float v2) { f[0] = v1; f[1] = v2; valid = true; }

private:
    float f[2];
    bool valid;
};

class AniData;
class AnimationEffectPrivate;
class KWINEFFECTS_EXPORT AnimationEffect : public Effect
{
    Q_OBJECT
    Q_ENUMS(Anchor)
    Q_ENUMS(Attribute)
    Q_ENUMS(MetaType)
public:
    enum Anchor { Left = 1<<0, Top = 1<<1, Right = 1<<2, Bottom = 1<<3,
                  Horizontal = Left|Right, Vertical = Top|Bottom, Mouse = 1<<4  };
    enum Attribute {
        Opacity = 0, Brightness, Saturation, Scale, Rotation,
        Position, Size, Translation, Clip, Generic, CrossFadePrevious,
        NonFloatBase = Position
    };
    enum MetaType { SourceAnchor, TargetAnchor,
                    RelativeSourceX, RelativeSourceY, RelativeTargetX, RelativeTargetY, Axis };
    /**
     * Whenever you intend to connect to the EffectsHandler::windowClosed() signal, do so when reimplementing the constructor.
     * Do *not* add private slots named _windowClosed( EffectWindow* w ) or _windowDeleted( EffectWindow* w ) !!
     * The AnimationEffect connects them right *after* the construction.
     * If you shadow the _windowDeleted slot (it doesn't matter that it's a private slot!), this will lead to segfaults.
     * If you shadow _windowClosed() or connect your slot to EffectsHandler::windowClosed() after _windowClosed() was connected, animations for closing windows will fail.
     */
    AnimationEffect();
    ~AnimationEffect();

    bool isActive() const Q_DECL_OVERRIDE;
    /**
     * Set and get predefined metatypes.
     * The first 24 bits are reserved for the AnimationEffect class - you can use the last 8 bits for custom hints.
     * In case you transform a Generic attribute, all 32 bits are yours and you can use them as you want and read them in your genericAnimation() implementation.
     */
    static int metaData(MetaType type, uint meta );
    static void setMetaData(MetaType type, uint value, uint &meta );

    /**
     * Reimplemented from KWIn::Effect
     */
    QString debug(const QString &parameter) const Q_DECL_OVERRIDE;
    void prePaintScreen( ScreenPrePaintData& data, int time ) Q_DECL_OVERRIDE;
    void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time ) Q_DECL_OVERRIDE;
    void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data ) Q_DECL_OVERRIDE;
    void postPaintScreen() Q_DECL_OVERRIDE;

    /**
     * Gaussian (bumper) animation curve for QEasingCurve
     */
    static qreal qecGaussian(qreal progress)
    {
        progress = 2*progress - 1;
        progress *= -5*progress;
        return qExp(progress);
    }

    static inline qint64 clock() {
        return s_clock.elapsed();
    }

protected:
    /**
     * The central function of this class - call it to create an animated transition of any supported attribute
     * @param w - The EffectWindow to manipulate
     * @param a - The @enum Attribute to manipulate
     * @param meta - Basically a wildcard to carry various extra information, eg. the anchor, relativity or rotation axis. You will probably use require it when performing Generic animations.
     * @param ms - How long the transition will last
     * @param to - The target value. FPx2 is an agnostic two component float type (like QPointF or QSizeF, but without requiring to be either and supporting an invalid state)
     * @param shape - How the animation progresses, eg. Linear progresses constantly while Exponential start slow and becomes very fast in the end
     * @param delay - When the animation will start compared to "now" (the window will remain at the "from" position until then)
     * @param from - the starting value, the default is invalid, ie. the attribute for the window is not transformed in the beginning
     * @return an ID that you can use to cancel a running animation
     */
    quint64 animate( EffectWindow *w, Attribute a, uint meta, int ms, FPx2 to, QEasingCurve curve = QEasingCurve(), int delay = 0, FPx2 from = FPx2() )
    { return p_animate(w, a, meta, ms, to, curve, delay, from, false); }

    /**
     * Equal to ::animate() with one important difference:
     * The target value for the attribute is kept until you ::cancel() this animation
     * @return an ID that you need to use to cancel this manipulation
     */
    quint64 set( EffectWindow *w, Attribute a, uint meta, int ms, FPx2 to, QEasingCurve curve = QEasingCurve(), int delay = 0, FPx2 from = FPx2() )
    { return p_animate(w, a, meta, ms, to, curve, delay, from, true); }

    /**
     * this allows to alter the target (but not type or curve) of a running animation
     * with the ID @param animationId
     * @param newTarget alters the "to" parameter of the animation
     * If @param newRemainingTime allows to lengthen (or shorten) the remaining time
     * of the animation. By default (-1) the remaining time remains unchanged
     *
     * Please use @function cancel to cancel an animation rather than altering it.
     * NOTICE that you can NOT retarget an animation that just has just @function animationEnded !
     * @return whether there was such animation and it could be altered
     */
    bool retarget(quint64 animationId, FPx2 newTarget, int newRemainingTime = -1);

    /**
     * Called whenever an animation end, passes the transformed @class EffectWindow @enum Attribute and originally supplied @param meta
     * You can reimplement it to keep a constant transformation for the window (ie. keep it a this opacity or position) or to start another animation
     */
    virtual void animationEnded( EffectWindow *, Attribute, uint meta ) {Q_UNUSED(meta);}

    /**
     * Cancel a running animation. @return true if an animation for @p animationId was found (and canceled)
     * NOTICE that there is NO animated reset of the original value. You'll have to provide that with a second animation
     * NOTICE as well that this will eventually release a Deleted window.
     * If you intend to run another animation on the (Deleted) window, you have to do that before cancelling the old animation (to keep the window around)
     */
    bool cancel(quint64 animationId);
    /**
     * Called if the transformed @enum Attribute is Generic. You should reimplement it if you transform this "Attribute".
     * You could use the meta information to eg. support more than one additional animations
     */
    virtual void genericAnimation( EffectWindow *w, WindowPaintData &data, float progress, uint meta )
    {Q_UNUSED(w); Q_UNUSED(data); Q_UNUSED(progress); Q_UNUSED(meta);}

private:
    quint64 p_animate( EffectWindow *w, Attribute a, uint meta, int ms, FPx2 to, QEasingCurve curve, int delay, FPx2 from, bool keepAtTarget );
    QRect clipRect(const QRect &windowRect, const AniData&) const;
    void clipWindow(const EffectWindow *, const AniData &, WindowQuadList &) const;
    float interpolated( const AniData&, int i = 0 ) const;
    float progress( const AniData& ) const;
    void disconnectGeometryChanges();
    void updateLayerRepaints();
    void validate(Attribute a, uint &meta, FPx2 *from, FPx2 *to, const EffectWindow *w) const;
private Q_SLOTS:
    void init();
    void triggerRepaint();
    void _windowClosed( KWin::EffectWindow* w );
    void _windowDeleted( KWin::EffectWindow* w );
    void _expandedGeometryChanged(KWin::EffectWindow *w, const QRect &old);
private:
    static QElapsedTimer s_clock;
    typedef QMap< EffectWindow*, QPair<QList<AniData>, QRect> > AniMap;
    AnimationEffectPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(AnimationEffect)
};


} // namespace
QDebug operator<<(QDebug dbg, const KWin::FPx2 &fpx2);
Q_DECLARE_METATYPE(KWin::FPx2)

#endif // ANIMATION_EFFECT_H
