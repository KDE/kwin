/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QQuickItem>

#include <memory>

class QSGTextureProvider;

namespace KWin
{

class BackgroundEffectItem;

class ScreenContentsItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    ScreenContentsItem(QQuickItem *parent = nullptr);
    ~ScreenContentsItem() override;

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

    QSGNode *updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *) override;

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value) override;
    void releaseResources() override;

private:
    void updateSourceGeometry();

    std::unique_ptr<BackgroundEffectItem> m_source;
    mutable QSGTextureProvider *m_provider = nullptr;
};

}
