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

} // namespace KWin
