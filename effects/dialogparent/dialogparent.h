/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_DIALOGPARENT_H
#define KWIN_DIALOGPARENT_H

// Include with base class for effects.
#include <kwineffects.h>


namespace KWin
{

/**
 * An effect which changes saturation and brighness of windows which have
 *  active modal dialogs.
 * This should make the dialog seem more important and emphasize that the
 *  window is inactive until the dialog is closed.
 **/
class DialogParentEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int changeTime READ configuredChangeTime)
public:
    DialogParentEffect();
    virtual void reconfigure(ReconfigureFlags);

    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintWindow(EffectWindow* w);

    virtual bool isActive() const;

    // for properties
    int configuredChangeTime() const {
        return changeTime;
    }
public Q_SLOTS:
    void slotWindowClosed(KWin::EffectWindow *c);
    void slotWindowActivated(KWin::EffectWindow *c);
protected:
    bool hasModalWindow(EffectWindow* t);
private:
    // The progress of the fading.
    QMap<EffectWindow*, float> effectStrength;
    float changeTime;
};

} // namespace

#endif
