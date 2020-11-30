#include "thumbnailitem2.h"

#include "workspace.h"
#include "abstract_client.h"
#include "kwingltexture.h"
#include "effects.h"

#include <QSGNode>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>


using namespace KWin;

ThumbnailItem2::ThumbnailItem2()
{
    // if we resize, update
    //TODO

    // if the texture changes, update

    // if the source texture resizes, recreate our QSGTextureNode
    //TODO

    // should we handle compositor reloading?
    // fallback for uncomposited case?

    // do something clever based on our own item visibility
    // and implement window->releaseResources

    // TODO report to parachute devs that they're active constantly...

    // Set this based on whether m_client is set
    setFlag(QQuickItem::ItemHasContents);
}

void ThumbnailItem2::updateFrame()
{
    m_damaged = true;
    update();
}

QSGNode * ThumbnailItem2::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *updatePaintNodeData)
{
    Q_UNUSED(updatePaintNodeData)
    if (!m_client || !m_client->effectWindow()) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode;

        // does this potentially means a release of the old texture with the main thread running?
        // Is there some quirk about how replacing a texture doesn't delete the old one?
        node->setOwnsTexture(true);
    }

    if (m_damaged) {
        // stored as a member variable to keep the shared pointer alive till we're destroyed or the frame is replaced
        //
        m_frameTexture = m_client->effectWindow()->sceneWindow()->windowTexture();

        // this is pretty wasteful, problem is we switch between two GLTextures for front and back. So wrapping is unlike other situations
        // options are:
        // * leave as is and create every update (it's apparently not too expnsive...)
        // * Create a single new FBO we own and blit the window render into here (best option if we're going through the else branch of OpenGLWindow::windowTexture which makes a new FBO anyway)
        // * some some sort of cache inside here
        // * store the QSGTexture as some object inside the KWin::GLTexture and have KWin::GLTexture manage the lifespan

        auto texture = window()->createTextureFromId(m_frameTexture->texture(), m_frameTexture->size());
        texture->setFiltering(QSGTexture::Linear);
//         qDebug() << texture << m_frameTexture->texture();
        node->setTexture(texture);
        m_damaged = false;
    }


//     node->markDirty(QSGNode::DirtyMaterial);

    // centre align keeping aspect ratio within our own size
    const QSizeF size(node->texture()->textureSize().scaled(boundingRect().size().toSize(), Qt::KeepAspectRatio));
    const qreal x = boundingRect().x() + (boundingRect().width() - size.width()) / 2;
    const qreal y = boundingRect().y() + (boundingRect().height() - size.height()) / 2;
    node->setRect(QRectF(QPointF(x, y), size));

    return node;
}

QUuid ThumbnailItem2::wId() const
 {
     return m_wId;
 }

void ThumbnailItem2::setWId(const QUuid &wId)
{
    if (m_wId == wId) {
        return;
    }
    m_wId = wId;
    if (m_wId != nullptr) {
        setClient(workspace()->findAbstractClient([this] (const AbstractClient *c) { return c->internalId() == m_wId; }));
    } else if (m_client) {
        m_client = nullptr;
        emit clientChanged();
    }
    emit wIdChanged();
}

 AbstractClient* ThumbnailItem2::client() const
 {
     return m_client;
 }

void ThumbnailItem2::setClient(AbstractClient *client)
{
    // this is copy pasted and needlessly recursive.

    if (m_client == client) {
        return;
    }
    if (m_client) {
        disconnect(m_client, &Toplevel::damaged, this, &ThumbnailItem2::updateFrame);
    }

    m_client = client;
    if (m_client) {
        connect(m_client, &Toplevel::damaged, this, &ThumbnailItem2::updateFrame);
        setWId(m_client->internalId());
    } else {
        setWId({});
    }

    polish();

    emit clientChanged();
}

