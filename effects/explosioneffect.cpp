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
    if(fragmentshader.isEmpty() || vertexshader.isEmpty())
    {
        kError() << k_funcinfo << "Couldn't locate shader files" << endl;
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

    if((mStartOffsetTex = loadTexture("explosion-start.png")) == 0)
        return false;
    if((mEndOffsetTex = loadTexture("explosion-end.png")) == 0)
        return false;

    return true;
    }

unsigned int ExplosionEffect::loadTexture(const QString& filename)
{
    QString fullfilename = KGlobal::dirs()->findResource("data", "kwin/" + filename);
    if(fullfilename.isEmpty())
    {
        kError() << k_funcinfo << "Couldn't find texture '" << filename << "'" << endl;
        return 0;
    }

    QImage img(fullfilename);
    if(img.isNull())
    {
        kError() << k_funcinfo << "Couldn't load image from file " << fullfilename << endl;
        return 0;
    }
    img = convertToGLFormat(img);

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, img.bits());
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

QImage ExplosionEffect::convertToGLFormat(const QImage& img) const
{
    // This method has been copied from Qt's QGLWidget::convertToGLFormat()
    QImage res = img.convertToFormat(QImage::Format_ARGB32);
    res = res.mirrored();

    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // Qt has ARGB; OpenGL wants RGBA
        for (int i=0; i < res.height(); i++) {
            uint *p = (uint*)res.scanLine(i);
            uint *end = p + res.width();
            while (p < end) {
                *p = (*p << 8) | ((*p >> 24) & 0xFF);
                p++;
            }
        }
    }
    else {
        // Qt has ARGB; OpenGL wants ABGR (i.e. RGBA backwards)
        res = res.rgbSwapped();
    }
    return res;
}

void ExplosionEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( mActiveAnimations > 0 )
        // We need to mark the screen as transformed. Otherwise the whole screen
        //  won't be repainted, resulting in artefacts
        *mask |= Scene::PAINT_SCREEN_TRANSFORMED;

    effects->prePaintScreen(mask, region, time);
    }

void ExplosionEffect::prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time )
    {
    if( mWindows.contains( w ))
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

    effects->prePaintWindow( w, mask, region, time );
    }

void ExplosionEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Make sure we have OpenGL compositing and the window is vidible and not a
    //  special window
    SceneOpenGL::Window* glwin = dynamic_cast< SceneOpenGL::Window* >( w->sceneWindow() );
    //Client* c = qobject_cast< Client* >( w->window() );
    bool useshader = ( mValid && glwin && mWindows.contains( w ) );
    if( useshader && !mInited )
        useshader = mValid = loadData();
    if( useshader )
        {
        float maxscaleadd = 1.5f;
        float scale = 1 + maxscaleadd*mWindows[w];
        //data.xTranslate = (f - 1)*
        data.xScale = scale;
        data.yScale = scale;
        data.xTranslate += int( w->window()->width() / 2 * ( 1 - scale ));
        data.yTranslate += int( w->window()->height() / 2 * ( 1 - scale ));
        data.opacity *= 0.99;  // Force blending
        mShader->bind();
        mShader->setUniform("factor", (float)mWindows[w]);
        mShader->setUniform("scale", scale);
        glActiveTexture(GL_TEXTURE4);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, mStartOffsetTex);
        glActiveTexture(GL_TEXTURE5);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, mEndOffsetTex);
        glActiveTexture(GL_TEXTURE0);
        glwin->setShader(mShader);
        }

    // Call the next effect.
    effects->paintWindow( w, mask, region, data );

    if( useshader )
        {
        mShader->unbind();
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
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

