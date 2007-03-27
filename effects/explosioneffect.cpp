/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "explosioneffect.h"

#include <scene_opengl.h>
#include <workspace.h>
#include <client.h>
#include <glutils.h>
#include <deleted.h>

#include <QString>
#include <KStandardDirs>

#include <math.h>


namespace KWinInternal
{

ExplosionEffect::ExplosionEffect() : Effect()
    {
    mActiveAnimations = 0;
    mValid = true;
    mInited = false;
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
        kError() << k_funcinfo << "Couldn't locate shader files" << endl;
        return false;
    }
    if(starttexture.isEmpty() || endtexture.isEmpty())
    {
        kError() << k_funcinfo << "Couldn't locate texture files" << endl;
        return false;
    }

    mShader = new GLShader(vertexshader, fragmentshader);
    if(!mShader->isValid())
    {
        kError() << k_funcinfo << "The shader failed to load!" << endl;
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
        kError() << k_funcinfo << "The textures failed to load!" << endl;
        return false;
    }
    else
    {
        mStartOffsetTex->setFilter( GL_LINEAR );
        mEndOffsetTex->setFilter( GL_LINEAR );
    }

    return true;
    }

void ExplosionEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( mActiveAnimations > 0 )
        // We need to mark the screen as transformed. Otherwise the whole screen
        //  won't be repainted, resulting in artefacts
        *mask |= Scene::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(mask, region, time);
    }

void ExplosionEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( mWindows.contains( w ))
        {
        SceneOpenGL::Window* glwin = dynamic_cast< SceneOpenGL::Window* >( w->sceneWindow() );
        if( mValid && glwin && !mInited )
            mValid = loadData();
        if( mValid )
            {
            mWindows[ w  ] += time / 700.0; // complete change in 700ms
            if( mWindows[ w  ] < 1 )
                {
                *mask |= Scene::PAINT_WINDOW_TRANSLUCENT | Scene::PAINT_WINDOW_TRANSFORMED;
                *mask &= ~Scene::PAINT_WINDOW_OPAQUE;
                w->enablePainting( Scene::Window::PAINT_DISABLED_BY_DELETE );
                }
            else
                {
                mWindows.remove( w );
                static_cast< Deleted* >( w->window())->unrefWindow();
                mActiveAnimations--;
                }
            }
        }

    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void ExplosionEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Make sure we have OpenGL compositing and the window is vidible and not a
    //  special window
    SceneOpenGL::Window* glwin = dynamic_cast< SceneOpenGL::Window* >( w->sceneWindow() );
    bool useshader = ( mValid && glwin && mWindows.contains( w ) );
    if( useshader )
        {
        float maxscaleadd = 1.5f;
        float scale = 1 + maxscaleadd*mWindows[w];
        data.xScale = scale;
        data.yScale = scale;
        data.xTranslate += int( w->window()->width() / 2 * ( 1 - scale ));
        data.yTranslate += int( w->window()->height() / 2 * ( 1 - scale ));
        data.opacity *= 0.99;  // Force blending
        mShader->bind();
        mShader->setUniform("factor", (float)mWindows[w]);
        mShader->setUniform("scale", scale);
        glActiveTexture(GL_TEXTURE4);
        mStartOffsetTex->bind();
        glActiveTexture(GL_TEXTURE5);
        mEndOffsetTex->bind();
        glActiveTexture(GL_TEXTURE0);
        glwin->setShader(mShader);
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
        workspace()->addRepaintFull();

    // Call the next effect.
    effects->postPaintScreen();
    }

void ExplosionEffect::windowClosed( EffectWindow* c )
    {
    Client* cc = dynamic_cast< Client* >( c->window());
    if( cc == NULL || (cc->isOnCurrentDesktop() && !cc->isMinimized()))
        {
        mWindows[ c ] = 0; // count up to 1
        c->window()->addRepaintFull();
        static_cast< Deleted* >( c->window())->refWindow();
        mActiveAnimations++;
        }
    }

void ExplosionEffect::windowDeleted( EffectWindow* c )
    {
        mWindows.remove( c );
    }

} // namespace

