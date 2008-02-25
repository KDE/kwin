/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2007 Martin Gräßlin <ubuntu@martin-graesslin.com

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
    KConfigGroup conf = effects->effectConfig("Snow");
    mNumberFlakes = conf.readEntry("Number", 50);
    mMinFlakeSize = conf.readEntry("MinFlakes", 10);
    mMaxFlakeSize = conf.readEntry("MaxFlakes", 50);

    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = static_cast< KAction* >( actionCollection->addAction( "Snow" ));
    a->setText( i18n("Snow" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::META + Qt::Key_F12 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( toggle()));
    }

SnowEffect::~SnowEffect()
    {
    delete texture;
    delete flakes;
    }

void SnowEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if ( active )
        {
        if (! flakes )
            {
            flakes = new QList<QRect>();
            lastFlakeTime.start();
            }
        int count = flakes->count();
        for (int i=0; i<count; i++)
            {
            // move flake to bottom. Therefore pop the flake, change y and push
            // flake back to QVector
            QRect flake = flakes->first();
            flakes->pop_front();
            int size = flake.height();
            int y = flake.y();
            // if flake has reached bottom, don't push it back
            if ( y >= QApplication::desktop()->geometry().bottom() )
                {
                continue;
                }
            int speed;
            float factor = (float)(size-mMinFlakeSize) / (float)(mMaxFlakeSize-mMinFlakeSize);
            if (factor >= 0.5) speed = 2;
            else speed = 1;
            flake.setY(y + speed);
            flake.setHeight(size);
            flakes->append(flake);
            }
            // if number of active snowflakes is smaller than maximum number
            // create a random new snowflake
            if ( ( lastFlakeTime.elapsed() >= nextFlakeMillis ) && flakes->count() < mNumberFlakes)
                {
                int size = 0;
                while ( size < mMinFlakeSize )
                    size = random() % mMaxFlakeSize;
                QRect flake = QRect( random() % (QApplication::desktop()->geometry().right() - size), -1 * size, size, size );
                flakes->append( flake );

                // calculation of next time of snowflake
                // depends on the time the flow needs to get to the bottom (screen size)
                // and the fps
                int speed;
                float factor = (float)(size-mMinFlakeSize) / (float)(mMaxFlakeSize-mMinFlakeSize);
                if (factor >= 0.5) speed = 4;
                else speed = 2;
                long next = ((1000/(time+5))*(Effect::displayHeight()/speed))/mNumberFlakes;
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
                texture->render( region, flakes->at(i));
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


} // namespace
