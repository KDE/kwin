/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2007 Martin Gräßlin <ubuntu@martin-graesslin.com>
 Copyright (C) 2008 Torgny Johansson <torgny.johansson@gmail.com>

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

#include "snow.h"

#include <kwinconfig.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <kconfiggroup.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include <kdebug.h>

#include <ctime>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif

namespace KWin
{

KWIN_EFFECT( snow, SnowEffect )

SnowEffect::SnowEffect()
    : texture( NULL )
    , active( false)
    , snowBehindWindows( false )
    , mShader( 0 )
    , mInited( false )
    , mUseShader( true )
    , hasSnown( false )
    {
    srandom( std::time( NULL ) );
    nextFlakeMillis = 0;
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = static_cast< KAction* >( actionCollection->addAction( "Snow" ));
    a->setText( i18n("Snow" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::META + Qt::Key_F12 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle()));
    reconfigure( ReconfigureAll );
    }

SnowEffect::~SnowEffect()
    {
    delete texture;
    if( active )
        delete mShader;
    }

void SnowEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig("Snow");
    mNumberFlakes = conf.readEntry("Number", 200);
    mMinFlakeSize = conf.readEntry("MinFlakes", 10);
    mMaxFlakeSize = conf.readEntry("MaxFlakes", 50);
    mMaxVSpeed = conf.readEntry("MaxVSpeed", 2);
    mMaxHSpeed = conf.readEntry("MaxHSpeed", 1);
    snowBehindWindows = conf.readEntry("BehindWindows", false);
    }

void SnowEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if ( active && effects->activeFullScreenEffect() == NULL )
        {
            // if number of active snowflakes is smaller than maximum number
            // create a random new snowflake
            nextFlakeMillis -= time;
            if ( ( nextFlakeMillis <= 0 ) && flakes.count() < mNumberFlakes)
                {
                int size = 0;
                while ( size < mMinFlakeSize )
                    size = random() % mMaxFlakeSize;
                SnowFlake flake = SnowFlake( random() % (displayWidth() - size), -1 * size, size, size, mMaxVSpeed, mMaxHSpeed );
                flakes.append( flake );

                // calculation of next time of snowflake
                // depends on the time the flow needs to get to the bottom (screen size)
                // and the fps
                long next = ((500/(time+5))*(Effect::displayHeight()/flake.getVSpeed()))/mNumberFlakes;
                nextFlakeMillis = next;
                }
        data.mask |= PAINT_SCREEN_TRANSFORMED;
        hasSnown = false;
        }
    effects->prePaintScreen( data, time );
    }

void SnowEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( active && snowBehindWindows )        
        repaintRegion = QRegion( 0, 0, displayWidth(), displayHeight() );
    effects->paintScreen( mask, region, data ); // paint normal screen
    if( active && !snowBehindWindows )
        snowing( region );
    }

void SnowEffect::snowing( QRegion& region )
    {
    if( effects->activeFullScreenEffect() != NULL )
        return;
    if(! texture ) loadTexture();
    if( texture )
        {
        glActiveTexture(GL_TEXTURE0);
        texture->bind();
        if( mUseShader && !mInited )
            mUseShader = loadShader();
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        if( mUseShader )
            {
            mShader->bind();
            }
        else
            glNewList( list, GL_COMPILE_AND_EXECUTE );
        for (int i=0; i<flakes.count(); i++)
            {
            SnowFlake& flake = flakes[i];
            if( !hasSnown )
                {
                // only update during first paint in one frame
                if( flake.addFrame() == 0 )
                    {
                    flakes.removeAt( i-- );
                    continue;
                    }
                }

            if( mUseShader )
                {
                // use shader
                mShader->setUniform( "angle", flake.getRotationSpeed() );
                mShader->setUniform( "x", flake.getX() );
                mShader->setUniform( "hSpeed", flake.getHSpeed() );
                mShader->setUniform( "vSpeed", flake.getVSpeed() );
                mShader->setUniform( "frames", flake.getFrames() );
                mShader->setUniform( "width", flake.getWidth() );
                mShader->setUniform( "height", flake.getHeight() );
                glCallList( list );
                }
            else
                {
                if( !hasSnown )
                    {
                    // only update during first paint in one frame
                    flake.updateSpeedAndRotation();
                    }
                // no shader
                // save the matrix
                glPushMatrix();
                
                // translate to the center of the flake
                glTranslatef(flake.getWidth()/2 + flake.getX(), flake.getHeight()/2 + flake.getY(), 0);
                
                // rotate around the Z-axis
                glRotatef(flake.getRotationAngle(), 0.0, 0.0, 1.0);
                
                // translate back to the starting point
                glTranslatef(-flake.getWidth()/2 - flake.getX(), -flake.getHeight()/2 - flake.getY(), 0);
                
                // paint the snowflake
                texture->render( region, flake.getRect());
                
                // restore the matrix
                glPopMatrix();
                }
            }
        if( mUseShader )
            {
            mShader->unbind();
            }
        else
            glEndList();
        glDisable( GL_BLEND );
        texture->unbind();
        glPopAttrib();
        hasSnown = true;
        }
    }

void SnowEffect::postPaintScreen()
    {
    if( active )
        {
        if( snowBehindWindows )
            effects->addRepaint( repaintRegion );
        else
            effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }

void SnowEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( active && snowBehindWindows && !w->isDesktop() && !w->isDock() )
        {
        repaintRegion -= QRegion( w->geometry() );
        }
    effects->paintWindow( w, mask, region, data );
    if( active && w->isDesktop() && snowBehindWindows )
        snowing( region );
    }

void SnowEffect::toggle()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    active = !active;
    if( active )
        {
        list = glGenLists(1);
        }
    else
        {
        glDeleteLists( list, 1 );
        flakes.clear();
        if( mUseShader )
            {
            delete mShader;
            mInited = false;
            mUseShader = true;
            }
        }
    effects->addRepaintFull();
#endif
    }

bool SnowEffect::loadShader()
    {
    mInited = true;
    if( !(GLShader::fragmentShaderSupported() &&
        (effects->compositingType() == OpenGLCompositing)) )
        {
        kDebug(1212) << "Shaders not supported - waisting CPU cycles" << endl;
        return false;
        }
    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/snow.frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/snow.vert");
    if(fragmentshader.isEmpty() || vertexshader.isEmpty())
        {
        kDebug(1212) << "Couldn't locate shader files" << endl;
        return false;
        }

    mShader = new GLShader(vertexshader, fragmentshader);
    if(!mShader->isValid())
        {
        kDebug(1212) << "The shader failed to load!" << endl;
        return false;
        }
    else
        {
        mShader->bind();
        mShader->setUniform( "snowTexture", 0 );
        mShader->unbind();
        }
    kDebug(1212) << "using shader";
    
    glNewList( list, GL_COMPILE );
    glBegin( GL_QUADS );
    glTexCoord2f( 0.0f, 0.0f );
    glVertex2i( 0, 0 );
    glTexCoord2f( 1.0f, 0.0f );
    glVertex2i( 0, 0 );
    glTexCoord2f( 1.0f, 1.0f );
    glVertex2i( 0, 0 );
    glTexCoord2f( 0.0f, 1.0f );
    glVertex2i( 0, 0 );
    glEnd();
    glEndList();
    return true;
    }

void SnowEffect::loadTexture()
    {
    QString file = KGlobal::dirs()->findResource( "appdata", "snowflake.png" );
    if( file.isEmpty())
        return;
    texture = new GLTexture( file );
    }


// the snowflake class
SnowFlake::SnowFlake(int x, int y, int width, int height, int maxVSpeed, int maxHSpeed)
    {
    int minVSpeed = maxVSpeed - 8; // 8 gives a nice difference in speed
    if(minVSpeed < 1) minVSpeed = 1;
    vSpeed = random()%maxVSpeed + minVSpeed;
    
    hSpeed = random()%(maxHSpeed+1);
    if(random()%2 < 1) hSpeed = -hSpeed; // to create negative hSpeeds at random
    
    rotationAngle = 0;
    rotationSpeed = random()%4 - 2;
    if(rotationSpeed == 0) rotationSpeed = 0.5;
    rect = QRect(x, y, width, height);
    frameCounter = 0;
    maxFrames = displayHeight() / vSpeed;
    if( hSpeed > 0 )
        maxFrames = qMin( maxFrames, (displayWidth() - x)/hSpeed );
    else if( hSpeed < 0 )
        maxFrames = qMin( maxFrames, (x + width)/(-hSpeed) );
    }

SnowFlake::~SnowFlake()
    {
    }

int SnowFlake::getHSpeed()
    {
    return hSpeed;
    }

QRect SnowFlake::getRect()
    {
    return rect;
    }

void SnowFlake::updateSpeedAndRotation()
    {
    rotationAngle = rotationAngle+rotationSpeed;
    rect.translate(hSpeed, vSpeed);
    }

float SnowFlake::getRotationAngle()
    {
    return rotationAngle;
    }

float SnowFlake::getRotationSpeed()
    {
    return rotationSpeed;
    }

int SnowFlake::getVSpeed()
    {
    return vSpeed;
    }

int SnowFlake::getHeight()
    {
    return rect.height();
    }

int SnowFlake::getWidth()
    {
    return rect.width();
    }

int SnowFlake::getX()
    {
    return rect.x();
    }

int SnowFlake::getY()
    {
    return rect.y();
    }

void SnowFlake::setHeight(int height)
    {
    rect.setHeight(height);
    }

void SnowFlake::setWidth(int width)
    {
    rect.setWidth(width);
    }

void SnowFlake::setX(int x)
    {
    rect.setX(x);
    }

void SnowFlake::setY(int y)
    {
    rect.setY(y);
    }

int SnowFlake::addFrame()
    {
    return ( maxFrames - (++frameCounter) );
    }

int SnowFlake::getFrames()
    {
    return frameCounter;
    }

} // namespace
