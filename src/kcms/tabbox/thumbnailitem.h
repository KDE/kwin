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
    /**
     * TODO Plasma 6: Remove.
     * @deprecated clipTo has no replacement
     */
    Q_PROPERTY(QQuickItem *clipTo READ clipTo WRITE setClipTo NOTIFY clipToChanged)
    /**
     * TODO Plasma 6: Remove.
     * @deprecated use a shader effect to change the brightness
     */
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    /**
     * TODO Plasma 6: Remove.
     * @deprecated use a shader effect to change color saturation
     */
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    Q_PROPERTY(QSize sourceSize READ sourceSize WRITE setSourceSize NOTIFY sourceSizeChanged)
public:
    explicit WindowThumbnailItem(QQuickItem *parent = nullptr);
    ~WindowThumbnailItem() override;

    qulonglong wId() const
    {
        return m_wId;
    }
    QQuickItem *clipTo() const
    {
        return m_clipToItem;
    }
    qreal brightness() const;
    qreal saturation() const;
    QSize sourceSize() const;
    void setWId(qulonglong wId);
    void setClipTo(QQuickItem *clip);
    void setBrightness(qreal brightness);
    void setSaturation(qreal saturation);
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
    void clipToChanged();
    void brightnessChanged();
    void saturationChanged();
    void sourceSizeChanged();

private:
    void findImage();
    qulonglong m_wId;
    QImage m_image;
    QQuickItem *m_clipToItem;
    qreal m_brightness;
    qreal m_saturation;
    QSize m_sourceSize;
};

} // KWin
