/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "scene/item.h"

namespace KWin
{

class KWIN_EXPORT MirrorItem : public Item
{
    Q_OBJECT

public:
    explicit MirrorItem(Item *toMirror, Item *parent);
    ~MirrorItem() override;

    Item *source() const;

private:
    void addChild(Item *item);
    void removeChild(Item *item);

    Item *const m_source;
    std::vector<std::unique_ptr<MirrorItem>> m_childMirrors;
};

}
