/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/dndiconitem.h"
#include "scene/surfaceitem_wayland.h"
#include "wayland/datadevice.h"
#include "wayland/surface.h"

namespace KWin
{

DragAndDropIconItem::DragAndDropIconItem(DragAndDropIcon *icon, Scene *scene, Item *parent)
    : Item(scene, parent)
{
    m_surfaceItem = std::make_unique<SurfaceItemWayland>(icon->surface(), scene, this);
    m_surfaceItem->setPosition(icon->position());

    connect(icon, &DragAndDropIcon::destroyed, this, [this]() {
        m_surfaceItem.reset();
    });
    connect(icon, &DragAndDropIcon::changed, this, [this, icon]() {
        m_surfaceItem->setPosition(icon->position());
    });
}

DragAndDropIconItem::~DragAndDropIconItem()
{
}

void DragAndDropIconItem::frameRendered(quint32 timestamp)
{
    if (m_surfaceItem) {
        m_surfaceItem->surface()->frameRendered(timestamp);
    }
}

void DragAndDropIconItem::setOutput(Output *output)
{
    if (m_surfaceItem && output) {
        m_surfaceItem->surface()->setPreferredBufferScale(output->scale());
    }
}

} // namespace KWin

#include "moc_dndiconitem.cpp"
