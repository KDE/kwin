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
#ifndef PREVIEWDELEGATE_H
#define PREVIEWDELEGATE_H
#include <KWidgetItemDelegate>

class KPushButton;

namespace KWin
{

class DecorationDelegate : public KWidgetItemDelegate
{
    Q_OBJECT
    public:
        DecorationDelegate( QAbstractItemView* itemView, QObject* parent = 0 );
        ~DecorationDelegate();

        virtual void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const;
        virtual QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const;
        virtual QList< QWidget* > createItemWidgets() const;
        virtual void updateItemWidgets(const QList< QWidget* > widgets, const QStyleOptionViewItem& option, const QPersistentModelIndex& index) const;

    signals:
        void regeneratePreview( const QModelIndex& index, const QSize& size ) const;

    private slots:
        void slotConfigure();
        void slotInfo();

    private:
        KPushButton* m_button;
        QAbstractItemView* m_itemView;
};

} // namespace KWin

#endif // PREVIEWDELEGATE_H
