/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Marco Martin notmart @gmail.com
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SLIDINGPOPUPS_H
#define KWIN_SLIDINGPOPUPS_H

// Include with base class for effects.
#include <kwineffects.h>

#include <KWaylandServer/slide_interface.h>

namespace KWin
{

class SlidingPopupsEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int slideInDuration READ slideInDuration)
    Q_PROPERTY(int slideOutDuration READ slideOutDuration)

public:
    SlidingPopupsEffect();
    ~SlidingPopupsEffect() override;

    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void postPaintWindow(EffectWindow *w) override;
    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 40;
    }

    static bool supported();

    int slideInDuration() const;
    int slideOutDuration() const;

    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void slotWindowAdded(EffectWindow *w);
    void slotWindowDeleted(EffectWindow *w);
    void slotPropertyNotify(EffectWindow *w, long atom);
    void slotWaylandSlideOnShowChanged(EffectWindow *w);

    void slideIn(EffectWindow *w);
    void slideOut(EffectWindow *w);
    void stopAnimations();

private:
    void setupAnimData(EffectWindow *w);
    void setupInternalWindowSlide(EffectWindow *w);
    void setupSlideData(EffectWindow *w);

    static KWaylandServer::SlideManagerInterface *s_slideManager;
    static QTimer *s_slideManagerRemoveTimer;
    long m_atom = 0;

    int m_slideLength;
    std::chrono::milliseconds m_slideInDuration;
    std::chrono::milliseconds m_slideOutDuration;

    enum class AnimationKind {
        In,
        Out
    };

    struct Animation {
        AnimationKind kind;
        TimeLine timeLine;
        std::chrono::milliseconds lastPresentTime = std::chrono::milliseconds::zero();
    };
    QHash<EffectWindow *, Animation> m_animations;

    enum class Location {
        Left,
        Top,
        Right,
        Bottom
    };

    struct AnimationData {
        int offset;
        Location location;
        std::chrono::milliseconds slideInDuration;
        std::chrono::milliseconds slideOutDuration;
        int slideLength;
    };
    QHash<const EffectWindow*, AnimationData> m_animationsData;
};

inline int SlidingPopupsEffect::slideInDuration() const
{
    return m_slideInDuration.count();
}

inline int SlidingPopupsEffect::slideOutDuration() const
{
    return m_slideOutDuration.count();
}

} // namespace

#endif
