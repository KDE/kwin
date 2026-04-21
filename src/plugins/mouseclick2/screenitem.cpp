/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenitem.h"
#include "compositor.h"
#include "effect/effecthandler.h"
#include "opengl/gltexture.h"
#include "scene/backgroundeffectitem.h"
#include "scene/opengl/texture.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "window.h"

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGTextureProvider>

namespace KWin
{

namespace
{

class BackgroundTextureProvider : public QSGTextureProvider
{
public:
    explicit BackgroundTextureProvider(QQuickWindow *window)
        : m_window(window)
    {
    }

    QSGTexture *texture() const override
    {
        return m_texture.get();
    }

    void setTexture(GLTexture *nativeTexture)
    {
        if (!nativeTexture || !m_window) {
            if (m_texture) {
                m_texture.reset();
                m_textureId = 0;
                Q_EMIT textureChanged();
            }
            return;
        }

        const GLuint textureId = nativeTexture->texture();
        if (!m_texture || m_textureId != textureId || m_size != nativeTexture->size()) {
            m_texture.reset(QNativeInterface::QSGOpenGLTexture::fromNative(textureId,
                                                                           m_window,
                                                                           nativeTexture->size(),
                                                                           QQuickWindow::TextureHasAlphaChannel));
            m_texture->setFiltering(QSGTexture::Linear);
            m_texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            m_texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
            m_textureId = textureId;
            m_size = nativeTexture->size();
        }

        Q_EMIT textureChanged();
    }

private:
    QQuickWindow *const m_window;
    std::unique_ptr<QSGTexture> m_texture;
    GLuint m_textureId = 0;
    QSize m_size;
};

class QuickBackgroundEffectItem : public BackgroundEffectItem
{
public:
    explicit QuickBackgroundEffectItem(WindowItem *parentItem)
        : BackgroundEffectItem(parentItem)
    {
        setNeedsBackgroundTexture(true);
    }

    Texture *prepareRendering(GLTexture *background) const override
    {
        m_backgroundTexture = background;
        return nullptr;
    }

    GLTexture *backgroundTexture() const
    {
        return m_backgroundTexture;
    }

private:
    mutable GLTexture *m_backgroundTexture = nullptr;
};

}

ScreenContentsItem::ScreenContentsItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

ScreenContentsItem::~ScreenContentsItem()
{
    delete m_provider;
}

bool ScreenContentsItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *ScreenContentsItem::textureProvider() const
{
    if (QQuickItem::isTextureProvider()) {
        return QQuickItem::textureProvider();
    }
    if (!m_provider && window()) {
        m_provider = new BackgroundTextureProvider(window());
    }
    return m_provider;
}

QSGNode *ScreenContentsItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGImageNode *>(oldNode);
    if (!node && window()) {
        node = window()->createImageNode();
        node->setFiltering(QSGTexture::Linear);
    }

    auto *source = static_cast<QuickBackgroundEffectItem *>(m_source.get());
    auto *provider = static_cast<BackgroundTextureProvider *>(textureProvider());
    if (!source || !provider || !node) {
        return oldNode;
    }

    provider->setTexture(source->backgroundTexture());
    if (!provider->texture()) {
        delete node;
        return nullptr;
    }

    node->setTexture(provider->texture());
    node->setTextureCoordinatesTransform(QSGImageNode::NoTransform);
    //    node->setRect(paintedRect());

    return node;
}

void ScreenContentsItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateSourceGeometry();
}

void ScreenContentsItem::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    if (change == QQuickItem::ItemSceneChange && value.window) {
        KWin::EffectWindow *ew = effects->findWindow(value.window);
        if (ew) {
            KWin::WindowItem *item = ew->windowItem();
            Q_ASSERT(item);
            m_source = std::make_unique<QuickBackgroundEffectItem>(item);
            updateSourceGeometry();
            connect(kwinApp()->scene(), &WorkspaceScene::preFrameRender, this, &QQuickItem::update, Qt::UniqueConnection);
        }
    }
    QQuickItem::itemChange(change, value);
}

void ScreenContentsItem::releaseResources()
{
    delete m_provider;
    m_provider = nullptr;
    QQuickItem::releaseResources();
}

void ScreenContentsItem::updateSourceGeometry()
{
    auto *source = static_cast<QuickBackgroundEffectItem *>(m_source.get());
    if (!source) {
        return;
    }

    source->setEffectBoundingRect(RectF(boundingRect()));
}

} // namespace KWin

#include "moc_screenitem.cpp"
