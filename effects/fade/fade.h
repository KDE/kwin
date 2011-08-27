/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

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

#ifndef KWIN_FADE_H
#define KWIN_FADE_H

#include <kwineffects.h>

namespace KWin
{

class FadeEffect
    : public Effect
{
    Q_OBJECT
public:
    FadeEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual bool isActive() const;

    // TODO react also on virtual desktop changes

    bool isFadeWindow(EffectWindow* w);

public Q_SLOTS:
    void slotWindowAdded(EffectWindow* c);
    void slotWindowClosed(EffectWindow *c);
    void slotWindowDeleted(EffectWindow *w);
    void slotWindowOpacityChanged(EffectWindow *w, qreal oldOpacity);
private:
    class WindowInfo;
    QHash< const EffectWindow*, WindowInfo > windows;
    double fadeInStep, fadeOutStep;
    int fadeInTime, fadeOutTime;
    bool fadeWindows;
};

class FadeEffect::WindowInfo
{
public:
    WindowInfo()
        : fadeInStep(0.0)
        , fadeOutStep(0.0)
        , opacity(1.0)
        , saturation(1.0)
        , brightness(1.0)
        , deleted(false)
    {}
    double fadeInStep, fadeOutStep;
    double opacity;
    double saturation, brightness;
    bool deleted;
};

} // namespace

#endif
