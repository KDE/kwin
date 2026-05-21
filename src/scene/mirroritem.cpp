/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mirroritem.h"

namespace KWin
{

MirrorItem::MirrorItem(Item *toMirror, Item *parent)
    : Item(parent)
    , m_source(toMirror)
{
    const auto children = toMirror->sortedChildItems();
    for (Item *child : children) {
        addChild(child);
    }
    connect(toMirror, &Item::childAdded, this, &MirrorItem::addChild);
    connect(toMirror, &Item::childRemoved, this, &MirrorItem::removeChild);
    m_source->addMirror(this);
}

MirrorItem::~MirrorItem()
{
    m_source->removeMirror(this);
}

void MirrorItem::addChild(Item *item)
{
    m_childMirrors.push_back(std::make_unique<MirrorItem>(item, this));
    m_childMirrors.back()->setPosition(item->position());
    m_childMirrors.back()->setZ(item->z());
    connect(item, &Item::positionChanged, m_childMirrors.back().get(), [mirror = m_childMirrors.back().get(), item]() {
        mirror->setPosition(item->position());
    });
    connect(item, &Item::zChanged, m_childMirrors.back().get(), [mirror = m_childMirrors.back().get(), item]() {
        mirror->setZ(item->z());
    });
}

void MirrorItem::removeChild(Item *item)
{
    std::erase_if(m_childMirrors, [item](const auto &other) {
        return other.get() == item;
    });
}

Item *MirrorItem::source() const
{
    return m_source;
}

}
