/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "logout.h"

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( logout, LogoutEffect )

LogoutEffect::LogoutEffect()
    : progress( 0 )
    , logout_window( NULL )
    {
    }

void LogoutEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( logout_window != NULL )
        {
        progress = qBound( 0., progress + time / 1000., 1. );
        data.mask &= ~Effect::PAINT_SCREEN_REGION;
        }
    effects->prePaintScreen( data, time );
    }

void LogoutEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( w != logout_window && progress != 0 )
        {
        data.saturation *= ( 1 - progress * 0.6 );
        data.brightness *= ( 1 - progress * 0.6 ); // otherwise saturation doesn't work ???
        }
    effects->paintWindow( w, mask, region, data );
    }

void LogoutEffect::postPaintScreen()
    {
    if( logout_window != NULL && progress != 1 )
        effects->addRepaintFull();
    effects->postPaintScreen();
    }

void LogoutEffect::windowAdded( EffectWindow* w )
    {
    if( isLogoutDialog( w ))
        {
        logout_window = w;
        progress = 0;
        effects->addRepaintFull();
        }
    }

void LogoutEffect::windowClosed( EffectWindow* w )
    {
    if( w == logout_window )
        {
        logout_window = NULL;
        progress = 0;
        effects->addRepaintFull();
        }
    }

bool LogoutEffect::isLogoutDialog( EffectWindow* w )
    { // TODO there should be probably a better way (window type?)
    if( w->windowClass() == "ksmserver ksmserver" && w->windowRole() == "logoutdialog" )
        return true;
    return false;
    }

} // namespace
