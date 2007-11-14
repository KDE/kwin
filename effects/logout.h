/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_LOGOUT_H
#define KWIN_LOGOUT_H

#include <kwineffects.h>


namespace KWin
{

class LogoutEffect
    : public Effect
    {
    public:
        LogoutEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        virtual void windowAdded( EffectWindow* w );
        virtual void windowClosed( EffectWindow* w );
    private:
        bool isLogoutDialog( EffectWindow* w );
        double progress; // 0-1
        EffectWindow* logout_window;
    };

} // namespace

#endif
