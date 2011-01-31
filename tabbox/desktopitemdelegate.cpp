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
//own
#include "desktopitemdelegate.h"
// tabbox
#include "clientitemdelegate.h"
#include "clientmodel.h"
#include "desktopmodel.h"
#include "tabboxconfig.h"
// Qt
#include <QPainter>
// KDE
#include <KGlobalSettings>
#include <KIcon>
#include <KIconEffect>
#include <KIconLoader>
#include <Plasma/FrameSvg>
#include <Plasma/Theme>

namespace KWin
{
namespace TabBox
{

DesktopItemDelegate::DesktopItemDelegate(QObject* parent)
    : QAbstractItemDelegate(parent)
{
    m_frame = new Plasma::FrameSvg(this);
    m_frame->setImagePath("widgets/viewitem");
    m_frame->setElementPrefix("hover");
    m_frame->setCacheAllRenderedFrames(true);
    m_frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    m_clientDelegate = new ClientItemDelegate(this);
}

DesktopItemDelegate::~DesktopItemDelegate()
{
}

void DesktopItemDelegate::setConfig(const KWin::TabBox::ItemLayoutConfig& config)
{
    m_config = config;
}

void DesktopItemDelegate::setLayouts(QMap< QString, ItemLayoutConfig >& layouts)
{
    m_layouts = layouts;
}

QSize DesktopItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option)
    if (!index.isValid())
        return QSize(0, 0);

    qreal width = 0.0;
    qreal height = 0.0;
    for (int i = 0; i < m_config.count(); i++) {
        QSizeF row = rowSize(index, i);
        width = qMax<qreal>(width, row.width());
        height += row.height();
    }
    qreal left, top, right, bottom;
    m_frame->getMargins(left, top, right, bottom);

    // find icon elements which have a row span
    for (int i = 0; i < m_config.count(); i++) {
        ItemLayoutConfigRow row = m_config.row(i);
        for (int j = 0; j < row.count(); j++) {
            ItemLayoutConfigRowElement element = row.element(j);
            if (element.type() == ItemLayoutConfigRowElement::ElementIcon && element.isRowSpan())
                height = qMax<qreal>(height, element.iconSize().height());
        }
    }
    return QSize(width + left + right, height + top + bottom);
}

void DesktopItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid())
        return;
    qreal left, top, right, bottom;
    m_frame->getMargins(left, top, right, bottom);

    qreal y = option.rect.top() + top;
    for (int i = 0; i < m_config.count(); i++) {
        qreal rowHeight = rowSize(index, i).height();
        qreal x = option.rect.left() + left ;
        ItemLayoutConfigRow row = m_config.row(i);
        for (int j = 0; j < row.count(); j++) {
            ItemLayoutConfigRowElement element = row.element(j);
            switch(element.type()) {
            case ItemLayoutConfigRowElement::ElementDesktopName: {
                x += paintTextElement(painter, option, element, x, y, rowHeight,
                                      index.model()->data(index, Qt::DisplayRole).toString());
                break;
            }
            case ItemLayoutConfigRowElement::ElementIcon: {
                qreal rectWidth = (qreal)option.rect.width();
                qreal maxWidth = qMin<qreal>(element.width(), rectWidth);
                if (element.isStretch())
                    maxWidth = qMax<qreal>(maxWidth, option.rect.left() + option.rect.width() - x);
                qreal iconX = x + maxWidth * 0.5 - element.iconSize().width() * 0.5;
                qreal iconY = y + rowHeight * 0.5 - element.iconSize().height() * 0.5;
                if (element.isRowSpan())
                    iconY = option.rect.top() + option.rect.height() * 0.5 - element.iconSize().height() * 0.5;
                QRectF iconRect = QRectF(iconX, iconY, element.iconSize().width(), element.iconSize().height());

                // icon
                painter->save();
                KIcon icon("user-desktop");
                QPixmap iconPixmap = icon.pixmap(QSize(64, 64));
                if (m_config.isHighlightSelectedIcons() && option.state & QStyle::State_Selected) {
                    KIconEffect *effect = KIconLoader::global()->iconEffect();
                    iconPixmap = effect->apply(iconPixmap, KIconLoader::Desktop, KIconLoader::ActiveState);
                }
                if (m_config.isGrayscaleDeselectedIcons() && !(option.state & QStyle::State_Selected)) {
                    KIconEffect *effect = KIconLoader::global()->iconEffect();
                    iconPixmap = effect->apply(iconPixmap, KIconLoader::Desktop, KIconLoader::DisabledState);
                }
                QRectF sourceRect = QRectF(0.0, 0.0, iconPixmap.width(), iconPixmap.height());
                painter->drawPixmap(iconRect, iconPixmap, sourceRect);
                painter->restore();
                x += element.width();
                break;
            }
            case ItemLayoutConfigRowElement::ElementEmpty:
                x += element.width();
                break;
            case ItemLayoutConfigRowElement::ElementClientList: {
                m_clientDelegate->setConfig(m_layouts[ element.clientListLayoutName()]);
                ClientModel* clientModel = static_cast< ClientModel* >(
                                               index.model()->data(index, DesktopModel::ClientModelRole).value< void* >());
                TabBoxClientList clients = clientModel->clientList();
                qreal partX = 0.0;
                qreal partY = 0.0;
                qreal itemWidth = element.width();
                if (element.isStretch())
                    itemWidth = option.rect.x() + option.rect.width() - x;
                foreach (TabBoxClient * client, clients) {
                    if (!client)
                        continue;
                    QModelIndex clientIndex = clientModel->index(client);
                    QStyleOptionViewItem itemOption = option;
                    itemOption.state = QStyle::State_Item;
                    QSizeF itemSize = m_clientDelegate->sizeHint(itemOption, clientIndex);
                    // TabBoxHandler gives the same config to the client models as used here
                    // so row and column do not represent the actual used layout for this list
                    switch(element.clientListLayoutMode()) {
                    case TabBoxConfig::HorizontalLayout:
                        itemOption.rect = QRect(partX + x, y, itemWidth - partX, rowHeight);
                        partX += qMin(itemWidth - partX, itemSize.width());
                        break;
                    case TabBoxConfig::VerticalLayout:
                        itemOption.rect = QRect(x, y + partY, itemWidth, itemSize.height());
                        partY += itemSize.height();
                        break;
                    case TabBoxConfig::HorizontalVerticalLayout:
                        // TODO: complicated
                        break;
                    default:
                        break; // nothing
                    }
                    if ((partX > itemWidth) || (partX + x > option.rect.x() + option.rect.width())) {
                        partX = itemWidth;
                        break;
                    }
                    if ((partY > rowHeight) || (y + partY > option.rect.y() + option.rect.height())) {
                        partY = rowHeight;
                        break;
                    }
                    m_clientDelegate->paint(painter, itemOption, clientIndex);
                }
                x += itemWidth;
                break;
            }
            default:
                break; // do nothing
            }
        }
        y += rowHeight;
    }
}

qreal DesktopItemDelegate::paintTextElement(QPainter* painter, const QStyleOptionViewItem& option,
        const ItemLayoutConfigRowElement& element,
        const qreal& x, const qreal& y, const qreal& rowHeight, QString text) const
{
    painter->save();
    QFont font = KGlobalSettings::generalFont();
    if (element.isSmallTextSize())
        font = KGlobalSettings::smallestReadableFont();
    font.setBold(element.isBold());
    font.setItalic(element.isItalic());
    text = element.prefix() + text + element.suffix();
    painter->setFont(font);
    painter->setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    qreal width = element.width();
    if (element.isStretch()) {
        qreal left, top, right, bottom;
        m_frame->getMargins(left, top, right, bottom);
        width = option.rect.left() + option.rect.width() - x - right;
    }
    text = QFontMetricsF(font).elidedText(text, Qt::ElideRight, width);
    QRectF rect = QRectF(x, y, width, rowHeight);
    painter->drawText(rect, element.alignment() | Qt::TextSingleLine, text);
    painter->restore();
    return width;
}

QSizeF DesktopItemDelegate::rowSize(const QModelIndex& index, int row) const
{
    ItemLayoutConfigRow currentRow = m_config.row(row);
    qreal rowWidth = 0.0;
    qreal rowHeight = 0.0;
    for (int j = 0; j < currentRow.count(); j++) {
        ItemLayoutConfigRowElement element = currentRow.element(j);
        switch(element.type()) {
        case ItemLayoutConfigRowElement::ElementDesktopName: {
            QSizeF size = textElementSizeHint(index, element,
                                              index.model()->data(index, Qt::DisplayRole).toString());
            rowWidth += size.width();
            rowHeight = qMax<qreal>(rowHeight, size.height());
            break;
        }
        case ItemLayoutConfigRowElement::ElementIcon:
            rowWidth += qMax<qreal>(element.iconSize().width(), element.width());
            if (!element.isRowSpan())
                rowHeight = qMax<qreal>(rowHeight, element.iconSize().height());
            break;
        case ItemLayoutConfigRowElement::ElementEmpty:
            rowWidth += element.width();
            break;
        case ItemLayoutConfigRowElement::ElementClientList: {
            m_clientDelegate->setConfig(m_layouts[ element.clientListLayoutName()]);
            ClientModel* clientModel = static_cast< ClientModel* >(
                                           index.model()->data(index, DesktopModel::ClientModelRole).value< void* >());
            TabBoxClientList clients = clientModel->clientList();
            qreal elementWidth = 0.0;
            qreal elementHeight = 0.0;
            foreach (TabBoxClient * client, clients) {
                if (!client)
                    continue;
                QModelIndex clientIndex = clientModel->index(client);
                QStyleOptionViewItem itemOption = QStyleOptionViewItem();
                QSizeF itemSize = m_clientDelegate->sizeHint(itemOption, clientIndex);
                switch(element.clientListLayoutMode()) {
                case TabBoxConfig::HorizontalLayout:
                    elementWidth += itemSize.width();
                    elementHeight = qMax<qreal>(elementHeight, itemSize.height());
                    break;
                case TabBoxConfig::VerticalLayout:
                    elementWidth = qMax<qreal>(elementWidth, itemSize.width());
                    elementHeight += itemSize.height();
                    break;
                case TabBoxConfig::HorizontalVerticalLayout:
                    // TODO: complicated
                    break;
                default:
                    break; // nothing
                }
            }
            if (element.isStretch())
                rowWidth += elementWidth;
            else
                rowWidth += qMin(elementWidth, element.width());
            rowHeight = qMax<qreal>(rowHeight, elementHeight);
            break;
        }
        default:
            break; // do nothing
        }
    }
    return QSizeF(rowWidth, rowHeight);
}

QSizeF DesktopItemDelegate::textElementSizeHint(const QModelIndex& index, const ItemLayoutConfigRowElement& element, QString text) const
{
    Q_UNUSED(index)
    QFont font = KGlobalSettings::generalFont();
    if (element.isSmallTextSize())
        font = KGlobalSettings::smallestReadableFont();
    font.setBold(element.isBold());
    font.setItalic(element.isItalic());
    text = element.prefix() + text + element.suffix();
    QFontMetricsF fm(font);
    qreal width = element.width();
    if (element.isStretch())
        width = fm.width(text);
    qreal height = fm.boundingRect(text).height();
    return QSizeF(width, height);
}

} // namespace Tabbox
} // namespace KWin
