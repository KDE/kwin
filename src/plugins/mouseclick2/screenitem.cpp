/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenitem.h"
#include "cursor.h"
#include "cursorsource.h"
#include "effect/effecthandler.h"
#include "effect/offscreenquickview.h"
#include "input_event.h"
#include "pointer_input.h"
#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"

#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{

// Dave, does it make sense to do this? We're always updating the main buffer anyway
// so planes don't help
// it'd make it easier to split this to a new effect.

// then we can re-add the glow

MyCursorItem::MyCursorItem(const CursorTheme &theme, Item *parent)
    : Item(parent)
{
    m_source = std::make_unique<ShapeCursorSource>();
    m_source->setTheme(theme);
    m_source->setShape(Qt::ArrowCursor);

    refresh();
    connect(m_source.get(), &CursorSource::changed, this, &MyCursorItem::refresh);
    connect(m_source.get(), &CursorSource::changed, this, &MyCursorItem::refresh);
}

void MyCursorItem::refresh()
{
    if (!m_imageItem) {
        m_imageItem = std::make_unique<ImageItem>(this);
    }
    m_imageItem->setImage(m_source->image());
    m_imageItem->setPosition(-m_source->hotspot());
    m_imageItem->setSize(m_source->image().deviceIndependentSize());
}

} // namespace KWin

#include "moc_mouseclickeffect.cpp"
