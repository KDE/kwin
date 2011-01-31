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
#ifndef KWIN_DECORATIONDELEGATE_H
#define KWIN_DECORATIONDELEGATE_H
#include <QtGui/QStyledItemDelegate>

class KPushButton;

namespace KWin
{

class DecorationDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    DecorationDelegate(QObject* parent = 0);
    ~DecorationDelegate();

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;

signals:
    void regeneratePreview(const QModelIndex& index, const QSize& size) const;
};

} // namespace KWin

#endif // KWIN_DECORATIONDELEGATE_H
