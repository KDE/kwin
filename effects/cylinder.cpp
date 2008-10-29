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
#include "cylinder.h"

#include <kdebug.h>
#include <KStandardDirs>

#include <math.h>

#include <GL/gl.h>

namespace KWin
{

KWIN_EFFECT( cylinder, CylinderEffect )
KWIN_EFFECT_SUPPORTED( cylinder, CylinderEffect::supported() )

CylinderEffect::CylinderEffect()
    : CubeEffect()
    , mInited( false )
    , mValid( true )
    , mShader( 0 )
    {
    if( wallpaper )
        wallpaper->discard();
    reconfigure( ReconfigureAll );
    }

CylinderEffect::~CylinderEffect()
    {
    delete mShader;
    }

void CylinderEffect::reconfigure( ReconfigureFlags )
    {
    loadConfig( "Cylinder" );
    animateDesktopChange = false;
    bigCube = true;
    }

bool CylinderEffect::supported()
    {
    return GLShader::fragmentShaderSupported() &&
            (effects->compositingType() == OpenGLCompositing);
    }

bool CylinderEffect::loadData()
    {
    mInited = true;
    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/cylinder.frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/cylinder.vert");
    if(fragmentshader.isEmpty() || vertexshader.isEmpty())
        {
        kError() << "Couldn't locate shader files" << endl;
        return false;
        }

    mShader = new GLShader(vertexshader, fragmentshader);
    if(!mShader->isValid())
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
        mShader->unbind();
        }
    return true;
    }

void CylinderEffect::paintScene( int mask, QRegion region, ScreenPaintData& data )
    {
    glPushMatrix();
    QRect rect = effects->clientArea( FullArea, activeScreen, effects->currentDesktop());

    float cubeAngle = (effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f;
    float radian = (cubeAngle*0.5)*M_PI/180;
    // height of the triangle compound of  one side of the cube and the two bisecting lines
    float midpoint = rect.width()*0.5*tan(radian);
    // radius of the circle
    float radius = (rect.width()*0.5)/cos(radian);
    glTranslatef( 0.0, 0.0, midpoint - radius );
    CubeEffect::paintScene( mask, region, data );
    glPopMatrix();
    }

void CylinderEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
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

void CylinderEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
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

void CylinderEffect::desktopChanged( int old )
    {
    // cylinder effect is not useful to slide
    }

void CylinderEffect::paintCap( float z, float zTexture )
    {
    if( ( !paintCaps ) || effects->numberOfDesktops() <= 2 )
        return;
    CubeEffect::paintCap( z, zTexture );
    QRect rect = effects->clientArea( FullArea, activeScreen, painting_desktop );

    float cubeAngle = (effects->numberOfDesktops() - 2 )/(float)effects->numberOfDesktops() * 180.0f;
    float radian = (cubeAngle*0.5)*M_PI/180;
    // height of the triangle compound of  one side of the cube and the two bisecting lines
    float midpoint = rect.width()*0.5*tan(radian);
    // radius of the circle
    float radius = (rect.width()*0.5)/cos(radian);
    
    // paint round part of the cap
    glPushMatrix();
    float zTranslate = zPosition + zoom;
    if( start )
        zTranslate *= timeLine.value();
    if( stop )
        zTranslate *= ( 1.0 - timeLine.value() );
    glTranslatef( 0.0, 0.0, -zTranslate );
    float triangleWidth = 40.0;

    for( int i=0; i<effects->numberOfDesktops(); i++ )
        {
        glPushMatrix();
        glTranslatef( rect.width()/2, 0.0, -z );
        glRotatef(  i*(360.0f/effects->numberOfDesktops()), 0.0, 1.0, 0.0 );
        glTranslatef( -rect.width()/2, 0.0, z );
        glBegin( GL_TRIANGLE_STRIP );
        for( int j=0; j<rect.width()/triangleWidth; j++ )
            {
            float zValue = 0.0;
            // distance from midpoint of desktop to x coord
            // calculation is same as in shader -> see comments
            float distance = rect.width()*0.5 - (j*triangleWidth);
            if( (j*triangleWidth) > rect.width()*0.5 )
                {
                distance = (j*triangleWidth) - rect.width()*0.5;
                }
            // distance in correct format
            float angle = acos( distance/radius );
            float h = radius;
            // if distance == 0 -> angle=90 -> tan(90) singularity
            if( distance != 0.0 )
                h = tan( angle ) * distance;
            zValue = h - midpoint;
            glVertex3f( j*triangleWidth, 0.0, zValue );
            glVertex3f( (j+1)*triangleWidth, 0.0, 0.0 );
            }
        glEnd();
        glPopMatrix();
        }
    glPopMatrix();
    }

} // namespace
