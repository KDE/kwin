#pragma once

#include <QQuickItem>
#include <QSharedPointer>
#include <QUuid>

namespace KWin
{
class AbstractClient;
class GLTexture;

//TODO copy abstract/window/desktop
class ThumbnailItem2 : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QUuid wId READ wId WRITE setWId NOTIFY wIdChanged)
    Q_PROPERTY(KWin::AbstractClient *client READ client WRITE setClient)

public:
    ThumbnailItem2();
    void updateFrame();
    QSGNode *updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *updatePaintNodeData) override;

    QUuid wId() const;
    void setWId(const QUuid &wId);

    AbstractClient *client() const;
    void setClient(AbstractClient *client);

Q_SIGNALS:
    void wIdChanged();
    void clientChanged();

private:
    bool m_damaged = true;
    QUuid m_wId;
    QSharedPointer<GLTexture> m_frameTexture;
    QPointer<AbstractClient> m_client;
};

}
