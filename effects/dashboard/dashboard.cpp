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

#include "dashboard.h"
#include <KDE/KConfigGroup>

namespace KWin
{
KWIN_EFFECT(dashboard, DashboardEffect)

DashboardEffect::DashboardEffect()
    : transformWindow(false)
    , retransformWindow(false)
    , activateAnimation(false)
    , deactivateAnimation(false)
    , window(NULL)
{
    // propagate that the effect is loaded
    propagate();

    // read settings
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowActivated(KWin::EffectWindow*)), this, SLOT(slotWindowActivated(KWin::EffectWindow*)));
}

DashboardEffect::~DashboardEffect()
{
    unpropagate();
}

void DashboardEffect::propagate()
{
    // TODO: better namespacing for atoms
    atom = XInternAtom(display(), "_WM_EFFECT_KDE_DASHBOARD", false);
    effects->registerPropertyType(atom, true);

    // TODO: maybe not the best way to propagate the loaded effect
    unsigned char dummy = 0;
    XChangeProperty(display(), rootWindow(), atom, atom, 8, PropModeReplace, &dummy, 1);
}

void DashboardEffect::unpropagate()
{
    effects->registerPropertyType(atom, false);
    XDeleteProperty(display(), rootWindow(), atom);
}

void DashboardEffect::reconfigure(ReconfigureFlags)
{
    // read settings again
    KConfigGroup config = EffectsHandler::effectConfig("Dashboard");

    brightness = qreal(config.readEntry<int>("Brightness", 50)) / 100.0;
    saturation = qreal(config.readEntry<int>("Saturation", 50)) / 100.0;
    duration = config.readEntry<int>("Duration", 500);

    blur = config.readEntry("Blur", false);

    timeline.setDuration(animationTime(duration));
}

void DashboardEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (transformWindow && (w != window) && w->isManaged() && !isDashboard(w)) {
        // dashboard active, transform other windows
        data.brightness *= (1 - ((1.0 - brightness) * timeline.currentValue()));
        data.saturation *= (1 - ((1.0 - saturation) * timeline.currentValue()));
    }

    else if (transformWindow && (w == window) && w->isManaged()) {
        // transform dashboard
        if ((timeline.currentValue() * 2) <= 1) {
            data.opacity *= timeline.currentValue() * 2;
        } else {
            data.opacity *= 1;
        }
    }

    effects->paintWindow(w, mask, region, data);
}

void DashboardEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (transformWindow) {
        if (activateAnimation)
            timeline.setCurrentTime(timeline.currentTime() + time);
        if (deactivateAnimation)
            timeline.setCurrentTime(timeline.currentTime() - time);
    }
    effects->prePaintScreen(data, time);
}

void DashboardEffect::postPaintScreen()
{
    if (transformWindow) {
        if (retransformWindow) {
            retransformWindow = false;
            transformWindow = false;
            effects->addRepaintFull();
            window = NULL;
            effects->setActiveFullScreenEffect(0);
        }

        if (activateAnimation) {
            if (timeline.currentValue() == 1.0)
                activateAnimation = false;

            effects->addRepaintFull();
        }

        if (deactivateAnimation) {
            if (timeline.currentValue() == 0.0) {
                deactivateAnimation = false;
                transformWindow = false;
                window = NULL;
                effects->setActiveFullScreenEffect(0);
            }

            effects->addRepaintFull();
        }
    }

    effects->postPaintScreen();
}

bool DashboardEffect::isDashboard(EffectWindow *w)
{
    if (w->windowClass() == "dashboard dashboard") {
        return true;
    } else {
        return false;
    }
}

void DashboardEffect::slotWindowActivated(EffectWindow *w)
{
    if (!w)
        return;

    // apply effect on dashboard activation
    if (isDashboard(w)) {
        effects->setActiveFullScreenEffect(this);
        transformWindow = true;
        window = w;

        if (blur) {
            w->setData(WindowBlurBehindRole, w->geometry());
            w->setData(WindowForceBlurRole, QVariant(true));
        }

        effects->addRepaintFull();
    } else {
        if (transformWindow) {
            retransformWindow = true;
            effects->addRepaintFull();
        }
    }
}

void DashboardEffect::slotWindowAdded(EffectWindow* w)
{
    if (isDashboard(w)) {
        // Tell other windowAdded() effects to ignore this window
        w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

        activateAnimation = true;
        deactivateAnimation = false;
        timeline.setCurrentTime(0);

        w->addRepaintFull();
    }
}

void DashboardEffect::slotWindowClosed(EffectWindow* w)
{
    if (isDashboard(w)) {
        // Tell other windowClosed() effects to ignore this window
        w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));
        w->addRepaintFull();
    }
}

bool DashboardEffect::isActive() const
{
    return transformWindow;
}

} // namespace
