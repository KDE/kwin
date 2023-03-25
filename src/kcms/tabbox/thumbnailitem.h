/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011, 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QImage>
#include <QQuickItem>

namespace KWin
{

class WindowThumbnailItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qulonglong wId READ wId WRITE setWId NOTIFY wIdChanged SCRIPTABLE true)
    Q_PROPERTY(QSize sourceSize READ sourceSize WRITE setSourceSize NOTIFY sourceSizeChanged)
public:
    explicit WindowThumbnailItem(QQuickItem *parent = nullptr);
    ~WindowThumbnailItem() override;

    qulonglong wId() const
    {
        return m_wId;
    }
    QSize sourceSize() const;
    void setWId(qulonglong wId);
    void setSourceSize(const QSize &size);
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;

    enum Thumbnail {
        Konqueror = 1,
        KMail,
        Systemsettings,
        Dolphin,
        Desktop,
    };
Q_SIGNALS:
    void wIdChanged(qulonglong wid);
    void sourceSizeChanged();

private:
    void findImage();
    qulonglong m_wId;
    QImage m_image;
    QSize m_sourceSize;
};

} // KWin
