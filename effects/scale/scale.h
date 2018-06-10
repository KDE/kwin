/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

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

#ifndef KWIN_SCALE_H
#define KWIN_SCALE_H

// kwineffects
#include <kwineffects.h>

namespace KWin
{

class ScaleEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int duration READ duration)
    Q_PROPERTY(qreal inScale READ inScale)
    Q_PROPERTY(qreal inOpacity READ inOpacity)
    Q_PROPERTY(qreal outScale READ outScale)
    Q_PROPERTY(qreal outOpacity READ outOpacity)

public:
    ScaleEffect();
    ~ScaleEffect() override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void postPaintScreen() override;

    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    static bool supported();

    int duration() const;
    qreal inScale() const;
    qreal inOpacity() const;
    qreal outScale() const;
    qreal outOpacity() const;

private Q_SLOTS:
    void windowAdded(EffectWindow *w);
    void windowClosed(EffectWindow *w);
    void windowDeleted(EffectWindow *w);
    void windowDataChanged(EffectWindow *w, int role);

private:
    bool isScaleWindow(EffectWindow *w) const;

    std::chrono::milliseconds m_duration;
    QHash<EffectWindow*, TimeLine> m_animations;

    struct ScaleParams {
        struct {
            qreal from;
            qreal to;
        } scale, opacity;
    };

    ScaleParams m_inParams;
    ScaleParams m_outParams;
};

inline int ScaleEffect::requestedEffectChainPosition() const
{
    return 50;
}

inline int ScaleEffect::duration() const
{
    return m_duration.count();
}

inline qreal ScaleEffect::inScale() const
{
    return m_inParams.scale.from;
}

inline qreal ScaleEffect::inOpacity() const
{
    return m_inParams.opacity.from;
}

inline qreal ScaleEffect::outScale() const
{
    return m_outParams.scale.to;
}

inline qreal ScaleEffect::outOpacity() const
{
    return m_outParams.opacity.to;
}

} // namespace KWin

#endif
