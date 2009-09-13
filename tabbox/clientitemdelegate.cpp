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
// own
#include "clientitemdelegate.h"
// tabbox
#include "clientmodel.h"
#include "itemlayoutconfig.h"
// Qt
#include <QPainter>
// KDE
#include <KGlobalSettings>
#include <KIconEffect>
#include <KIconLoader>
#include <Plasma/FrameSvg>
#include <Plasma/Theme>

namespace KWin
{
namespace TabBox
{

ClientItemDelegate::ClientItemDelegate( QObject* parent )
    : QAbstractItemDelegate( parent )
    , m_showSelection( true )
    {
    m_frame = new Plasma::FrameSvg( this );
    m_frame->setImagePath( "widgets/viewitem" );
    m_frame->setElementPrefix( "hover" );
    m_frame->setCacheAllRenderedFrames( true );
    m_frame->setEnabledBorders( Plasma::FrameSvg::AllBorders );
    }

ClientItemDelegate::~ClientItemDelegate()
    {
    }

void ClientItemDelegate::setConfig(const KWin::TabBox::ItemLayoutConfig& config)
    {
    m_config = config;
    }

QSize ClientItemDelegate::sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const
    {
    Q_UNUSED( option )
    if( !index.isValid() )
        return QSize( 0, 0 );

    // check for empty client list
    if( index.model()->data( index, ClientModel::EmptyRole ).toBool() )
        {
        QFont font = KGlobalSettings::generalFont();
        font.setBold( true );
        font.setPointSize( font.pointSize() + 4 );
        QFontMetrics fm( font );
        QString text = index.model()->data( index, Qt::DisplayRole ).toString();
        qreal width = 20.0 + fm.width( text );
        qreal height = fm.boundingRect( text ).height();
        return QSize( width, height );
        }

    qreal width = 0.0;
    qreal height = 0.0;
    for( int i = 0; i<m_config.count(); i++ )
        {
        QSizeF row = rowSize( index, i );
        width = qMax<qreal>( width, row.width() );
        height += row.height();
        }
    qreal left, top, right, bottom;
    m_frame->getMargins( left, top, right, bottom );

    // find icon elements which have a row span
    for( int i = 0; i<m_config.count(); i++ )
        {
        ItemLayoutConfigRow row = m_config.row( i );
        for( int j=0; j<row.count(); j++ )
            {
            ItemLayoutConfigRowElement element = row.element( j );
            if( element.type() == ItemLayoutConfigRowElement::ElementIcon && element.isRowSpan() )
                height = qMax<qreal>( height, element.iconSize().height() );
            }
        }
    return QSize( width + left + right, height + top + bottom );
    }

void ClientItemDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
    {
    if( !index.isValid() )
        return;

    // check for empty client list
    if( index.model()->data( index, ClientModel::EmptyRole ).toBool() )
        {
        painter->save();
        QFont font = KGlobalSettings::generalFont();
        font.setBold( true );
        font.setPointSize( font.pointSize() + 4 );
        painter->setFont( font );
        painter->drawText( option.rect, Qt::AlignCenter | Qt::TextSingleLine,
                           index.model()->data( index, Qt::DisplayRole ).toString() );
        painter->restore();
        return;
        }

    painter->save();
    if( option.state & QStyle::State_Selected && m_showSelection )
        {
        m_frame->resizeFrame( option.rect.size() );
        m_frame->paintFrame( painter, option.rect.topLeft() );
        }
    painter->restore();
    qreal left, top, right, bottom;
    m_frame->getMargins( left, top, right, bottom );

    qreal y = option.rect.top() + top;
    for( int i = 0; i< m_config.count(); i++ )
        {
        qreal rowHeight = rowSize( index, i ).height();
        qreal x = option.rect.left() + left ;
        ItemLayoutConfigRow row = m_config.row( i );
        for( int j=0; j<row.count(); j++ )
            {
            ItemLayoutConfigRowElement element = row.element( j );
            switch( element.type() )
                {
                case ItemLayoutConfigRowElement::ElementClientName:
                    x += paintTextElement( painter, option, index, element, x, y, rowHeight,
                                      index.model()->data( index, ClientModel::CaptionRole ).toString() );
                    break;
                case ItemLayoutConfigRowElement::ElementDesktopName:
                    {
                    x += paintTextElement( painter, option, index, element, x, y, rowHeight,
                                      index.model()->data( index, ClientModel::DesktopNameRole ).toString() );
                    break;
                    }
                case ItemLayoutConfigRowElement::ElementIcon:
                    {
                    TabBoxClient* client = static_cast< TabBoxClient* >(index.model()->data(
                                        index, ClientModel::ClientRole ).value<void *>() );
                    qreal rectWidth = (qreal)option.rect.width();
                    qreal maxWidth = qMin<qreal>( element.width(), rectWidth );
                    if( element.isStretch() )
                        maxWidth = qMax<qreal>( maxWidth, option.rect.left() + option.rect.width() - x );
                    qreal iconX = x + maxWidth * 0.5 - element.iconSize().width() * 0.5;
                    qreal iconY = y + rowHeight * 0.5 - element.iconSize().height() * 0.5;
                    if( element.isRowSpan() )
                        iconY = option.rect.top() + option.rect.height() * 0.5 - element.iconSize().height() * 0.5;
                    QRectF iconRect = QRectF( iconX, iconY, element.iconSize().width(), element.iconSize().height() );
                    QPixmap icon = client->icon();
                    if( !icon.isNull() )
                        {
                        if( m_config.isHighlightSelectedIcons() && option.state & QStyle::State_Selected )
                            {
                            KIconEffect *effect = KIconLoader::global()->iconEffect();
                            icon = effect->apply( icon, KIconLoader::Desktop, KIconLoader::ActiveState );
                            }
                        if( m_config.isGrayscaleDeselectedIcons() && !(option.state & QStyle::State_Selected) )
                            {
                            KIconEffect *effect = KIconLoader::global()->iconEffect();
                            icon = effect->apply( icon, KIconLoader::Desktop, KIconLoader::DisabledState );
                            }
                        QRectF sourceRect = QRectF( 0.0, 0.0, icon.width(), icon.height() );
                        painter->drawPixmap( iconRect, icon, sourceRect );
                        }
                    x += element.width();
                    break;
                    }
                case ItemLayoutConfigRowElement::ElementEmpty:
                    x += element.width();
                    break;
                default:
                    break; // do nothing
                }
            }
        y += rowHeight;
        }
    }

qreal ClientItemDelegate::paintTextElement( QPainter* painter, const QStyleOptionViewItem& option,
                                        const QModelIndex& index, const ItemLayoutConfigRowElement& element,
                                        const qreal& x, const qreal& y, const qreal& rowHeight, QString text  ) const
    {
    painter->save();
    QFont font = KGlobalSettings::generalFont();
    if( element.isSmallTextSize() )
        font = KGlobalSettings::smallestReadableFont();
    font.setBold( element.isBold() );
    font.setItalic( element.isItalic() );
    text = element.prefix() + text + element.suffix();
    if( index.model()->data( index, ClientModel::MinimizedRole ).toBool() )
        {
        text = element.prefixMinimized() + text + element.suffixMinimized();
        if( element.isItalicMinimized() )
            font.setItalic( true );
        }
    painter->setFont( font );
    painter->setPen( Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor ) );
    qreal width = element.width();
    if( element.isStretch() )
        {
        qreal left, top, right, bottom;
        m_frame->getMargins( left, top, right, bottom );
        width = option.rect.left() + option.rect.width() - x - right;
        }
    text = QFontMetricsF( font ).elidedText( text, Qt::ElideRight, width );
    QRectF rect = QRectF( x, y, width, rowHeight );
    painter->drawText( rect, element.alignment() | Qt::TextSingleLine, text );
    painter->restore();
    return width;
    }

QSizeF ClientItemDelegate::rowSize( const QModelIndex& index, int row ) const
    {
    ItemLayoutConfigRow currentRow = m_config.row( row );
    qreal rowWidth = 0.0;
    qreal rowHeight = 0.0;
    for( int j=0; j<currentRow.count(); j++ )
        {
        ItemLayoutConfigRowElement element = currentRow.element( j );
        switch( element.type() )
            {
            case ItemLayoutConfigRowElement::ElementClientName:
                {
                QSizeF size = textElementSizeHint( index, element,
                                                    index.model()->data( index, ClientModel::CaptionRole ).toString() );
                rowWidth += size.width();
                rowHeight = qMax<qreal>( rowHeight, size.height() );
                break;
                }
            case ItemLayoutConfigRowElement::ElementDesktopName:
                {
                QSizeF size = textElementSizeHint( index, element,
                                                    index.model()->data( index, ClientModel::DesktopNameRole ).toString() );
                rowWidth += size.width();
                rowHeight = qMax<qreal>( rowHeight, size.height() );
                break;
                }
            case ItemLayoutConfigRowElement::ElementIcon:
                rowWidth += qMax<qreal>( element.iconSize().width(), element.width() );
                if( !element.isRowSpan() )
                    rowHeight = qMax<qreal>( rowHeight, element.iconSize().height() );
                break;
            case ItemLayoutConfigRowElement::ElementEmpty:
                rowWidth += element.width();
                break;
            default:
                break; // do nothing
            }
        }
    return QSizeF( rowWidth, rowHeight );
    }

QSizeF ClientItemDelegate::textElementSizeHint( const QModelIndex& index, const ItemLayoutConfigRowElement& element, QString text ) const
    {
    QFont font = KGlobalSettings::generalFont();
    if( element.isSmallTextSize() )
        font = KGlobalSettings::smallestReadableFont();
    font.setBold( element.isBold() );
    font.setItalic( element.isItalic() );
    text = element.prefix() + text + element.suffix();
    if( index.model()->data( index, ClientModel::MinimizedRole ).toBool() )
        {
        text = element.prefixMinimized() + text + element.suffixMinimized();
        if( element.isItalicMinimized() )
            font.setItalic( true );
        }
    QFontMetricsF fm( font );
    qreal width = element.width();
    if( element.isStretch() )
        width = fm.width( text );
    qreal height = fm.boundingRect( text ).height();
    return QSizeF( width, height );
    }

} // namespace Tabbox
} // namespace KWin
