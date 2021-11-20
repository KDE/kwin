/*
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QQuickItem>
#include <QUuid>

#include <epoxy/gl.h>

namespace KWin
{
class AbstractClient;
class GLRenderTarget;
class GLTexture;
class ThumbnailTextureProvider;

class ThumbnailItemBase : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QSize sourceSize READ sourceSize WRITE setSourceSize NOTIFY sourceSizeChanged)
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
    /**
     * TODO Plasma 6: Remove.
     * @deprecated clipTo has no replacement
     */
    Q_PROPERTY(QQuickItem *clipTo READ clipTo WRITE setClipTo NOTIFY clipToChanged)

public:
    explicit ThumbnailItemBase(QQuickItem *parent = nullptr);
    ~ThumbnailItemBase() override;

    qreal brightness() const { return 1; }
    void setBrightness(qreal brightness);

    qreal saturation() const { return 1; }
    void setSaturation(qreal saturation);

    QQuickItem *clipTo() const { return nullptr; }
    void setClipTo(QQuickItem *clip);

    QSize sourceSize() const;
    void setSourceSize(const QSize &sourceSize);

    QSGTextureProvider *textureProvider() const override;
    bool isTextureProvider() const override;
    QSGNode *updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *) override;

Q_SIGNALS:
    void brightnessChanged();
    void saturationChanged();
    void clipToChanged();
    void sourceSizeChanged();

protected:
    void releaseResources() override;

    virtual QImage fallbackImage() const = 0;
    virtual QRectF paintedRect() const = 0;
    virtual void invalidateOffscreenTexture() = 0;
    virtual void updateOffscreenTexture() = 0;
    void destroyOffscreenTexture();

    mutable ThumbnailTextureProvider *m_provider = nullptr;
    QSharedPointer<GLTexture> m_offscreenTexture;
    QScopedPointer<GLRenderTarget> m_offscreenTarget;
    GLsync m_acquireFence = 0;
    qreal m_devicePixelRatio = 1;

private:
    void updateFrameRenderingConnection();
    QMetaObject::Connection m_frameRenderingConnection;

    QSize m_sourceSize;
};

class WindowThumbnailItem : public ThumbnailItemBase
{
    Q_OBJECT
    Q_PROPERTY(QUuid wId READ wId WRITE setWId NOTIFY wIdChanged)
    Q_PROPERTY(KWin::AbstractClient *client READ client WRITE setClient NOTIFY clientChanged)

public:
    explicit WindowThumbnailItem(QQuickItem *parent = nullptr);

    QUuid wId() const;
    void setWId(const QUuid &wId);

    AbstractClient *client() const;
    void setClient(AbstractClient *client);

Q_SIGNALS:
    void wIdChanged();
    void clientChanged();

protected:
    QImage fallbackImage() const override;
    QRectF paintedRect() const override;
    void invalidateOffscreenTexture() override;
    void updateOffscreenTexture() override;
    void updateImplicitSize();

private:
    QUuid m_wId;
    QPointer<AbstractClient> m_client;
    bool m_dirty = false;
};

class DesktopThumbnailItem : public ThumbnailItemBase
{
    Q_OBJECT
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)

public:
    explicit DesktopThumbnailItem(QQuickItem *parent = nullptr);

    int desktop() const;
    void setDesktop(int desktop);

Q_SIGNALS:
    void desktopChanged();

protected:
    QImage fallbackImage() const override;
    QRectF paintedRect() const override;
    void invalidateOffscreenTexture() override;
    void updateOffscreenTexture() override;

private:
    int m_desktop = 1;
};

} // namespace KWin
