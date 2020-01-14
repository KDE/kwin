/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>
Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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

#ifndef KWIN_SHEET_H
#define KWIN_SHEET_H

// kwineffects
#include <kwineffects.h>

namespace KWin
{

class SheetEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int duration READ duration)

public:
    SheetEffect();

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void postPaintWindow(EffectWindow *w) override;

    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    static bool supported();

    int duration() const;

private Q_SLOTS:
    void slotWindowAdded(EffectWindow *w);
    void slotWindowClosed(EffectWindow *w);
    void slotWindowDeleted(EffectWindow *w);

private:
    bool isSheetWindow(EffectWindow *w) const;

private:
    std::chrono::milliseconds m_duration;

    struct Animation {
        TimeLine timeLine;
        int parentY;
    };

    QHash<EffectWindow*, Animation> m_animations;
};

inline int SheetEffect::requestedEffectChainPosition() const
{
    return 60;
}

inline int SheetEffect::duration() const
{
    return m_duration.count();
}

} // namespace KWin

#endif
