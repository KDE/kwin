/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
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

#include "taskbarthumbnail.h"

#include <kdebug.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    #include <kwinglutils.h>
#endif

#include <qmath.h>
#include <cmath>

// This effect shows a preview inside a window that has a special property set
// on it that says which window and where to render. It is used by the taskbar
// to show window previews in tooltips.

namespace KWin
{

KWIN_EFFECT( taskbarthumbnail, TaskbarThumbnailEffect )

TaskbarThumbnailEffect::TaskbarThumbnailEffect()
    {
    atom = XInternAtom( display(), "_KDE_WINDOW_PREVIEW", False );
    effects->registerPropertyType( atom, true );
    // TODO hackish way to announce support, make better after 4.0
    unsigned char dummy = 0;
    XChangeProperty( display(), rootWindow(), atom, atom, 8, PropModeReplace, &dummy, 1 );

    // With the NVIDIA driver the alpha values are set to 0 when drawing a window texture
    // without an alpha channel on an FBO that has an alpha channel.
    // This creates problems when the FBO is drawn on the backbuffer with blending enabled,
    // so for now we only do high quality scaling with windows with alpha channels with
    // the NVIDIA driver.
    const QByteArray vendor = (const char *) glGetString( GL_VENDOR );
    alphaWindowsOnly = vendor.startsWith("NVIDIA");

    if ( GLShader::fragmentShaderSupported() &&
         GLShader::vertexShaderSupported() &&
         GLRenderTarget::supported() )
        {
        shader = new GLShader(":/taskbarthumbnail/vertex.glsl", ":/taskbarthumbnail/fragment.glsl");
        shader->bind();
        uTexUnit    = shader->uniformLocation("texUnit");
        uKernelSize = shader->uniformLocation("kernelSize");
        uKernel     = shader->uniformLocation("kernel");
        uOffsets    = shader->uniformLocation("offsets");
        shader->unbind();
        }
    else
        {
        shader = 0;
        }
    offscreenTex = 0;
    offscreenTarget = 0;
    }

TaskbarThumbnailEffect::~TaskbarThumbnailEffect()
    {
    XDeleteProperty( display(), rootWindow(), atom );
    effects->registerPropertyType( atom, false );
    delete offscreenTarget;
    delete offscreenTex;
    delete shader;
    }

void TaskbarThumbnailEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
//    if( thumbnails.count() > 0 )
//        // TODO this should not be needed (it causes whole screen repaint)
//        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(data, time);
    }

void TaskbarThumbnailEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    // TODO what if of the windows is translucent and one not? change data.mask?
    effects->prePaintWindow( w, data, time );
    }

void TaskbarThumbnailEffect::updateOffscreenSurfaces()
    {
    int w = displayWidth();
    int h = displayHeight();
    if ( !GLTexture::NPOTTextureSupported() )
        {
        w = nearestPowerOfTwo( w );
        h = nearestPowerOfTwo( h );
        }
    if ( !offscreenTex || offscreenTex->width() != w || offscreenTex->height() != h )
        {
        if ( offscreenTex )
            {
            delete offscreenTex;
            delete offscreenTarget;
            }
        offscreenTex = new GLTexture( w, h );
        offscreenTex->setFilter( GL_LINEAR );
        offscreenTex->setWrapMode( GL_CLAMP_TO_EDGE );
        offscreenTarget = new GLRenderTarget( offscreenTex );
        }
    }

static float sinc( float x )
    {
    return std::sin( x * M_PI ) / ( x * M_PI );
    }

static float lanczos( float x, float a )
    {
    if ( qFuzzyCompare( x + 1.0, 1.0 ) )
        return 1.0;

    if ( qAbs( x ) >= a )
        return 0.0;

    return sinc( x ) * sinc( x / a );
    }

QVector<QVector4D> TaskbarThumbnailEffect::createKernel( float delta )
    {
    const float a = 2.0;

    // The two outermost samples always fall at points where the lanczos
    // function returns 0, so we'll skip them.
    const int sampleCount = qBound( 3, qCeil(delta * a) * 2 + 1 - 2, 49 );
    const int center = sampleCount / 2;
    const int kernelSize = center + 1;
    const float factor = 1.0 / delta;

    QVector<float> values( kernelSize );
    QVector<QVector4D> kernel( kernelSize );
    float sum = 0;

    for ( int i = 0; i < kernelSize; i++ ) {
        const float val = lanczos( i * factor, a );
        sum += i > 0 ? val * 2 : val;
        values[i] = val;
    }

    // Normalize the kernel 
    for ( int i = 0; i < kernelSize; i++ ) {
        const float val = values[i] / sum;
        kernel[i] = QVector4D( val, val, val, val );
    }

    return kernel;
    }

QVector<QVector2D> TaskbarThumbnailEffect::createOffsets( int count, float width, Qt::Orientation direction )
    {
    QVector<QVector2D> offsets( count );
    for ( int i = 0; i < count; i++ ) {
        offsets[i] = ( direction == Qt::Horizontal ) ?
                QVector2D( i / width, 0 ) : QVector2D( 0, i / width );
    }
    return offsets;
    }

void TaskbarThumbnailEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->paintWindow( w, mask, region, data ); // paint window first
    if( thumbnails.contains( w ))
        { // paint thumbnails on it
        int mask = PAINT_WINDOW_TRANSFORMED;
        if( data.opacity == 1.0 )
            mask |= PAINT_WINDOW_OPAQUE;
        else
            mask |= PAINT_WINDOW_TRANSLUCENT;
        foreach( const Data &thumb, thumbnails.values( w ))
            {
            EffectWindow* thumbw = effects->findWindow( thumb.window );
            if( thumbw == NULL )
                continue;
            WindowPaintData thumbData( thumbw );
            thumbData.opacity = data.opacity;
            QRect r;

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
            if( effects->compositingType() == KWin::OpenGLCompositing )
                {
                if ( shader && (!alphaWindowsOnly || thumbw->hasAlpha()) )
                    {
                    int tx = thumb.rect.x() + w->x();
                    int ty = thumb.rect.y() + w->y();
                    int tw = thumb.rect.width();
                    int th = thumb.rect.height();

                    int sw = thumbw->width();
                    int sh = thumbw->height();

                    setPositionTransformations( thumbData, r,
                            thumbw, QRect(0, 0, sw, sh), Qt::KeepAspectRatio );

                    // Bind the offscreen FBO and draw the window on it unscaled
                    updateOffscreenSurfaces();
                    effects->pushRenderTarget( offscreenTarget );

                    glClear( GL_COLOR_BUFFER_BIT );
                    effects->drawWindow( thumbw, mask, r, thumbData );

                    // Create a scratch texture and copy the rendered window into it
                    GLTexture tex( sw, sh );
                    tex.setFilter( GL_LINEAR );
                    tex.setWrapMode( GL_CLAMP_TO_EDGE );
                    tex.bind();

                    glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, offscreenTex->height() - sh, sw, sh );

                    // Set up the shader for horizontal scaling
                    float dx = sw / float(tw);
                    QVector<QVector4D> kernel = createKernel( dx );
                    QVector<QVector2D> offsets = createOffsets( kernel.size(), sw, Qt::Horizontal );

                    shader->bind();
                    glUniform1i( uTexUnit, 0 );
                    glUniform1i( uKernelSize, kernel.size() );
                    glUniform2fv( uOffsets, offsets.size(), (const GLfloat*)offsets.constData() );
                    glUniform4fv( uKernel, kernel.size(), (const GLfloat*)kernel.constData() );

                    // Draw the window back into the FBO, this time scaled horizontally
                    glClear( GL_COLOR_BUFFER_BIT );

                    glBegin( GL_QUADS );
                    glTexCoord2f( 0, 0 ); glVertex2i( 0,   0 ); // Top left
                    glTexCoord2f( 1, 0 ); glVertex2i( tw,  0 ); // Top right
                    glTexCoord2f( 1, 1 ); glVertex2i( tw, sh ); // Bottom right
                    glTexCoord2f( 0, 1 ); glVertex2i( 0,  sh ); // Bottom left
                    glEnd();

                    // At this point we don't need the scratch texture anymore
                    tex.unbind();
                    tex.discard();

                    // Unbind the FBO
                    effects->popRenderTarget();

                    // Set up the shader for vertical scaling
                    float dy = sh / float(th);
                    kernel = createKernel( dy );
                    offsets = createOffsets( kernel.size(), offscreenTex->height(), Qt::Vertical );

                    glUniform1i( uKernelSize, kernel.size() );
                    glUniform2fv( uOffsets, offsets.size(), (const GLfloat*)offsets.constData() );
                    glUniform4fv( uKernel, kernel.size(), (const GLfloat*)kernel.constData() );

                    float sx2 = tw / float(offscreenTex->width());
                    float sy2 = 1 - (sh / float(offscreenTex->height()));

                    // Now draw the horizontally scaled window in the FBO at the right
                    // coordinates on the screen, while scaling it vertically and blending it.
                    offscreenTex->bind();

                    glPushAttrib( GL_COLOR_BUFFER_BIT );
                    glEnable( GL_BLEND );
                    glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

                    glBegin( GL_QUADS );
                    glTexCoord2f( 0,    sy2 ); glVertex2i( tx,       ty );      // Top left
                    glTexCoord2f( sx2,  sy2 ); glVertex2i( tx + tw,  ty );      // Top right
                    glTexCoord2f( sx2,  1 );   glVertex2i( tx + tw,  ty + th ); // Bottom right
                    glTexCoord2f( 0,    1 );   glVertex2i( tx,       ty + th ); // Bottom left
                    glEnd();

                    glPopAttrib();
                    offscreenTex->unbind();
                    shader->unbind();

                    // Delete the offscreen surface after 5 seconds
                    timer.start( 5000, this );
                    continue;
                    }
                if ( data.shader )
                    {
                    // there is a shader - update texture width and height
                    int texw = thumbw->width();
                    int texh = thumbw->height();
                    if( !GLTexture::NPOTTextureSupported() )
                        {
                        kWarning( 1212 ) << "NPOT textures not supported, wasting some memory" ;
                        texw = nearestPowerOfTwo( texw );
                        texh = nearestPowerOfTwo( texh );
                        }
                    thumbData.shader = data.shader;
                    thumbData.shader->setTextureWidth( (float)texw );
                    thumbData.shader->setTextureHeight( (float)texh );
                    }
                } // if ( effects->compositingType() == KWin::OpenGLCompositing ) 
#endif
            setPositionTransformations( thumbData, r,
                    thumbw, thumb.rect.translated( w->pos()), Qt::KeepAspectRatio );
            effects->drawWindow( thumbw, mask, r, thumbData );
            }
        }
    } // End of function

void TaskbarThumbnailEffect::windowDamaged( EffectWindow* w, const QRect& damage )
    {
    Q_UNUSED( damage );
    // Update the thumbnail if the window was damaged
    foreach( EffectWindow* window, thumbnails.uniqueKeys() )
        foreach( const Data &thumb, thumbnails.values( window ))
            if( w == effects->findWindow( thumb.window ))
                effects->addRepaint( thumb.rect.translated( window->pos() ));
    }

void TaskbarThumbnailEffect::windowAdded( EffectWindow* w )
    {
    propertyNotify( w, atom ); // read initial value
    }

void TaskbarThumbnailEffect::windowDeleted( EffectWindow* w )
    {
    thumbnails.remove( w );
    }

void TaskbarThumbnailEffect::propertyNotify( EffectWindow* w, long a )
    {
    if( !w || a != atom )
        return;
    thumbnails.remove( w );
    QByteArray data = w->readProperty( atom, atom, 32 );
    if( data.length() < 1 )
        return;
    long* d = reinterpret_cast< long* >( data.data());
    int len = data.length() / sizeof( d[ 0 ] );
    int pos = 0;
    int cnt = d[ 0 ];
    ++pos;
    for( int i = 0;
         i < cnt;
         ++i )
        {
        int size = d[ pos ];
        if( len - pos < size )
            return; // format error
        ++pos;
        Data data;
        data.window = d[ pos ];
        data.rect = QRect( d[ pos + 1 ], d[ pos + 2 ], d[ pos + 3 ], d[ pos + 4 ] );
        thumbnails.insert( w, data );
        pos += size;
        }
    }


void TaskbarThumbnailEffect::timerEvent( QTimerEvent *event )
    {
    if (event->timerId() == timer.timerId())
        {
        timer.stop();

        delete offscreenTarget;
        delete offscreenTex;
        offscreenTarget = 0;
        offscreenTex = 0;
        }
    }

} // namespace
