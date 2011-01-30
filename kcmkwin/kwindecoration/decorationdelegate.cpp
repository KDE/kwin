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
#include "decorationdelegate.h"
#include "decorationmodel.h"
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QApplication>

namespace KWin
{

const int margin = 5;

DecorationDelegate::DecorationDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

DecorationDelegate::~DecorationDelegate()
{
}

void DecorationDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // highlight selected item
    QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, painter);

    QPixmap pixmap = index.model()->data(index, DecorationModel::PixmapRole).value<QPixmap>();

    const QSize previewArea = option.rect.size() - QSize(2 * margin, 2 * margin);
    if (pixmap.isNull() || pixmap.size() != previewArea) {
        emit regeneratePreview(static_cast< const QSortFilterProxyModel* >(index.model())->mapToSource(index),
                               previewArea);
    } else
        painter->drawPixmap(option.rect.topLeft() + QPoint(margin, margin), pixmap);
}

QSize DecorationDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(450, 150);
}

} // namespace KWin
