/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effectwindow.h"
#include "effect/offscreeneffect.h"
#include "effect/timeline.h"

namespace KWin
{

struct FallApartAnimation
{
    EffectWindowDeletedRef deletedRef;
    AnimationClock clock;
    qreal progress = 0;
};

class FallApartEffect : public OffscreenEffect
{
    Q_OBJECT
    Q_PROPERTY(int blockSize READ configuredBlockSize)
public:
    FallApartEffect();
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 70;
    }

    // for properties
    int configuredBlockSize() const
    {
        return blockSize;
    }

    static bool supported();

protected:
    void apply(EffectWindow *w, int mask, WindowPaintData &data, WindowQuadList &quads) override;

public Q_SLOTS:
    void slotWindowClosed(KWin::EffectWindow *c);
    void slotWindowDataChanged(KWin::EffectWindow *w, int role);

private:
    QHash<EffectWindow *, FallApartAnimation> windows;
    bool isRealWindow(EffectWindow *w);
    int blockSize;
};

} // namespace
