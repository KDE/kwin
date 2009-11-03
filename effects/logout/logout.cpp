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

#include <math.h>
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
    if( !logoutWindow && progress == 0.0 )
        {
        if( blurTexture )
            {
            delete blurTexture;
            blurTexture = NULL;
            delete blurTarget;
            blurTarget = NULL;
            blurSupported = false;
            }
        }
    else if( !blurTexture )
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

    if( frameDelay )
        --frameDelay;
    else
#endif
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
            if( w != logoutWindow && !logoutWindowPassed )
                {
                if( blurSupported )
                    data.saturation *= ( 1.0 - progress * 0.2 );
                else
                    {
                    data.saturation *= ( 1.0 - progress * 0.8 );
                    data.brightness *= ( 1.0 - progress * 0.3 );
                    }
                }
            if( blurSupported && logoutWindowPassed )
                {
                windows.append( w );
                windowsOpacities[ w ] = data.opacity;
                data.opacity = 0.0;
                }
            }
#else
        if( w != logoutWindow && !logoutWindowPassed )
            {
            data.saturation *= ( 1.0 - progress * 0.8 );
            data.brightness *= ( 1.0 - progress * 0.3 );
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
        Q_UNUSED( target );

        //--------------------------
        // Render the screen effect

        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );

        // Unmodified base image
        blurTexture->bind();
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

        // Blurred image
        GLfloat bias[1];
        glGetTexEnvfv( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, bias );
        glTexEnvf( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 1.75 );
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glColor4f( 1.0f, 1.0f, 1.0f, progress * 0.4 );
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

        // Vignetting (Radial gradient with transparent middle and black edges)
        for( int screen = 0; screen < effects->numScreens(); screen++ )
            { // TODO: Cache
            QRect screenGeom = effects->clientArea( ScreenArea, screen, 0 );
            glScissor( screenGeom.x(), displayHeight() - screenGeom.y() - screenGeom.height(),
                screenGeom.width(), screenGeom.height() ); // GL coords are flipped
            glEnable( GL_SCISSOR_TEST ); // Geom must be set before enable
            float ro = float(( screenGeom.width() > screenGeom.height() )
                             ? screenGeom.width() : screenGeom.height() ) * 0.8f; // Outer radius
            glBegin( GL_TRIANGLES );
                const float a = M_PI / 8.0f; // Angle of increment
                for( float i = 0.0f; i < M_PI * 1.99f; i += a )
                    {
                    float x, y;

                    glColor4f( 0.0f, 0.0f, 0.0f, 0.0f );

                    x = screenGeom.x() + screenGeom.width() / 2;
                    y = screenGeom.y() + screenGeom.height() / 2;
                    glVertex3f( x, y, 0 );

                    glColor4f( 0.0f, 0.0f, 0.0f, progress * 0.9f );

                    x = ro * cos( i ) + screenGeom.x() + screenGeom.width() / 2;
                    y = ro * sin( i ) + screenGeom.y() + screenGeom.height() / 2;
                    glVertex3f( x, y, 0 );

                    x = ro * cos( i + a ) + screenGeom.x() + screenGeom.width() / 2;
                    y = ro * sin( i + a ) + screenGeom.y() + screenGeom.height() / 2;
                    glVertex3f( x, y, 0 );
                    }
            glEnd();
            glDisable( GL_SCISSOR_TEST );
            }

        glPopAttrib();

        //--------------------------

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
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if(( progress != 0.0 && progress != 1.0 ) || frameDelay )
        effects->addRepaintFull();
#else
    if( progress != 0.0 && progress != 1.0 )
        effects->addRepaintFull();
#endif

    if( progress > 0.0 )
        logoutWindowPassed = false;
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
