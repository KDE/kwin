/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "logout.h"

#include "kwinglutils.h"

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
        progress = qBound( 0., progress + time / 4000., 1. );
    else if( progress != 0 )
        progress = qBound( 0., progress - time / 500., 1. );
    effects->prePaintScreen( data, time );
    }

void LogoutEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( w != logout_window && progress != 0 )
        {
        // When saturation isn't supported then reduce brightness a bit more
#ifdef HAVE_OPENGL
        if( effects->compositingType() == OpenGLCompositing && GLTexture::saturationSupported() )
            {
            data.saturation *= ( 1 - progress * 0.8 );
            data.brightness *= ( 1 - progress * 0.3 );
            }
        else
#endif
            {
            data.brightness *= ( 1 - progress * 0.6 );
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

void LogoutEffect::postPaintScreen()
    {
    if( progress != 0 && progress != 1 )
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
