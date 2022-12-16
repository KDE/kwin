/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>

namespace KWin
{

class ShowPaintEffect : public Effect
{
    Q_OBJECT

public:
    ShowPaintEffect();

    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool isActive() const override;

private Q_SLOTS:
    void toggle();

private:
    void paintGL(const QMatrix4x4 &projection, qreal scale);
    void paintQPainter();

    bool m_active = false;
    QRegion m_painted; // what's painted in one pass
    int m_colorIndex = 0;
};

} // namespace KWin
