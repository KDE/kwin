/********************************************************************
Tabstrip KWin window decoration
This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "tabstripdecoration.h"
#include "tabstripbutton.h"
#include "tabstripfactory.h"

#include <KLocale>

#include <QPainter>

TabstripDecoration::TabstripDecoration( KDecorationBridge *bridge, KDecorationFactory *factory )
    : KCommonDecorationUnstable( bridge, factory )
    {
    click_in_progress = false;
    drag_in_progress = false;
    button = Qt::NoButton;
    }

KCommonDecorationButton *TabstripDecoration::createButton( ButtonType type )
    {
    switch( type )
        {
        case HelpButton:
            return ( new TabstripButton( type, this, i18n("Help") ) );
            break;
        case MaxButton:
            return ( new TabstripButton( type, this, i18n("Maximize") ) );
            break;
        case MinButton:
            return ( new TabstripButton( type, this, i18n("Minimize") ) );
            break;
        case CloseButton:
            return ( new TabstripButton( type, this, i18n("Close") ) );
            break;
        case MenuButton:
            return ( new TabstripButton( type, this, i18n("Menu") ) );
            break;
        case OnAllDesktopsButton:
            return ( new TabstripButton( type, this, i18n("All Desktops") ) );
            break;
        case AboveButton:
            return ( new TabstripButton( type, this, i18n("Above") ) );
            break;
        case BelowButton:
            return ( new TabstripButton( type, this, i18n("Below") ) );
            break;
        case ShadeButton:
            return ( new TabstripButton( type, this, i18n("Shade") ) );
            break;
        default:
            break;
        };
    return 0;
    }

void TabstripDecoration::paintTab( QPainter &painter, const QRect &geom, ClientGroupItem &item, bool active )
    {
    // Determine painting colors
    QColor bgColor = options()->color( ColorTitleBar, active );
    QColor textColor = options()->color( ColorFont, active );

    // Draw border around the tab
    painter.setPen( Qt::black );
    painter.drawRect( geom.adjusted( 0, 0, -1, -1 ));
    painter.setPen( TabstripFactory::outlineColor() );
    painter.drawRect( geom.adjusted( 1, 1, -2, -2 ));

    // Background
    painter.fillRect( geom.adjusted( 2, 2, -2, -2 ), bgColor );

    // Window title and icon
    painter.setPen( textColor );
    if( TabstripFactory::showIcon() )
        {
        QRect rect( geom.x() + 25, geom.y(), geom.width() - 48, geom.height() );
        QRect text;
        QFont font = options()->font( active );
        QFontMetrics metrics( font );
        QString string = metrics.elidedText( item.title(), Qt::ElideRight, rect.width() );
        painter.setFont( font );
        painter.drawText( rect, TabstripFactory::titleAlign() | Qt::AlignVCenter, string, &text );
        painter.drawPixmap( text.x() - 22, rect.y() + 3, item.icon().pixmap( 16 ));
        }
    else
        {
        QRect rect( geom.x() + 5, geom.y(), geom.width() - 28, geom.height() );
        QFont font = options()->font( active );
        QFontMetrics metrics( font );
        QString string = metrics.elidedText( item.title(), Qt::ElideRight, rect.width() );
        painter.setFont( font );
        painter.drawText( rect, TabstripFactory::titleAlign() | Qt::AlignVCenter, string );
        }
    }

void TabstripDecoration::paintEvent( QPaintEvent * )
    {
    QPainter painter( widget() );

    // Determine painting colors
    QColor bgColor = options()->color( ColorTitleBar, isClientGroupActive() );
    QColor textColor = options()->color( ColorFont, isClientGroupActive() );

    // Determine section geometry
    QRect frame( QPoint( 0, 0 ), widget()->frameGeometry().size() );
    QRect titlebar( frame.topLeft(), QSize( frame.width(),
        layoutMetric( LM_TitleEdgeTop ) + layoutMetric( LM_TitleHeight ) +
        layoutMetric( LM_TitleEdgeBottom ) - 1 // Titlebar and main frame overlap by 1px
        ));

    // Slight optimization as we are drawing solid straight lines
    painter.setRenderHint( QPainter::Antialiasing, false );

    // Draw black/white border around the main window
    painter.setPen( Qt::black );
    painter.drawRect( 0, titlebar.height() - 1, frame.width() - 1, frame.height() - titlebar.height() );
    painter.setPen( TabstripFactory::outlineColor() );
    painter.drawRect( 1, titlebar.height(), frame.width() - 3, frame.height() - titlebar.height() - 2 );

    QList< ClientGroupItem > tabList = clientGroupItems();
    const int tabCount = tabList.count();

    // Delete unneeded tab close buttons
    while( tabCount < closeButtons.size() || ( tabCount == 1 && closeButtons.size() > 0 ))
        {
        TabstripButton *btn = closeButtons.takeFirst();
        btn->hide();
        btn->deleteLater();
        }

    if( tabCount > 1 )
        {
        QRect allTabGeom = titleRect().adjusted( -1, -layoutMetric( LM_TitleEdgeTop ), 1, 0 );
        QRect tabGeom = allTabGeom;
        tabGeom.setWidth( tabGeom.width() / tabCount + 1 ); // Split titlebar evenly
        for( int i = 0; i < tabCount; ++i )
            {
            // Last tab may have a different width due to rounding
            if( i == tabCount - 1 )
                tabGeom.setWidth( allTabGeom.width() - tabGeom.width() * i + i - 1 );

            // Actually paint the tab
            paintTab( painter, tabGeom, tabList[i], isActive() && visibleClientGroupItem() == i );

            // Create new close button if required
            if( i >= closeButtons.size() )
                closeButtons.append( new TabstripButton( ItemCloseButton, this, i18n( "Close Item" ) ) );
            closeButtons[ i ]->setActive( isActive() && visibleClientGroupItem() == i );
            closeButtons[ i ]->move( tabGeom.right() - 18, tabGeom.bottom() - 18 );
            closeButtons[ i ]->installEventFilter( this );
            closeButtons[ i ]->show();

            // Prepare for next iteration
            tabGeom.translate( tabGeom.width() - 1, 0 );
            }

        // Draw border around the buttons
        painter.setPen( Qt::black );
        painter.drawRect( 0, 0, allTabGeom.left(), allTabGeom.height() - 1 );
        painter.drawRect( allTabGeom.right() - 1, 0, frame.width() - allTabGeom.right(), allTabGeom.height() - 1 );
        painter.setPen( TabstripFactory::outlineColor() );
        painter.drawRect( 1, 1, allTabGeom.left() - 2, allTabGeom.height() - 3 );
        painter.drawRect( allTabGeom.right(), 1, frame.width() - allTabGeom.right() - 2, allTabGeom.height() - 3 );

        // Background behind the buttons
        painter.fillRect( 2, 2, allTabGeom.left() - 3, allTabGeom.height() - 4, bgColor );
        painter.fillRect( allTabGeom.right() + 1, 2, frame.width() - allTabGeom.right() - 3, allTabGeom.height() - 4, bgColor );
        }
    else
        {
        // Draw border around the titlebar
        painter.setPen( Qt::black );
        painter.drawRect( titlebar.adjusted( 0, 0, -1, -1 ));
        painter.setPen( TabstripFactory::outlineColor() );
        painter.drawRect( titlebar.adjusted( 1, 1, -2, -2 ));

        // Background
        painter.fillRect( titlebar.adjusted( 2, 2, -2, -2 ), bgColor );

        // Window title
        painter.setPen( textColor );
        QRect rect( titleRect().x() + 2, titleRect().y(),
            titleRect().width() - 6, titleRect().height() - 3 );
        QFont font = options()->font( isActive() );
        QFontMetrics metrics( font );
        QString string = metrics.elidedText( caption(), Qt::ElideRight, rect.width() );
        painter.setFont( font );
        painter.drawText( rect, TabstripFactory::titleAlign() | Qt::AlignVCenter, string );
        }
    }

QString TabstripDecoration::visibleName() const
    {
    return i18n("Tabstrip");
    }

void TabstripDecoration::init()
    {
    KCommonDecoration::init();
    widget()->setAutoFillBackground( false );
    widget()->setAttribute( Qt::WA_OpaquePaintEvent );
    widget()->setAcceptDrops( true );
    }


bool TabstripDecoration::eventFilter( QObject* o, QEvent* e )
    {
    if( TabstripButton *btn = dynamic_cast< TabstripButton* >( o ))
        {
        if( e->type() == QEvent::MouseButtonPress )
            return true; // No-op
        else if( e->type() == QEvent::MouseButtonRelease )
            {
            const QMouseEvent* me = static_cast< QMouseEvent* >( e );
            if( me->button() == Qt::LeftButton && btn->rect().contains( me->pos() ))
                closeClientGroupItem( closeButtons.indexOf( btn ));
            return true;
            }
        }

    bool state = false;
    if( e->type() == QEvent::MouseButtonPress )
        state = mouseButtonPressEvent( static_cast< QMouseEvent* >( e ));
    else if( e->type() == QEvent::MouseButtonRelease && widget() == o )
        state = mouseButtonReleaseEvent( static_cast< QMouseEvent* >( e ));
    else if( e->type() == QEvent::MouseMove )
        state = mouseMoveEvent( static_cast< QMouseEvent* >( e ));
    else if( e->type() == QEvent::DragEnter && widget() == o )
        state = dragEnterEvent( static_cast< QDragEnterEvent* >( e ));
    else if( e->type() == QEvent::DragMove && widget() == o )
        state = dragMoveEvent( static_cast< QDragMoveEvent* >( e ));
    else if( e->type() == QEvent::DragLeave && widget() == o )
        state = dragLeaveEvent( static_cast< QDragLeaveEvent* >( e ));
    else if( e->type() == QEvent::Drop && widget() == o )
        state = dropEvent( static_cast< QDropEvent* >( e ));

    return state || KCommonDecorationUnstable::eventFilter( o, e ); 
    }

bool TabstripDecoration::mouseButtonPressEvent( QMouseEvent* e )
    {
    click = widget()->mapToParent( e->pos() );
    int item = itemClicked( click );
    if( buttonToWindowOperation( e->button() ) == OperationsOp )
        {
        displayClientMenu( item, widget()->mapToGlobal( click ));
        return true;
        }
    if( item >= 0 )
        {
        click_in_progress = true;
        button = e->button();
        return true;
        }
    click_in_progress = false;
    return false;
    }

bool TabstripDecoration::mouseButtonReleaseEvent( QMouseEvent* e )
    {
    release = e->pos();
    int item = itemClicked( release );
    if( click_in_progress && item >= 0 )
        {
        click_in_progress = false;
        setVisibleClientGroupItem( item );
        return true;
        }
    click_in_progress = false;
    return false;
    }

bool TabstripDecoration::mouseMoveEvent( QMouseEvent* e )
    {
    QPoint c = e->pos();
    int item = itemClicked( c );
    if( item >= 0 && click_in_progress && buttonToWindowOperation( button ) == ClientGroupDragOp &&
        ( c - click ).manhattanLength() >= 4 )
        {
        click_in_progress = false;
        drag_in_progress = true;
        QDrag *drag = new QDrag( widget() );
        QMimeData *group_data = new QMimeData();
        group_data->setData( clientGroupItemDragMimeType(), QString().setNum( itemId( item )).toAscii() );
        drag->setMimeData( group_data );

        // Create draggable tab pixmap
        QList< ClientGroupItem > tabList = clientGroupItems();
        const int tabCount = tabList.count();
        QRect frame( QPoint( 0, 0 ), widget()->frameGeometry().size() );
        QRect titlebar( frame.topLeft(), QSize( frame.width(),
            layoutMetric( LM_TitleEdgeTop ) + layoutMetric( LM_TitleHeight ) +
            layoutMetric( LM_TitleEdgeBottom ) - 1 // Titlebar and main frame overlap by 1px
            ));
        QRect geom = titleRect().adjusted( -1, -layoutMetric( LM_TitleEdgeTop ), 1, 0 );
        geom.setWidth( geom.width() / tabCount + 1 ); // Split titlebar evenly
        geom.translate( geom.width() * item - item, 0 );
        QPixmap pix( geom.size() );
        QPainter painter( &pix );
        paintTab( painter, QRect( QPoint( 0, 0 ), geom.size() ), tabList[item],
            isActive() && visibleClientGroupItem() == item );
        drag->setPixmap( pix );
        // If the cursor is on top of the pixmap then it makes the movement jerky on some systems
        //drag->setHotSpot( QPoint( c.x() - geom.x(), c.y() - geom.y() ));
        drag->setHotSpot( QPoint( c.x() - geom.x(), -1 ));

        drag->exec( Qt::MoveAction );
        drag_in_progress = false;
        if( drag->target() == 0 && tabList.count() > 1 )
            { // Remove window from group and move to where the cursor is located
            QPoint pos = QCursor::pos();
            frame.moveTo( pos.x() - c.x(), pos.y() - c.y() );
            removeFromClientGroup( itemClicked( click ), frame );
            }
        return true;
        }
    return false;
    }

bool TabstripDecoration::dragEnterEvent( QDragEnterEvent* e )
    {
    if( e->source() != 0 && e->source()->objectName() == "decoration widget" )
        {
        drag_in_progress = true;
        e->acceptProposedAction();
        QPoint point = widget()->mapToParent( e->pos() );
        targetTab = itemClicked( point );
        widget()->update();
        return true;
        }
    return false;
    }

bool TabstripDecoration::dropEvent( QDropEvent* e )
    {
    QPoint point = widget()->mapToParent( e->pos() );
    drag_in_progress = false;
    int tabClick = itemClicked( point );
    if( tabClick >= 0 )
        {
        const QMimeData *group_data = e->mimeData();
        if( group_data->hasFormat( clientGroupItemDragMimeType() ) )
            {
            if( widget() == e->source() )
                {
                int from = itemClicked( click );
                moveItemInClientGroup( from, itemClicked( point, true ));
                }
            else
                {
                long source = QString( group_data->data( clientGroupItemDragMimeType() ) ).toLong();
                moveItemToClientGroup( source, itemClicked( point, true ));
                }
            return true;
            }
        }
    return false; 
    }

bool TabstripDecoration::dragMoveEvent( QDragMoveEvent* )
    {
    return false;
    }

bool TabstripDecoration::dragLeaveEvent( QDragLeaveEvent* )
    {
    return false;
    }

int TabstripDecoration::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *button) const
    {
    switch ( lm )
        {
        case LM_BorderBottom:
            return 2;
        case LM_BorderLeft:
        case LM_BorderRight:
            return 2;
        case LM_TitleHeight:
            return 17;
        case LM_TitleBorderLeft:
            return 3;
        case LM_TitleBorderRight:
            return 1;
        case LM_TitleEdgeTop:
        case LM_TitleEdgeBottom:
        case LM_TitleEdgeLeft:
        case LM_TitleEdgeRight:
            return 3;
        case LM_ButtonWidth:
        case LM_ButtonHeight:
            return 16;
        case LM_ButtonSpacing:
            return 6;
        case LM_ExplicitButtonSpacer:
            return -2;
        default:
            return KCommonDecoration::layoutMetric( lm, respectWindowState, button );
            break;
        }
    }

int TabstripDecoration::itemClicked( const QPoint &point, bool between )
    {
    QRect frame = widget()->frameGeometry();
    QList< ClientGroupItem > list = clientGroupItems();
    int tabs = list.count();
    int t_x = titleRect().x();
    int t_y = frame.y();
    int t_w = titleRect().width();
    int t_h = layoutMetric( LM_TitleEdgeTop ) + layoutMetric( LM_TitleHeight ) + layoutMetric( LM_TitleEdgeBottom );
    int tabWidth = t_w/tabs;
    if( between ) // We are inserting a new tab between two existing ones
        t_x -= tabWidth / 2;
    int rem = t_w%tabs;
    int tab_x = t_x;
    for( int i = 0; i < tabs; ++i )
        {
        QRect tabRect( tab_x, t_y, i<rem?tabWidth+1:tabWidth, t_h );
        if( tabRect.contains( point ) )
            return i;
        tab_x += tabRect.width();
        }
    return -1;
    }
