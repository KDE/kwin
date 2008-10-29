/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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
#include "cube.h"
#include "sphere.h"

#include <kdebug.h>
#include <KStandardDirs>
#include <kconfiggroup.h>

#include <math.h>

#include <GL/gl.h>

namespace KWin
{

KWIN_EFFECT( sphere, SphereEffect )
KWIN_EFFECT_SUPPORTED( sphere, SphereEffect::supported() )

SphereEffect::SphereEffect()
    : CubeEffect()
    , mInited( false )
    , mValid( true )
    , mShader( 0 )
    {
    if( wallpaper )
        wallpaper->discard();
    reconfigure( ReconfigureAll );
    }

SphereEffect::~SphereEffect()
    {
    delete mShader;
    }

void SphereEffect::reconfigure( ReconfigureFlags )
    {
    loadConfig( "Sphere" );
    reflection = false;
    animateDesktopChange = false;
    KConfigGroup conf = effects->effectConfig( "Sphere" );
    zPosition = conf.readEntry( "ZPosition", 450.0 );
    capDeformationFactor = conf.readEntry( "CapDeformation", 0 )/100.0f;
    bigCube = true;
    }

bool SphereEffect::supported()
    {
    return GLShader::fragmentShaderSupported() &&
            (effects->compositingType() == OpenGLCompositing);
    }

bool SphereEffect::loadData()
    {
    mInited = true;
    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/cylinder.frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/sphere.vert");
    if( fragmentshader.isEmpty() || vertexshader.isEmpty() )
        {
        kError() << "Couldn't locate shader files" << endl;
        return false;
        }

    mShader = new GLShader( vertexshader, fragmentshader );
    if( !mShader->isValid() )
        {
        kError() << "The shader failed to load!" << endl;
        return false;
        }
    else
        {
        mShader->bind();
        mShader->setUniform( "winTexture", 0 );
        mShader->setUniform( "opacity", cubeOpacity );
        QRect rect = effects->clientArea( FullScreenArea, activeScreen, effects->currentDesktop());
        if( effects->numScreens() > 1 && (slide || bigCube ) )
            rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop() );
        mShader->setUniform( "width", (float)rect.width() );
        mShader->setUniform( "height", (float)rect.height() );
        mShader->unbind();
        }
    return true;
    }

void SphereEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( activated )
        {
        if( cube_painting )
            {
            if( w->isOnDesktop( painting_desktop ))
                {
                data.quads = data.quads.makeGrid( 40 );
                QRect rect = effects->clientArea( FullArea, activeScreen, painting_desktop );
                if( w->x() < rect.width()/2 && w->x() + w->width() > rect.width()/ 2 )
                    data.quads = data.quads.splitAtX( rect.width()/2 - w->x() );
                w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                }
            else
                {
                w->disablePainting( EffectWindow::PAINT_DISABLED_BY_DESKTOP );
                }
            }
        }
    effects->prePaintWindow( w, data, time );
    }

void SphereEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( activated && cube_painting )
        {
        if( mValid && !mInited )
            mValid = loadData();
        bool useShader = mValid;
        if( useShader )
            {
            mShader->bind();
            mShader->setUniform( "windowWidth", (float)w->width() );
            mShader->setUniform( "windowHeight", (float)w->height() );
            mShader->setUniform( "xCoord", (float)w->x() );
            mShader->setUniform( "yCoord", (float)w->y() );
            mShader->setUniform( "cubeAngle", (effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f );
            data.shader = mShader;
            }
        CubeEffect::paintWindow( w, mask, region, data );
        if( useShader )
            {
            mShader->unbind();
            }        
        }
    else
        effects->paintWindow( w, mask, region, data );
    }

void SphereEffect::paintCap( float z, float zTexture )
    {
    if( bottomCap )
        {
        glPushMatrix();
        glScalef( 1.0, -1.0, 1.0 );
        }
    CubeEffect::paintCap( z, zTexture );
    if( bottomCap )
        glPopMatrix();
    }

void SphereEffect::paintCapStep( float z, float zTexture, bool texture )
    {
    QRect rect = effects->clientArea( FullArea, activeScreen, painting_desktop );
    float cubeAngle = (effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f;
    float radius = (rect.width()*0.5)/cos(cubeAngle*0.5*M_PI/180.0);
    float angle = acos( (rect.height()*0.5)/radius )*180.0/M_PI;
    angle /= 30;
    if( texture )
        capTexture->bind();
    glPushMatrix();
    glTranslatef( 0.0, -rect.height()*0.5, 0.0 );
    glBegin( GL_QUADS );
    for( int i=0; i<30; i++ )
        {
        float topAngle = angle*i*M_PI/180.0;
        float bottomAngle = angle*(i+1)*M_PI/180.0;
        float yTop = rect.height() - radius * cos( topAngle );
        yTop -= (yTop-rect.height()*0.5)*capDeformationFactor;
        float yBottom = rect.height() -radius * cos( bottomAngle );
        yBottom -= (yBottom-rect.height()*0.5)*capDeformationFactor;
        for( int j=0; j<36; j++ )
            {
            float x = radius * sin( topAngle ) * sin( (90.0+j*10.0)*M_PI/180.0 );
            float z = radius * sin( topAngle ) * cos( (90.0+j*10.0)*M_PI/180.0 );
            if( texture )
                glTexCoord2f( x/(rect.width())+0.5, 0.5 - z/zTexture * 0.5 );
            glVertex3f( x, yTop, z );
            x = radius * sin( bottomAngle ) * sin( (90.0+j*10.0)*M_PI/180.00 );
            z = radius * sin( bottomAngle ) * cos( (90.0+j*10.0)*M_PI/180.0 );
            if( texture )
                glTexCoord2f( x/(rect.width())+0.5, 0.5 - z/zTexture * 0.5 );
            glVertex3f( x, yBottom, z );
            x = radius * sin( bottomAngle ) * sin( (90.0+(j+1)*10.0)*M_PI/180.0 );
            z = radius * sin( bottomAngle ) * cos( (90.0+(j+1)*10.0)*M_PI/180.0 );
            if( texture )
                glTexCoord2f( x/(rect.width())+0.5, 0.5 - z/zTexture * 0.5 );
            glVertex3f( x, yBottom, z );
            x = radius * sin( topAngle ) * sin( (90.0+(j+1)*10.0)*M_PI/180.0 );
            z = radius * sin( topAngle ) * cos( (90.0+(j+1)*10.0)*M_PI/180.0 );
            if( texture )
                glTexCoord2f( x/(rect.width())+0.5, 0.5 - z/zTexture * 0.5 );
            glVertex3f( x, yTop, z );            
            }
        }
    glEnd();
    glPopMatrix();
    if( texture )
        {
        capTexture->unbind();
        }
    }


} // namespace
