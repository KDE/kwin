/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "demo_liquid.h"

#include <kwinglutils.h>

#include <QString>
#include <KStandardDirs>

#include <kdebug.h>


namespace KWin
{

KWIN_EFFECT( Demo_Liquid, LiquidEffect );
KWIN_EFFECT_SUPPORTED( Demo_Liquid, LiquidEffect::supported() );


LiquidEffect::LiquidEffect() : Effect()
    {
    mTime = 0.0f;
    mValid = loadData();
    }

bool LiquidEffect::loadData()
    {
    // Create texture and render target
    mTexture = new GLTexture(displayWidth(), displayHeight());
    mTexture->setFilter(GL_LINEAR_MIPMAP_LINEAR);

    mRenderTarget = new GLRenderTarget(mTexture);
    if( !mRenderTarget->valid() )
        return false;

    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/liquid.frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/liquid.vert");
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
    mShader->bind();
    mShader->setUniform("sceneTex", 0);
    mShader->setUniform("displayWidth", (float)displayWidth());
    mShader->setUniform("displayHeight", (float)displayHeight());
    mShader->unbind();

    return true;
    }

bool LiquidEffect::supported()
    {
    return hasGLExtension("GL_EXT_framebuffer_object") &&
            GLShader::fragmentShaderSupported() &&
            (effects->compositingType() == OpenGLCompositing);
    }


void LiquidEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    mTime += time / 1000.0f;
    if(mValid)
        {
        *mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        // Start rendering to texture
        mRenderTarget->enable();
        }

    effects->prePaintScreen(mask, region, time);
    }

void LiquidEffect::postPaintScreen()
    {
    // Call the next effect.
    effects->postPaintScreen();

    if(mValid)
        {
        // Disable render texture
        mRenderTarget->disable();
        mTexture->bind();

        // Use the shader
        mShader->bind();
        mShader->setUniform("time", mTime);

        // Render fullscreen quad with screen contents
        glBegin(GL_QUADS);
            glVertex2f(0.0, displayHeight());
            glVertex2f(displayWidth(), displayHeight());
            glVertex2f(displayWidth(), 0.0);
            glVertex2f(0.0, 0.0);
        glEnd();

        mShader->unbind();
        mTexture->unbind();

        // Make sure the animation continues
        effects->addRepaintFull();
        }

    }


} // namespace

