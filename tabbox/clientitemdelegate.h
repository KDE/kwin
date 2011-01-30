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

#ifndef CLIENTITEMDELEGATE_H
#define CLIENTITEMDELEGATE_H
#include "itemlayoutconfig.h"

#include <QAbstractItemDelegate>
/**
* @file
* This file defines the class ClientItemDelegate, the ItemDelegate for TabBoxClients.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/

namespace Plasma
{
class FrameSvg;
}

namespace KWin
{
namespace TabBox
{
class ItemLayoutConfigRowElement;

/**
* The ItemDelegate for TabBoxClients used in TabBox.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class ClientItemDelegate
    : public QAbstractItemDelegate
{
public:
    ClientItemDelegate(QObject* parent = 0);
    ~ClientItemDelegate();
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

    void setConfig(const ItemLayoutConfig& config);
    /**
    * @param show Show selection mark for selected item
    */
    void setShowSelection(bool show = true) {
        m_showSelection = show;
    }

private:
    /**
    * Paints the given text at the given position based on the given ItemLayoutConfigRowElement.
    * @param painter The painter to paint the text
    * @param option The current QStyleOptionViewItem
    * @param index The current QModelIndex - unused should be removed
    * @param element The current ItemLayoutConfigRowElement
    * @param x The left x position for drawing the text
    * @param y The top y position for drawing the text
    * @param rowHeight The height of the current rendered row
    * @param text The text which should be drawn
    */
    qreal paintTextElement(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index, const ItemLayoutConfigRowElement& element,
                           const qreal& x, const qreal& y, const qreal& rowHeight, QString text) const;
    /**
    * Calculates the size hint of given text.
    * This method is used to calculate the maximum size of a row.
    * @param index The current model index
    * @param element The ItemLayoutConfigRowElement defining how the text has to be laid out.
    * @param text The text whose size has to be calculated
    * @return The size of the given text if it would be rendered
    */
    QSizeF textElementSizeHint(const QModelIndex& index, const ItemLayoutConfigRowElement& element, QString text) const;
    /**
    * Calculates the size hint of the given row.
    * @param index The current QModelIndex
    * @param row The row for which the size hint has to be calculated
    * @return The size hint of the given row
    */
    QSizeF rowSize(const QModelIndex& index, int row) const;
    /**
    * The FrameSvg to render selected items
    */
    Plasma::FrameSvg* m_frame;

    ItemLayoutConfig m_config;
    bool m_showSelection;
};

} // namespace Tabbox
} // namespace KWin

#endif // CLIENTITEMDELEGATE_H
