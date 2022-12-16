/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Philip Falkner <philip.falkner@gmail.com>
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
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

    struct Animation
    {
        EffectWindowDeletedRef deletedRef;
        EffectWindowVisibleRef visibleRef;
        TimeLine timeLine;
        int parentY;
    };

    QHash<EffectWindow *, Animation> m_animations;
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
