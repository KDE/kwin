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
    char net_wm_cm_name[ 100 ];
    sprintf( net_wm_cm_name, "_NET_WM_CM_S%d", DefaultScreen( display()));
    Atom net_wm_cm = XInternAtom( display(), net_wm_cm_name, False );
    Window sel = XGetSelectionOwner( display(), net_wm_cm );
    Atom hack = XInternAtom( display(), "_KWIN_LOGOUT_EFFECT", False );
    XChangeProperty( display(), sel, hack, hack, 8, PropModeReplace, (unsigned char*)&hack, 1 );
    // the atom is not removed when effect is destroyed, this is temporary anyway
    }

void LogoutEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( logout_window != NULL )
        progress = qBound( 0., progress + time / 2000., 1. );
    else if( progress != 0 )
        progress = qBound( 0., progress - time / 500., 1. );
    effects->prePaintScreen( data, time );
    }

void LogoutEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( w != logout_window && progress != 0 )
        {
        // When saturation isn't supported then reduce brightness a bit more
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
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
    if( w->windowClass() == "ksmserver ksmserver"
        && ( w->windowRole() == "logoutdialog" || w->windowRole() == "logouteffect" ))
        {
        return true;
        }
    return false;
    }

} // namespace
