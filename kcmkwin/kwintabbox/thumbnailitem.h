/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011, 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_THUMBNAILITEM_H
#define KWIN_THUMBNAILITEM_H

#include <QImage>
#include <QQuickItem>
#include <QSGTextureMaterial>

namespace KWin
{

class BrightnessSaturationShader : public QSGMaterialShader
{
public:
    BrightnessSaturationShader();
    const char* vertexShader() const override;
    const char* fragmentShader() const override;
    const char*const* attributeNames() const override;
    void updateState(const RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial) override;
    void initialize() override;
private:
    int m_id_matrix;
    int m_id_opacity;
    int m_id_saturation;
    int m_id_brightness;
};

class BrightnessSaturationMaterial : public QSGTextureMaterial
{
public:
    QSGMaterialShader* createShader() const override {
        return new BrightnessSaturationShader;
    }
    QSGMaterialType *type() const override {
        static QSGMaterialType type;
        return &type;
    }
    qreal brightness;
    qreal saturation;
};

class WindowThumbnailItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qulonglong wId READ wId WRITE setWId NOTIFY wIdChanged SCRIPTABLE true)
    Q_PROPERTY(QQuickItem *clipTo READ clipTo WRITE setClipTo NOTIFY clipToChanged)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
public:
    explicit WindowThumbnailItem(QQuickItem *parent = nullptr);
    ~WindowThumbnailItem() override;

    qulonglong wId() const {
        return m_wId;
    }
    QQuickItem *clipTo() const {
        return m_clipToItem;
    }
    qreal brightness() const;
    qreal saturation() const;
    void setWId(qulonglong wId);
    void setClipTo(QQuickItem *clip);
    void setBrightness(qreal brightness);
    void setSaturation(qreal saturation);
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;

    enum Thumbnail {
        Konqueror = 1,
        KMail,
        Systemsettings,
        Dolphin
    };
Q_SIGNALS:
    void wIdChanged(qulonglong wid);
    void clipToChanged();
    void brightnessChanged();
    void saturationChanged();
private:
    void findImage();
    qulonglong m_wId;
    QImage m_image;
    QQuickItem *m_clipToItem;
    qreal m_brightness;
    qreal m_saturation;
};

} // KWin

#endif // KWIN_THUMBNAILITEM_H
