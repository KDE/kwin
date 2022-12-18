/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWaylandServer
{
class DragAndDropIcon;
}

namespace KWin
{

class SurfaceItemWayland;

class DragAndDropIconItem : public Item
{
    Q_OBJECT

public:
    explicit DragAndDropIconItem(KWaylandServer::DragAndDropIcon *icon, Scene *scene, Item *parent = nullptr);
    ~DragAndDropIconItem() override;

    void frameRendered(quint32 timestamp);

private:
    std::unique_ptr<SurfaceItemWayland> m_surfaceItem;
};

} // namespace KWin
