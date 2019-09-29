/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
Copyright (C) 2018 Vlad Zahorodnii <vladzzag@gmail.com>

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

#ifndef KWIN_DIMINACTIVE_H
#define KWIN_DIMINACTIVE_H

// kwineffects
#include <kwineffects.h>

namespace KWin
{

class DimInactiveEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int dimStrength READ dimStrength)
    Q_PROPERTY(bool dimPanels READ dimPanels)
    Q_PROPERTY(bool dimDesktop READ dimDesktop)
    Q_PROPERTY(bool dimKeepAbove READ dimKeepAbove)
    Q_PROPERTY(bool dimByGroup READ dimByGroup)
    Q_PROPERTY(bool dimFullScreen READ dimFullScreen)

public:
    DimInactiveEffect();
    ~DimInactiveEffect() override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void postPaintScreen() override;

    int requestedEffectChainPosition() const override;
    bool isActive() const override;

    int dimStrength() const;
    bool dimPanels() const;
    bool dimDesktop() const;
    bool dimKeepAbove() const;
    bool dimByGroup() const;
    bool dimFullScreen() const;

private Q_SLOTS:
    void windowActivated(EffectWindow *w);
    void windowClosed(EffectWindow *w);
    void windowDeleted(EffectWindow *w);
    void activeFullScreenEffectChanged();

    void updateActiveWindow(EffectWindow *w);

private:
    void dimWindow(WindowPaintData &data, qreal strength);
    bool canDimWindow(const EffectWindow *w) const;
    void scheduleInTransition(EffectWindow *w);
    void scheduleGroupInTransition(EffectWindow *w);
    void scheduleOutTransition(EffectWindow *w);
    void scheduleGroupOutTransition(EffectWindow *w);
    void scheduleRepaint(EffectWindow *w);

private:
    qreal m_dimStrength;
    bool m_dimPanels;
    bool m_dimDesktop;
    bool m_dimKeepAbove;
    bool m_dimByGroup;
    bool m_dimFullScreen;

    EffectWindow *m_activeWindow = nullptr;
    const EffectWindowGroup *m_activeWindowGroup;
    QHash<EffectWindow*, TimeLine> m_transitions;
    QHash<EffectWindow*, qreal> m_forceDim;

    struct {
        bool active = false;
        TimeLine timeLine;
    } m_fullScreenTransition;
};

inline int DimInactiveEffect::requestedEffectChainPosition() const
{
    return 50;
}

inline bool DimInactiveEffect::isActive() const
{
    return true;
}

inline int DimInactiveEffect::dimStrength() const
{
    return qRound(m_dimStrength * 100.0);
}

inline bool DimInactiveEffect::dimPanels() const
{
    return m_dimPanels;
}

inline bool DimInactiveEffect::dimDesktop() const
{
    return m_dimDesktop;
}

inline bool DimInactiveEffect::dimKeepAbove() const
{
    return m_dimKeepAbove;
}

inline bool DimInactiveEffect::dimByGroup() const
{
    return m_dimByGroup;
}

inline bool DimInactiveEffect::dimFullScreen() const
{
    return m_dimFullScreen;
}

} // namespace KWin

#endif
