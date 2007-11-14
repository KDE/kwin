/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_LOGIN_H
#define KWIN_LOGIN_H

#include <kwineffects.h>


namespace KWin
{

class LoginEffect
    : public Effect
    {
    public:
        LoginEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowAdded( EffectWindow* w );
        virtual void windowClosed( EffectWindow* w );
    private:
        bool isLoginSplash( EffectWindow* w );
        double progress; // 0-1
        EffectWindow* login_window;
    };

} // namespace

#endif
