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

DragAndDropIconItem::DragAndDropIconItem(DragAndDropIcon *icon, Item *parent)
    : Item(parent)
{
    m_surfaceItem = std::make_unique<SurfaceItemWayland>(icon->surface(), this);
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

SurfaceInterface *DragAndDropIconItem::surface() const
{
    return m_surfaceItem ? m_surfaceItem->surface() : nullptr;
}

void DragAndDropIconItem::setOutput(Output *output)
{
    if (m_surfaceItem && output) {
        m_output = output;
        m_surfaceItem->surface()->setPreferredBufferScale(output->scale());
        m_surfaceItem->surface()->setPreferredColorDescription(output->colorDescription());
    }
}

} // namespace KWin

#include "moc_dndiconitem.cpp"
