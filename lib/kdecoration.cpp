/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#include "kdecoration.h"

#include <kdebug.h>
#include <QApplication>
#include <kglobal.h>
#include <assert.h>
#if defined Q_WS_X11 && ! defined K_WS_QTONLY
#include <X11/Xlib.h>
#include <fixx11h.h>
#endif

#include "kdecoration_p.h"
#include "kdecorationfactory.h"

KDecorationOptions* KDecoration::options_;

KDecoration::KDecoration( KDecorationBridge* bridge, KDecorationFactory* factory )
    :   bridge_( bridge ),
	w_( NULL ),
        factory_( factory )
    {
    factory->addDecoration( this );
    }

KDecoration::~KDecoration()
    {
    factory()->removeDecoration( this );
    delete w_;
    }

const KDecorationOptions* KDecoration::options()
    {
    return options_;
    }

void KDecoration::createMainWidget( Qt::WFlags flags )
    {
    // FRAME check flags?
    QWidget *w = new QWidget( initialParentWidget(), initialWFlags() | flags );
    w->setObjectName("decoration widget");
    w->setAttribute( Qt::WA_PaintOnScreen );
    setMainWidget(w);
    }

void KDecoration::setMainWidget( QWidget* w )
    {
    assert( w_ == NULL );
    w_ = w;
    w->setMouseTracking( true );
    widget()->resize( geometry().size());
    }

QWidget* KDecoration::initialParentWidget() const
    {
    return bridge_->initialParentWidget();
    }
    
Qt::WFlags KDecoration::initialWFlags() const
    {
    return bridge_->initialWFlags();
    }

bool KDecoration::isActive() const
    {
    return bridge_->isActive();
    }
    
bool KDecoration::isCloseable() const
    {
    return bridge_->isCloseable();
    }

bool KDecoration::isMaximizable() const
    {
    return bridge_->isMaximizable();
    }
    
KDecoration::MaximizeMode KDecoration::maximizeMode() const
    {
    return bridge_->maximizeMode();
    }
    
bool KDecoration::isMinimizable() const
    {
    return bridge_->isMinimizable();
    }

bool KDecoration::providesContextHelp() const
    {
    return bridge_->providesContextHelp();
    }
    
int KDecoration::desktop() const
    {
    return bridge_->desktop();
    }
    
bool KDecoration::isModal() const
    {
    return bridge_->isModal();
    }
    
bool KDecoration::isShadeable() const
    {
    return bridge_->isShadeable();
    }
    
bool KDecoration::isShade() const
    {
    return bridge_->isShade();
    }
    
bool KDecoration::isSetShade() const
    {
    return bridge_->isSetShade();
    }
    
bool KDecoration::keepAbove() const
    {
    return bridge_->keepAbove();
    }

bool KDecoration::keepBelow() const
    {
    return bridge_->keepBelow();
    }

bool KDecoration::isMovable() const
    {
    return bridge_->isMovable();
    }

bool KDecoration::isResizable() const
    {
    return bridge_->isResizable();
    }

NET::WindowType KDecoration::windowType( unsigned long supported_types ) const
    { // this one is also duplicated in KDecorationFactory
    return bridge_->windowType( supported_types );
    }

QIcon KDecoration::icon() const
    {
    return bridge_->icon();
    }
    
QString KDecoration::caption() const
    {
    return bridge_->caption();
    }

void KDecoration::processMousePressEvent( QMouseEvent* e )
    {
    return bridge_->processMousePressEvent( e );
    }

void KDecoration::showWindowMenu( const QRect &pos )
    {
    bridge_->showWindowMenu( pos );
    }
    
void KDecoration::showWindowMenu( QPoint pos )
    {
    bridge_->showWindowMenu( pos );
    }

void KDecoration::performWindowOperation( WindowOperation op )
    {
    bridge_->performWindowOperation( op );
    }

void KDecoration::setMask( const QRegion& reg, int mode )
    {
    bridge_->setMask( reg, mode );
    }
    
void KDecoration::clearMask()
    {
    bridge_->setMask( QRegion(), 0 );
    }
    
bool KDecoration::isPreview() const
    {
    return bridge_->isPreview();
    }
    
QRect KDecoration::geometry() const
    {
    return bridge_->geometry();
    }
    
QRect KDecoration::iconGeometry() const
    {
    return bridge_->iconGeometry();
    }

QRegion KDecoration::unobscuredRegion( const QRegion& r ) const
    {
    return bridge_->unobscuredRegion( r );
    }

QWidget* KDecoration::workspaceWidget() const
    {
    return bridge_->workspaceWidget();
    }
        
WId KDecoration::windowId() const
    {
    return bridge_->windowId();
    }
        
void KDecoration::closeWindow()
    {
    bridge_->closeWindow();
    }

void KDecoration::maximize( Qt::MouseButtons button )
    {
    performWindowOperation( options()->operationMaxButtonClick( button ));
    }

void KDecoration::maximize( MaximizeMode mode )
    {
    bridge_->maximize( mode );
    }
    
void KDecoration::minimize()
    {
    bridge_->minimize();
    }

void KDecoration::showContextHelp()
    {
    bridge_->showContextHelp();
    }
    
void KDecoration::setDesktop( int desktop )
    {
    bridge_->setDesktop( desktop );
    }

void KDecoration::toggleOnAllDesktops()
    {
    if( isOnAllDesktops())
        setDesktop( bridge_->currentDesktop());
    else
        setDesktop( NET::OnAllDesktops );
    }

void KDecoration::titlebarDblClickOperation()
    {
    bridge_->titlebarDblClickOperation();
    }

void KDecoration::titlebarMouseWheelOperation( int delta )
    {
    bridge_->titlebarMouseWheelOperation( delta );
    }

void KDecoration::setShade( bool set )
    {
    bridge_->setShade( set );
    }
        
void KDecoration::setKeepAbove( bool set )
    {
    bridge_->setKeepAbove( set );
    }
    
void KDecoration::setKeepBelow( bool set )
    {
    bridge_->setKeepBelow( set );
    }

bool KDecoration::drawbound( const QRect&, bool )
    {
    return false;
    }

bool KDecoration::animateMinimize( bool )
    {
    return false;
    }

bool KDecoration::windowDocked( Position )
    {
    return false;
    }

void KDecoration::helperShowHide( bool show )
    {
    bridge_->helperShowHide( show );
    }

void KDecoration::reset( unsigned long )
    {
    }

void KDecoration::grabXServer()
    {
    bridge_->grabXServer( true );
    }
    
void KDecoration::ungrabXServer()
    {
    bridge_->grabXServer( false );
    }
    
KDecoration::Position KDecoration::mousePosition( const QPoint& p ) const
{
    const int range = 16;
    int bleft, bright, btop, bbottom;
    borders( bleft, bright, btop, bbottom );
    btop = qMin( btop, 4 ); // otherwise whole titlebar would have resize cursor

    Position m = PositionCenter;

    if ( ( p.x() > bleft && p.x() < widget()->width() - bright )
         && ( p.y() > btop && p.y() < widget()->height() - bbottom ) )
        return PositionCenter;

    if ( p.y() <= qMax( range, btop ) && p.x() <= qMax( range, bleft ))
        m = PositionTopLeft;
    else if ( p.y() >= widget()->height()- qMax( range, bbottom )
        && p.x() >= widget()->width()- qMax( range, bright ))
        m = PositionBottomRight;
    else if ( p.y() >= widget()->height()- qMax( range, bbottom ) && p.x() <= qMax( range, bleft ))
        m = PositionBottomLeft;
    else if ( p.y() <= qMax( range, btop ) && p.x() >= widget()->width()- qMax( range, bright ))
        m = PositionTopRight;
    else if ( p.y() <= btop )
        m = PositionTop;
    else if ( p.y() >= widget()->height()-bbottom )
        m = PositionBottom;
    else if ( p.x() <= bleft )
        m = PositionLeft;
    else if ( p.x() >= widget()->width()-bright )
        m = PositionRight;
    else
        m = PositionCenter;
    return m;
}

KDecorationOptions::KDecorationOptions()
    {
    assert( KDecoration::options_ == NULL );
    KDecoration::options_ = this;
    }

KDecorationOptions::~KDecorationOptions()
    {
    assert( KDecoration::options_ == this );
    KDecoration::options_ = NULL;
    }

const QColor& KDecorationOptions::color(ColorType type, bool active) const
{
    return(d->colors[type + (active ? 0 : NUM_COLORS)]);
}

const QFont& KDecorationOptions::font(bool active, bool small) const
{
    if ( small )
        return(active ? d->activeFontSmall : d->inactiveFontSmall);
    else
        return(active ? d->activeFont : d->inactiveFont);
}

const QPalette& KDecorationOptions::palette(ColorType type, bool active) const
{
    int idx = type + (active ? 0 : NUM_COLORS);
    if(d->pal[idx])
        return(*d->pal[idx]);
#warning KDE4 : why construct the palette this way?
		// TODO: Is this worth 'porting' to Qt4?
//     d->pal[idx] = new QPalette(Qt::black, d->colors[idx], d->colors[idx].light(150),
//                               d->colors[idx].dark(), d->colors[idx].dark(120),
//                               Qt::black, QApplication::palette().active().
//                               base());
    d->pal[idx] = new QPalette(d->colors[idx]);
    return(*d->pal[idx]);
}

bool KDecorationOptions::customButtonPositions() const
{
    return d->custom_button_positions;
}

QString KDecorationOptions::titleButtonsLeft() const
{
    return d->title_buttons_left;
}

QString KDecorationOptions::titleButtonsRight() const
{
    return d->title_buttons_right;
}

bool KDecorationOptions::showTooltips() const
{
    return d->show_tooltips;
}

KDecorationOptions::BorderSize KDecorationOptions::preferredBorderSize( KDecorationFactory* factory ) const
{
    assert( factory != NULL );
    if( d->cached_border_size == BordersCount ) // invalid
        d->cached_border_size = d->findPreferredBorderSize( d->border_size,
            factory->borderSizes());
    return d->cached_border_size;
}

bool KDecorationOptions::moveResizeMaximizedWindows() const
{
    return d->move_resize_maximized_windows;
}

KDecorationDefines::WindowOperation KDecorationOptions::operationMaxButtonClick( Qt::MouseButtons button ) const
    {
    return button == Qt::RightButton? d->OpMaxButtonRightClick : 
           button == Qt::MidButton?   d->OpMaxButtonMiddleClick :
                                      d->OpMaxButtonLeftClick;
    }

#warning Redo all the XYZChange() virtuals as signals.

#include "kdecoration.moc"
