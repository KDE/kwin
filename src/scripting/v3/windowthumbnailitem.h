/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "decorationitem.h"
#include "surface.h"

#include <QQuickItem>
#include <QUuid>

namespace KDecoration2
{
class Decoration;
}

namespace KWin
{

class AbstractClient;
class Shadow;

/**
 * The KQuickShadowItem class provides a way to display a server-side decoration of a window.
 */
class KQuickShadowItem : public QQuickItem
{
    Q_OBJECT

public:
    explicit KQuickShadowItem(Shadow *shadow, AbstractClient *client, QQuickItem *parent = nullptr);

    Shadow *shadow() const;

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private Q_SLOTS:
    void handleGeometryInvalidated();
    void handleTextureInvalidated();

private:
    Shadow *m_shadow;
    AbstractClient *m_client;
    bool m_geometryInvalidated = true;
    bool m_textureInvalidated = true;
};

/**
 * The KQuickDecorationItem class represents a server-side decoration of the window.
 */
class KQuickDecorationItem : public QQuickItem
{
    Q_OBJECT

public:
    explicit KQuickDecorationItem(KDecoration2::Decoration *decoration,
                                  AbstractClient *client, QQuickItem *parent = nullptr);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    void handleDecorationInvalidated();

    KDecoration2::Decoration *m_decoration;
    AbstractClient *m_client;
    QScopedPointer<DecorationRenderer> m_renderer;
    bool m_decorationInvalidated = true;
};

/**
 * The KQuickSurfaceItem class represents a surface with some contents. Surface items form
 * a tree.
 */
class KQuickSurfaceItem : public QQuickItem, public SurfaceView
{
    Q_OBJECT

public:
    explicit KQuickSurfaceItem(Surface *surface, QQuickItem *parent = nullptr);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

    void surfacePixmapInvalidated() override;
    void surfaceQuadsInvalidated() override;

private:
    KQuickSurfaceItem *getOrCreateChild(Surface *surface);

    void handleMappedChanged();
    void handlePositionChanged();
    void handleSizeChanged();
    void handleDamaged(const QRegion &region);
    void handleBelowChanged();
    void handleAboveChanged();

    QScopedPointer<SurfacePixmap> m_surfacePixmap;
    QRegion m_damage;
    QHash<Surface *, KQuickSurfaceItem *> m_children;
    bool m_pixmapInvalidated = true;
    bool m_geometryInvalidated = true;
};

/**
 * The KQuickWindowItem class represents a window. The window is made of a KQuickSurfaceItem,
 * and optionally a KQuickShadowItem and a KQuickDecorationItem.
 */
class KQuickWindowItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QUuid sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
    Q_PROPERTY(KWin::AbstractClient *source READ source WRITE setSource NOTIFY sourceChanged)

public:
    explicit KQuickWindowItem(QQuickItem *parent = nullptr);

    QUuid sourceId() const;
    void setSourceId(const QUuid &sourceId);

    AbstractClient *source() const;
    void setSource(AbstractClient *source);

protected:
    void updatePolish() override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

Q_SIGNALS:
    void sourceChanged();
    void sourceIdChanged();

private:
    void updateSurfaceItem();
    void updateDecorationItem();
    void updateShadowItem();
    void updateImplicitSize();
    void updateItems();

    QScopedPointer<KQuickShadowItem> m_shadowItem;
    QScopedPointer<KQuickDecorationItem> m_decorationItem;
    QScopedPointer<KQuickSurfaceItem> m_surfaceItem;

    QUuid m_sourceId;
    QPointer<AbstractClient> m_source;
};

} // namespace KWin
