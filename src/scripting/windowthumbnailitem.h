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
class Window;
class GLFramebuffer;
class GLTexture;
class ThumbnailTextureProvider;

class WindowThumbnailItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QUuid wId READ wId WRITE setWId NOTIFY wIdChanged)
    Q_PROPERTY(KWin::Window *client READ client WRITE setClient NOTIFY clientChanged)

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
    explicit WindowThumbnailItem(QQuickItem *parent = nullptr);
    ~WindowThumbnailItem() override;

    QUuid wId() const;
    void setWId(const QUuid &wId);

    Window *client() const;
    void setClient(Window *client);

    qreal brightness() const;
    void setBrightness(qreal brightness);

    qreal saturation() const;
    void setSaturation(qreal saturation);

    QQuickItem *clipTo() const;
    void setClipTo(QQuickItem *clip);

    QSize sourceSize() const;
    void setSourceSize(const QSize &sourceSize);

    QSGTextureProvider *textureProvider() const override;
    bool isTextureProvider() const override;
    QSGNode *updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *) override;

protected:
    void releaseResources() override;
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value) override;

Q_SIGNALS:
    void wIdChanged();
    void clientChanged();
    void brightnessChanged();
    void saturationChanged();
    void clipToChanged();
    void sourceSizeChanged();

private:
    QImage fallbackImage() const;
    QRectF paintedRect() const;
    void invalidateOffscreenTexture();
    void updateOffscreenTexture();
    void destroyOffscreenTexture();
    void updateImplicitSize();
    void updateFrameRenderingConnection();
    static bool useGlThumbnails();

    QSize m_sourceSize;
    QUuid m_wId;
    QPointer<Window> m_client;
    bool m_dirty = false;

    mutable ThumbnailTextureProvider *m_provider = nullptr;
    std::shared_ptr<GLTexture> m_offscreenTexture;
    std::unique_ptr<GLFramebuffer> m_offscreenTarget;
    GLsync m_acquireFence = 0;
    qreal m_devicePixelRatio = 1;

    QMetaObject::Connection m_frameRenderingConnection;
};

} // namespace KWin
