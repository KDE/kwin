/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_KSCREEN_H
#define KWIN_KSCREEN_H

#include <kwineffects.h>
// Qt
#include <QTimeLine>

namespace KWin
{

class KscreenEffect : public Effect
{
    Q_OBJECT

public:
    KscreenEffect();
    virtual ~KscreenEffect();

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
    QTimeLine m_timeLine;
    FadeOutState m_state;
    xcb_atom_t m_atom;
};


} // namespace KWin
#endif // KWIN_KSCREEN_H
