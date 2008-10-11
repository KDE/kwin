/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "desktopgrid.h"

#include <math.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <klocale.h>
#include <kconfiggroup.h>
#include <netwm_def.h>
#include <QEvent>
#include <QMouseEvent>

namespace KWin
{

KWIN_EFFECT( desktopgrid, DesktopGridEffect )

DesktopGridEffect::DesktopGridEffect()
    : borderActivate( ElectricNone )
    , activated( false )
    , timeline()
    , keyboardGrab( false )
    , wasWindowMove( false )
    , windowMove( NULL )
    , windowMoveDiff()
    , gridSize()
    , orientation( Qt::Horizontal )
    , activeCell( 1, 1 )
    , scale()
    , unscaledBorder()
    , scaledSize()
    , scaledOffset()
    {
    // Load shortcuts
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*) actionCollection->addAction( "ShowDesktopGrid" );
    a->setText( i18n( "Show Desktop Grid" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F8 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle() ));

    // Load all other configuration details
    reconfigure( ReconfigureAll );
    }

DesktopGridEffect::~DesktopGridEffect()
    {
    effects->unreserveElectricBorder( borderActivate );
    }

void DesktopGridEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig( "DesktopGrid" );

    effects->unreserveElectricBorder( borderActivate );
    borderActivate = ElectricBorder( conf.readEntry( "BorderActivate", int( ElectricNone )));
    effects->reserveElectricBorder( borderActivate );

    zoomDuration = animationTime( conf, "ZoomDuration", 300 );
    timeline.setCurveShape( TimeLine::EaseInOutCurve );
    timeline.setDuration( zoomDuration );

    border = conf.readEntry( "BorderWidth", 10 );
    desktopNameAlignment = Qt::Alignment( conf.readEntry( "DesktopNameAlignment", 0 ));
    layoutMode = conf.readEntry( "LayoutMode", int( LayoutPager ));
    customLayoutRows = conf.readEntry( "CustomLayoutRows", 2 );
    }

//-----------------------------------------------------------------------------
// Screen painting

void DesktopGridEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( timeline.value() != 0 || activated )
        {
        if( activated )
            timeline.addTime(time);
        else
            timeline.removeTime(time);
        for( int i = 0; i < effects->numberOfDesktops(); i++ )
            {
            if( i == highlightedDesktop - 1 )
                hoverTimeline[i].addTime(time);
            else
                hoverTimeline[i].removeTime(time);
            }
        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if( timeline.value() != 0 )
            data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        if( !activated && timeline.value() == 0 )
            finish();
        }
    effects->prePaintScreen( data, time );
    }

void DesktopGridEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( timeline.value() == 0 )
        {
        effects->paintScreen( mask, region, data );
        return;
        }
    for( int desktop = 1; desktop <= effects->numberOfDesktops(); desktop++ )
        {
        ScreenPaintData d = data;
        paintingDesktop = desktop;
        effects->paintScreen( mask, region, d );
        }

    if( desktopNameAlignment )
        {
        double progress = timeline.value();
        QColor textColor( 255, 255, 255, 255 * progress );
        QColor bgColor( 0, 0, 0, 178 * progress ); // 70%
        QFont f;
        f.setBold( true );
        f.setPointSize( 12 );
        for( int screen = 0; screen < effects->numScreens(); screen++ )
            {
            QRect screenGeom = effects->clientArea( ScreenArea, screen, 0 );
            PaintClipper pc( screenGeom ); // TODO: Doesn't work in XRender for some reason?
            for( int desktop = 1; desktop <= effects->numberOfDesktops(); desktop++ )
                {
                QPointF posTL( scalePos( screenGeom.topLeft(), desktop, screen ));
                QPointF posBR( scalePos( screenGeom.bottomRight(), desktop, screen ));
                QRect textArea( posTL.x(), posTL.y(), posBR.x() - posTL.x(), posBR.y() - posTL.y() );
                textArea.adjust( textArea.width() / 10, textArea.height() / 10,
                    -textArea.width() / 10, -textArea.height() / 10 );
                effects->paintTextWithBackground( effects->desktopName( desktop ),
                    textArea, textColor, bgColor, f, desktopNameAlignment );
                }
            }
        }
    }

void DesktopGridEffect::postPaintScreen()
    {
    if( activated ? timeline.value() != 1 : timeline.value() != 0 )
        effects->addRepaintFull(); // Repaint during zoom
    if( activated )
        {
        for( int i = 0; i < effects->numberOfDesktops(); i++ )
            {
            if( hoverTimeline[i].value() != 0.0 && hoverTimeline[i].value() != 1.0 )
                { // Repaint during soft highlighting
                effects->addRepaintFull();
                break;
                }
            }
        }
    effects->postPaintScreen();
    }

//-----------------------------------------------------------------------------
// Window painting

void DesktopGridEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( timeline.value() != 0 )
        {
        if( w->isOnDesktop( paintingDesktop ))
            {
            w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
            data.mask |= PAINT_WINDOW_TRANSFORMED;

            // Split windows at screen edges
            for( int screen = 0; screen < effects->numScreens(); screen++ )
                {
                QRect screenGeom = effects->clientArea( ScreenArea, screen, 0 );
                if( w->x() < screenGeom.x() )
                    data.quads = data.quads.splitAtX( screenGeom.x() - w->x() );
                if( w->x() + w->width() > screenGeom.x() + screenGeom.width() )
                    data.quads = data.quads.splitAtX( screenGeom.x() + screenGeom.width() - w->x() );
                if( w->y() < screenGeom.y() )
                    data.quads = data.quads.splitAtY( screenGeom.y() - w->y() );
                if( w->y() + w->height() > screenGeom.y() + screenGeom.height() )
                    data.quads = data.quads.splitAtY( screenGeom.y() + screenGeom.height() - w->y() );
                }
            }
        else
            w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
        }
    effects->prePaintWindow( w, data, time );
    }

void DesktopGridEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( timeline.value() != 0 )
        {
        double xScale = data.xScale;
        double yScale = data.yScale;
        
        data.brightness *= 1.0 - ( 0.3 * ( 1.0 - hoverTimeline[paintingDesktop - 1].value() ));
        
        for( int screen = 0; screen < effects->numScreens(); screen++ )
            {
            QRect screenGeom = effects->clientArea( ScreenArea, screen, 0 );

            // Display all quads on the same screen on the same pass
            WindowQuadList screenQuads;
            foreach( const WindowQuad &quad, data.quads )
                {
                QRect quadRect(
                    w->x() + quad.left(), w->y() + quad.top(),
                    quad.right() - quad.left(), quad.bottom() - quad.top()
                    );
                if( quadRect.intersects( screenGeom ))
                    screenQuads.append( quad );
                }
            if( screenQuads.isEmpty() )
                continue; // Nothing is being displayed, don't bother
            WindowPaintData d = data;
            d.quads = screenQuads;

            QPointF newPos = scalePos( QPoint( w->x(), w->y() ), paintingDesktop, screen);
            double progress = timeline.value();
            d.xScale = interpolate( 1, xScale * scale[screen], progress);
            d.yScale = interpolate( 1, yScale * scale[screen], progress);
            d.xTranslate += qRound( newPos.x() - w->x() );
            d.yTranslate += qRound( newPos.y() - w->y() );

            if( effects->compositingType() == XRenderCompositing )
                { // More exact clipping as XRender displays the entire window instead of just the quad
                QPointF screenPosF = scalePos( screenGeom.topLeft(), paintingDesktop ).toPoint();
                QPoint screenPos(
                    qRound( screenPosF.x() ),
                    qRound( screenPosF.y() )
                    );
                QSize screenSize(
                    qRound( interpolate( screenGeom.width(), scaledSize[screen].width(), progress )),
                    qRound( interpolate( screenGeom.height(), scaledSize[screen].height(), progress ))
                    );
                PaintClipper pc( effects->clientArea( ScreenArea, screen, 0 ) & QRect( screenPos, screenSize ));
                effects->paintWindow( w, mask, region, d );
                }
            else
                {
                PaintClipper pc( effects->clientArea( ScreenArea, screen, 0 ));
                effects->paintWindow( w, mask, region, d );
                }
            }
        }
    else
        effects->paintWindow( w, mask, region, data );
    }

//-----------------------------------------------------------------------------
// User interaction

void DesktopGridEffect::windowClosed( EffectWindow* w )
    {
    if( w == windowMove )
        {
        effects->setElevatedWindow( windowMove, false );
        windowMove = NULL;
        }
    }

void DesktopGridEffect::windowInputMouseEvent( Window, QEvent* e )
    {
    if((   e->type() != QEvent::MouseMove
        && e->type() != QEvent::MouseButtonPress
        && e->type() != QEvent::MouseButtonRelease )
        || timeline.value() != 1 ) // Block user input during animations
        return;
    QMouseEvent* me = static_cast< QMouseEvent* >( e );
    if( e->type() == QEvent::MouseMove )
        {
        int d = posToDesktop( me->pos());
        if( windowMove != NULL )
            { // Handle window moving
            if( !wasWindowMove ) // Activate on move
                effects->activateWindow( windowMove );
            wasWindowMove = true;
            effects->moveWindow( windowMove, unscalePos( me->pos(), NULL ) + windowMoveDiff );
            // TODO: Window snap
            if( d != highlightedDesktop && !windowMove->isOnAllDesktops() )
                effects->windowToDesktop( windowMove, d ); // Not true all desktop move
            }
        if( d != highlightedDesktop ) // Highlight desktop
            setHighlightedDesktop( d );
        }
    if( e->type() == QEvent::MouseButtonPress )
        {
        if( me->buttons() == Qt::LeftButton )
            {
            QRect rect;
            EffectWindow* w = windowAt( me->pos());
            if( w != NULL && w->isMovable())
                { // Prepare it for moving
                windowMoveDiff = w->pos() - unscalePos( me->pos(), NULL );
                windowMove = w;
                }
            }
        else if(( me->buttons() == Qt::MidButton || me->buttons() == Qt::RightButton ) && windowMove == NULL )
            {
            EffectWindow* w = windowAt( me->pos());
            if( w != NULL )
                {
                if( w->isOnAllDesktops())
                    effects->windowToDesktop( w, posToDesktop( me->pos()));
                else
                    effects->windowToDesktop( w, NET::OnAllDesktops );
                effects->addRepaintFull();
                }
            }
        }
    if( e->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton )
        {
        if( wasWindowMove )
            effects->activateWindow( windowMove ); // Just in case it was deactivated
        else
            {
            setCurrentDesktop( highlightedDesktop );
            setActive( false );
            }
        windowMove = NULL;
        wasWindowMove = false;
        }
    }

void DesktopGridEffect::grabbedKeyboardEvent( QKeyEvent* e )
    {
    if( e->type() == QEvent::KeyPress )
        {
        int desktop = -1;
        // switch by F<number> or just <number>
        if( e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35 )
            desktop = e->key() - Qt::Key_F1 + 1;
        else if( e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9 )
            desktop = e->key() == Qt::Key_0 ? 10 : e->key() - Qt::Key_0;
        if( desktop != -1 )
            {
            if( desktop <= effects->numberOfDesktops())
                {
                setHighlightedDesktop( desktop );
                setCurrentDesktop( desktop );
                setActive( false );
                }
            return;
            }
        switch( e->key())
            { // Wrap only on autorepeat
            case Qt::Key_Left:
                setHighlightedDesktop( effects->desktopToLeft( highlightedDesktop, !e->isAutoRepeat()));
                break;
            case Qt::Key_Right:
                setHighlightedDesktop( effects->desktopToRight( highlightedDesktop, !e->isAutoRepeat()));
                break;
            case Qt::Key_Up:
                setHighlightedDesktop( effects->desktopUp( highlightedDesktop, !e->isAutoRepeat()));
                break;
            case Qt::Key_Down:
                setHighlightedDesktop( effects->desktopDown( highlightedDesktop, !e->isAutoRepeat()));
                break;
            case Qt::Key_Escape:
                setActive( false );
                return;
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Space:
                setCurrentDesktop( highlightedDesktop );
                setActive( false );
                return;
            default:
                break;
            }
        }
    }

bool DesktopGridEffect::borderActivated( ElectricBorder border )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return false;
    if( border == borderActivate && !activated )
        {
        toggle();
        return true;
        }
    return false;
    }

//-----------------------------------------------------------------------------
// Helper functions

// Transform a point to its position on the scaled grid
QPointF DesktopGridEffect::scalePos( const QPoint& pos, int desktop, int screen ) const
    {
    if( screen == -1 )
        screen = effects->screenNumber( pos );
    QRect screenGeom = effects->clientArea( ScreenArea, screen, 0 );
    QPoint desktopCell;
    if( orientation == Qt::Horizontal )
        {
        desktopCell.setX(( desktop - 1 ) % gridSize.width() + 1 );
        desktopCell.setY(( desktop - 1 ) / gridSize.width() + 1 );
        }
    else
        {
        desktopCell.setX(( desktop - 1 ) / gridSize.height() + 1 );
        desktopCell.setY(( desktop - 1 ) % gridSize.height() + 1 );
        }

    double progress = timeline.value();
    QPointF point(
        interpolate(
            (
                ( screenGeom.width() + unscaledBorder[screen] ) * ( desktopCell.x() - 1 )
                - ( screenGeom.width() + unscaledBorder[screen] ) * ( activeCell.x() - 1 )
            ) + pos.x(),
            (
                ( scaledSize[screen].width() + border ) * ( desktopCell.x() - 1 )
                + scaledOffset[screen].x()
                + ( pos.x() - screenGeom.x() ) * scale[screen]
            ),
            progress ),
        interpolate(
            (
                ( screenGeom.height() + unscaledBorder[screen] ) * ( desktopCell.y() - 1 )
                - ( screenGeom.height() + unscaledBorder[screen] ) * ( activeCell.y() - 1 )
            ) + pos.y(),
            (
                ( scaledSize[screen].height() + border ) * ( desktopCell.y() - 1 )
                + scaledOffset[screen].y()
                + ( pos.y() - screenGeom.y() ) * scale[screen]
            ),
            progress )
        );

    return point;
    }

// Detransform a point to its position on the full grid
// TODO: Doesn't correctly interpolate (Final position is correct though), don't forget to copy to posToDesktop()
QPoint DesktopGridEffect::unscalePos( const QPoint& pos, int* desktop ) const
    {
    int screen = effects->screenNumber( pos );
    QRect screenGeom = effects->clientArea( ScreenArea, screen, 0 );
    
    //double progress = timeline.value();
    double scaledX = /*interpolate(
        ( pos.x() - screenGeom.x() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.width() + unscaledBorder[screen] ) + activeCell.x() - 1,*/
        ( pos.x() - scaledOffset[screen].x() + double( border ) / 2.0 ) / ( scaledSize[screen].width() + border )/*,
        progress )*/;
    double scaledY = /*interpolate(
        ( pos.y() - screenGeom.y() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.height() + unscaledBorder[screen] ) + activeCell.y() - 1,*/
        ( pos.y() - scaledOffset[screen].y() + double( border ) / 2.0 ) / ( scaledSize[screen].height() + border )/*,
        progress )*/;
    int gx = qBound( 0, int( scaledX ), gridSize.width() - 1 ); // Zero-based
    int gy = qBound( 0, int( scaledY ), gridSize.height() - 1 );
    scaledX -= gx;
    scaledY -= gy;
    if( desktop != NULL )
        {
        if( orientation == Qt::Horizontal )
            *desktop = gy * gridSize.width() + gx + 1;
        else
            *desktop = gx * gridSize.height() + gy + 1;
        }

    return QPoint(
        qBound(
            screenGeom.x(),
            qRound(
                scaledX * ( screenGeom.width() + unscaledBorder[screen] )
                - unscaledBorder[screen] / 2.0
                + screenGeom.x()
                ),
            screenGeom.right()
            ),
        qBound(
            screenGeom.y(),
            qRound(
                scaledY * ( screenGeom.height() + unscaledBorder[screen] )
                - unscaledBorder[screen] / 2.0
                + screenGeom.y()
                ),
            screenGeom.bottom()
            )
        );
    }

int DesktopGridEffect::posToDesktop( const QPoint& pos ) const
    { // Copied from unscalePos()
    int screen = effects->screenNumber( pos );
    QRect screenGeom = effects->clientArea( ScreenArea, screen, 0 );
    
    //double progress = timeline.value();
    double scaledX = /*interpolate(
        ( pos.x() - screenGeom.x() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.width() + unscaledBorder[screen] ) + activeCell.x() - 1,*/
        ( pos.x() - scaledOffset[screen].x() + double( border ) / 2.0 ) / ( scaledSize[screen].width() + border )/*,
        progress )*/;
    double scaledY = /*interpolate(
        ( pos.y() - screenGeom.y() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.height() + unscaledBorder[screen] ) + activeCell.y() - 1,*/
        ( pos.y() - scaledOffset[screen].y() + double( border ) / 2.0 ) / ( scaledSize[screen].height() + border )/*,
        progress )*/;
    int gx = qBound( 0, int( scaledX ), gridSize.width() - 1 ); // Zero-based
    int gy = qBound( 0, int( scaledY ), gridSize.height() - 1 );
    scaledX -= gx;
    scaledY -= gy;
    if( orientation == Qt::Horizontal )
        return gy * gridSize.width() + gx + 1;
    return gx * gridSize.height() + gy + 1;
    }

EffectWindow* DesktopGridEffect::windowAt( QPoint pos ) const
    {
    // Get stacking order top first
    EffectWindowList windows = effects->stackingOrder();
    EffectWindowList::Iterator begin = windows.begin();
    EffectWindowList::Iterator end = windows.end();
    --end;
    while( begin < end )
        qSwap( *begin++, *end-- );

    int desktop;
    pos = unscalePos( pos, &desktop );
    foreach( EffectWindow* w, windows )
        if( w->isOnDesktop( desktop ) && w->geometry().contains( pos ))
            return w;
    return NULL;
    }

void DesktopGridEffect::setCurrentDesktop( int desktop )
    {
    if( orientation == Qt::Horizontal )
        {
        activeCell.setX(( desktop - 1 ) % gridSize.width() + 1 );
        activeCell.setY(( desktop - 1 ) / gridSize.width() + 1 );
        }
    else
        {
        activeCell.setX(( desktop - 1 ) / gridSize.height() + 1 );
        activeCell.setY(( desktop - 1 ) % gridSize.height() + 1 );
        }
    if( effects->currentDesktop() != desktop )
        effects->setCurrentDesktop( desktop );
    }

void DesktopGridEffect::setHighlightedDesktop( int d )
    {
    if( d == highlightedDesktop || d <= 0 || d > effects->numberOfDesktops() )
        return;
    highlightedDesktop = d;
    effects->addRepaintFull();
    }

//-----------------------------------------------------------------------------
// Activation

void DesktopGridEffect::toggle()
    {
    setActive( !activated );
    }

void DesktopGridEffect::setActive( bool active )
    {
    if( effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this )
        return; // Only one fullscreen effect at a time thanks
    if( effects->numberOfDesktops() < 2 )
        return; // No point if there is only one desktop
    if( activated == active )
        return; // Already in that state

    activated = active;
    if( activated && timeline.value() == 0 )
        setup();
    effects->addRepaintFull();
    }

void DesktopGridEffect::setup()
    {
    keyboardGrab = effects->grabKeyboard( this );
    input = effects->createInputWindow( this, 0, 0, displayWidth(), displayHeight(),
        Qt::PointingHandCursor );
    effects->setActiveFullScreenEffect( this );
    setHighlightedDesktop( effects->currentDesktop() );

    // Soft highlighting
    hoverTimeline.clear();
    for( int i = 0; i < effects->numberOfDesktops(); i++ )
        {
        TimeLine newTimeline( animationTime( 250 ));
        newTimeline.setCurveShape( TimeLine::EaseInOutCurve );
        hoverTimeline.append( newTimeline );
        }
    hoverTimeline[effects->currentDesktop() - 1].setProgress( 1.0 );

    // We need these variables for every paint so lets cache them
    int x, y;
    int numDesktops = effects->numberOfDesktops();
    switch( layoutMode )
        {
        default:
        case LayoutPager:
            effects->calcDesktopLayout( &gridSize.rwidth(), &gridSize.rheight(), &orientation );
            break;
        case LayoutAutomatic:
            y = sqrt( numDesktops ) + 0.5;
            x = float( numDesktops ) / float( y ) + 0.5;
            if( x * y < numDesktops )
                x++;
            orientation = Qt::Horizontal;
            gridSize.setWidth( x );
            gridSize.setHeight( y );
            break;
        case LayoutCustom:
            effects->calcDesktopLayout( &gridSize.rwidth(), &gridSize.rheight(), &orientation );
            gridSize.setWidth( ceil( effects->numberOfDesktops() / double( customLayoutRows )));
            gridSize.setHeight( customLayoutRows );
            break;
        }
    setCurrentDesktop( effects->currentDesktop() );
    scale.clear();
    unscaledBorder.clear();
    scaledSize.clear();
    scaledOffset.clear();
    for( int i = 0; i < effects->numScreens(); i++ )
        {
        QRect geom = effects->clientArea( ScreenArea, i, 0 );
        double sScale;
        if( gridSize.width() > gridSize.height() )
            sScale = ( geom.width() - border * ( gridSize.width() + 1 )) / double( geom.width() * gridSize.width() );
        else
            sScale = ( geom.height() - border * ( gridSize.height() + 1 )) / double( geom.height() * gridSize.height() );
        double sBorder = border / sScale;
        QSizeF size(
            double( geom.width() ) * sScale,
            double( geom.height() ) * sScale
            );
        QPointF offset(
            geom.x() + ( geom.width() - size.width() * gridSize.width() - border * ( gridSize.width() - 1 )) / 2.0,
            geom.y() + ( geom.height() - size.height() * gridSize.height() - border * ( gridSize.height() - 1 )) / 2.0
            );
        scale.append( sScale );
        unscaledBorder.append( sBorder );
        scaledSize.append( size );
        scaledOffset.append( offset );
        }
    }

void DesktopGridEffect::finish()
    {
    if( keyboardGrab )
        effects->ungrabKeyboard();
    keyboardGrab = false;
    effects->destroyInputWindow( input );
    effects->setActiveFullScreenEffect( 0 );
    }

} // namespace

#include "desktopgrid.moc"
