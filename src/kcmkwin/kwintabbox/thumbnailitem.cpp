/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011, 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbnailitem.h"
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
    , m_clipToItem(nullptr)
    , m_brightness(1.0)
    , m_saturation(1.0)
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

void WindowThumbnailItem::setClipTo(QQuickItem *clip)
{
    qWarning() << "ThumbnailItem.clipTo is removed and it has no replacements";
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
    case Desktop:
        imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "wallpapers/Next/contents/screenshot.png");
        if (imagePath.isNull()) {
            imagePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kwin/kcm_kwintabbox/desktop.png");
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

qreal WindowThumbnailItem::brightness() const
{
    return m_brightness;
}

qreal WindowThumbnailItem::saturation() const
{
    return m_saturation;
}

QSize WindowThumbnailItem::sourceSize() const
{
    return m_sourceSize;
}

void WindowThumbnailItem::setBrightness(qreal brightness)
{
    qWarning() << "ThumbnailItem.brightness is removed. Use a shader effect to change brightness";
}

void WindowThumbnailItem::setSaturation(qreal saturation)
{
    qWarning() << "ThumbnailItem.saturation is removed. Use a shader effect to change saturation";
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
