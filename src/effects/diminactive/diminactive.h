/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
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
    QHash<EffectWindow *, TimeLine> m_transitions;
    QHash<EffectWindow *, qreal> m_forceDim;

    struct
    {
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
