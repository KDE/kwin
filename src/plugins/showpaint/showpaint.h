/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"

namespace KWin
{

class ShowPaintEffect : public Effect
{
    Q_OBJECT

public:
    ShowPaintEffect();

    bool paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;

private:
    void paintGL(const RenderTarget &renderTarget, const RenderViewport &viewport, const Region &deviceRegion);

    int m_colorIndex = 0;
};

} // namespace KWin
