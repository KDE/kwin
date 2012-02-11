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

#include <kcommondecoration.h>
#include "tabstripbutton.h"
#include "tabstripdecoration.h"
#include "tabstripfactory.h"

#include <KLocale>

#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRect>

TabstripButton::TabstripButton( ButtonType type, TabstripDecoration *parent, QString tip )
    : KCommonDecorationButton( type, parent ), SIZE( 16 )
    {
    setAutoFillBackground( false );
    setFixedSize( SIZE, SIZE );
    setCursor( Qt::ArrowCursor );
    client = parent;
    btype = type;
    setToolTip( tip );
    active_item = true;
    hovering = false;
    }

TabstripButton::~TabstripButton()
    {
    }

void TabstripButton::reset( unsigned long )
    {
    update();
    }

QSize TabstripButton::sizeHint() const
    {
    return QSize( SIZE, SIZE );
    }

void TabstripButton::paintEvent( QPaintEvent * )
    {
    QPainter p( this );
    const bool active = client->isActive() && active_item;

    // Icon geometry
    QRect geom = QRect( 3, 3, width() - 6, height() - 6 );

    // Determine painting colors
    QColor bgColor = client->options()->color( KDecoration::ColorTitleBar, active );
    QColor textColor = client->options()->color( KDecoration::ColorFont, active );
    if( hovering )
        { // Invert if the mouse cursor is hovering over the button
        textColor.setRed( 255 - textColor.red() );
        textColor.setGreen( 255 - textColor.green() );
        textColor.setBlue( 255 - textColor.blue() );
        }

    // Slight optimization as we are drawing solid straight lines
    p.setRenderHint( QPainter::Antialiasing, false );

    // Background
    p.fillRect( 0, 0, width(), height(), bgColor );
    //p.fillRect( 0, 0, width(), height(), QColor( 255, 0, 0 ));

    // Paint buttons with the text color
    p.setPen( textColor );

    switch( btype )
        {
        case HelpButton:
            {
            QFont font;
            font.setBold( true );
            p.setFont( font );
            p.drawText( geom.adjusted( 0, 1, 0, 0), Qt::AlignVCenter | Qt::AlignHCenter, "?" );
            }
            break;
        case MaxButton:
            switch( client->maximizeMode() )
                {
                case TabstripDecoration::MaximizeRestore:
                case TabstripDecoration::MaximizeVertical:
                case TabstripDecoration::MaximizeHorizontal:
                    // TL
                    p.drawLine( geom.x() + 3, geom.y(),
                        geom.x(), geom.y() );
                    p.drawLine( geom.x(), geom.y() + 3,
                        geom.x(), geom.y() );
                    p.drawLine( geom.x() + 3, geom.y() + 1,
                        geom.x(), geom.y() + 1 );
                    p.drawLine( geom.x() + 1, geom.y() + 3,
                        geom.x(), geom.y() + 1 );
                    // TR
                    p.drawLine( geom.x() + geom.width() - 3, geom.y(),
                        geom.x() + geom.width(), geom.y() );
                    p.drawLine( geom.x() + geom.width(), geom.y() + 3,
                        geom.x() + geom.width(), geom.y() );
                    p.drawLine( geom.x() + geom.width() - 3, geom.y() + 1,
                        geom.x() + geom.width(), geom.y() + 1 );
                    p.drawLine( geom.x() + geom.width() - 1, geom.y() + 3,
                        geom.x() + geom.width() - 1, geom.y() );
                    // BL
                    p.drawLine( geom.x() + 3, geom.y() + geom.height(),
                        geom.x(), geom.y() + geom.height() );
                    p.drawLine( geom.x(), geom.y() + geom.height() - 3,
                        geom.x(), geom.y() + geom.height() );
                    p.drawLine( geom.x() + 3, geom.y() + geom.height() - 1,
                        geom.x(), geom.y() + geom.height() - 1 );
                    p.drawLine( geom.x() + 1, geom.y() + geom.height() - 3,
                        geom.x() + 1, geom.y() + geom.height() );
                    // BR
                    p.drawLine( geom.x() + geom.width() - 3, geom.y() + geom.height(),
                        geom.x() + geom.width(), geom.y() + geom.height() );
                    p.drawLine( geom.x() + geom.width(), geom.y() + geom.height() - 3,
                        geom.x() + geom.width(), geom.y() + geom.height() );
                    p.drawLine( geom.x() + geom.width() - 3, geom.y() + geom.height() - 1,
                        geom.x() + geom.width(), geom.y() + geom.height() - 1 );
                    p.drawLine( geom.x() + geom.width() - 1, geom.y() + geom.height() - 3,
                        geom.x() + geom.width() - 1, geom.y() + geom.height() );
                    break;
                case TabstripDecoration::MaximizeFull:
                    // TL
                    p.drawLine( geom.x() + 2, geom.y(),
                        geom.x() + 2, geom.y() + 2 );
                    p.drawLine( geom.x(), geom.y() + 2,
                        geom.x() + 2, geom.y() + 2 );
                    p.drawLine( geom.x() + 3, geom.y(),
                        geom.x() + 3, geom.y() + 3 );
                    p.drawLine( geom.x(), geom.y() + 3,
                        geom.x() + 3, geom.y() + 3 );
                    // TR
                    p.drawLine( geom.x() + geom.width() - 2, geom.y(),
                        geom.x() + geom.width() - 2, geom.y() + 2 );
                    p.drawLine( geom.x() + geom.width(), geom.y() + 2,
                        geom.x() + geom.width() - 2, geom.y() + 2 );
                    p.drawLine( geom.x() + geom.width() - 3, geom.y(),
                        geom.x() + geom.width() - 3, geom.y() + 3 );
                    p.drawLine( geom.x() + geom.width(), geom.y() + 3,
                        geom.x() + geom.width() - 3, geom.y() + 3 );
                    // BL
                    p.drawLine( geom.x() + 2, geom.y() + geom.height(),
                        geom.x() + 2, geom.y() + geom.height() - 2 );
                    p.drawLine( geom.x(), geom.y() + geom.height() - 2,
                        geom.x() + 2, geom.y() + geom.height() - 2 );
                    p.drawLine( geom.x() + 3, geom.y() + geom.height(),
                        geom.x() + 3, geom.y() + geom.height() - 3 );
                    p.drawLine( geom.x(), geom.y() + geom.height() - 3,
                        geom.x() + 3, geom.y() + geom.height() - 3 );
                    // BR
                    p.drawLine( geom.x() + geom.width() - 2, geom.y() + geom.height(),
                        geom.x() + geom.width() - 2, geom.y() + geom.height() - 2 );
                    p.drawLine( geom.x() + geom.width(), geom.y() + geom.height() - 2,
                        geom.x() + geom.width() - 2, geom.y() + geom.height() - 2 );
                    p.drawLine( geom.x() + geom.width() - 3, geom.y() + geom.height(),
                        geom.x() + geom.width() - 3, geom.y() + geom.height() - 3 );
                    p.drawLine( geom.x() + geom.width(), geom.y() + geom.height() - 3,
                        geom.x() + geom.width() - 3, geom.y() + geom.height() - 3 );
                    break;
                }
            break;
        case MinButton:
            // B
            p.drawLine( geom.x(), geom.y() + geom.height(),
                geom.x() + geom.width(), geom.y() + geom.height() );
            p.drawLine( geom.x(), geom.y() + geom.height() - 1,
                geom.x() + geom.width(), geom.y() + geom.height() - 1 );
            // L
            p.drawLine( geom.x(), geom.y() + geom.height() - 3,
                geom.x(), geom.y() + geom.height() );
            p.drawLine( geom.x() + 1, geom.y() + geom.height() - 3,
                geom.x() + 1, geom.y() + geom.height() );
            // R
            p.drawLine( geom.x() + geom.width(), geom.y() + geom.height() - 3,
                geom.x() + geom.width(), geom.y() + geom.height() );
            p.drawLine( geom.x() + geom.width() - 1, geom.y() + geom.height() - 3,
                geom.x() + geom.width() - 1, geom.y() + geom.height() );
            break;
        case CloseButton:
        case ItemCloseButton:
            // TL-BR
            p.drawLine( geom.x() + 1, geom.y() + 1,
                geom.x() + geom.width() - 1, geom.y() + geom.height() - 1 );
            p.drawLine( geom.x() + 2, geom.y() + 1,
                geom.x() + geom.width() - 1, geom.y() + geom.height() - 2 );
            p.drawLine( geom.x() + 1, geom.y() + 2,
                geom.x() + geom.width() - 2, geom.y() + geom.height() - 1 );
            // TR-BL
            p.drawLine( geom.x() + 1, geom.y() + geom.height() - 1,
                geom.x() + geom.width() - 1, geom.y() + 1 );
            p.drawLine( geom.x() + 2, geom.y() + geom.height() - 1,
                geom.x() + geom.width() - 1, geom.y() + 2 );
            p.drawLine( geom.x() + 1, geom.y() + geom.height() - 2,
                geom.x() + geom.width() - 2, geom.y() + 1 );
            break;
        case MenuButton:
            if( client->clientGroupItems().count() > 1 || !TabstripFactory::showIcon() )
                {
                p.drawRect( geom.x(), geom.y() + geom.height() / 2 - 5,
                    1, 1 );
                p.drawRect( geom.x(), geom.y() + geom.height() / 2 - 1,
                    1, 1 );
                p.drawRect( geom.x(), geom.y() + geom.height() / 2 + 3,
                    1, 1 );
                p.drawRect( geom.x() + 4, geom.y() + geom.height() / 2 - 5,
                    geom.width() - 5, 1 );
                p.drawRect( geom.x() + 4, geom.y() + geom.height() / 2 - 1,
                    geom.width() - 5, 1 );
                p.drawRect( geom.x() + 4, geom.y() + geom.height() / 2 + 3,
                    geom.width() - 5, 1 );
                }
            else
                p.drawPixmap( 0, 0, client->icon().pixmap( SIZE ));
            break;
        case OnAllDesktopsButton:
            {
            if( isChecked() )
                p.fillRect( geom.x() + geom.width() / 2 - 1, geom.y() + geom.height() / 2 - 1,
                    3, 3, textColor );
            else
                {
                p.fillRect( geom.x() + geom.width() / 2 - 4, geom.y() + geom.height() / 2 - 4,
                    3, 3, textColor );
                p.fillRect( geom.x() + geom.width() / 2 - 4, geom.y() + geom.height() / 2 + 2,
                    3, 3, textColor );
                p.fillRect( geom.x() + geom.width() / 2 + 2, geom.y() + geom.height() / 2 - 4,
                    3, 3, textColor );
                p.fillRect( geom.x() + geom.width() / 2 + 2, geom.y() + geom.height() / 2 + 2,
                    3, 3, textColor );
                }
            }
            break;
        case AboveButton:
            {
            int o = -2;
            if( isChecked() )
                {
                o = -4;
                p.drawRect( geom.x() + geom.width() / 2 - 4, geom.y() + geom.height() / 2 + 3,
                    8, 1 );
                }
            p.drawPoint( geom.x() + geom.width() / 2, geom.y() + geom.height() / 2 + o );
            p.drawLine( geom.x() + geom.width() / 2 - 1, geom.y() + geom.height() / 2 + o + 1,
                geom.x() + geom.width() / 2 + 1, geom.y() + geom.height() / 2 + o + 1 );
            p.drawLine( geom.x() + geom.width() / 2 - 2, geom.y() + geom.height() / 2 + o + 2,
                geom.x() + geom.width() / 2 + 2, geom.y() + geom.height() / 2 + o + 2 );
            p.drawLine( geom.x() + geom.width() / 2 - 3, geom.y() + geom.height() / 2 + o + 3,
                geom.x() + geom.width() / 2 + 3, geom.y() + geom.height() / 2 + o + 3 );
            p.drawLine( geom.x() + geom.width() / 2 - 4, geom.y() + geom.height() / 2 + o + 4,
                geom.x() + geom.width() / 2 + 4, geom.y() + geom.height() / 2 + o + 4 );
            }
            break;
        case BelowButton:
            {
            int o = 1;
            if( isChecked() )
                {
                o = 3;
                p.drawRect( geom.x() + geom.width() / 2 - 4, geom.y() + geom.height() / 2 - 5,
                    8, 1 );
                }
            p.drawPoint( geom.x() + geom.width() / 2, geom.y() + geom.height() / 2 + o );
            p.drawLine( geom.x() + geom.width() / 2 - 1, geom.y() + geom.height() / 2 + o - 1,
                geom.x() + geom.width() / 2 + 1, geom.y() + geom.height() / 2 + o - 1 );
            p.drawLine( geom.x() + geom.width() / 2 - 2, geom.y() + geom.height() / 2 + o - 2,
                geom.x() + geom.width() / 2 + 2, geom.y() + geom.height() / 2 + o - 2 );
            p.drawLine( geom.x() + geom.width() / 2 - 3, geom.y() + geom.height() / 2 + o - 3,
                geom.x() + geom.width() / 2 + 3, geom.y() + geom.height() / 2 + o - 3 );
            p.drawLine( geom.x() + geom.width() / 2 - 4, geom.y() + geom.height() / 2 + o - 4,
                geom.x() + geom.width() / 2 + 4, geom.y() + geom.height() / 2 + o - 4 );
            }
            break;
        case ShadeButton:
            p.drawLine( geom.x(), geom.y(),
                geom.x() + geom.width(), geom.y() );
            p.drawLine( geom.x(), geom.y() + 1,
                geom.x() + geom.width(), geom.y() + 1 );
            break;
        case NumButtons:
        default:
            break;
        };
    }

void TabstripButton::enterEvent( QEvent *e )
    {
    hovering = true;
    repaint();
    KCommonDecorationButton::enterEvent( e );
    }

void TabstripButton::leaveEvent( QEvent *e )
    {
    hovering = false;
    repaint();
    KCommonDecorationButton::leaveEvent( e );
    }
