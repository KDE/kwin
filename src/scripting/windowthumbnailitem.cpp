/*
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowthumbnailitem.h"
#include "compositor.h"
#include "core/renderbackend.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "opengl/glframebuffer.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "scripting_logging.h"
#include "window.h"
#include "workspace.h"

#include "opengl/gltexture.h"

#include <QOpenGLContext>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGImageNode>
#include <QSGTextureProvider>

namespace KWin
{

static bool useGlThumbnails()
{
    static bool qtQuickIsSoftware = QStringList({QStringLiteral("software"), QStringLiteral("softwarecontext")}).contains(QQuickWindow::sceneGraphBackend());
    return Compositor::self()->backend() && Compositor::self()->backend()->compositingType() == OpenGLCompositing && !qtQuickIsSoftware;
}

WindowThumbnailSource::WindowThumbnailSource(QQuickWindow *view, Window *handle)
    : m_view(view)
    , m_handle(handle)
{
    connect(handle, &Window::frameGeometryChanged, this, [this]() {
        m_dirty = true;
        Q_EMIT changed();
    });
    connect(handle, &Window::damaged, this, [this]() {
        m_dirty = true;
        Q_EMIT changed();
    });

    connect(Compositor::self()->scene(), &WorkspaceScene::preFrameRender, this, &WindowThumbnailSource::update);

    m_handle->refOffscreenRendering();
}

WindowThumbnailSource::~WindowThumbnailSource()
{

    if (m_handle) {
        m_handle->unrefOffscreenRendering();
    }

    if (!m_offscreenTexture) {
        return;
    }
    if (!QOpenGLContext::currentContext()) {
        Compositor::self()->scene()->makeOpenGLContextCurrent();
    }
    m_offscreenTarget.reset();
    m_offscreenTexture.reset();

    if (m_acquireFence) {
        glDeleteSync(m_acquireFence);
        m_acquireFence = 0;
    }
}

std::shared_ptr<WindowThumbnailSource> WindowThumbnailSource::getOrCreate(QQuickWindow *window, Window *handle)
{
    using WindowThumbnailSourceKey = std::pair<QQuickWindow *, Window *>;
    const WindowThumbnailSourceKey key{window, handle};

    static std::map<WindowThumbnailSourceKey, std::weak_ptr<WindowThumbnailSource>> sources;
    auto &source = sources[key];
    if (!source.expired()) {
        return source.lock();
    }

    auto s = std::make_shared<WindowThumbnailSource>(window, handle);
    source = s;

    QObject::connect(handle, &Window::destroyed, [key]() {
        sources.erase(key);
    });
    QObject::connect(window, &QQuickWindow::destroyed, [key]() {
        sources.erase(key);
    });
    return s;
}

WindowThumbnailSource::Frame WindowThumbnailSource::acquire()
{
    return Frame{
        .texture = m_offscreenTexture,
        .fence = std::exchange(m_acquireFence, nullptr),
    };
}

void WindowThumbnailSource::update()
{
    if (m_acquireFence || !m_dirty || !m_handle) {
        return;
    }
    Q_ASSERT(m_view);

    const QRectF geometry = m_handle->visibleGeometry();
    const qreal devicePixelRatio = m_view->devicePixelRatio();
    const QSize textureSize = geometry.toAlignedRect().size() * devicePixelRatio;

    if (!m_offscreenTexture || m_offscreenTexture->size() != textureSize) {
        m_offscreenTexture = GLTexture::allocate(GL_RGBA8, textureSize);
        if (!m_offscreenTexture) {
            return;
        }
        m_offscreenTexture->setContentTransform(OutputTransform::FlipY);
        m_offscreenTexture->setFilter(GL_LINEAR);
        m_offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_offscreenTarget = std::make_unique<GLFramebuffer>(m_offscreenTexture.get());
    }

    RenderTarget offscreenRenderTarget(m_offscreenTarget.get());
    RenderViewport offscreenViewport(geometry, devicePixelRatio, offscreenRenderTarget);
    GLFramebuffer::pushFramebuffer(m_offscreenTarget.get());
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // The thumbnail must be rendered using kwin's opengl context as VAOs are not
    // shared across contexts. Unfortunately, this also introduces a latency of 1
    // frame, which is not ideal, but it is acceptable for things such as thumbnails.
    const int mask = Scene::PAINT_WINDOW_TRANSFORMED;
    Compositor::self()->scene()->renderer()->renderItem(offscreenRenderTarget, offscreenViewport, m_handle->windowItem(), mask, infiniteRegion(), WindowPaintData{});
    GLFramebuffer::popFramebuffer();

    // The fence is needed to avoid the case where qtquick renderer starts using
    // the texture while all rendering commands to it haven't completed yet.
    m_dirty = false;
    m_acquireFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    Q_EMIT changed();
}

class ThumbnailTextureProvider : public QSGTextureProvider
{
public:
    explicit ThumbnailTextureProvider(QQuickWindow *window);

    QSGTexture *texture() const override;
    void setTexture(const std::shared_ptr<GLTexture> &nativeTexture);
    void setTexture(QSGTexture *texture);

private:
    QQuickWindow *m_window;
    std::shared_ptr<GLTexture> m_nativeTexture;
    std::unique_ptr<QSGTexture> m_texture;
};

ThumbnailTextureProvider::ThumbnailTextureProvider(QQuickWindow *window)
    : m_window(window)
{
}

QSGTexture *ThumbnailTextureProvider::texture() const
{
    return m_texture.get();
}

void ThumbnailTextureProvider::setTexture(const std::shared_ptr<GLTexture> &nativeTexture)
{
    if (m_nativeTexture != nativeTexture) {
        const GLuint textureId = nativeTexture->texture();
        m_nativeTexture = nativeTexture;
        m_texture.reset(QNativeInterface::QSGOpenGLTexture::fromNative(textureId, m_window,
                                                                       nativeTexture->size(),
                                                                       QQuickWindow::TextureHasAlphaChannel));
        m_texture->setFiltering(QSGTexture::Linear);
        m_texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        m_texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
    }

    // The textureChanged signal must be emitted also if only texture data changes.
    Q_EMIT textureChanged();
}

void ThumbnailTextureProvider::setTexture(QSGTexture *texture)
{
    m_nativeTexture = nullptr;
    m_texture.reset(texture);
    Q_EMIT textureChanged();
}

class ThumbnailTextureProviderCleanupJob : public QRunnable
{
public:
    explicit ThumbnailTextureProviderCleanupJob(ThumbnailTextureProvider *provider)
        : m_provider(provider)
    {
    }

    void run() override
    {
        m_provider.reset();
    }

private:
    std::unique_ptr<ThumbnailTextureProvider> m_provider;
};

WindowThumbnailItem::WindowThumbnailItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);

    connect(Compositor::self(), &Compositor::aboutToToggleCompositing,
            this, &WindowThumbnailItem::resetSource);
    connect(Compositor::self(), &Compositor::compositingToggled,
            this, &WindowThumbnailItem::updateSource);
}

WindowThumbnailItem::~WindowThumbnailItem()
{
    if (m_provider) {
        if (window()) {
            window()->scheduleRenderJob(new ThumbnailTextureProviderCleanupJob(m_provider),
                                        QQuickWindow::AfterSynchronizingStage);
        } else {
            qCCritical(KWIN_SCRIPTING) << "Can't destroy thumbnail texture provider because window is null";
        }
    }
}

void WindowThumbnailItem::releaseResources()
{
    if (m_provider) {
        window()->scheduleRenderJob(new ThumbnailTextureProviderCleanupJob(m_provider),
                                    QQuickWindow::AfterSynchronizingStage);
        m_provider = nullptr;
    }
}

void WindowThumbnailItem::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    if (change == QQuickItem::ItemSceneChange) {
        updateSource();
    }
    QQuickItem::itemChange(change, value);
}

bool WindowThumbnailItem::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *WindowThumbnailItem::textureProvider() const
{
    if (QQuickItem::isTextureProvider()) {
        return QQuickItem::textureProvider();
    }
    if (!m_provider) {
        m_provider = new ThumbnailTextureProvider(window());
    }
    return m_provider;
}

void WindowThumbnailItem::resetSource()
{
    m_source.reset();
}

void WindowThumbnailItem::updateSource()
{
    if (useGlThumbnails() && window() && m_client) {
        m_source = WindowThumbnailSource::getOrCreate(window(), m_client);
        connect(m_source.get(), &WindowThumbnailSource::changed, this, &WindowThumbnailItem::update);
    } else {
        m_source.reset();
    }
}

QSGNode *WindowThumbnailItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if (Compositor::compositing()) {
        if (!m_source) {
            return oldNode;
        }

        auto [texture, acquireFence] = m_source->acquire();
        if (!texture) {
            return oldNode;
        }

        // Wait for rendering commands to the offscreen texture complete if there are any.
        if (acquireFence) {
            glWaitSync(acquireFence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(acquireFence);
        }

        if (!m_provider) {
            m_provider = new ThumbnailTextureProvider(window());
        }
        m_provider->setTexture(texture);
    } else {
        if (!m_provider) {
            m_provider = new ThumbnailTextureProvider(window());
        }

        const QImage placeholderImage = fallbackImage();
        m_provider->setTexture(window()->createTextureFromImage(placeholderImage));
    }

    QSGImageNode *node = static_cast<QSGImageNode *>(oldNode);
    if (!node) {
        node = window()->createImageNode();
        node->setFiltering(QSGTexture::Linear);
    }
    node->setTexture(m_provider->texture());
    node->setTextureCoordinatesTransform(QSGImageNode::NoTransform);
    node->setRect(paintedRect());

    return node;
}

QUuid WindowThumbnailItem::wId() const
{
    return m_wId;
}

void WindowThumbnailItem::setWId(const QUuid &wId)
{
    if (m_wId == wId) {
        return;
    }
    m_wId = wId;
    if (!m_wId.isNull()) {
        setClient(workspace()->findWindow(wId));
    } else if (m_client) {
        m_client = nullptr;
        updateSource();
        updateImplicitSize();
        Q_EMIT clientChanged();
    }

    Q_EMIT wIdChanged();
}

Window *WindowThumbnailItem::client() const
{
    return m_client;
}

void WindowThumbnailItem::setClient(Window *client)
{
    if (m_client == client) {
        return;
    }
    if (m_client) {
        disconnect(m_client, &Window::frameGeometryChanged,
                   this, &WindowThumbnailItem::updateImplicitSize);
    }
    m_client = client;
    if (m_client) {
        connect(m_client, &Window::frameGeometryChanged,
                this, &WindowThumbnailItem::updateImplicitSize);
        setWId(m_client->internalId());
    } else {
        setWId(QUuid());
    }
    updateSource();
    updateImplicitSize();
    Q_EMIT clientChanged();
}

void WindowThumbnailItem::updateImplicitSize()
{
    QSize frameSize;
    if (m_client) {
        frameSize = m_client->frameGeometry().toAlignedRect().size();
    }
    setImplicitSize(frameSize.width(), frameSize.height());
}

QImage WindowThumbnailItem::fallbackImage() const
{
    if (m_client) {
        return m_client->icon().pixmap(window(), boundingRect().size().toSize()).toImage();
    }
    return QImage();
}

static QRectF centeredSize(const QRectF &boundingRect, const QSizeF &size)
{
    const QSizeF scaled = size.scaled(boundingRect.size(), Qt::KeepAspectRatio);
    const qreal x = boundingRect.x() + (boundingRect.width() - scaled.width()) / 2;
    const qreal y = boundingRect.y() + (boundingRect.height() - scaled.height()) / 2;
    return QRectF(QPointF(x, y), scaled);
}

QRectF WindowThumbnailItem::paintedRect() const
{
    if (!m_client) {
        return QRectF();
    }
    if (!Compositor::compositing()) {
        const QSizeF iconSize = m_client->icon().actualSize(window(), boundingRect().size().toSize());
        return centeredSize(boundingRect(), iconSize);
    }

    const QRectF visibleGeometry = m_client->visibleGeometry();
    const QRectF frameGeometry = m_client->frameGeometry();
    const QSizeF scaled = QSizeF(frameGeometry.size()).scaled(boundingRect().size(), Qt::KeepAspectRatio);

    const qreal xScale = scaled.width() / frameGeometry.width();
    const qreal yScale = scaled.height() / frameGeometry.height();

    QRectF paintedRect(boundingRect().x() + (boundingRect().width() - scaled.width()) / 2,
                       boundingRect().y() + (boundingRect().height() - scaled.height()) / 2,
                       visibleGeometry.width() * xScale,
                       visibleGeometry.height() * yScale);

    paintedRect.moveLeft(paintedRect.x() + (visibleGeometry.x() - frameGeometry.x()) * xScale);
    paintedRect.moveTop(paintedRect.y() + (visibleGeometry.y() - frameGeometry.y()) * yScale);

    return paintedRect;
}

} // namespace KWin

#include "moc_windowthumbnailitem.cpp"
