/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWin
{

class ImageItem;
class SurfaceItemWayland;

class CursorItem : public Item
{
    Q_OBJECT

public:
    explicit CursorItem(Scene *scene, Item *parent = nullptr);
    ~CursorItem() override;

private:
    void refresh();
    void setSurface(KWaylandServer::SurfaceInterface *surface);
    void setImage(const QImage &image);

    std::unique_ptr<ImageItem> m_imageItem;
    std::unique_ptr<SurfaceItemWayland> m_surfaceItem;
};

} // namespace KWin
