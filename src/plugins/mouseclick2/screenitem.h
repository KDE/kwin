/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/bufferitem.h"

namespace KWin
{

class ScreenContentsItem : public BufferItem
{
    Q_OBJECT

public:
    ScreenContentsItem(const CursorTheme &theme, Item *parent);

private:
};

}
