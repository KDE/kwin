/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "explosioneffect.h"

#include <kwinglutils.h>


#include <KStandardDirs>
#include <kdebug.h>

#include <math.h>


namespace KWin
{

KWIN_EFFECT( explosion, ExplosionEffect )
KWIN_EFFECT_SUPPORTED( explosion, ExplosionEffect::supported() )

ExplosionEffect::ExplosionEffect() : Effect()
    {
    mShader = 0;
    mStartOffsetTex = 0;
    mEndOffsetTex = 0;

    mActiveAnimations = 0;
    mValid = true;
    mInited = false;
    }

ExplosionEffect::~ExplosionEffect()
    {
    delete mShader;
    delete mStartOffsetTex;
    delete mEndOffsetTex;
    }

bool ExplosionEffect::supported()
    {
    return GLShader::fragmentShaderSupported() &&
            (effects->compositingType() == OpenGLCompositing);
    }

bool ExplosionEffect::loadData()
{
    mInited = true;
    QString shadername("explosion");
    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/explosion.frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/explosion.vert");
    QString starttexture =  KGlobal::dirs()->findResource("data", "kwin/explosion-start.png");
    QString endtexture =  KGlobal::dirs()->findResource("data", "kwin/explosion-end.png");
    if(fragmentshader.isEmpty() || vertexshader.isEmpty())
    {
        kError() << "Couldn't locate shader files" << endl;
        return false;
    }
    if(starttexture.isEmpty() || endtexture.isEmpty())
    {
        kError() << "Couldn't locate texture files" << endl;
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
        mShader->setUniform("winTexture", 0);
        mShader->setUniform("startOffsetTexture", 4);
        mShader->setUniform("endOffsetTexture", 5);
        mShader->unbind();
    }

    mStartOffsetTex = new GLTexture(starttexture);
    mEndOffsetTex = new GLTexture(endtexture);
    if(mStartOffsetTex->isNull() || mEndOffsetTex->isNull())
    {
        kError() << "The textures failed to load!" << endl;
        return false;
    }
    else
    {
        mStartOffsetTex->setFilter( GL_LINEAR );
        mEndOffsetTex->setFilter( GL_LINEAR );
    }

    return true;
    }

void ExplosionEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( mActiveAnimations > 0 )
        // We need to mark the screen as transformed. Otherwise the whole screen
        //  won't be repainted, resulting in artefacts
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
    }

void ExplosionEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    if( mWindows.contains( w ))
        {
        if( mValid && !mInited )
            mValid = loadData();
        if( mValid )
            {
            mWindows[ w  ] += time / animationTime( 700.0 ); // complete change in 700ms
            if( mWindows[ w  ] < 1 )
                {
                data.setTranslucent();
                data.setTransformed();
                w->enablePainting( EffectWindow::PAINT_DISABLED_BY_DELETE );
                }
            else
                {
                mWindows.remove( w );
                w->unrefWindow();
                mActiveAnimations--;
                }
            }
        }

    effects->prePaintWindow( w, data, time );
    }

void ExplosionEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Make sure we have OpenGL compositing and the window is vidible and not a
    //  special window
    bool useshader = ( mValid && mWindows.contains( w ) );
    if( useshader )
        {
        double maxscaleadd = 1.5f;
        double scale = 1 + maxscaleadd*mWindows[w];
        data.xScale = scale;
        data.yScale = scale;
        data.xTranslate += int( w->width() / 2 * ( 1 - scale ));
        data.yTranslate += int( w->height() / 2 * ( 1 - scale ));
        data.opacity *= 0.99;  // Force blending
        mShader->bind();
        mShader->setUniform("factor", (float)mWindows[w]);
        mShader->setUniform("scale", (float)scale);
        glActiveTexture(GL_TEXTURE4);
        mStartOffsetTex->bind();
        glActiveTexture(GL_TEXTURE5);
        mEndOffsetTex->bind();
        glActiveTexture(GL_TEXTURE0);
        data.shader = mShader;
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );

    if( useshader )
        {
        mShader->unbind();
        glActiveTexture(GL_TEXTURE4);
        mStartOffsetTex->unbind();
        glActiveTexture(GL_TEXTURE5);
        mEndOffsetTex->unbind();
        glActiveTexture(GL_TEXTURE0);
        }
    }

void ExplosionEffect::postPaintScreen()
    {
    if( mActiveAnimations > 0 )
        effects->addRepaintFull();

    // Call the next effect.
    effects->postPaintScreen();
    }

void ExplosionEffect::windowClosed( EffectWindow* c )
    {
    if( c->isOnCurrentDesktop() && !c->isMinimized())
        {
        mWindows[ c ] = 0; // count up to 1
        c->addRepaintFull();
        c->refWindow();
        mActiveAnimations++;
        }
    }

void ExplosionEffect::windowDeleted( EffectWindow* c )
    {
    mWindows.remove( c );
    }

} // namespace

