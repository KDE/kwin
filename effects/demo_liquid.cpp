/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "demo_liquid.h"

#include <kwinglutils.h>


#include <KStandardDirs>

#include <kdebug.h>
#include <assert.h>


namespace KWin
{

KWIN_EFFECT( Demo_Liquid, LiquidEffect )
KWIN_EFFECT_SUPPORTED( Demo_Liquid, LiquidEffect::supported() )


LiquidEffect::LiquidEffect() : Effect()
    {
    mTexture = 0;
    mRenderTarget = 0;
    mShader = 0;

    mTime = 0.0f;
    mValid = loadData();
    }
LiquidEffect::~LiquidEffect()
    {
    delete mTexture;
    delete mRenderTarget;
    delete mShader;
    }

bool LiquidEffect::loadData()
    {
    // If NPOT textures are not supported, use nearest power-of-two sized
    //  texture. It wastes memory, but it's possible to support systems without
    //  NPOT textures that way
    int texw = displayWidth();
    int texh = displayHeight();
    if( !GLTexture::NPOTTextureSupported() )
    {
        kWarning( 1212 ) << k_funcinfo << "NPOT textures not supported, wasting some memory" << endl;
        texw = nearestPowerOfTwo(texw);
        texh = nearestPowerOfTwo(texh);
    }
    // Create texture and render target
    mTexture = new GLTexture(texw, texh);
    mTexture->setFilter(GL_LINEAR_MIPMAP_LINEAR);
    mTexture->setWrapMode(GL_CLAMP);

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
    mShader->setUniform("textureWidth", (float)texw);
    mShader->setUniform("textureHeight", (float)texh);
    mShader->unbind();

    return true;
    }

bool LiquidEffect::supported()
    {
    return GLRenderTarget::supported() &&
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
        effects->pushRenderTarget(mRenderTarget);
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
        assert( effects->popRenderTarget() == mRenderTarget );
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

