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
#include <QtCore/qmath.h>
#include <kwineffects.h>


namespace KWin
{

class KWIN_EXPORT FPx2 {
public:
    FPx2() { f[0] = f[1] = 0.0; valid = false; }
    FPx2(float v) { f[0] = f[1] = v; valid = true; }
    FPx2(float v1, float v2) { f[0] = v1; f[1] = v2; valid = true; }
    FPx2(const FPx2 &other) { f[0] = other.f[0]; f[1] = other.f[1]; valid = other.valid; }
    FPx2(const QPoint &other) { f[0] = other.x(); f[1] = other.y(); valid = true; }
    FPx2(const QPointF &other) { f[0] = other.x(); f[1] = other.y(); valid = true; }
    FPx2(const QSize &other) { f[0] = other.width(); f[1] = other.height(); valid = true; }
    FPx2(const QSizeF &other) { f[0] = other.width(); f[1] = other.height(); valid = true; }
    inline void invalidate() { valid = false; }
    inline bool isValid() const { return valid; }
    inline float operator[](int n) const { return f[n]; }
    inline QString toString() const {
        QString ret;
        if (valid)
            ret = QString::number(f[0]) + ',' + QString::number(f[1]);
        else
            ret = QString("");
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
class KWIN_EXPORT AnimationEffect : public Effect
{
    Q_OBJECT
public:
    enum Anchor { Left = 1<<0, Top = 1<<1, Right = 1<<2, Bottom = 1<<3,
                  Horizontal = Left|Right, Vertical = Top|Bottom, Mouse = 1<<4  };
    enum Attribute {
        Opacity = 0, Brightness, Saturation, Scale, Rotation,
        Position, Size, Translation, Generic,
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

    bool isActive() const;
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
    virtual void prePaintScreen( ScreenPrePaintData& data, int time );
    virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
    virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
    virtual void postPaintScreen();

    /**
     * Gaussian (bumper) animation curve for QEasingCurve
     */
    static qreal qecGaussian(qreal progress)
    {
        progress = 2*progress - 1;
        progress *= -5*progress;
        return qExp(progress);
    }

protected:
    /**
     * The central function of this class - call it to create an animated transition of any supported attribute
     * @param w - The EffectWindow to manipulate
     * @param a - The @enum Attribute to manipulate
     * @param meta - Basically a wildcard to carry various extra informations, eg. the anchor, relativity or rotation axis. You will probably use require it when performing Generic animations.
     * @param ms - How long the transition will last
     * @param to - The target value. FPx2 is an agnostic two component float type (like QPointF or QSizeF, but without requiring to be either and supporting an invalid state)
     * @param shape - How the animation progresses, eg. Linear progresses constantly while Exponential start slow and becomes very fast in the end
     * @param delay - When the animation will start compared to "now" (the window will remain at the "from" position until then)
     * @param from - the starting value, the default is invalid, ie. the attribute for the window is not transformed in the beginning
     */
    void animate( EffectWindow *w, Attribute a, uint meta, int ms, FPx2 to, QEasingCurve curve = QEasingCurve(), int delay = 0, FPx2 from = FPx2() );
    /**
     * Called whenever an animation end, passes the transformed @class EffectWindow and @enum Attribute
     * You can reimplement it to keep a constant transformation for the window (ie. keep it a this opacity or position) or to start another animation
     */
    virtual void animationEnded( EffectWindow *, Attribute ) {}
    /**
     * Called if the transformed @enum Attribute is Generic. You should reimplement it if you transform this "Attribute".
     * You could use the meta information to eg. support more than one additional animations
     */
    virtual void genericAnimation( EffectWindow *w, WindowPaintData &data, float progress, uint meta )
    {Q_UNUSED(w); Q_UNUSED(data); Q_UNUSED(progress); Q_UNUSED(meta);}

private:
    float interpolated( const AniData&, int i = 0 ) const;
    float progress( const AniData& ) const;
private slots:
    void init();
    void triggerRepaint();
    void _windowClosed( EffectWindow* w );
    void _windowDeleted( EffectWindow* w );
private:
    typedef QMap< EffectWindow*, QList<AniData> > AniMap;
    AnimationEffectPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(AnimationEffect)
};


} // namespace

#endif // ANIMATION_EFFECT_H
