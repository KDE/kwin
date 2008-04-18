/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Cédric Borgese <cedric.borgese@gmail.com>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef WOBBLYWINDOWS_H
#define WOBBLYWINDOWS_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

/**
 * Effect which wobble windows
 **/
class WobblyWindowsEffect : public Effect
{
    public:

        enum GridFilter
        {
            NoFilter,
            FourRingLinearMean,
            MeanWithMean,
            MeanWithMedian
        };


        WobblyWindowsEffect();
        virtual ~WobblyWindowsEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintScreen();
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        virtual void windowAdded( EffectWindow* w );
        virtual void windowClosed( EffectWindow* w );

        // Wobbly model parameters
        void setRaideur(qreal raideur);
        void setAmortissement(qreal amortissement);
        void setVelocityThreshold(qreal velocityThreshold);
        void setMoveFactor(qreal factor);

        void setVelocityFilter(GridFilter filter);
        void setAccelerationFilter(GridFilter filter);
        GridFilter velocityFilter() const;
        GridFilter accelerationFilter() const;

        struct Pair
        {
            qreal x;
            qreal y;
        };

    private:

        bool updateWindowWobblyDatas(EffectWindow* w, qreal time);

        enum WindowStatus
        {
            Free,
            Moving,
            Openning,
            Closing
        };

        struct WindowWobblyInfos
        {
            Pair* origin;
            Pair* position;
            Pair* velocity;
            Pair* acceleration;
            Pair* buffer;

            // if true, the point is constraint to its "normal" destination
            // given by the window position.
            // if false, the point is free (i.e. use the physics system to move it)
            bool* constraint;

            unsigned int width;
            unsigned int height;
            unsigned int count;

            Pair* bezierSurface;
            unsigned int bezierWidth;
            unsigned int bezierHeight;
            unsigned int bezierCount;

            WindowStatus status;
        };

        QHash< const EffectWindow*,  WindowWobblyInfos > windows;

        QRect m_updateRegion;

        qreal m_raideur;
        qreal m_amortissement;
        qreal m_move_factor;

        // the default tesselation for windows
        // use qreal instead of int as I really often need
        // these values as real to do divisions.
        qreal m_xTesselation;
        qreal m_yTesselation;

        GridFilter m_velocityFilter;
        GridFilter m_accelerationFilter;

        qreal m_minVelocity;
        qreal m_maxVelocity;
        qreal m_stopVelocity;
        qreal m_minAcceleration;
        qreal m_maxAcceleration;
        qreal m_stopAcceleration;

        void initWobblyInfo(WindowWobblyInfos& wwi, QRect geometry) const;
        void freeWobblyInfo(WindowWobblyInfos& wwi) const;
        void wobblyOpenInit(WindowWobblyInfos& wwi) const;
        void wobblyCloseInit(WindowWobblyInfos& wwi) const;

        WobblyWindowsEffect::Pair computeBezierPoint(const WindowWobblyInfos& wwi, Pair point) const;

        static void fourRingLinearMean(Pair** datas, WindowWobblyInfos& wwi);
        static void meanWithMean(Pair** datas, WindowWobblyInfos& wwi);
        static void meanWithMedian(Pair** datas, WindowWobblyInfos& wwi);
};

} // namespace KWin

#endif // WOBBLYWINDOWS_H
