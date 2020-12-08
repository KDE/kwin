/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_FALLAPART_H
#define KWIN_FALLAPART_H

#include <kwineffects.h>

namespace KWin
{

class FallApartEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int blockSize READ configuredBlockSize)
public:
    FallApartEffect();
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 70;
    }

    // for properties
    int configuredBlockSize() const {
        return blockSize;
    }

    static bool supported();

public Q_SLOTS:
    void slotWindowClosed(KWin::EffectWindow *c);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowDataChanged(KWin::EffectWindow *w, int role);

private:
    QHash< EffectWindow*, double > windows;
    bool isRealWindow(EffectWindow* w);
    int blockSize;
};

} // namespace

#endif
