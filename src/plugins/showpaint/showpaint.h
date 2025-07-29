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

    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &deviceRegion, Output *screen) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &deviceGeometry, WindowPaintData &data) override;

private:
    void paintGL(const RenderTarget &renderTarget, const RenderViewport &viewport);
    void paintQPainter(const RenderViewport &viewport);

    QRegion m_painted; // what's painted in one pass
    int m_colorIndex = 0;
};

} // namespace KWin
