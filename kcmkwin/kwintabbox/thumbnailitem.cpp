/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#include "thumbnailitem.h"
// Qt
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeView>
// KDE
#include <KDE/KDebug>
#include <KDE/KStandardDirs>

namespace KWin
{
ThumbnailItem::ThumbnailItem(QDeclarativeItem* parent)
    : QDeclarativeItem(parent)
    , m_wId(0)
    , m_image()
{
    setFlags(flags() & ~QGraphicsItem::ItemHasNoContents);
}

ThumbnailItem::~ThumbnailItem()
{
}

void ThumbnailItem::setWId(qulonglong wId)
{
    m_wId = wId;
    emit wIdChanged(wId);
    findImage();
}

void ThumbnailItem::findImage()
{
    QString imagePath;
    switch (m_wId) {
    case Konqueror:
        imagePath = KStandardDirs::locate("data", "kwin/kcm_kwintabbox/konqueror.png");
        break;
    case Systemsettings:
        imagePath = KStandardDirs::locate("data", "kwin/kcm_kwintabbox/systemsettings.png");
        break;
    case KMail:
        imagePath = KStandardDirs::locate("data", "kwin/kcm_kwintabbox/kmail.png");
        break;
    case Dolphin:
        imagePath = KStandardDirs::locate("data", "kwin/kcm_kwintabbox/dolphin.png");
        break;
    default:
        // ignore
        break;
    }
    if (imagePath.isNull()) {
        m_image = QImage();
    } else {
        m_image = QImage(imagePath);
    }
}

void ThumbnailItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (m_image.isNull()) {
        // no image: default behavior
        QDeclarativeItem::paint(painter, option, widget);
    }
    QSizeF difference(boundingRect().width() - m_image.width(), boundingRect().height() - m_image.height());
    const QRectF drawRect(boundingRect().x() + difference.width()/2.0, boundingRect().y(), m_image.width(), m_image.height());
    painter->drawImage(drawRect, m_image);
}

} // namespace KWin
