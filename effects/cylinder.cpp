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

CylinderEffect::CylinderEffect()
    : CubeEffect()
    , mInited( false )
    , mValid( true )
    , mShader( 0 )
    {
    }

CylinderEffect::~CylinderEffect()
    {
    delete mShader;
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
        QRect rect = effects->clientArea( FullArea, effects->activeScreen(), effects->currentDesktop());
        mShader->setUniform( "width", (float)rect.width() );
        mShader->setUniform( "origWidth", (float) (2.0 * xmax * ( zNear*2+1 + 1.0f * 0.8f ) * tan( fovy * M_PI / 360.0f )/ymax) );
        mShader->unbind();
        }
    return true;
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

} // namespace
