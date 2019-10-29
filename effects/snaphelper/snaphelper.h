/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
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

#ifndef KWIN_SNAPHELPER_H
#define KWIN_SNAPHELPER_H

#include <kwineffects.h>

namespace KWin
{

class SnapHelperEffect : public Effect
{
    Q_OBJECT

public:
    SnapHelperEffect();
    ~SnapHelperEffect() override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;

    bool isActive() const override;

private Q_SLOTS:
    void slotWindowClosed(EffectWindow *w);
    void slotWindowStartUserMovedResized(EffectWindow *w);
    void slotWindowFinishUserMovedResized(EffectWindow *w);
    void slotWindowGeometryShapeChanged(EffectWindow *w, const QRect &old);

private:
    QRect m_geometry;
    EffectWindow *m_window = nullptr;

    struct Animation {
        bool active = false;
        TimeLine timeLine;
    };

    Animation m_animation;
};

} // namespace KWin

#endif
