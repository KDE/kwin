/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "libkwineffects/kwinglobals.h"
#include "scene/item.h"

namespace KWin
{

class DragAndDropIcon;
class SurfaceItemWayland;

class DragAndDropIconItem : public Item
{
    Q_OBJECT

public:
    explicit DragAndDropIconItem(DragAndDropIcon *icon, Scene *scene, Item *parent = nullptr);
    ~DragAndDropIconItem() override;

    void frameRendered(quint32 timestamp);
    void presented(Output *output, std::chrono::nanoseconds refreshCycleDuration, std::chrono::nanoseconds timestamp, PresentationMode mode);

    void setOutput(Output *output);

private:
    std::unique_ptr<SurfaceItemWayland> m_surfaceItem;
    Output *m_output = nullptr;
};

} // namespace KWin
