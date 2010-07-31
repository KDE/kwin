/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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
#include "screenshot.h"
#include <kwinglutils.h>
#include <QDir>
#include <KDE/KAction>
#include <KDE/KActionCollection>

namespace KWin
{

KWIN_EFFECT( screenshot, ScreenShotEffect )
KWIN_EFFECT_SUPPORTED( screenshot, ScreenShotEffect::supported() )

bool ScreenShotEffect::supported()
    {
    return effects->compositingType() == KWin::OpenGLCompositing && GLRenderTarget::supported();
    }

ScreenShotEffect::ScreenShotEffect()
    : m_scheduledScreenshot( 0 )
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* cubeAction = static_cast< KAction* >( actionCollection->addAction( "Screenshot Effect" ));
    cubeAction->setText( i18n("Save screenshot of active window" ));
    cubeAction->setGlobalShortcut( KShortcut(), KAction::ActiveShortcut);
    connect( cubeAction, SIGNAL(triggered(bool)), SLOT(screenshot()) );
    }

ScreenShotEffect::~ScreenShotEffect()
    {
    }
void ScreenShotEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    if( m_scheduledScreenshot )
        {
        int w = displayWidth();
        int h = displayHeight();
        if( !GLTexture::NPOTTextureSupported() )
            {
            w = nearestPowerOfTwo( w );
            h = nearestPowerOfTwo( h );
            }
        GLTexture* offscreenTexture = new GLTexture( w, h );
        offscreenTexture->setFilter( GL_LINEAR );
        offscreenTexture->setWrapMode( GL_CLAMP_TO_EDGE );
        GLRenderTarget* target = new GLRenderTarget( offscreenTexture );
        if( target->valid() )
            {
            WindowPaintData d( m_scheduledScreenshot );
            double left = 0;
            double top = 0;
            double right = m_scheduledScreenshot->width();
            double bottom = m_scheduledScreenshot->height();
            foreach( const WindowQuad& quad, d.quads )
                {
                // we need this loop to include the decoration padding
                left   = qMin(left, quad.left());
                top    = qMin(top, quad.top());
                right  = qMax(right, quad.right());
                bottom = qMax(bottom, quad.bottom());
                }
            int width = right - left;
            int height = bottom - top;
            d.xTranslate = -m_scheduledScreenshot->x() - left;
            d.yTranslate = -m_scheduledScreenshot->y() - top;
            // render window into offscreen texture
            int mask = PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_TRANSLUCENT;
            effects->pushRenderTarget( target );
            glClear( GL_COLOR_BUFFER_BIT );
            effects->drawWindow( m_scheduledScreenshot, mask, QRegion( 0, 0, width, height ), d );
            // Create a scratch texture and copy the rendered window into it
            GLTexture* tex = new GLTexture( width, height );
            tex->setFilter( GL_LINEAR );
            tex->setWrapMode( GL_CLAMP_TO_EDGE );
            tex->bind();

            glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, offscreenTexture->height() - height, width, height );
            effects->popRenderTarget();
            // copy content from GL texture into image
            QImage img( QSize( width, height ), QImage::Format_ARGB32 );
            tex->bind();
            glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits() );
            tex->unbind();
            delete tex;
            ScreenShotEffect::convertFromGLImage( img, width, height );

            // save screenshot in home directory
            QString filePart( QDir::homePath() + '/' + m_scheduledScreenshot->caption() );
            QString file( filePart + ".png" );
            int counter = 1;
            while( QFile::exists( file ) )
                {
                file = QString( filePart + '_' + QString::number( counter ) + ".png" );
                counter++;
                }
            img.save( file );
            }
        delete offscreenTexture;
        delete target;
        m_scheduledScreenshot = NULL;
        }
    }

void ScreenShotEffect::screenshot()
    {
    EffectWindow* w = effects->activeWindow();
    m_scheduledScreenshot = w;
    w->addRepaintFull();
    }

void ScreenShotEffect::convertFromGLImage(QImage &img, int w, int h)
{
    // from QtOpenGL/qgl.cpp
    // Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
    // see http://qt.gitorious.org/qt/qt/blobs/master/src/opengl/qgl.cpp
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = (uint*)img.bits();
        uint *end = p + w*h;
        while (p < end) {
            uint a = *p << 24;
            *p = (*p >> 8) | a;
            p++;
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
            uint *q = (uint*)img.scanLine(y);
            for (int x=0; x < w; ++x) {
                const uint pixel = *q;
                *q = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff)
                        | (pixel & 0xff00ff00);

                q++;
            }
        }

    }
    img = img.mirrored();
}

} // namespace
