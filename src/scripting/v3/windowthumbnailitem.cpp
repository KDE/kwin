/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowthumbnailitem.h"
#include "abstract_client.h"
#include "composite.h"
#include "decorations/decoratedclient.h"
#include "kwingltexture.h"
#include "openglsurfacetexture.h"
#include "scene.h"
#include "shadow.h"
#include "workspace.h"

#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGTextureMaterial>
#include <QQuickWindow>

namespace KWin
{

class TextureNode : public QSGGeometryNode
{
public:
    explicit TextureNode(QQuickWindow *window)
        : m_window(window)
    {
        setFlags(OwnsGeometry);
        setMaterial(&m_material);
        m_material.setFiltering(QSGTexture::Linear);
        m_material.setHorizontalWrapMode(QSGTexture::ClampToEdge);
        m_material.setVerticalWrapMode(QSGTexture::ClampToEdge);
    }

    void setTexture(GLTexture *texture)
    {
        if (m_texture == texture) {
            return;
        }

        const GLuint textureId = texture->texture();
        auto sgTexture = m_window->createTextureFromNativeObject(QQuickWindow::NativeObjectTexture,
                                                                 &textureId, 0,
                                                                 texture->size(),
                                                                 QQuickWindow::TextureHasAlphaChannel);
        sgTexture->setFiltering(QSGTexture::Linear);
        sgTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        sgTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);

        delete m_material.texture();
        m_material.setTexture(sgTexture);
        markDirty(DirtyMaterial);
    }

    void setGeometry(const QRegion &shape, const QMatrix4x4 &matrix)
    {
        QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), shape.rectCount() * 4);
        geometry->setVertexDataPattern(QSGGeometry::StaticPattern);
        QSGGeometry::TexturedPoint2D *v = geometry->vertexDataAsTexturedPoint2D();

        const QSize textureSize = m_material.texture()->textureSize();
        for (const QRectF rect : shape) {
            const QPointF bufferTopLeft = matrix.map(rect.topLeft());
            const QPointF bufferTopRight = matrix.map(rect.topRight());
            const QPointF bufferBottomRight = matrix.map(rect.bottomRight());
            const QPointF bufferBottomLeft = matrix.map(rect.bottomLeft());

            v[0].x = rect.left();
            v[0].y = rect.top();
            v[0].tx = bufferTopLeft.x() / textureSize.width();
            v[0].ty = bufferTopLeft.y() / textureSize.height();

            v[1].x = rect.left();
            v[1].y = rect.bottom();
            v[1].tx = bufferBottomLeft.x() / textureSize.width();
            v[1].ty = bufferBottomLeft.y() / textureSize.height();

            v[2].x = rect.right();
            v[2].y = rect.top();
            v[2].tx = bufferTopRight.x() / textureSize.width();
            v[2].ty = bufferTopRight.y() / textureSize.height();

            v[3].x = rect.right();
            v[3].y = rect.bottom();
            v[3].tx = bufferBottomRight.x() / textureSize.width();
            v[3].ty = bufferBottomRight.y() / textureSize.height();

            v += 4;
        }

        QSGGeometryNode::setGeometry(geometry);
    }

    void setGeometry(const WindowQuadList &quads)
    {
        QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), quads.count() * 6);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::StaticPattern);
        QSGGeometry::TexturedPoint2D *v = geometry->vertexDataAsTexturedPoint2D();

        const QSize textureSize = m_material.texture()->textureSize();
        for (const WindowQuad &quad : quads) {
            double tx0 = quad[0].u() / textureSize.width();
            double ty0 = quad[0].v() / textureSize.height();

            double tx1 = quad[1].u() / textureSize.width();
            double ty1 = quad[1].v() / textureSize.height();

            double tx2 = quad[2].u() / textureSize.width();
            double ty2 = quad[2].v() / textureSize.height();

            double tx3 = quad[3].u() / textureSize.width();
            double ty3 = quad[3].v() / textureSize.height();

            v[0].x = quad[0].x();
            v[0].y = quad[0].y();
            v[0].tx = tx0;
            v[0].ty = ty0;

            v[1].x = quad[1].x();
            v[1].y = quad[1].y();
            v[1].tx = tx1;
            v[1].ty = ty1;

            v[2].x = quad[2].x();
            v[2].y = quad[2].y();
            v[2].tx = tx2;
            v[2].ty = ty2;

            v[3].x = quad[0].x();
            v[3].y = quad[0].y();
            v[3].tx = tx0;
            v[3].ty = ty0;

            v[4].x = quad[2].x();
            v[4].y = quad[2].y();
            v[4].tx = tx2;
            v[4].ty = ty2;

            v[5].x = quad[3].x();
            v[5].y = quad[3].y();
            v[5].tx = tx3;
            v[5].ty = ty3;

            v += 6;
        }

        QSGGeometryNode::setGeometry(geometry);
    }

private:
    QQuickWindow *m_window;
    QSGTextureMaterial m_material;
    GLTexture *m_texture = nullptr;
};

KQuickShadowItem::KQuickShadowItem(Shadow *shadow, AbstractClient *client, QQuickItem *parent)
    : QQuickItem(parent)
    , m_shadow(shadow)
    , m_client(client)
{
    setFlag(ItemHasContents);

    connect(shadow, &Shadow::offsetChanged, this, &KQuickShadowItem::handleGeometryInvalidated);
    connect(shadow, &Shadow::rectChanged, this, &KQuickShadowItem::handleGeometryInvalidated);
    connect(shadow, &Shadow::textureChanged, this, &KQuickShadowItem::handleTextureInvalidated);
}

Shadow *KQuickShadowItem::shadow() const
{
    return m_shadow;
}

void KQuickShadowItem::handleGeometryInvalidated()
{
    m_geometryInvalidated = true;
    update();
}

void KQuickShadowItem::handleTextureInvalidated()
{
    m_geometryInvalidated = true;
    m_textureInvalidated = true;
    update();
}

QSGNode *KQuickShadowItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    TextureNode *node = static_cast<TextureNode *>(oldNode);
    if (!node) {
        node = new TextureNode(window());
        m_geometryInvalidated = true;
        m_textureInvalidated = true;
    }

    if (m_textureInvalidated) {
        node->setTexture(m_shadow->texture().data()); // FIXME: ownership
    }
    if (m_geometryInvalidated) {
        node->setGeometry(m_shadow->geometry());
    }

    return node;
}

KQuickDecorationItem::KQuickDecorationItem(KDecoration2::Decoration *decoration,
                                           AbstractClient *client, QQuickItem *parent)
    : QQuickItem(parent)
    , m_decoration(decoration)
    , m_client(client)
{
    setFlag(ItemHasContents);
    setSize(client->size());

    m_renderer.reset(Compositor::self()->scene()->createDecorationRenderer(client->decoratedClient()));

    connect(m_renderer.data(), &DecorationRenderer::damaged, this, &KQuickDecorationItem::update);
    connect(m_renderer.data(), &DecorationRenderer::invalidated, this, &KQuickDecorationItem::handleDecorationInvalidated);
}

void KQuickDecorationItem::handleDecorationInvalidated()
{
    m_decorationInvalidated = true;
    update();
}

QSGNode *KQuickDecorationItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_renderer->damage().isEmpty()) {
        m_renderer->render(m_renderer->damage());
        m_renderer->resetDamage();
    }

    TextureNode *node = static_cast<TextureNode *>(oldNode);
    if (!node) {
        node = new TextureNode(window());
        m_decorationInvalidated = true;
    }

    node->setTexture(m_renderer->toOpenGLTexture().data()); // FIXME: Ownership.
    if (m_decorationInvalidated) {
        m_decorationInvalidated = false;
        node->setGeometry(m_renderer->buildQuads(m_client));
    }

    return node;
}

KQuickSurfaceItem::KQuickSurfaceItem(Surface *surface, QQuickItem *parent)
    : QQuickItem(parent)
    , SurfaceView(surface)
{
    setFlag(ItemHasContents);

    setVisible(surface->isMapped());
    setPosition(surface->position());
    setSize(surface->size());

    handleBelowChanged();
    handleAboveChanged();

    connect(surface, &Surface::damaged, this, &KQuickSurfaceItem::handleDamaged);
    connect(surface, &Surface::positionChanged, this, &KQuickSurfaceItem::handlePositionChanged);
    connect(surface, &Surface::sizeChanged, this, &KQuickSurfaceItem::handleSizeChanged);
    connect(surface, &Surface::belowChanged, this, &KQuickSurfaceItem::handleBelowChanged);
    connect(surface, &Surface::aboveChanged, this, &KQuickSurfaceItem::handleAboveChanged);
    connect(surface, &Surface::mappedChanged, this, &KQuickSurfaceItem::handleMappedChanged);
}

void KQuickSurfaceItem::handleMappedChanged()
{
    setVisible(surface()->isMapped());
}

void KQuickSurfaceItem::handlePositionChanged()
{
    setPosition(surface()->position());
}

void KQuickSurfaceItem::handleSizeChanged()
{
    setSize(surface()->size());
}

void KQuickSurfaceItem::handleDamaged(const QRegion &region)
{
    m_damage += region;
    update();
}

KQuickSurfaceItem *KQuickSurfaceItem::getOrCreateChild(Surface *surface)
{
    KQuickSurfaceItem *&item = m_children[surface];
    if (!item) {
        item = new KQuickSurfaceItem(surface, this);
        item->setParent(this);
        connect(surface, &QObject::destroyed, this, [this, surface]() {
            delete m_children.take(surface);
        });
    }
    return item;
}

void KQuickSurfaceItem::handleBelowChanged()
{
    const QList<Surface *> below = surface()->below();
    for (int i = 0; i < below.size(); ++i) {
        getOrCreateChild(below[i])->setZ(i - below.size());
    }
}

void KQuickSurfaceItem::handleAboveChanged()
{
    const QList<Surface *> above = surface()->above();
    for (int i = 0; i < above.size(); ++i) {
        getOrCreateChild(above[i])->setZ(i);
    }
}

void KQuickSurfaceItem::surfacePixmapInvalidated()
{
    m_pixmapInvalidated = true;
    update();
}

void KQuickSurfaceItem::surfaceQuadsInvalidated()
{
    m_geometryInvalidated = true;
    update();
}

QSGNode *KQuickSurfaceItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (m_pixmapInvalidated || !m_surfacePixmap) {
        m_pixmapInvalidated = false;
        m_surfacePixmap.reset(surface()->createPixmap());
    }

    if (m_surfacePixmap->isValid()) {
        m_surfacePixmap->update();
    } else {
        m_surfacePixmap->create();
        if (!m_surfacePixmap->isValid()) {
            delete oldNode;
            return nullptr;
        }
    }

    auto surfaceTexture = static_cast<OpenGLSurfaceTexture *>(m_surfacePixmap->texture());
    if (surfaceTexture->texture()) {
        if (!m_damage.isEmpty()) {
            surfaceTexture->update(m_damage);
            m_damage = QRegion();
        }
    } else {
        if (!surfaceTexture->create()) {
            delete oldNode;
            return nullptr;
        }
        m_damage = QRegion();
    }

    TextureNode *node = static_cast<TextureNode *>(oldNode);
    if (!node) {
        node = new TextureNode(window());
        m_geometryInvalidated = true;
    }

    node->setTexture(surfaceTexture->texture());
    if (m_geometryInvalidated) {
        m_geometryInvalidated = false;
        node->setGeometry(surface()->shape(), surface()->surfaceToBufferMatrix());
    }

    return node;
}

KQuickWindowItem::KQuickWindowItem(QQuickItem *parent)
    : QQuickItem(parent)
{
}

QUuid KQuickWindowItem::sourceId() const
{
    return m_sourceId;
}

void KQuickWindowItem::setSourceId(const QUuid &sourceId)
{
    if (m_sourceId == sourceId) {
        return;
    }

    m_sourceId = sourceId;

    if (!m_sourceId.isNull()) {
        setSource(workspace()->findAbstractClient(sourceId));
    } else if (m_source) {
        m_source = nullptr;
        Q_EMIT sourceChanged();
    }

    Q_EMIT sourceIdChanged();
}

AbstractClient *KQuickWindowItem::source() const
{
    return m_source;
}

void KQuickWindowItem::setSource(AbstractClient *source)
{
    if (m_source == source) {
        return;
    }

    Compositor *compositor = Compositor::self();
    if (m_source) {
        disconnect(compositor, &Compositor::compositingToggled, this, &KQuickWindowItem::updateItems);
        disconnect(m_source, &Toplevel::sceneSurfaceChanged, this, &KQuickWindowItem::updateSurfaceItem);
        disconnect(m_source, &Toplevel::shadowChanged, this, &KQuickWindowItem::updateShadowItem);
        disconnect(m_source, &AbstractClient::decorationChanged, this, &KQuickWindowItem::updateDecorationItem);
        disconnect(m_source, &Toplevel::frameGeometryChanged, this, &KQuickWindowItem::polish);
        disconnect(m_source, &Toplevel::bufferGeometryChanged, this, &KQuickWindowItem::polish);
        disconnect(m_source, &Toplevel::frameGeometryChanged, this, &KQuickWindowItem::updateImplicitSize);
    }

    m_source = source;

    m_shadowItem.reset();
    m_decorationItem.reset();
    m_surfaceItem.reset();

    if (m_source) {
        setSourceId(source->internalId());

        updateImplicitSize();
        updateShadowItem();
        updateDecorationItem();
        updateSurfaceItem();

        connect(compositor, &Compositor::compositingToggled, this, &KQuickWindowItem::updateItems);
        connect(m_source, &Toplevel::sceneSurfaceChanged, this, &KQuickWindowItem::updateSurfaceItem);
        connect(m_source, &Toplevel::shadowChanged, this, &KQuickWindowItem::updateShadowItem);
        connect(m_source, &AbstractClient::decorationChanged, this, &KQuickWindowItem::updateDecorationItem);
        connect(m_source, &Toplevel::frameGeometryChanged, this, &KQuickWindowItem::polish);
        connect(m_source, &Toplevel::bufferGeometryChanged, this, &KQuickWindowItem::polish);
        connect(m_source, &Toplevel::frameGeometryChanged, this, &KQuickWindowItem::updateImplicitSize);
    } else {
        m_sourceId = QUuid();
        Q_EMIT sourceIdChanged();
    }

    Q_EMIT sourceChanged();
}

void KQuickWindowItem::updateImplicitSize()
{
    setImplicitSize(m_source->width(), m_source->height());
}

void KQuickWindowItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (newGeometry.size() != oldGeometry.size()) {
        polish();
    }
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

void KQuickWindowItem::updatePolish()
{
    if (Q_LIKELY(Compositor::compositing())) {
        if (Q_LIKELY(m_source)) {
            const qreal scale = width() / m_source->width();
            if (m_shadowItem) {
                const QMargins offset = m_shadowItem->shadow()->offset();
                m_shadowItem->setPosition(-QPointF(offset.left(), offset.top()) * scale);
                m_shadowItem->setScale(scale);
                m_shadowItem->setTransformOrigin(QQuickItem::TopLeft);
            }
            if (m_decorationItem) {
                m_decorationItem->setScale(scale);
                m_decorationItem->setTransformOrigin(QQuickItem::TopLeft);
            }
            if (m_surfaceItem) {
                const QPointF framePos = m_source->frameGeometry().topLeft();
                const QPointF bufferPos = m_source->bufferGeometry().topLeft();
                m_surfaceItem->setPosition((bufferPos - framePos) * scale);
                m_surfaceItem->setScale(scale);
                m_surfaceItem->setTransformOrigin(QQuickItem::TopLeft);
            }
        }
    }
}

void KQuickWindowItem::updateItems()
{
    updateShadowItem();
    updateDecorationItem();
    updateSurfaceItem();
}

void KQuickWindowItem::updateShadowItem()
{
    if (Compositor::compositing() && m_source->shadow()) {
        if (!m_shadowItem || m_shadowItem->shadow() != m_source->shadow()) {
            m_shadowItem.reset(new KQuickShadowItem(m_source->shadow(), m_source, this));
        }
        polish();
        if (m_decorationItem) {
            m_shadowItem->stackBefore(m_decorationItem.data());
        } else if (m_surfaceItem) {
            m_shadowItem->stackBefore(m_decorationItem.data());
        }
    } else {
        m_shadowItem.reset();
    }
}

void KQuickWindowItem::updateDecorationItem()
{
    if (Compositor::compositing() && m_source->decoration()) {
        m_decorationItem.reset(new KQuickDecorationItem(m_source->decoration(), m_source, this));
        polish();
        if (m_shadowItem) {
            m_decorationItem->stackAfter(m_shadowItem.data());
        } else if (m_surfaceItem) {
            m_decorationItem->stackBefore(m_surfaceItem.data());
        }
    } else {
        m_decorationItem.reset();
    }
}

void KQuickWindowItem::updateSurfaceItem()
{
    if (Compositor::compositing() && m_source->sceneSurface()) {
        m_surfaceItem.reset(new KQuickSurfaceItem(m_source->sceneSurface(), this));
        polish();
    } else {
        m_surfaceItem.reset();
    }
}

} // namespace KWin
