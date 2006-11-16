/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "showfps.h"

#include <workspace.h>

#include <GL/gl.h>

namespace KWinInternal
{

ShowFpsEffect::ShowFpsEffect( Workspace* ws )
    : wspace( ws )
    , paints_pos( 0 )
    , frames_pos( 0 )
    {
    for( int i = 0;
         i < NUM_PAINTS;
         ++i )
        paints[ i ] = 0;
    for( int i = 0;
         i < MAX_FPS;
         ++i )
        frames[ i ] = 0;
    }

void ShowFpsEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    *mask &= ~Scene::PAINT_SCREEN_REGION;
    if( time == 0 )
        ; // TODO optimized away
    t.start();
    frames[ frames_pos ] = t.second() * 1000 + t.msec();
    if( ++frames_pos == MAX_FPS )
        frames_pos = 0;
    effects->prePaintScreen( mask, region, time );
    }

void ShowFpsEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    const double ALPHA = 0.5;
    const int MAX_TIME = 100;
    int x = 100;
    int y = 100;
    int FPS_WIDTH = 10;
    glColor4f( 1, 1, 1, ALPHA ); // white
    glBegin( GL_QUADS );
    glVertex2i( x, y );
    glVertex2i( x + NUM_PAINTS + FPS_WIDTH, y );
    glVertex2i( x + NUM_PAINTS + FPS_WIDTH, y + MAX_TIME );
    glVertex2i( x, y + MAX_TIME );
    glEnd();
    y += MAX_TIME;
    glBegin( GL_LINES );
    for( int i = 0;
         i < NUM_PAINTS;
         ++i )
        {
        int value = paints[ ( i + paints_pos ) % NUM_PAINTS ];
        if( value > MAX_TIME )
            value = MAX_TIME; // limit
        if( value <= 10 )
            glColor4f( 0, 1, 0, ALPHA ); // green
        else if( value <= 20 )
            glColor4f( 1, 1, 0, ALPHA ); // yellow
        else if( value <= 50 )
            glColor4f( 1, 0, 0, ALPHA ); // red
        else
            glColor4f( 0, 0, 0, ALPHA ); // black
        glVertex2i( x + i, y );
        glVertex2i( x + i, y - value );
        }
    glEnd();
    int fps = 0;
    for( int i = 0;
         i < MAX_FPS;
         ++i )
        if( t.second() * 1000 + t.msec() - frames[ i ] < 1000 )
            ++fps; // count all frames in the last second
    if( fps > MAX_TIME )
        fps = MAX_TIME; // keep it the same height (TODO change later)
    glBegin( GL_QUADS );
    x += NUM_PAINTS; // paint to the right
    glColor4f( 0, 0, 1, ALPHA ); // blue
    glVertex2i( x, y );
    glVertex2i( x + FPS_WIDTH, y );
    glVertex2i( x + FPS_WIDTH, y - fps );
    glVertex2i( x, y - fps );
    glEnd();
    glPopAttrib();
    }

void ShowFpsEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    paints[ paints_pos ] = t.elapsed();
    if( ++paints_pos == NUM_PAINTS )
        paints_pos = 0;
    wspace->addDamageFull();
    }

} // namespace
