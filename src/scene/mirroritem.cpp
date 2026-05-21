/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mirroritem.h"
#include "window.h"
#include "windowitem.h"

namespace KWin
{

MirrorItem::MirrorItem(Item *toMirror, Item *parent)
    : Item(parent)
    , m_source(toMirror)
{
    if (auto window = qobject_cast<WindowItem *>(toMirror)) {
        // TODO handle this more nicely
        window->window()->refOffscreenRendering();
    }

    updateSize();
    connect(toMirror, &Item::sizeChanged, this, &MirrorItem::updateSize);

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
    if (auto window = qobject_cast<WindowItem *>(m_source)) {
        window->window()->unrefOffscreenRendering();
    }
}

void MirrorItem::updateSize()
{
    setSize(m_source->size());
}

void MirrorItem::updatePosition()
{
    setPosition(m_source->position());
}

void MirrorItem::updateZ()
{
    setZ(m_source->z());
}

void MirrorItem::addChild(Item *item)
{
    m_childMirrors.push_back(std::make_unique<MirrorItem>(item, this));
    m_childMirrors.back()->updatePosition();
    m_childMirrors.back()->updateZ();
    connect(item, &Item::positionChanged, m_childMirrors.back().get(), &MirrorItem::updatePosition);
    connect(item, &Item::zChanged, m_childMirrors.back().get(), &MirrorItem::updateZ);
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

void MirrorItem::preprocess()
{
    m_source->preprocess();
}

}
