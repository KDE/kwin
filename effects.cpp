/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "effects.h"

#include "toplevel.h"
#include "client.h"
#include "scene.h"

#include "effects/dialogparent.h"
#include "effects/fadein.h"
#include "effects/fadeout.h"
#include "effects/howto.h"
#include "effects/maketransparent.h"
#include "effects/presentwindows.h"
#include "effects/scalein.h"
#include "effects/shakymove.h"
#include "effects/shiftworkspaceup.h"
#include "effects/showfps.h"
#include "effects/wavywindows.h"
#include "effects/zoom.h"

#include "effects/test_input.h"

namespace KWinInternal
{

//****************************************
// Effect
//****************************************

Effect::~Effect()
    {
    }

void Effect::windowUserMovedResized( EffectWindow* , bool, bool )
    {
    }

void Effect::windowAdded( EffectWindow* )
    {
    }

void Effect::windowClosed( EffectWindow* )
    {
    }

void Effect::windowDeleted( EffectWindow* )
    {
    }

void Effect::windowActivated( EffectWindow* )
    {
    }

void Effect::windowMinimized( EffectWindow* )
    {
    }

void Effect::windowUnminimized( EffectWindow* )
    {
    }

void Effect::windowInputMouseEvent( Window, QEvent* )
    {
    }

void Effect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    effects->prePaintScreen( mask, region, time );
    }

void Effect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    }
    
void Effect::postPaintScreen()
    {
    effects->postPaintScreen();
    }

void Effect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    effects->prePaintWindow( w, mask, region, time );
    }

void Effect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->paintWindow( w, mask, region, data );
    }

void Effect::postPaintWindow( EffectWindow* w )
    {
    effects->postPaintWindow( w );
    }

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler( Workspace* ws )
    : current_paint_window( 0 )
    , current_paint_screen( 0 )
    {
    if( !compositing())
        return;
    KWinInternal::effects = this;
    effects.append( new ShowFpsEffect( ws ));
//    effects.append( new ZoomEffect( ws ));
//    effects.append( new PresentWindowsEffect( ws ));
//    effects.append( new WavyWindowsEffect( ws ));
//    effects.append( new HowtoEffect );
//    effects.append( new MakeTransparentEffect );
//    effects.append( new ShakyMoveEffect );
//    effects.append( new ShiftWorkspaceUpEffect( ws ));
//    effects.append( new FadeInEffect );
    effects.append( new FadeOutEffect );
//    effects.append( new ScaleInEffect );
//    effects.append( new DialogParentEffect );

//    effects.append( new TestInputEffect );
    }

EffectsHandler::~EffectsHandler()
    {
    foreach( Effect* e, effects )
        delete e;
    foreach( InputWindowPair pos, input_windows )
        XDestroyWindow( display(), pos.second );
    }

void EffectsHandler::windowUserMovedResized( EffectWindow* c, bool first, bool last )
    {
    foreach( Effect* e, effects )
        e->windowUserMovedResized( c, first, last );
    }

void EffectsHandler::windowAdded( EffectWindow* c )
    {
    foreach( Effect* e, effects )
        e->windowAdded( c );
    }

void EffectsHandler::windowDeleted( EffectWindow* c )
    {
    foreach( Effect* e, effects )
        e->windowDeleted( c );
    }

void EffectsHandler::windowClosed( EffectWindow* c )
    {
    foreach( Effect* e, effects )
        e->windowClosed( c );
    }

void EffectsHandler::windowActivated( EffectWindow* c )
    {
    foreach( Effect* e, effects )
        e->windowActivated( c );
    }

void EffectsHandler::windowMinimized( EffectWindow* c )
    {
    foreach( Effect* e, effects )
        e->windowMinimized( c );
    }

void EffectsHandler::windowUnminimized( EffectWindow* c )
    {
    foreach( Effect* e, effects )
        e->windowUnminimized( c );
    }

// start another painting pass
void EffectsHandler::startPaint()
    {
    assert( current_paint_screen == 0 );
    assert( current_paint_window == 0 );
    }

// the idea is that effects call this function again which calls the next one
void EffectsHandler::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( current_paint_screen < effects.size())
        {
        effects[ current_paint_screen++ ]->prePaintScreen( mask, region, time );
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandler::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( current_paint_screen < effects.size())
        {
        effects[ current_paint_screen++ ]->paintScreen( mask, region, data );
        --current_paint_screen;
        }
    else
        scene->finalPaintScreen( mask, region, data );
    }

void EffectsHandler::postPaintScreen()
    {
    if( current_paint_screen < effects.size())
        {
        effects[ current_paint_screen++ ]->postPaintScreen();
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandler::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( current_paint_window < effects.size())
        {
        effects[ current_paint_window++ ]->prePaintWindow( w, mask, region, time );
        --current_paint_window;
        }
    // no special final code
    }

void EffectsHandler::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( current_paint_window < effects.size())
        {
        effects[ current_paint_window++ ]->paintWindow( w, mask, region, data );
        --current_paint_window;
        }
    else
        scene->finalPaintWindow( w, mask, region, data );
    }

void EffectsHandler::postPaintWindow( EffectWindow* w )
    {
    if( current_paint_window < effects.size())
        {
        effects[ current_paint_window++ ]->postPaintWindow( w );
        --current_paint_window;
        }
    // no special final code
    }

Window EffectsHandler::createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor )
    {
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    Window win = XCreateWindow( display(), rootWindow(), x, y, w, h, 0, 0, InputOnly, CopyFromParent,
        CWOverrideRedirect, &attrs );
// TODO keeping on top?
// TODO enter/leave notify?
    XSelectInput( display(), win, ButtonPressMask | ButtonReleaseMask | PointerMotionMask );
    XDefineCursor( display(), win, cursor.handle());
    XMapWindow( display(), win );
    input_windows.append( qMakePair( e, win ));
    return win;
    }

Window EffectsHandler::createInputWindow( Effect* e, const QRect& r, const QCursor& cursor )
    {
    return createInputWindow( e, r.x(), r.y(), r.width(), r.height(), cursor );
    }

void EffectsHandler::destroyInputWindow( Window w )
    {
    foreach( InputWindowPair pos, input_windows )
        {
        if( pos.second == w )
            {
            input_windows.removeAll( pos );
            XDestroyWindow( display(), w );
            return;
            }
        }
    assert( false );
    }

bool EffectsHandler::checkInputWindowEvent( XEvent* e )
    {
    if( e->type != ButtonPress && e->type != ButtonRelease && e->type != MotionNotify )
        return false;
    foreach( InputWindowPair pos, input_windows )
        {
        if( pos.second == e->xany.window )
            {
            switch( e->type )
                {
                case ButtonPress:
                case ButtonRelease:
                    {
                    XButtonEvent* e2 = &e->xbutton;
                    QMouseEvent ev( e->type == ButtonPress ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                        QPoint( e2->x, e2->y ), QPoint( e2->x_root, e2->y_root ), x11ToQtMouseButton( e2->button ),
                        x11ToQtMouseButtons( e2->state ), x11ToQtKeyboardModifiers( e2->state ));
                    pos.first->windowInputMouseEvent( pos.second, &ev );
                    break; // --->
                    }
                case MotionNotify:
                    {
                    XMotionEvent* e2 = &e->xmotion;
                    QMouseEvent ev( QEvent::MouseMove, QPoint( e2->x, e2->y ), QPoint( e2->x_root, e2->y_root ),
                        Qt::NoButton, x11ToQtMouseButtons( e2->state ), x11ToQtKeyboardModifiers( e2->state ));
                    pos.first->windowInputMouseEvent( pos.second, &ev );
                    break; // --->
                    }
                }
            return true; // eat event
            }
        }
    return false;
    }

void EffectsHandler::checkInputWindowStacking()
    {
    if( input_windows.count() == 0 )
        return;
    Window* wins = new Window[ input_windows.count() ];
    int pos = 0;
    foreach( InputWindowPair it, input_windows )
        wins[ pos++ ] = it.second;
    XRaiseWindow( display(), wins[ 0 ] );
    XRestackWindows( display(), wins, pos );
    delete[] wins;
    }

void EffectsHandler::activateWindow( EffectWindow* c )
    {
    if( Client* cl = dynamic_cast< Client* >( c->window()))
        Workspace::self()->activateClient( cl, true );
    }

EffectsHandler* effects;

} // namespace
