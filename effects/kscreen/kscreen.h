/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_KSCREEN_H
#define KWIN_KSCREEN_H

#include <kwineffects.h>

namespace KWin
{

class KscreenEffect : public Effect
{
    Q_OBJECT

public:
    KscreenEffect();
    ~KscreenEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 99;
    }

private Q_SLOTS:
    void propertyNotify(KWin::EffectWindow *window, long atom);

private:
    void switchState();
    enum FadeOutState {
        StateNormal,
        StateFadingOut,
        StateFadedOut,
        StateFadingIn
    };
    TimeLine m_timeLine;
    FadeOutState m_state;
    xcb_atom_t m_atom;
};


} // namespace KWin
#endif // KWIN_KSCREEN_H
