/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_LOGIN_H
#define KWIN_LOGIN_H

#include <kwineffects.h>


namespace KWin
{

class LoginEffect
    : public Effect
{
    Q_OBJECT
public:
    LoginEffect();
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void reconfigure(ReconfigureFlags);
    virtual bool isActive() const;

public Q_SLOTS:
    void slotWindowClosed(EffectWindow *w);

private:
    bool isLoginSplash(EffectWindow* w);
    double progress; // 0-1
    EffectWindow* login_window;
    bool m_fadeToBlack;
};

} // namespace

#endif
