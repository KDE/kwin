/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011, 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbnailitem.h"

#include "config-kwin.h"

// Qt
#include <QDebug>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QStandardPaths>

namespace KWin
{

WindowThumbnailItem::WindowThumbnailItem(QQuickItem *parent)
    : QQuickItem(parent)
    , m_wId(0)
    , m_image()
    , m_sourceSize(QSize())
{
    setFlag(ItemHasContents);
}

WindowThumbnailItem::~WindowThumbnailItem()
{
}

void WindowThumbnailItem::setWId(qulonglong wId)
{
    m_wId = wId;
    Q_EMIT wIdChanged(wId);
    findImage();
}

void WindowThumbnailItem::findImage()
{
    QString imagePath;
    switch (m_wId) {
    case Konqueror:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + "/kcm_kwintabbox/falkon.png");
        break;
    case Systemsettings:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + "/kcm_kwintabbox/systemsettings.png");
        break;
    case KMail:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + "/kcm_kwintabbox/kmail.png");
        break;
    case Dolphin:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + "/kcm_kwintabbox/dolphin.png");
        break;
    case Desktop:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "wallpapers/Next/contents/images/1280x800.png");
        if (imagePath.isNull()) {
            imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + "/kcm_kwintabbox/desktop.png");
        }
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

    setImplicitSize(m_image.width(), m_image.height());
}

QSGNode *WindowThumbnailItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData)
{
    auto *node = static_cast<QSGImageNode *>(oldNode);
    if (!node) {
        node = window()->createImageNode();
        node->setOwnsTexture(true);
        qsgnode_set_description(node, QStringLiteral("windowthumbnail"));
        node->setFiltering(QSGTexture::Linear);
    }

    node->setTexture(window()->createTextureFromImage(m_image));

    const QSize size(m_image.size().scaled(boundingRect().size().toSize(), Qt::KeepAspectRatio));
    const qreal x = boundingRect().x() + (boundingRect().width() - size.width()) / 2;
    const qreal y = boundingRect().y() + (boundingRect().height() - size.height()) / 2;

    node->setRect(QRectF(QPointF(x, y), size));

    return node;
}

QSize WindowThumbnailItem::sourceSize() const
{
    return m_sourceSize;
}

void WindowThumbnailItem::setSourceSize(const QSize &size)
{
    if (m_sourceSize == size) {
        return;
    }
    m_sourceSize = size;
    update();
    Q_EMIT sourceSizeChanged();
}

} // namespace KWin

#include "moc_thumbnailitem.cpp"
