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
#include <QtCore/QStandardPaths>
#include <QPainter>

namespace KWin
{
WindowThumbnailItem::WindowThumbnailItem(QQuickPaintedItem* parent)
    : QQuickPaintedItem(parent)
    , m_wId(0)
    , m_image()
{
}

WindowThumbnailItem::~WindowThumbnailItem()
{
}

void WindowThumbnailItem::setWId(qulonglong wId)
{
    m_wId = wId;
    emit wIdChanged(wId);
    findImage();
}

void WindowThumbnailItem::findImage()
{
    QString imagePath;
    switch (m_wId) {
    case Konqueror:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kwin/kcm_kwintabbox/konqueror.png");
        break;
    case Systemsettings:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kwin/kcm_kwintabbox/systemsettings.png");
        break;
    case KMail:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kwin/kcm_kwintabbox/kmail.png");
        break;
    case Dolphin:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kwin/kcm_kwintabbox/dolphin.png");
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

void WindowThumbnailItem::paint(QPainter *painter)
{
    if (m_image.isNull()) {
        return;
    }
    QSizeF difference(boundingRect().width() - m_image.width(), boundingRect().height() - m_image.height());
    const QRectF drawRect(boundingRect().x() + difference.width()/2.0, boundingRect().y(), m_image.width(), m_image.height());
    painter->drawImage(drawRect, m_image);
}

} // namespace KWin
