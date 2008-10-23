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
#include <QApplication>
#include <QDesktopWidget>

#include <ctime>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif

namespace KWin
{

KWIN_EFFECT( snow, SnowEffect )

SnowEffect::SnowEffect()
    : texture( NULL )
    , flakes( NULL )
    , active( false)
    {
    srandom( std::time( NULL ) );
    lastFlakeTime = QTime::currentTime();
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
    delete flakes;
    }

void SnowEffect::reconfigure( ReconfigureFlags )
    {
    KConfigGroup conf = effects->effectConfig("Snow");
    mNumberFlakes = conf.readEntry("Number", 200);
    mMinFlakeSize = conf.readEntry("MinFlakes", 10);
    mMaxFlakeSize = conf.readEntry("MaxFlakes", 50);
    mMaxVSpeed = conf.readEntry("MaxVSpeed", 2);
    mMaxHSpeed = conf.readEntry("MaxHSpeed", 1);
    }

void SnowEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if ( active )
        {
        if (! flakes )
            {
            flakes = new QList<SnowFlake>();
            lastFlakeTime.start();
            }
        int count = flakes->count();
        for (int i=0; i<count; i++)
            {
            // move flake to bottom. Therefore pop the flake, change x and y and push
            // flake back to QVector
            SnowFlake flake = flakes->first();
            flakes->pop_front();
            // if flake has reached bottom, left or right don't push it back
            if ( flake.getY() >= QApplication::desktop()->geometry().bottom() )
                {
                continue;
                }
            else if (flake.getX()+flake.getWidth() <= QApplication::desktop()->geometry().left() )
                {
                continue;
                }
            else if (flake.getX() >= QApplication::desktop()->geometry().right() )
                {
                continue;
                }
            flake.updateSpeedAndRotation();
            flakes->append(flake);
            }
            // if number of active snowflakes is smaller than maximum number
            // create a random new snowflake
            if ( ( lastFlakeTime.elapsed() >= nextFlakeMillis ) && flakes->count() < mNumberFlakes)
                {
                int size = 0;
                while ( size < mMinFlakeSize )
                    size = random() % mMaxFlakeSize;
                SnowFlake flake = SnowFlake( random() % (QApplication::desktop()->geometry().right() - size), -1 * size, size, size, mMaxVSpeed, mMaxHSpeed );
                flakes->append( flake );

                // calculation of next time of snowflake
                // depends on the time the flow needs to get to the bottom (screen size)
                // and the fps
                long next = ((500/(time+5))*(Effect::displayHeight()/flake.getVSpeed()))/mNumberFlakes;
                nextFlakeMillis = next;
                lastFlakeTime.restart();
                }
        }
    effects->prePaintScreen( data, time );
    }

void SnowEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data ); // paint normal screen
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( active )
        {
        if(! texture ) loadTexture();
        if( texture )
            {
            glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
            texture->bind();
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

            for (int i=0; i<flakes->count(); i++)
                {
                SnowFlake flake = flakes->at(i);
                
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
            texture->unbind();
            glPopAttrib();
            }
        }
#endif
    }

void SnowEffect::postPaintScreen()
    {
    if( active )
        {
        effects->addRepaintFull();
        }
    effects->postPaintScreen();
    }

void SnowEffect::toggle()
    {
    active = !active;
    if (!active) flakes->clear();
    effects->addRepaintFull();
    }

void SnowEffect::loadTexture()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    QString file = KGlobal::dirs()->findResource( "appdata", "snowflake.png" );
    if( file.isEmpty())
        return;
    texture = new GLTexture( file );
#endif
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

} // namespace
