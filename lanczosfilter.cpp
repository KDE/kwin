/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 by Fredrik Höglund <fredrik@kde.org>

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

#include "lanczosfilter.h"
#include "effects.h"

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    #include <kwinglutils.h>
#endif

#include <kwineffects.h>
#include <KDE/KGlobalSettings>

#include <qmath.h>
#include <cmath>

namespace KWin
{

LanczosFilter::LanczosFilter( QObject* parent )
    : QObject( parent )
    , m_offscreenTex( 0 )
    , m_offscreenTarget( 0 )
    , m_shader( 0 )
    , m_inited( false)
    {
    }

LanczosFilter::~LanczosFilter()
    {
    delete m_offscreenTarget;
    delete m_offscreenTex;
    delete m_shader;
    }

void LanczosFilter::init()
    {
    if (m_inited)
        return;
    m_inited = true;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if ( GLShader::fragmentShaderSupported() &&
         GLShader::vertexShaderSupported() &&
         GLRenderTarget::supported() )
        {
        m_shader = new GLShader(":/resources/lanczos-vertex.glsl", ":/resources/lanczos-fragment.glsl");
        if (m_shader->isValid())
            {
            m_shader->bind();
            m_uTexUnit    = m_shader->uniformLocation("texUnit");
            m_uKernelSize = m_shader->uniformLocation("kernelSize");
            m_uKernel     = m_shader->uniformLocation("kernel");
            m_uOffsets    = m_shader->uniformLocation("offsets");
            m_shader->unbind();
            }
        else
            {
            kDebug(1212) << "Shader is not valid";
            m_shader = 0;
            }
        }
#endif
    }


void LanczosFilter::updateOffscreenSurfaces()
    {
    int w = displayWidth();
    int h = displayHeight();
    if ( !GLTexture::NPOTTextureSupported() )
        {
        w = nearestPowerOfTwo( w );
        h = nearestPowerOfTwo( h );
        }
    if ( !m_offscreenTex || m_offscreenTex->width() != w || m_offscreenTex->height() != h )
        {
        if ( m_offscreenTex )
            {
            delete m_offscreenTex;
            delete m_offscreenTarget;
            }
        m_offscreenTex = new GLTexture( w, h );
        m_offscreenTex->setFilter( GL_LINEAR );
        m_offscreenTex->setWrapMode( GL_CLAMP_TO_EDGE );
        m_offscreenTarget = new GLRenderTarget( m_offscreenTex );
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

QVector<QVector4D> LanczosFilter::createKernel( float delta )
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

QVector<QVector2D> LanczosFilter::createOffsets( int count, float width, Qt::Orientation direction )
    {
    QVector<QVector2D> offsets( count );
    for ( int i = 0; i < count; i++ ) {
        offsets[i] = ( direction == Qt::Horizontal ) ?
                QVector2D( i / width, 0 ) : QVector2D( 0, i / width );
    }
    return offsets;
    }

void LanczosFilter::performPaint( EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data )
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == KWin::OpenGLCompositing &&
        KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects )
        {
        if (!m_inited)
            init();
        if ( m_shader )
            {
            int tx = data.xTranslate + w->x();
            int ty = data.yTranslate + w->y();
            int tw = w->width()*data.xScale;
            int th = w->height()*data.yScale;

            int sw = w->width();
            int sh = w->height();

            WindowPaintData thumbData = data;
            thumbData.xScale = 1.0;
            thumbData.yScale = 1.0;
            thumbData.xTranslate = -w->x();
            thumbData.yTranslate = -w->y();

            // Bind the offscreen FBO and draw the window on it unscaled
            updateOffscreenSurfaces();
            effects->pushRenderTarget( m_offscreenTarget );

            glClear( GL_COLOR_BUFFER_BIT );
            w->sceneWindow()->performPaint( mask, QRegion(0, 0, sw, sh), thumbData );

            // Create a scratch texture and copy the rendered window into it
            GLTexture tex( sw, sh );
            tex.setFilter( GL_LINEAR );
            tex.setWrapMode( GL_CLAMP_TO_EDGE );
            tex.bind();

            glCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, m_offscreenTex->height() - sh, sw, sh );

            // Set up the shader for horizontal scaling
            float dx = sw / float(tw);
            QVector<QVector4D> kernel = createKernel( dx );
            QVector<QVector2D> offsets = createOffsets( kernel.size(), sw, Qt::Horizontal );

            m_shader->bind();
            glUniform1i( m_uTexUnit, 0 );
            glUniform1i( m_uKernelSize, kernel.size() );
            glUniform2fv( m_uOffsets, offsets.size(), (const GLfloat*)offsets.constData() );
            glUniform4fv( m_uKernel, kernel.size(), (const GLfloat*)kernel.constData() );

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
            offsets = createOffsets( kernel.size(), m_offscreenTex->height(), Qt::Vertical );

            glUniform1i( m_uKernelSize, kernel.size() );
            glUniform2fv( m_uOffsets, offsets.size(), (const GLfloat*)offsets.constData() );
            glUniform4fv( m_uKernel, kernel.size(), (const GLfloat*)kernel.constData() );

            float sx2 = tw / float(m_offscreenTex->width());
            float sy2 = 1 - (sh / float(m_offscreenTex->height()));

            // Now draw the horizontally scaled window in the FBO at the right
            // coordinates on the screen, while scaling it vertically and blending it.
            m_offscreenTex->bind();

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
            m_offscreenTex->unbind();
            m_shader->unbind();

            // Delete the offscreen surface after 5 seconds
            m_timer.start( 5000, this );
            return;
            }
        } // if ( effects->compositingType() == KWin::OpenGLCompositing )
#endif
    w->sceneWindow()->performPaint( mask, region, data );
    } // End of function

void LanczosFilter::timerEvent( QTimerEvent *event )
    {
    if (event->timerId() == m_timer.timerId())
        {
        m_timer.stop();

        delete m_offscreenTarget;
        delete m_offscreenTex;
        m_offscreenTarget = 0;
        m_offscreenTex = 0;
        }
    }

} // namespace

