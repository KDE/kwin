/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SLIDE_H
#define KWIN_SLIDE_H

// kwineffects
#include <kwineffects.h>

namespace KWin
{

class SlideEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int duration READ duration)
    Q_PROPERTY(int horizontalGap READ horizontalGap)
    Q_PROPERTY(int verticalGap READ verticalGap)
    Q_PROPERTY(bool slideDocks READ slideDocks)
    Q_PROPERTY(bool slideBackground READ slideBackground)

public:
    SlideEffect();
    ~SlideEffect() override;

    void reconfigure(ReconfigureFlags) override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;

    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool isActive() const override {
        return m_active;
    }

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();

    int duration() const;
    int horizontalGap() const;
    int verticalGap() const;
    bool slideDocks() const;
    bool slideBackground() const;

private Q_SLOTS:
    void desktopChanged(int old, int current, EffectWindow *with);
    void windowAdded(EffectWindow *w);
    void windowDeleted(EffectWindow *w);

    void numberDesktopsChanged(uint old);
    void numberScreensChanged();

private:
    QPoint desktopCoords(int id) const;
    QRect desktopGeometry(int id) const;
    int workspaceWidth() const;
    int workspaceHeight() const;

    bool isTranslated(const EffectWindow *w) const;
    bool isPainted(const EffectWindow *w) const;
    bool shouldElevate(const EffectWindow *w) const;

    void start(int old, int current, EffectWindow *movingWindow = nullptr);
    void stop();

private:
    int m_hGap;
    int m_vGap;
    bool m_slideDocks;
    bool m_slideBackground;

    bool m_active = false;
    TimeLine m_timeLine;
    QPoint m_startPos;
    QPoint m_diff;
    EffectWindow *m_movingWindow = nullptr;

    struct {
        int desktop;
        bool firstPass;
        bool lastPass;
        QPoint translation;

        EffectWindowList fullscreenWindows;
    } m_paintCtx;

    EffectWindowList m_elevatedWindows;
};

inline int SlideEffect::duration() const
{
    return m_timeLine.duration().count();
}

inline int SlideEffect::horizontalGap() const
{
    return m_hGap;
}

inline int SlideEffect::verticalGap() const
{
    return m_vGap;
}

inline bool SlideEffect::slideDocks() const
{
    return m_slideDocks;
}

inline bool SlideEffect::slideBackground() const
{
    return m_slideBackground;
}

} // namespace KWin

#endif
