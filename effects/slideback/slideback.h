/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Michael Zanetti <michael_zanetti@gmx.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SLIDEBACK_H
#define KWIN_SLIDEBACK_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

class SlideBackEffect
    : public Effect
{
    Q_OBJECT
public:
    SlideBackEffect();

    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintWindow(EffectWindow* w) override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 50;
    }

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowUnminimized(KWin::EffectWindow *w);
    void slotStackingOrderChanged();
    void slotTabBoxAdded();
    void slotTabBoxClosed();

private:

    WindowMotionManager motionManager;
    EffectWindowList usableOldStackingOrder;
    EffectWindowList oldStackingOrder;
    EffectWindowList coveringWindows;
    EffectWindowList elevatedList;
    EffectWindow *m_justMapped, *m_upmostWindow;
    QHash<EffectWindow *, QRect> destinationList;
    int m_tabboxActive;
    QList <QRegion> clippedRegions;

    QRect getSlideDestination(const QRect &windowUnderGeometry, const QRect &windowOverGeometry);
    bool isWindowUsable(EffectWindow *w);
    bool intersects(EffectWindow *windowUnder, const QRect &windowOverGeometry);
    EffectWindowList usableWindows(const EffectWindowList &allWindows);
    QRect getModalGroupGeometry(EffectWindow *w);
    void windowRaised(EffectWindow *w);

};

} // namespace

#endif
