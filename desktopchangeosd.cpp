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


#include "desktopchangeosd.h"
#include <QTextStream>
#include "workspace.h"

#include <X11/extensions/shape.h>

#include <QHash>
#include <QGraphicsScene>
#include <QRect>
#include <KDE/Plasma/Theme>
#include <KDE/Plasma/Animator>
#include <KDE/Plasma/PaintUtils>
#include <kiconloader.h>

namespace KWin
{

DesktopChangeOSD::DesktopChangeOSD( Workspace* ws )
    : QGraphicsView()
    , m_wspace( ws )
    , m_scene( 0 )
    , m_active( false )
    , m_show( false )
    , m_delayTime( 0 )
    {
    setWindowFlags( Qt::X11BypassWindowManagerHint );
    setFrameStyle( QFrame::NoFrame );
    viewport()->setAutoFillBackground( false );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setAttribute( Qt::WA_TranslucentBackground );
    m_frame.setImagePath( "dialogs/background" );
    m_frame.setCacheAllRenderedFrames( true );
    m_frame.setEnabledBorders( Plasma::FrameSvg::AllBorders );

    m_item_frame.setImagePath( "widgets/pager" );
    m_item_frame.setCacheAllRenderedFrames( true );
    m_item_frame.setEnabledBorders( Plasma::FrameSvg::AllBorders );

    reconfigure();

    m_delayedHideTimer.setSingleShot( true );
    connect( &m_delayedHideTimer, SIGNAL(timeout()), this, SLOT(hide()) );

    m_scene = new QGraphicsScene( 0 );
    setScene( m_scene );

    m_scene->addItem( new DesktopChangeText( m_wspace ) );
    }

DesktopChangeOSD::~DesktopChangeOSD()
    {
    delete m_scene;
    }

void DesktopChangeOSD::reconfigure()
    {
    KSharedConfigPtr c(KGlobal::config());
    const KConfigGroup cg = c->group( "PopupInfo" );
    m_show = cg.readEntry( "ShowPopup", false );
    m_delayTime = cg.readEntry( "PopupHideDelay", 1500 );
    }

void DesktopChangeOSD::desktopChanged( int old )
    {
    if( !m_show )
        return;
    // we have to stop in case the old desktop does not exist anymore
    if( old > m_wspace->numberOfDesktops() )
        return;
    // calculate where icons have to be shown
    QPoint diff = m_wspace->desktopGridCoords( m_wspace->currentDesktop() ) - m_wspace->desktopGridCoords( old );
    QHash< int, DesktopChangeItem::Arrow > hash = QHash< int, DesktopChangeItem::Arrow>();
    int desktop = old;
    int target = m_wspace->currentDesktop();
    int x = diff.x();
    int y = diff.y();
    if( y >= 0 )
        {
        // first go in x direction, then in y
        while( desktop != target )
            {
            if( x != 0 )
                {
                if( x < 0 )
                    {
                    x++;
                    hash.insert( desktop, DesktopChangeItem::LEFT );
                    desktop = m_wspace->desktopToLeft( desktop );
                    }
                else
                    {
                    x--;
                    hash.insert( desktop, DesktopChangeItem::RIGHT );
                    desktop = m_wspace->desktopToRight( desktop );
                    }
                continue;
                }
            y--;
            hash.insert( desktop, DesktopChangeItem::DOWN );
            desktop = m_wspace->desktopBelow( desktop );
            }
        }
    else
        {
        // first go in y direction, then in x
        while( target != desktop )
            {
            if( y != 0 )
                {
                // only go upward
                y++;
                hash.insert( desktop, DesktopChangeItem::UP );
                desktop = m_wspace->desktopAbove( desktop );
                continue;
                }
            if( x != 0 )
                {
                if( x < 0 )
                    {
                    x++;
                    hash.insert( desktop, DesktopChangeItem::LEFT );
                    desktop = m_wspace->desktopToLeft( desktop );
                    }
                else
                    {
                    x--;
                    hash.insert( desktop, DesktopChangeItem::RIGHT );
                    desktop = m_wspace->desktopToRight( desktop );
                    }
                }
            }
        }
    // now we know which desktop has to show an arrow -> set the arrow for each desktop
    int numberOfArrows = qAbs( diff.x() ) + qAbs( diff.y() );
    foreach( QGraphicsItem* it, m_scene->items() )
        {
        DesktopChangeItem* item = qgraphicsitem_cast< DesktopChangeItem* >( it );
        if( item )
            {
            if( hash.contains( item->desktop() ) )
                {
                QPoint distance = m_wspace->desktopGridCoords( m_wspace->currentDesktop() )
                    - m_wspace->desktopGridCoords( item->desktop() );
                int desktopDistance = numberOfArrows - ( qAbs( distance.x() ) + qAbs( distance.y() ) ); 
                int start = m_delayTime/numberOfArrows * desktopDistance - m_delayTime*0.15f;
                int stop = m_delayTime/numberOfArrows * (desktopDistance + 1) + m_delayTime*0.15f;
                start = qMax( start, 0 );
                item->setArrow( hash[ item->desktop() ], start, stop );
                }
            else
                {
                item->setArrow( DesktopChangeItem::NONE, 0, 0 );
                }
            if( old != m_wspace->currentDesktop() )
                {
                if( item->desktop() == m_wspace->currentDesktop() )
                    item->startDesktopHighlightAnimation( m_delayTime * 0.33 );
                if( m_active && item->desktop() == old )
                    item->stopDesktopHightlightAnimation( m_delayTime * 0.33 );
                }
            }
        }
    if( m_active )
        {
        // already active - just update and reset timer
        update();
        }
    else
        {
        m_active = true;
        resize();
        show();
        raise();
        }
    // Set a zero inputmask, effectively making clicks go "through" the popup
    // For those who impatiently wait to click on a dialog behind the it
    XShapeCombineRectangles( display(), winId(), ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted );
    m_delayedHideTimer.start( m_delayTime );
    }

void DesktopChangeOSD::hideEvent( QHideEvent* )
    {
    m_delayedHideTimer.stop();
    m_active = false;
    }

void DesktopChangeOSD::drawBackground( QPainter* painter, const QRectF& rect )
    {
    painter->save();
    painter->setCompositionMode( QPainter::CompositionMode_Source );
    qreal left, top, right, bottom;
    m_frame.getMargins( left, top, right, bottom );
    m_frame.paintFrame( painter, rect.adjusted( -left, -top, right, bottom ) );
    painter->restore();
    }

void DesktopChangeOSD::numberDesktopsChanged()
    {
    foreach( QGraphicsItem* it, m_scene->items() )
        {
        DesktopChangeItem* item = qgraphicsitem_cast<DesktopChangeItem*>(it);
        if( item )
            {
            m_scene->removeItem( item );
            }
        }

    for( int i=1; i<=m_wspace->numberOfDesktops(); i++ )
        {
        DesktopChangeItem* item = new DesktopChangeItem( m_wspace, this, i );
        m_scene->addItem( item );
        }
    }

void DesktopChangeOSD::resize()
    {
    QRect screenRect = m_wspace->clientArea( ScreenArea, m_wspace->activeScreen(), m_wspace->currentDesktop() );
    QRect fullRect = m_wspace->clientArea( FullArea, m_wspace->activeScreen(), m_wspace->currentDesktop() );
    qreal left, top, right, bottom;
    m_frame.getMargins( left, top, right, bottom );

    QSize desktopGridSize = m_wspace->desktopGridSize();
    float itemWidth = fullRect.width()*0.1f;
    float itemHeight = fullRect.height()*0.1f;
    // 2 px distance between each desktop + each desktop a width of 5 % of full screen + borders
    float width = (desktopGridSize.width()-1)*2 + desktopGridSize.width()*itemWidth + left + right;
    float height = (desktopGridSize.height()-1)*2 + top + bottom;

    // bound width between ten and 33 percent of active screen
    float tempWidth = qBound( screenRect.width()*0.25f, width, screenRect.width()*0.5f );
    if( tempWidth != width )
        {
        // have to adjust the height
        width = tempWidth;
        itemWidth = (width - (desktopGridSize.width()-1)*2 - left - right)/desktopGridSize.width();
        itemHeight = itemWidth*(float)((float)fullRect.height()/(float)fullRect.width());
        }
    height += itemHeight*desktopGridSize.height();
    height += fontMetrics().height() + 4;

    // we do not increase height, but it's bound to a third of screen height
    float tempHeight = qMin( height, screenRect.height()*0.5f );
    float itemOffset = 0.0f;
    if( tempHeight != height )
        {
        // have to adjust item width
        height = tempHeight;
        itemHeight = (height - (fontMetrics().height() + 4) - top - bottom - (desktopGridSize.height()-1)*2)/
            desktopGridSize.height();
        itemOffset = itemWidth;
        itemWidth = itemHeight*(float)((float)fullRect.width()/(float)fullRect.height());
        itemOffset -= itemWidth;
        itemOffset *= (float)desktopGridSize.width()*0.5f;
        }

    QRect rect = QRect( screenRect.x() + (screenRect.width()-width)/2,
        screenRect.y() + (screenRect.height()-height)/2,
        width,
        height );
    setGeometry( rect );
    m_scene->setSceneRect( 0, 0, width, height );
    m_frame.resizeFrame( QSize( width, height ) );
    setMask( m_frame.mask() );

    // resize item frame
    m_item_frame.setElementPrefix( "normal" );
    m_item_frame.resizeFrame( QSize( itemWidth, itemHeight ) );
    m_item_frame.setElementPrefix( "hover" );
    m_item_frame.resizeFrame( QSize( itemWidth, itemHeight ) );

    // reset the items
    foreach( QGraphicsItem* it, m_scene->items() )
        {
        DesktopChangeItem* item = qgraphicsitem_cast<DesktopChangeItem*>(it);
        if( item )
            {
            item->setWidth( itemWidth );
            item->setHeight( itemHeight );
            QPoint coords = m_wspace->desktopGridCoords( item->desktop() );
            item->setPos( left + itemOffset + coords.x()*(itemWidth+2),
                top + fontMetrics().height() + 4 + coords.y()*(itemHeight+4) );
            }
        DesktopChangeText* text = qgraphicsitem_cast<DesktopChangeText*>(it);
        if( text )
            {
            text->setPos( left, top );
            text->setWidth( width - left - right );
            text->setHeight( fontMetrics().height() );
            }
        }
    }

//*******************************
// DesktopChangeText
//*******************************
DesktopChangeText::DesktopChangeText( Workspace* ws )
    : QGraphicsItem()
    , m_wspace( ws )
    , m_width( 0.0f )
    , m_height( 0.0f )
    {
    }

DesktopChangeText::~DesktopChangeText()
    {
    }

QRectF DesktopChangeText::boundingRect() const
    {
    return QRectF( 0, 0, m_width, m_height );
    }

void DesktopChangeText::paint( QPainter* painter, const QStyleOptionGraphicsItem* , QWidget* )
    {
    painter->setPen( Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor ) );
    painter->drawText( boundingRect(), Qt::AlignCenter | Qt:: AlignVCenter,
        m_wspace->desktopName( m_wspace->currentDesktop() ) );
    }

//*******************************
// DesktopChangeItem
//*******************************
DesktopChangeItem::DesktopChangeItem( Workspace* ws, DesktopChangeOSD* parent, int desktop )
    : QGraphicsItem()
    , m_wspace( ws )
    , m_parent( parent )
    , m_desktop( desktop )
    , m_width( 0.0f )
    , m_height( 0.0f )
    , m_hightlight_anim_id( 0 )
    , m_fadein_highlight( 0 )
    , m_arrow( NONE )
    , m_arrow_shown( 0 )
    , m_arrow_anim_id( 0 )
    , m_fadein_arrow( 0 )
    , m_arrow_progress( 0.0 )
    {
    m_delayed_show_arrow_timer.setSingleShot( true );
    m_delayed_hide_arrow_timer.setSingleShot( true );
    connect( &m_delayed_show_arrow_timer, SIGNAL(timeout()), this, SLOT(showArrow()));
    connect( &m_delayed_hide_arrow_timer, SIGNAL(timeout()), this, SLOT(hideArrow()));
    connect( Plasma::Animator::self(), SIGNAL(customAnimationFinished(int)), this, SLOT(animationFinished(int)));
    }

DesktopChangeItem::~DesktopChangeItem()
    {
    }

QRectF DesktopChangeItem::boundingRect() const
    {
    return QRectF( 0, 0, m_width, m_height );
    }

void DesktopChangeItem::setArrow( Arrow arrow, int start_delay, int hide_delay )
    {
    // stop timers
    m_delayed_show_arrow_timer.stop();
    m_delayed_hide_arrow_timer.stop();
    if( m_arrow_anim_id != 0 )
        {
        Plasma::Animator::self()->stopCustomAnimation( m_arrow_anim_id );
        m_arrow_anim_id = 0;
        }
    m_arrow_shown = false;
    m_arrow = arrow;
    if( m_arrow != NONE )
        {
        m_delayed_show_arrow_timer.start( start_delay );
        m_delayed_hide_arrow_timer.start( hide_delay );
        }
    }

void DesktopChangeItem::showArrow()
    {
    m_arrow_shown = true;
    if( m_arrow_anim_id != 0 )
        {
        Plasma::Animator::self()->stopCustomAnimation( m_arrow_anim_id );
        }
    m_fadein_arrow = true;
    m_arrow_anim_id = Plasma::Animator::self()->customAnimation(40 / (1000 / (m_parent->getDelayTime()*0.15f)),
        m_parent->getDelayTime()*0.15f, Plasma::Animator::EaseInCurve, this, "animationUpdate" );
    }

void DesktopChangeItem::hideArrow()
    {
    if( m_arrow_anim_id != 0 )
        {
        Plasma::Animator::self()->stopCustomAnimation( m_arrow_anim_id );
        }
    m_fadein_arrow = false;
    m_arrow_anim_id = Plasma::Animator::self()->customAnimation(40 / (1000 / (m_parent->getDelayTime()*0.15f)),
        m_parent->getDelayTime()*0.15f, Plasma::Animator::EaseOutCurve, this, "animationUpdate" );
    }

void DesktopChangeItem::startDesktopHighlightAnimation( int time )
    {
    if( m_hightlight_anim_id != 0 )
        {
        // stop current animation
        Plasma::Animator::self()->stopCustomAnimation( m_hightlight_anim_id );
        }
    m_fadein_highlight = true;
    m_hightlight_anim_id = Plasma::Animator::self()->customAnimation(40 / (1000 / (qreal)time),
        time, Plasma::Animator::EaseInCurve, this, "animationUpdate" );
    }

void DesktopChangeItem::stopDesktopHightlightAnimation( int time )
    {
    if( m_hightlight_anim_id != 0 )
        {
        // stop current animation
        Plasma::Animator::self()->stopCustomAnimation( m_hightlight_anim_id );
        }
    m_fadein_highlight = false;
    m_hightlight_anim_id = Plasma::Animator::self()->customAnimation(40 / (1000 / (qreal)time),
        time, Plasma::Animator::EaseOutCurve, this, "animationUpdate" );
    }

void DesktopChangeItem::animationUpdate( qreal progress, int id )
    {
    if( id == m_hightlight_anim_id )
        {
        if( m_fadein_highlight )
            m_hightlight_progress = progress;
        else
            m_hightlight_progress = 1.0 - progress;
        update();
        }
    if( id == m_arrow_anim_id )
        {
        if( m_fadein_arrow )
            m_arrow_progress = progress;
        else
            m_arrow_progress = 1.0 - progress;
        update();
        }
    }

void DesktopChangeItem::animationFinished( int id )
    {
    if( id == m_hightlight_anim_id )
        {
        m_hightlight_anim_id = 0;
        update();
        }
    if( id == m_arrow_anim_id )
        {
        m_arrow_anim_id = 0;
        if( !m_fadein_arrow )
            m_arrow_shown = false;
        update();
        }
    }

void DesktopChangeItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* , QWidget* )
    {
    if( m_wspace->currentDesktop() == m_desktop || m_hightlight_anim_id != 0 )
        {
        qreal left, top, right, bottom;
        m_parent->itemFrame()->getMargins( left, top, right, bottom );
        if( m_hightlight_anim_id )
            {
            // there is an animation - so we use transition from normal to active or vice versa
            if( m_fadein_highlight )
                {
                m_parent->itemFrame()->setElementPrefix( "normal" );
                QPixmap normal = m_parent->itemFrame()->framePixmap();
                m_parent->itemFrame()->setElementPrefix( "hover" );
                QPixmap result = Plasma::PaintUtils::transition( normal,
                    m_parent->itemFrame()->framePixmap(), m_hightlight_progress );
                painter->drawPixmap( boundingRect().toRect(), result);
                }
            else
                {
                m_parent->itemFrame()->setElementPrefix( "hover" );
                QPixmap normal = m_parent->itemFrame()->framePixmap();
                m_parent->itemFrame()->setElementPrefix( "normal" );
                QPixmap result = Plasma::PaintUtils::transition( normal,
                    m_parent->itemFrame()->framePixmap(), 1.0 - m_hightlight_progress );
                painter->drawPixmap( boundingRect().toRect(), result);
                }
            }
        else
            {
            // no animation - just render the active frame
            m_parent->itemFrame()->setElementPrefix( "hover" );
            m_parent->itemFrame()->paintFrame( painter, boundingRect() );
            }

        QColor rectColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
        rectColor.setAlphaF( 0.6 * m_hightlight_progress );
        QBrush rectBrush = QBrush( rectColor );
        painter->fillRect( boundingRect().adjusted( left, top, -right, -bottom ), rectBrush );
        }
    else
        {
        m_parent->itemFrame()->setElementPrefix( "normal" );
        m_parent->itemFrame()->paintFrame( painter, boundingRect() );
        }

    if( !m_arrow_shown )
        return;
    // paint the arrow
    QPixmap icon;
    int iconWidth = 32;
    qreal maxsize = qMin( boundingRect().width(), boundingRect().height() );
    if( maxsize > 128.0 )
        iconWidth = 128;
    else if( maxsize > 64.0 )
        iconWidth = 64.0;
    else if( maxsize > 32.0 )
        iconWidth = 32.0;
    else
        iconWidth = 16.0;
    QRect iconRect = QRect( boundingRect().x() + boundingRect().width()/2 - iconWidth/2,
        boundingRect().y() + boundingRect().height()/2 - iconWidth/2,
        iconWidth, iconWidth );
    switch( m_arrow )
        {
        case UP:
            icon = KIconLoader::global()->loadIcon( "go-up", KIconLoader::Desktop, iconWidth );
            break;
        case DOWN:
            icon = KIconLoader::global()->loadIcon( "go-down", KIconLoader::Desktop, iconWidth );
            break;
        case LEFT:
            icon = KIconLoader::global()->loadIcon( "go-previous", KIconLoader::Desktop, iconWidth );
            break;
        case RIGHT:
            icon = KIconLoader::global()->loadIcon( "go-next", KIconLoader::Desktop, iconWidth );
            break;
        default:
            break;
        }
    if( m_arrow != NONE )
        {
        if( m_arrow_anim_id != 0 )
            {
            QPixmap temp( icon.size() );
            temp.fill( Qt::transparent );

            QPainter p( &temp );
            p.setCompositionMode( QPainter::CompositionMode_Source );
            p.drawPixmap( 0, 0, icon );
            p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
            p.fillRect( temp.rect(), QColor( 0, 0, 0, 255*m_arrow_progress ) );
            p.end();

            icon = temp;
            }
        painter->drawPixmap( iconRect, icon );
        }
    }
}

#include "desktopchangeosd.moc"
