/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "effects.h"

#include "deleted.h"
#include "client.h"
#include "workspace.h"

#include "kdebug.h"

#include <assert.h>


namespace KWin
{


EffectsHandlerImpl::EffectsHandlerImpl(CompositingType type) : EffectsHandler(type)
    {
    foreach( const QString& effect, options->defaultEffects )
        loadEffect( effect );
    }

EffectsHandlerImpl::~EffectsHandlerImpl()
    {
    // Can't be done in EffectsHandler since it would result in pure virtuals
    //  being called
    foreach( EffectPair ep, loaded_effects )
        unloadEffect( ep.first );

    foreach( InputWindowPair pos, input_windows )
        XDestroyWindow( display(), pos.second );
    }

// the idea is that effects call this function again which calls the next one
void EffectsHandlerImpl::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->prePaintScreen( mask, region, time );
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandlerImpl::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->paintScreen( mask, region, data );
        --current_paint_screen;
        }
    else
        scene->finalPaintScreen( mask, region, data );
    }

void EffectsHandlerImpl::postPaintScreen()
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->postPaintScreen();
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandlerImpl::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->prePaintWindow( w, mask, paint, clip, time );
        --current_paint_window;
        }
    // no special final code
    }

void EffectsHandlerImpl::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->paintWindow( w, mask, region, data );
        --current_paint_window;
        }
    else
        scene->finalPaintWindow( static_cast<EffectWindowImpl*>( w ), mask, region, data );
    }

void EffectsHandlerImpl::postPaintWindow( EffectWindow* w )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->postPaintWindow( w );
        --current_paint_window;
        }
    // no special final code
    }

void EffectsHandlerImpl::drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( current_draw_window < loaded_effects.size())
        {
        loaded_effects[current_draw_window++].second->drawWindow( w, mask, region, data );
        --current_draw_window;
        }
    else
        scene->finalDrawWindow( static_cast<EffectWindowImpl*>( w ), mask, region, data );
    }

void EffectsHandlerImpl::windowOpacityChanged( EffectWindow* c, double old_opacity )
    {
    if( static_cast<EffectWindowImpl*>(c)->window()->opacity() == old_opacity )
        return;
    EffectsHandler::windowOpacityChanged( c, old_opacity );
    }

void EffectsHandlerImpl::activateWindow( EffectWindow* c )
    {
    if( Client* cl = dynamic_cast< Client* >( static_cast<EffectWindowImpl*>(c)->window()))
        Workspace::self()->activateClient( cl, true );
    }

int EffectsHandlerImpl::currentDesktop() const
    {
    return Workspace::self()->currentDesktop();
    }

int EffectsHandlerImpl::displayWidth() const
{
    return KWin::displayWidth();
}

int EffectsHandlerImpl::displayHeight() const
{
    return KWin::displayWidth();
}

QPoint EffectsHandlerImpl::cursorPos() const
{
    return KWin::cursorPos();
}

EffectWindowList EffectsHandlerImpl::stackingOrder() const
{
    ClientList list = Workspace::self()->stackingOrder();
    EffectWindowList ret;
    foreach( Client* c, list )
        ret.append( effectWindow( c ));
    return ret;
}

void EffectsHandlerImpl::addRepaintFull()
{
    Workspace::self()->addRepaintFull();
}

QRect EffectsHandlerImpl::clientArea( clientAreaOption opt, const QPoint& p, int desktop ) const
{
    return Workspace::self()->clientArea( opt, p, desktop );
}

Window EffectsHandlerImpl::createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor )
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

void EffectsHandlerImpl::destroyInputWindow( Window w )
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

bool EffectsHandlerImpl::checkInputWindowEvent( XEvent* e )
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
void EffectsHandlerImpl::checkInputWindowStacking()
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

void EffectsHandlerImpl::checkElectricBorder(const QPoint &pos, Time time)
{
    Workspace::self()->checkElectricBorder( pos, time );
}

void EffectsHandlerImpl::reserveElectricBorder( ElectricBorder border )
{
    Workspace::self()->reserveElectricBorder( border );
}

void EffectsHandlerImpl::unreserveElectricBorder( ElectricBorder border )
{
    Workspace::self()->unreserveElectricBorder( border );
}

void EffectsHandlerImpl::reserveElectricBorderSwitching( bool reserve )
{
    Workspace::self()->reserveElectricBorderSwitching( reserve );
}

//****************************************
// EffectWindowImpl
//****************************************

EffectWindowImpl::EffectWindowImpl() : EffectWindow()
    , toplevel( NULL )
    , sw( NULL )
    {
    }

EffectWindowImpl::~EffectWindowImpl()
    {
    }

void EffectWindowImpl::enablePainting( int reason )
    {
    sceneWindow()->enablePainting( reason );
    }

void EffectWindowImpl::disablePainting( int reason )
    {
    sceneWindow()->disablePainting( reason );
    }

int EffectWindowImpl::desktop() const
    {
    return toplevel->desktop();
    }

bool EffectWindowImpl::isOnAllDesktops() const
    {
    return desktop() == NET::OnAllDesktops;
    }

QString EffectWindowImpl::caption() const
    {
    if( Client* c = dynamic_cast<Client*>( toplevel ))
        return c->caption();
    else
        return "";
    }

bool EffectWindowImpl::isMinimized() const
    {
    if( Client* c = dynamic_cast<Client*>( toplevel ))
        return c->isMinimized();
    else
        return false;
    }

bool EffectWindowImpl::isDeleted() const
    {
    return (dynamic_cast<Deleted*>( toplevel ) != 0);
    }

const Toplevel* EffectWindowImpl::window() const
    {
    return toplevel;
    }

Toplevel* EffectWindowImpl::window()
    {
    return toplevel;
    }

void EffectWindowImpl::setWindow( Toplevel* w )
    {
    toplevel = w;
    }

void EffectWindowImpl::setSceneWindow( Scene::Window* w )
    {
    sw = w;
    }

Scene::Window* EffectWindowImpl::sceneWindow()
    {
    return sw;
    }

int EffectWindowImpl::x() const
    {
    return toplevel->x();
    }

int EffectWindowImpl::y() const
    {
    return toplevel->y();
    }

int EffectWindowImpl::width() const
    {
    return toplevel->width();
    }

int EffectWindowImpl::height() const
    {
    return toplevel->height();
    }

QRect EffectWindowImpl::geometry() const
    {
    return toplevel->geometry();
    }

QSize EffectWindowImpl::size() const
    {
    return toplevel->size();
    }

QPoint EffectWindowImpl::pos() const
    {
    return toplevel->pos();
    }

QRect EffectWindowImpl::rect() const
    {
    return toplevel->rect();
    }

bool EffectWindowImpl::isDesktop() const
    {
    return toplevel->isDesktop();
    }

bool EffectWindowImpl::isDock() const
    {
    return toplevel->isDock();
    }

bool EffectWindowImpl::isToolbar() const
    {
    return toplevel->isToolbar();
    }

bool EffectWindowImpl::isTopMenu() const
    {
    return toplevel->isTopMenu();
    }

bool EffectWindowImpl::isMenu() const
    {
    return toplevel->isMenu();
    }

bool EffectWindowImpl::isNormalWindow() const
    {
    return toplevel->isNormalWindow();
    }

bool EffectWindowImpl::isSpecialWindow() const
    {
    if( Client* c = dynamic_cast<Client*>( toplevel ))
        return c->isSpecialWindow();
    else
        return false;
    }

bool EffectWindowImpl::isDialog() const
    {
    return toplevel->isDialog();
    }

bool EffectWindowImpl::isSplash() const
    {
    return toplevel->isSplash();
    }

bool EffectWindowImpl::isUtility() const
    {
    return toplevel->isUtility();
    }

bool EffectWindowImpl::isDropdownMenu() const
    {
    return toplevel->isDropdownMenu();
    }

bool EffectWindowImpl::isPopupMenu() const
    {
    return toplevel->isPopupMenu();
    }

bool EffectWindowImpl::isTooltip() const
    {
    return toplevel->isTooltip();
    }

bool EffectWindowImpl::isNotification() const
    {
    return toplevel->isNotification();
    }

bool EffectWindowImpl::isComboBox() const
    {
    return toplevel->isComboBox();
    }

bool EffectWindowImpl::isDNDIcon() const
    {
    return toplevel->isDNDIcon();
    }

EffectWindow* effectWindow( Toplevel* w )
    {
    EffectWindowImpl* ret = w->effectWindow();
    ret->setSceneWindow( NULL ); // just in case
    return ret;
    }

EffectWindow* effectWindow( Scene::Window* w )
    {
    EffectWindowImpl* ret = w->window()->effectWindow();
    ret->setSceneWindow( w );
    return ret;
    }


} // namespace
