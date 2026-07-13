/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "scene/item.h"

namespace KWin
{

class EffectWindow;

/*!
 * A helper item that allows you to move, scale and rotate another item.
 * NOTE at least until further effects API changes, this must not be
 *      used for WindowItems! Use WindowItem::effectContainer instead.
 */
class KWIN_EXPORT TransformItem : public Item
{
    Q_OBJECT

public:
    explicit TransformItem(EffectWindow *toTransform);
    explicit TransformItem(Item *toTransform);
    ~TransformItem() override;

private:
    void updateZ();
};

}
