/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "effect/globals.h"
#include "scene/item.h"

namespace KWin
{

class DragAndDropIcon;
class SurfaceInterface;
class SurfaceItemWayland;
class PresentationFeedback;

class DragAndDropIconItem : public Item
{
    Q_OBJECT

public:
    explicit DragAndDropIconItem(DragAndDropIcon *icon, Item *parent = nullptr);
    ~DragAndDropIconItem() override;

    SurfaceInterface *surface() const;

    void setOutput(Output *output);

private:
    std::unique_ptr<SurfaceItemWayland> m_surfaceItem;
    Output *m_output = nullptr;
};

} // namespace KWin
