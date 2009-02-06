/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#include "cubeslide.h"

#include <kwinconfig.h>
#include <kconfiggroup.h>

#include <math.h>

#include <GL/gl.h>

namespace KWin
{

KWIN_EFFECT( cubeslide, CubeSlideEffect )
KWIN_EFFECT_SUPPORTED( cubeslide, CubeSlideEffect::supported() )

CubeSlideEffect::CubeSlideEffect()
    {
    reconfigure( ReconfigureAll );
    }

CubeSlideEffect::~CubeSlideEffect()
    {
    }

bool CubeSlideEffect::supported()
    {
    return effects->compositingType() == OpenGLCompositing;
    }

void CubeSlideEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig( "CubeSlide" );
    int rotationDuration = animationTime( conf, "RotationDuration", 500 );
    timeLine.setCurveShape( TimeLine::EaseInOutCurve );
    timeLine.setDuration( rotationDuration );
    dontSlidePanels = conf.readEntry( "DontSlidePanels", true );
    dontSlideStickyWindows = conf.readEntry( "DontSlideStickyWindows", true );
    }

void CubeSlideEffect::prePaintScreen( ScreenPrePaintData& data, int time)
    {
    if( !slideRotations.empty() )
        {
        data.mask |= PAINT_SCREEN_TRANSFORMED | Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_SCREEN_BACKGROUND_FIRST;
        timeLine.addTime( time );
        if( dontSlidePanels )
            panels.clear();
        if( dontSlideStickyWindows )
            stickyWindows.clear();
        }
    effects->prePaintScreen( data, time );
    }

void CubeSlideEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data)
    {
    if( !slideRotations.empty() )
        {
        glPushMatrix();
        glNewList( glList, GL_COMPILE );
        paintSlideCube( mask, region, data );
        glEndList();
        glPopMatrix();

        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_CULL_FACE );
        glCullFace( GL_BACK );
        glPushMatrix();
        glCallList( glList );
        glPopMatrix();
        glCullFace( GL_FRONT );
        glPushMatrix();
        glCallList( glList );
        glPopMatrix();
        glDisable( GL_CULL_FACE );
        glPopAttrib();
        if( dontSlidePanels )
            {
            foreach( EffectWindow* w, panels )
                {
                WindowPaintData wData( w );
                effects->paintWindow( w, 0, QRegion( w->x(), w->y(), w->width(), w->height() ), wData );
                }
            }
        if( dontSlideStickyWindows )
            {
            foreach( EffectWindow* w, stickyWindows )
                {
                WindowPaintData wData( w );
                effects->paintWindow( w, 0, QRegion( w->x(), w->y(), w->width(), w->height() ), wData );
                }
            }
        }
    else
        effects->paintScreen( mask, region, data );
    }

void CubeSlideEffect::paintSlideCube(int mask, QRegion region, ScreenPaintData& data)
    {
    // slide cube only paints to desktops at a time
    // first the horizontal rotations followed by vertical rotations
    QRect rect = effects->clientArea( FullArea, effects->activeScreen(), effects->currentDesktop() );
    float point = rect.width()/2*tan(45.0f*M_PI/180.0f);
    cube_painting = true;
    painting_desktop = front_desktop;

    ScreenPaintData firstFaceData = data;
    ScreenPaintData secondFaceData = data;
    RotationData firstFaceRot = RotationData();
    RotationData secondFaceRot = RotationData();
    RotationDirection direction = slideRotations.head();
    int secondDesktop;
    switch ( direction )
        {
        case Left:
            firstFaceRot.axis = RotationData::YAxis;
            secondFaceRot.axis = RotationData::YAxis;
            secondDesktop = effects->desktopToLeft( front_desktop, true );
            firstFaceRot.angle = 90.0f*timeLine.value();
            secondFaceRot.angle = -90.0f*(1.0f - timeLine.value());
            break;
        case Right:
            firstFaceRot.axis = RotationData::YAxis;
            secondFaceRot.axis = RotationData::YAxis;
            secondDesktop = effects->desktopToRight( front_desktop, true );
            firstFaceRot.angle = -90.0f*timeLine.value();
            secondFaceRot.angle = 90.0f*(1.0f - timeLine.value());
            break;
        case Upwards:
            firstFaceRot.axis = RotationData::XAxis;
            secondFaceRot.axis = RotationData::XAxis;
            secondDesktop = effects->desktopUp( front_desktop, true);
            firstFaceRot.angle = -90.0f*timeLine.value();
            secondFaceRot.angle = 90.0f*(1.0f - timeLine.value());
            point = rect.height()/2*tan(45.0f*M_PI/180.0f);
            break;
        case Downwards:
            firstFaceRot.axis = RotationData::XAxis;
            secondFaceRot.axis = RotationData::XAxis;
            secondDesktop = effects->desktopDown( front_desktop, true );
            firstFaceRot.angle = 90.0f*timeLine.value();
            secondFaceRot.angle = -90.0f*(1.0f - timeLine.value());
            point = rect.height()/2*tan(45.0f*M_PI/180.0f);
            break;
        default:
            // totally impossible
            return;
        }
    // front desktop
    firstFaceRot.xRotationPoint = rect.width()/2;
    firstFaceRot.yRotationPoint = rect.height()/2;
    firstFaceRot.zRotationPoint = -point;
    firstFaceData.rotation = &firstFaceRot;
    effects->paintScreen( mask, region, firstFaceData );
    // second desktop
    painting_desktop = secondDesktop;
    secondFaceRot.xRotationPoint = rect.width()/2;
    secondFaceRot.yRotationPoint = rect.height()/2;
    secondFaceRot.zRotationPoint = -point;
    secondFaceData.rotation = &secondFaceRot;
    effects->paintScreen( mask, region, secondFaceData );
    cube_painting = false;
    painting_desktop = effects->currentDesktop();
    }

void CubeSlideEffect::prePaintWindow( EffectWindow* w,  WindowPrePaintData& data, int time)
    {
    if( !slideRotations.empty() && cube_painting )
        {
        if( dontSlidePanels && w->isDock())
            {
            panels.insert( w );
            }
        if( dontSlideStickyWindows && !w->isDock() &&
            !w->isDesktop() && w->isOnAllDesktops())
            {
            stickyWindows.insert( w );
            }
        if( w->isOnDesktop( painting_desktop ) )
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        else
            w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        }
    effects->prePaintWindow( w, data, time );
    }

void CubeSlideEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
    {
    if( !slideRotations.empty() && cube_painting )
        {
        if( dontSlidePanels && w->isDock() )
            return;
        if( dontSlideStickyWindows &&
            w->isOnAllDesktops() && !w->isDock() && !w->isDesktop() )
            return;
        }
    effects->paintWindow( w, mask, region, data );
    }

void CubeSlideEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    if( !slideRotations.empty() )
        {
        if( timeLine.value() == 1.0 )
            {
            RotationDirection direction = slideRotations.dequeue();
            switch (direction)
                {
                case Left:
                    front_desktop = effects->desktopToLeft( front_desktop, true );
                    break;
                case Right:
                    front_desktop = effects->desktopToRight( front_desktop, true );
                    break;
                case Upwards:
                    front_desktop = effects->desktopUp( front_desktop, true );
                    break;
                case Downwards:
                    front_desktop = effects->desktopDown( front_desktop, true );
                    break;
                }
            timeLine.setProgress( 0.0 );
            if( slideRotations.empty() )
                {
                effects->setActiveFullScreenEffect( 0 );
                glDeleteLists( glList, 1 );
                }
            }
        effects->addRepaintFull();
        }
    }

void CubeSlideEffect::desktopChanged( int old )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return;
    bool activate = true;
    if( !slideRotations.empty() )
        {
        // last slide still in progress
        activate = false;
        old = front_desktop;
        RotationDirection direction = slideRotations.dequeue();
        slideRotations.clear();
        slideRotations.enqueue( direction );
        }
    // calculate distance in respect to pager
    int x, y;
    Qt::Orientation orientation;
    effects->calcDesktopLayout( &x, &y, &orientation );
    int x_distance = (( effects->currentDesktop() - 1 ) % x ) - (( old - 1 ) % x );
    if( qAbs( x_distance ) > x/2 )
        {
        int sign = -1 * (x_distance/qAbs( x_distance ));
        x_distance = sign * ( x - qAbs( x_distance ));
        }
    if( x_distance > 0 )
        {
        for( int i=0; i<x_distance; i++ )
            {
            slideRotations.enqueue( Right );
            }
        }
    else if( x_distance < 0 )
        {
        x_distance *= -1;
        for( int i=0; i<x_distance; i++ )
            {
            slideRotations.enqueue( Left );
            }
        }
    int y_distance = (( effects->currentDesktop() -1 ) / x ) - (( old - 1 ) / x );
    if( qAbs( y_distance ) > y/2 )
        {
        int sign = -1 * (y_distance/qAbs( y_distance ));
        y_distance = sign * ( y - qAbs( y_distance ));
        }
    if( y_distance > 0 )
        {
        for( int i=0; i<y_distance; i++ )
            {
            slideRotations.enqueue( Downwards );
            }
        }
    if( y_distance < 0 )
        {
        y_distance *= -1;
        for( int i=0; i<y_distance; i++ )
            {
            slideRotations.enqueue( Upwards );
            }
        }
    if( activate )
        {
        effects->setActiveFullScreenEffect( this );
        glList = glGenLists( 1 );
        timeLine.setProgress( 0.0 );
        front_desktop = old;
        effects->addRepaintFull();
        }
    }

} // namespace
