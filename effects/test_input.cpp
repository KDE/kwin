/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 Testing of handling input in effects. This testing effect moves all windows
 by 100 pixels down, creates an input window that'll intercept all mouse events
 and activates the window that's been clicked (click position needs to be
 transformed). This is useful for effects that present something on the screen
 and let the user interact with it (e.g. a list of window thumbnails and the
 user can activate the window by clicking its thumbnail).

*/

#include "test_input.h"

#include <workspace.h>
#include <client.h>

namespace KWinInternal
{

TestInputEffect::TestInputEffect()
    {
    input = effects->createInputWindow( this, 0, 0, displayWidth(), displayHeight(), Qt::CrossCursor );
    }

TestInputEffect::~TestInputEffect()
    {
    effects->destroyInputWindow( input );
    }

void TestInputEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    *mask |= Scene::PAINT_SCREEN_TRANSFORMED;
    effects->prePaintScreen( mask, region, time );
    }

void TestInputEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    data.yTranslate += 100;
    effects->paintScreen( mask, region, data );
    }

void TestInputEffect::windowInputMouseEvent( Window w, QEvent* e )
    {
    assert( w == input );
    if( e->type() != QEvent::MouseButtonPress )
        return;
    QPoint pos = static_cast< QMouseEvent* >( e )->pos();
    pos -= QPoint( 0, 100 ); // adjust for transformation
    foreach( Client* c, Workspace::self()->stackingOrder())
        {
        if( c->isShown( true ) && c->isOnCurrentDesktop()
            && c->geometry().contains( pos ))
            {
            effects->activateWindow( c );
            return;
            }
        }
    }

} // namespace
