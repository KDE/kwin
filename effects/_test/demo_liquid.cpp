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

#include "demo_liquid.h"

#include <kwinglutils.h>



#include <kdebug.h>
#include <assert.h>


namespace KWin
{

KWIN_EFFECT(demo_liquid, LiquidEffect)
KWIN_EFFECT_SUPPORTED(demo_liquid, LiquidEffect::supported())


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
    if (!GLTexture::NPOTTextureSupported()) {
        kWarning(1212) << "NPOT textures not supported, wasting some memory" ;
        texw = nearestPowerOfTwo(texw);
        texh = nearestPowerOfTwo(texh);
    }
    // Create texture and render target
    mTexture = new GLTexture(texw, texh);
    mTexture->setFilter(GL_LINEAR_MIPMAP_LINEAR);
    mTexture->setWrapMode(GL_CLAMP);

    mRenderTarget = new GLRenderTarget(*mTexture);
    if (!mRenderTarget->valid())
        return false;

    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/liquid.frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/liquid.vert");
    if (fragmentshader.isEmpty() || vertexshader.isEmpty()) {
        kError(1212) << "Couldn't locate shader files" << endl;
        return false;
    }
    mShader = new GLShader(vertexshader, fragmentshader);
    if (!mShader->isValid()) {
        kError(1212) << "The shader failed to load!" << endl;
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


void LiquidEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    mTime += time / 1000.0f;
    if (mValid) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        // Start rendering to texture
        effects->pushRenderTarget(mRenderTarget);
    }

    effects->prePaintScreen(data, time);
}

void LiquidEffect::postPaintScreen()
{
    // Call the next effect.
    effects->postPaintScreen();

    if (mValid) {
        // Disable render texture
        assert(effects->popRenderTarget() == mRenderTarget);
        mTexture->bind();

        // Use the shader
        mShader->bind();
        mShader->setUniform("time", (float)mTime);

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

