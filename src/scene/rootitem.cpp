/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/rootitem.h"

namespace KWin
{

RootItem::RootItem(Scene *scene)
{
    setScene(scene);
}

void RootItem::framePainted(Output *output, OutputFrame *frame, std::chrono::milliseconds timestamp)
{
    handleFramePainted(output, frame, timestamp);
    const auto children = childItems();
    for (const auto child : children) {
        child->framePainted(output, frame, timestamp);
    }
}

} // namespace KWin
