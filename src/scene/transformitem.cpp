/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "transformitem.h"
#include "effect/effectwindow.h"
#include "scene/windowitem.h"

namespace KWin
{

TransformItem::TransformItem(EffectWindow *toTransform)
    : TransformItem(toTransform->windowItem()->transformContainer())
{
}

TransformItem::TransformItem(Item *toTransform)
    : Item(toTransform->parentItem())
{
    toTransform->setParentItem(this);
    // updateZ();
    // connect(toTransform, &Item::zChanged, this, &TransformItem::updateZ);
}

TransformItem::~TransformItem()
{
    const auto children = childItems();
    for (Item *item : children) {
        item->setParentItem(parentItem());
    }
}

// void TransformItem::updateZ()
// {
//     const auto children = childItems();
//     if (!children.isEmpty()) {
//         setZ(children.front()->z());
//     }
// }

}

#include "moc_transformitem.cpp"
