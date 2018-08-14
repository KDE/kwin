/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Marco Martin notmart@gmail.com

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

#ifndef KWIN_SLIDINGPOPUPS_H
#define KWIN_SLIDINGPOPUPS_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

class SlidingPopupsEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int slideInDuration READ slideInDuration)
    Q_PROPERTY(int slideOutDuration READ slideOutDuration)
public:
    SlidingPopupsEffect();
    ~SlidingPopupsEffect() override;

    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintWindow(EffectWindow* w) override;
    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 40;
    }

    static bool supported();

    // TODO react also on virtual desktop changes

    int slideInDuration() const;
    int slideOutDuration() const;

private Q_SLOTS:
    void slotWindowAdded(EffectWindow *w);
    void slotWindowDeleted(EffectWindow *w);
    void slotPropertyNotify(EffectWindow *w, long a);
    void slotWaylandSlideOnShowChanged(EffectWindow* w);

    void slideIn(EffectWindow *w);
    void slideOut(EffectWindow *w);

private:
    void setupAnimData(EffectWindow *w);

    long mAtom;

    int mSlideLength;
    std::chrono::milliseconds m_slideInDuration;
    std::chrono::milliseconds m_slideOutDuration;

    enum class AnimationKind {
        In,
        Out
    };

    struct Animation {
        AnimationKind kind;
        TimeLine timeLine;
    };
    QHash<const EffectWindow*, Animation> m_animations;

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
