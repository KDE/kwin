/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "showopaque.h"
#include "effect/effecthandler.h"
#include "scene/imageitem.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"

#include <QPainter>

namespace KWin
{

class OpaqueItem : public ImageItem
{
public:
    explicit OpaqueItem(SurfaceItem *parent)
        : ImageItem(parent)
    {
        setZ(1);
        connect(parent, &SurfaceItem::opaqueChanged, this, &OpaqueItem::update);
        update();
    }

    void update()
    {
        const auto opaque = parentItem()->opaque();
        const QSize size(std::ceil(parentItem()->size().width()), std::ceil(parentItem()->size().height()));
        QImage img(size, QImage::Format_RGBA8888_Premultiplied);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        painter.setBrush(QColor(0, 0, 128, 128));
        for (const RectF &rect : opaque.rects()) {
            painter.drawRect(rect);
        }
        setImage(img);
        setSize(size);
    }
};

ShowOpaque::ShowOpaque()
{
    connect(effects->scene(), &Scene::itemRemoved, this, &ShowOpaque::detachItem);
    recursiveAttach(effects->scene()->containerItem());

    connect(effects, &EffectsHandler::windowAdded, this, &ShowOpaque::addWindow);
}

ShowOpaque::~ShowOpaque()
{
    disconnect(effects->scene(), nullptr, this, nullptr);
    m_items.clear();
}

void ShowOpaque::addWindow(EffectWindow *window)
{
    recursiveAttach(window->windowItem());
}

void ShowOpaque::recursiveAttach(Item *item)
{
    auto surfaceItem = qobject_cast<SurfaceItem *>(item);
    if (surfaceItem) {
        m_items[surfaceItem] = std::make_unique<OpaqueItem>(surfaceItem);
    }
    connect(item, &Item::childAdded, this, &ShowOpaque::recursiveAttach);
    connect(item, &Item::childRemoved, this, &ShowOpaque::detachItem);
    const auto children = item->childItems();
    for (Item *item : children) {
        recursiveAttach(item);
    }
}

void ShowOpaque::detachItem(Item *item)
{
    m_items.erase(item);
}

}

#include "moc_showopaque.cpp"
