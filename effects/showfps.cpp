/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "showfps.h"

#include <workspace.h>

#include <options.h>
#include <scene_opengl.h>
#include <scene_xrender.h>

#include <GL/gl.h>

namespace KWinInternal
{

const int FPS_WIDTH = 10;
const int MAX_TIME = 100;

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
    alpha = options->effectShowFpsAlpha;
    x = options->effectShowFpsX;
    y = options->effectShowFpsY;
    if( x == -10000 ) // there's no -0 :(
        x = displayWidth() - NUM_PAINTS - FPS_WIDTH;
    else if ( x < 0 )
        x = displayWidth() - NUM_PAINTS - FPS_WIDTH - x;
    if( y == -10000 )
        y = displayHeight() - MAX_TIME;
    else if ( y < 0 )
        y = displayHeight() - MAX_TIME - y;
    }

void ShowFpsEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( time == 0 )
        ; // TODO optimized away
    t.start();
    frames[ frames_pos ] = t.minute() * 60000 + t.second() * 1000 + t.msec();
    if( ++frames_pos == MAX_FPS )
        frames_pos = 0;
    effects->prePaintScreen( mask, region, time );
    *region += QRect( x, y, FPS_WIDTH + NUM_PAINTS, MAX_TIME );
    }

void ShowFpsEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    int fps = 0;
    for( int i = 0;
         i < MAX_FPS;
         ++i )
        if( abs( t.minute() * 60000 + t.second() * 1000 + t.msec() - frames[ i ] ) < 1000 )
            ++fps; // count all frames in the last second
    if( fps > MAX_TIME )
        fps = MAX_TIME; // keep it the same height
    if( dynamic_cast< SceneOpenGL* >( scene ))
        paintGL( fps );
    else
        paintX( fps );
    }

void ShowFpsEffect::paintGL( int fps )
    {
    int x = this->x;
    int y = this->y;
    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    // TODO painting first the background white and then the contents
    // means that the contents also blend with the background, I guess
    glColor4f( 1, 1, 1, alpha ); // white
    glBegin( GL_QUADS );
    glVertex2i( x, y );
    glVertex2i( x + NUM_PAINTS + FPS_WIDTH, y );
    glVertex2i( x + NUM_PAINTS + FPS_WIDTH, y + MAX_TIME );
    glVertex2i( x, y + MAX_TIME );
    glEnd();
    y += MAX_TIME; // paint up from the bottom
    glBegin( GL_QUADS );
    glColor4f( 0, 0, 1, alpha ); // blue
    glVertex2i( x, y );
    glVertex2i( x + FPS_WIDTH, y );
    glVertex2i( x + FPS_WIDTH, y - fps );
    glVertex2i( x, y - fps );
    glEnd();
    glColor4f( 0, 0, 0, alpha ); // black
    glBegin( GL_LINES );
    for( int i = 10;
         i < MAX_TIME;
         i += 10 )
        {
        glVertex2i( x, y - i );
        glVertex2i( x + FPS_WIDTH, y - i );
        }
    glEnd();
    x += FPS_WIDTH;
    glBegin( GL_LINES );
    for( int i = 0;
         i < NUM_PAINTS;
         ++i )
        {
        int value = paints[ ( i + paints_pos ) % NUM_PAINTS ];
        if( value > MAX_TIME )
            value = MAX_TIME; // limit
        if( value <= 10 )
            glColor4f( 0, 1, 0, alpha ); // green
        else if( value <= 20 )
            glColor4f( 1, 1, 0, alpha ); // yellow
        else if( value <= 50 )
            glColor4f( 1, 0, 0, alpha ); // red
        else
            glColor4f( 0, 0, 0, alpha ); // black
        glVertex2i( x + NUM_PAINTS - i, y );
        glVertex2i( x + NUM_PAINTS - i, y - value );
        }
    glEnd();
    glPopAttrib();
    }

/*
 Differences between OpenGL and XRender:
 - differenly specified rectangles (X: width/height, O: x2,y2)
 - XRender uses pre-multiplied alpha
*/
void ShowFpsEffect::paintX( int fps )
    {
    Pixmap pixmap = XCreatePixmap( display(), rootWindow(), NUM_PAINTS + FPS_WIDTH, MAX_TIME, 32 );
    XRenderPictFormat* format = XRenderFindStandardFormat( display(), PictStandardARGB32 );
    Picture p = XRenderCreatePicture( display(), pixmap, format, 0, NULL );
    XFreePixmap( display(), pixmap );
    XRenderColor col;
    col.alpha = int( alpha * 0xffff );
    col.red = int( alpha * 0xffff ); // white
    col.green = int( alpha * 0xffff );
    col.blue= int( alpha * 0xffff );
    XRenderFillRectangle( display(), PictOpSrc, p, &col, 0, 0, NUM_PAINTS + FPS_WIDTH, MAX_TIME );
    col.red = 0; // blue
    col.green = 0;
    col.blue = int( alpha * 0xffff );
    XRenderFillRectangle( display(), PictOpSrc, p, &col, 0, MAX_TIME - fps, FPS_WIDTH, fps );
    col.red = 0; // black
    col.green = 0;
    col.blue = 0;
    for( int i = 10;
         i < MAX_TIME;
         i += 10 )
        {
        XRenderFillRectangle( display(), PictOpSrc, p, &col, 0, MAX_TIME - i, FPS_WIDTH, 1 );
        }
    for( int i = 0;
         i < NUM_PAINTS;
         ++i )
        {
        int value = paints[ ( i + paints_pos ) % NUM_PAINTS ];
        if( value > MAX_TIME )
            value = MAX_TIME; // limit
        if( value <= 10 )
            { // green
            col.red = 0;
            col.green = int( alpha * 0xffff );
            col.blue = 0;
            }
        else if( value <= 20 )
            { // yellow
            col.red = int( alpha * 0xffff );
            col.green = int( alpha * 0xffff );
            col.blue = 0;
            }
        else if( value <= 50 )
            { // red
            col.red = int( alpha * 0xffff );
            col.green = 0;
            col.blue = 0;
            }
        else
            { // black
            col.red = 0;
            col.green = 0;
            col.blue = 0;
            }
        XRenderFillRectangle( display(), PictOpSrc, p, &col, FPS_WIDTH + NUM_PAINTS - i, MAX_TIME - value, 1, value );
        }
    XRenderComposite( display(), alpha != 1.0 ? PictOpOver : PictOpSrc, p, None,
        static_cast< SceneXrender* >( scene )->bufferPicture(), 0, 0, 0, 0, x, y, FPS_WIDTH + NUM_PAINTS, MAX_TIME );
    XRenderFreePicture( display(), p );
    }

void ShowFpsEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    paints[ paints_pos ] = t.elapsed();
    if( ++paints_pos == NUM_PAINTS )
        paints_pos = 0;
    wspace->addDamage( x, y, FPS_WIDTH + NUM_PAINTS, MAX_TIME );
    }

} // namespace
