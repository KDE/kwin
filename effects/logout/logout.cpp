/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include <kconfiggroup.h>
#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( logout, LogoutEffect )

LogoutEffect::LogoutEffect()
    : progress( 0.0 )
    , logoutWindow( NULL )
    , logoutWindowClosed( true )
    , logoutWindowPassed( false )
    {
    char net_wm_cm_name[ 100 ];
    sprintf( net_wm_cm_name, "_NET_WM_CM_S%d", DefaultScreen( display()));
    Atom net_wm_cm = XInternAtom( display(), net_wm_cm_name, False );
    Window sel = XGetSelectionOwner( display(), net_wm_cm );
    Atom hack = XInternAtom( display(), "_KWIN_LOGOUT_EFFECT", False );
    XChangeProperty( display(), sel, hack, hack, 8, PropModeReplace, (unsigned char*)&hack, 1 );
    // the atom is not removed when effect is destroyed, this is temporary anyway
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    blurTexture = NULL;
    blurTarget = NULL;
#endif
    reconfigure( ReconfigureAll );
    }

LogoutEffect::~LogoutEffect()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    delete blurTexture;
    delete blurTarget;
#endif
    }

void LogoutEffect::reconfigure( ReconfigureFlags )
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    frameDelay = 0;
    KConfigGroup conf = effects->effectConfig( "Logout" );
    useBlur = conf.readEntry( "UseBlur", true );
    delete blurTexture;
    blurTexture = NULL;
    delete blurTarget;
    blurTarget = NULL;
    blurSupported = false;
#endif
    }

void LogoutEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if ( !logoutWindow )
        {
        if (blurTexture)
            {
            delete blurTexture;
            blurTexture = NULL;
            delete blurTarget;
            blurTarget = NULL;
            blurSupported = false;
            }
        }
    else if ( !blurTexture )
        {
        blurSupported = false;
        delete blurTarget; // catch as we just tested the texture ;-P
        if( effects->compositingType() == OpenGLCompositing && GLTexture::NPOTTextureSupported() && useBlur )
            { // TODO: It seems that it is not possible to create a GLRenderTarget that has
            //       a different size than the display right now. Most likely a KWin core bug.
            // Create texture and render target
            blurTexture = new GLTexture( displayWidth(), displayHeight() );
            blurTexture->setFilter( GL_LINEAR_MIPMAP_LINEAR );
            blurTexture->setWrapMode( GL_CLAMP_TO_EDGE );

            blurTarget = new GLRenderTarget( blurTexture );
            if( blurTarget->valid() )
                blurSupported = true;

            // As creating the render target takes time it can cause the first two frames of the
            // blur animation to be jerky. For this reason we only start the animation after the
            // third frame.
            frameDelay = 2;
            }
        }
#endif

    if( frameDelay )
        --frameDelay;
    else
        {
        if( logoutWindow != NULL && !logoutWindowClosed )
            progress = qMin( 1.0, progress + time / animationTime( 2000.0 ));
        else if( progress > 0.0 )
            progress = qMax( 0.0, progress - time / animationTime( 500.0 ));
        }

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( blurSupported && progress > 0.0 )
        {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        }
#endif

    effects->prePaintScreen( data, time );
    }

void LogoutEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( progress > 0.0 )
        {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        if( blurSupported && w == logoutWindow )
            {
            windowOpacity = data.opacity;
            data.opacity = 0.0; // Cheat, we need the opacity for later but don't want to blur it
            }
        else
            {
#endif
            if( w != logoutWindow && !logoutWindowPassed )
                {
                if( effects->saturationSupported() )
                    {
                    data.saturation *= ( 1.0 - progress * 0.8 );
                    data.brightness *= ( 1.0 - progress * 0.3 );
                    }
                else // When saturation isn't supported then reduce brightness a bit more
                    data.brightness *= ( 1.0 - progress * 0.6 );
                }
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
            if( blurSupported && logoutWindowPassed )
                {
                windows.append( w );
                windowsOpacities[ w ] = data.opacity;
                data.opacity = 0.0;
                }
            }
#endif
        if( w == logoutWindow )
            {
            // logout window - all following windows are on top and should not be altered
            logoutWindowPassed = true;
            }
        }
    effects->paintWindow( w, mask, region, data );
    }

void LogoutEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( blurSupported && progress > 0.0 )
        {
        effects->pushRenderTarget( blurTarget );
        }
#endif
    effects->paintScreen( mask, region, data );

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( blurSupported && progress > 0.0 )
        {
        GLRenderTarget* target = effects->popRenderTarget();
        assert( target == blurTarget );

        // Render the blurred scene
        blurTexture->bind();
        GLfloat bias[1];
        glGetTexEnvfv( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, bias );
        glTexEnvf( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, progress * 2.75 );
        glBegin( GL_QUADS );
            glTexCoord2f( 0.0, 0.0 );
            glVertex2f( 0.0, displayHeight() );
            glTexCoord2f( 1.0, 0.0 );
            glVertex2f( displayWidth(), displayHeight() );
            glTexCoord2f( 1.0, 1.0 );
            glVertex2f( displayWidth(), 0.0 );
            glTexCoord2f( 0.0, 1.0 );
            glVertex2f( 0.0, 0.0 );
        glEnd();
        glTexEnvf( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, bias[0] );
        blurTexture->unbind();

        // Render the logout window
        if( logoutWindow )
            {
            int winMask = logoutWindow->hasAlpha() ? PAINT_WINDOW_TRANSLUCENT : PAINT_WINDOW_OPAQUE;
            WindowPaintData winData( logoutWindow );
            winData.opacity = windowOpacity;
            effects->drawWindow( logoutWindow, winMask, region, winData );
            }

        // Render all windows on top of logout window
        foreach( EffectWindow* w, windows )
            {
            int winMask = w->hasAlpha() ? PAINT_WINDOW_TRANSLUCENT : PAINT_WINDOW_OPAQUE;
            WindowPaintData winData( w );
            winData.opacity = windowsOpacities[ w ];
            effects->drawWindow( w, winMask, region, winData );
            }

        windows.clear();
        windowsOpacities.clear();
        }
#endif
    }

void LogoutEffect::postPaintScreen()
    {
    if(( progress != 0.0 && progress != 1.0 ) || frameDelay )
        {
        effects->addRepaintFull();
        }
    if( progress > 0.0 )
        {
        logoutWindowPassed = false;
        }
    effects->postPaintScreen();
    }

void LogoutEffect::windowAdded( EffectWindow* w )
    {
    if( isLogoutDialog( w ))
        {
        logoutWindow = w;
        logoutWindowClosed = false; // So we don't blur the window on close
        progress = 0.0;
        effects->addRepaintFull();
        }
    }

void LogoutEffect::windowClosed( EffectWindow* w )
    {
    if( w == logoutWindow )
        {
        logoutWindowClosed = true;
        effects->addRepaintFull();
        }
    }

void LogoutEffect::windowDeleted( EffectWindow* w )
    {
    if( w == logoutWindow )
        logoutWindow = NULL;
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
