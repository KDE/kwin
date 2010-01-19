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
#include "configdialog.h"
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QApplication>
#include <KAboutData>
#include <KAboutApplicationDialog>
#include <KPushButton>

namespace KWin
{

const int margin = 5;

DecorationDelegate::DecorationDelegate( QAbstractItemView* itemView, QObject* parent )
    : KWidgetItemDelegate( itemView, parent )
    , m_button( new KPushButton )
    , m_itemView( itemView )
    {
        m_button->setIcon( KIcon( "configure" ) );
    }

DecorationDelegate::~DecorationDelegate()
    {
    delete m_button;
    }

void DecorationDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
    {
    // highlight selected item
    QApplication::style()->drawControl( QStyle::CE_ItemViewItem, &option, painter );

    QPixmap pixmap = index.model()->data( index, DecorationModel::PixmapRole ).value<QPixmap>();

    const QSize previewArea = option.rect.size() -
                                QSize( 3 * margin + m_button->sizeHint().width(),  2 * margin);
    if( pixmap.isNull() || pixmap.size() != previewArea )
        {
        emit regeneratePreview( static_cast< const QSortFilterProxyModel* >( index.model() )->mapToSource( index ),
                                previewArea );
        }
    else
        painter->drawPixmap( option.rect.topLeft() + QPoint( margin, margin ), pixmap );
    }

QSize DecorationDelegate::sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const
    {
    Q_UNUSED( option )
    Q_UNUSED( index )
    const QSize pixmapSize = QSize( 400, 150 );
    const QSize buttonSize = m_button->sizeHint();
    const int width = margin * 3 + pixmapSize.width() + buttonSize.width();
    const int height = qMax( margin * 2 + pixmapSize.height(), margin * 2 + buttonSize.height() * 2 );
    return QSize( width, height );
    }

QList< QWidget* > DecorationDelegate::createItemWidgets() const
    {
    QList<QWidget*> widgets;
    KPushButton *info = new KPushButton;
    info->setIcon( KIcon( "dialog-information" ) );
    setBlockedEventTypes( info, QList<QEvent::Type>() << QEvent::MouseButtonPress
                            << QEvent::MouseButtonRelease << QEvent::MouseButtonDblClick
                            << QEvent::KeyPress << QEvent::KeyRelease);

    KPushButton *configure = new KPushButton;
    configure->setIcon( KIcon( "configure" ) );
    setBlockedEventTypes( configure, QList<QEvent::Type>() << QEvent::MouseButtonPress
                            << QEvent::MouseButtonRelease << QEvent::MouseButtonDblClick
                            << QEvent::KeyPress << QEvent::KeyRelease);

    connect( configure, SIGNAL(clicked(bool)), SLOT(slotConfigure()) );
    connect( info, SIGNAL(clicked(bool)), SLOT(slotInfo()) );

    widgets << info << configure;
    return widgets;
    }

void DecorationDelegate::updateItemWidgets(const QList< QWidget* > widgets, const QStyleOptionViewItem& option, const QPersistentModelIndex& index) const
    {
    KPushButton *button = static_cast<KPushButton*>( widgets[0] );
    button->resize( button->sizeHint() );
    button->move( option.rect.left() + option.rect.width() - margin - button->width(),
                  option.rect.height() / 2 - button->sizeHint().height() );

    if( !index.isValid() )
        button->setVisible( false );

    if( widgets.size() > 1 )
        {
        button = static_cast<KPushButton*>( widgets[1] );
        button->resize( button->sizeHint() );
        button->move( option.rect.left() + option.rect.width() - margin - button->width(),
                      option.rect.height() / 2 );
        if( !index.isValid() )
            button->setVisible( false );
        }
    }

void DecorationDelegate::slotConfigure()
    {
    if (!focusedIndex().isValid())
        return;

    const QModelIndex index = focusedIndex();
    QString name = focusedIndex().model()->data( index , DecorationModel::LibraryNameRole ).toString();
    QList< QVariant > borderSizes = focusedIndex().model()->data( index , DecorationModel::BorderSizesRole ).toList();
    const KDecorationDefines::BorderSize size =
        static_cast<KDecorationDefines::BorderSize>( itemView()->model()->data( index, DecorationModel::BorderSizeRole ).toInt() );
    QPointer< KWinDecorationConfigDialog > configDialog =
        new KWinDecorationConfigDialog( name, borderSizes, size, m_itemView );
    if( configDialog->exec() == KDialog::Accepted )
        {
        DecorationModel* model = static_cast< DecorationModel* >(
            static_cast< QSortFilterProxyModel* >( itemView()->model() )->sourceModel() );
        model->setBorderSize( index, configDialog->borderSize() );
        model->regeneratePreview( focusedIndex() );
        }

    delete configDialog;
    }

void DecorationDelegate::slotInfo()
    {
    if (!focusedIndex().isValid())
        return;

    const QModelIndex index = focusedIndex();
    const QString name = index.model()->data( index, Qt::DisplayRole ).toString();
    const QString comment = index.model()->data( index, DecorationModel::PackageDescriptionRole ).toString();
    const QString author = index.model()->data( index, DecorationModel::PackageAuthorRole ).toString();
    const QString email = index.model()->data( index, DecorationModel::PackageEmailRole ).toString();
    const QString website = index.model()->data( index, DecorationModel::PackageWebsiteRole ).toString();
    const QString version = index.model()->data( index, DecorationModel::PackageVersionRole ).toString();
    const QString license = index.model()->data( index, DecorationModel::PackageLicenseRole ).toString();

    KAboutData aboutData(name.toUtf8(), name.toUtf8(), ki18n(name.toUtf8()), version.toUtf8(), ki18n(comment.toUtf8()), KAboutLicense::byKeyword(license).key(), ki18n(QByteArray()), ki18n(QByteArray()), website.toLatin1());
    aboutData.setProgramIconName("preferences-system-windows-action");
    const QStringList authors = author.split(',');
    const QStringList emails = email.split(',');
    int i = 0;
    if( authors.count() == emails.count() )
        {
        foreach( const QString &author, authors )
            {
            if( !author.isEmpty() )
                {
                aboutData.addAuthor(ki18n(author.toUtf8()), ki18n(QByteArray()), emails[i].toUtf8(), 0);
                }
            i++;
            }
        }
    QPointer<KAboutApplicationDialog> about = new KAboutApplicationDialog( &aboutData, itemView() );
    about->exec();
    delete about;
    }

} // namespace KWin
