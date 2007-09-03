/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "minimizeanimation.h"

namespace KWin
{

KWIN_EFFECT( minimizeanimation, MinimizeAnimationEffect )

MinimizeAnimationEffect::MinimizeAnimationEffect()
    {
    mActiveAnimations = 0;
    }


void MinimizeAnimationEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActiveAnimations > 0 )
        // We need to mark the screen windows as transformed. Otherwise the
        //  whole screen won't be repainted, resulting in artefacts
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
    }

void MinimizeAnimationEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    const double changeTime = 500;
    if( mAnimationProgress.contains( w ))
        {
        if( w->isMinimized() )
            {
            mAnimationProgress[w] += time / changeTime;
            if( mAnimationProgress[w] >= 1.0f )
                mAnimationProgress.remove( w );
            }
        else
            {
            mAnimationProgress[w] -= time / changeTime;
            if( mAnimationProgress[w] <= 0.0f )
                mAnimationProgress.remove( w );
            }

        // Schedule window for transformation if the animation is still in
        //  progress
        if( mAnimationProgress.contains( w ))
            {
            // We'll transform this window
            data.setTransformed();
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_MINIMIZE );
            }
        else
            // Animation just finished
            mActiveAnimations--;
        }

    effects->prePaintWindow( w, data, time );
    }

void MinimizeAnimationEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( mAnimationProgress.contains( w ))
    {
        // 0 = not minimized, 1 = fully minimized
        double progress = mAnimationProgress[w];

        QRect geo = w->geometry();
        QRect icon = w->iconGeometry();
        // If there's no icon geometry, minimize to the center of the screen
        if( !icon.isValid() )
            icon = QRect( displayWidth() / 2, displayHeight() / 2, 0, 0 );

        data.xScale *= interpolate(1.0, icon.width() / (double)geo.width(), progress);
        data.yScale *= interpolate(1.0, icon.height() / (double)geo.height(), progress);
        data.xTranslate = (int)interpolate(data.xTranslate, icon.x() - geo.x(), progress);
        data.yTranslate = (int)interpolate(data.yTranslate, icon.y() - geo.y(), progress);
    }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );
    }

void MinimizeAnimationEffect::postPaintScreen()
    {
    if( mActiveAnimations > 0 )
        // Repaint the workspace so that everything would be repainted next time
        effects->addRepaintFull();

    // Call the next effect.
    effects->postPaintScreen();
    }

void MinimizeAnimationEffect::windowMinimized( EffectWindow* w )
    {
    if( !mAnimationProgress.contains(w) )
        {
        mAnimationProgress[w] = 0.0f;
        mActiveAnimations++;
        }
    }

void MinimizeAnimationEffect::windowUnminimized( EffectWindow* w )
    {
    if( !mAnimationProgress.contains(w) )
        {
        mAnimationProgress[w] = 1.0f;
        mActiveAnimations++;
        }
    }

} // namespace

