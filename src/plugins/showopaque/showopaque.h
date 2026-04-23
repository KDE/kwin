/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "effect/effect.h"

namespace KWin
{

class Item;

class ShowOpaque : public Effect
{
    Q_OBJECT

public:
    explicit ShowOpaque();
    ~ShowOpaque() override;

private:
    void addWindow(EffectWindow *window);
    void detachItem(Item *item);
    void recursiveAttach(Item *item);

    std::unordered_map<Item *, std::unique_ptr<Item>> m_items;
};

}
