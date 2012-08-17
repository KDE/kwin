/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Andreas Demmer <ademmer@opensuse.org>

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

#ifndef KWIN_DASHBOARD_H
#define KWIN_DASHBOARD_H

#include <kwineffects.h>
#include <QtCore/QTimeLine>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <Plasma/WindowEffects>

namespace KWin
{


class DashboardEffect : public KWin::Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal brightness READ configuredBrightness)
    Q_PROPERTY(qreal saturation READ configuredSaturation)
    Q_PROPERTY(bool blur READ isBlur)
public:
    DashboardEffect();
    ~DashboardEffect();
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void postPaintScreen();
    virtual void propagate();
    virtual void reconfigure(ReconfigureFlags);
    virtual void unpropagate();
    virtual bool isActive() const;

    // for properties
    qreal configuredBrightness() const {
        return brightness;
    }
    qreal configuredSaturation() const {
        return saturation;
    }
    bool isBlur() const {
        return blur;
    }
public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow* c);
    void slotWindowClosed(KWin::EffectWindow *c);
    void slotWindowActivated(KWin::EffectWindow *w);
private:
    bool blur;
    bool isDashboard(EffectWindow* w);
    bool transformWindow;
    bool retransformWindow;
    bool activateAnimation;
    bool deactivateAnimation;
    QTimeLine timeline;
    long atom;
    qreal brightness;
    qreal saturation;
    int duration;
    EffectWindow* window;
};

} // namespace

#endif
