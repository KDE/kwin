/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>

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

#include "kwinshadereffect.h"

#include "kwinglutils.h"

#include <kstandarddirs.h>
#include <kdebug.h>

#include <assert.h>


namespace KWin
{


ShaderEffect::ShaderEffect(const QString& shadername) : Effect()
{
    mTexture = 0;
    mRenderTarget = 0;
    mShader = 0;

    mTime = 0.0f;
    mValid = loadData(shadername);
    mEnabled = false;
}

ShaderEffect::~ShaderEffect()
{
    delete mTexture;
    delete mRenderTarget;
    delete mShader;
}

bool ShaderEffect::loadData(const QString& shadername)
{
#ifndef KWIN_HAVE_OPENGL_COMPOSITING
    return false;
#else
    // If NPOT textures are not supported, use nearest power-of-two sized
    //  texture. It wastes memory, but it's possible to support systems without
    //  NPOT textures that way
    int texw = displayWidth();
    int texh = displayHeight();
    if( !GLTexture::NPOTTextureSupported() )
    {
        kWarning( 1212 ) << "NPOT textures not supported, wasting some memory" ;
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

    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/" + shadername + ".frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/" + shadername + ".vert");
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
    mShader->bind();
    mShader->setUniform("sceneTex", 0);
    mShader->setUniform("textureWidth", (float)texw);
    mShader->setUniform("textureHeight", (float)texh);
    mShader->unbind();

    return true;
#endif
}

bool ShaderEffect::supported()
{
#ifndef KWIN_HAVE_OPENGL_COMPOSITING
    return false;
#else
    return GLRenderTarget::supported() &&
            GLShader::fragmentShaderSupported() &&
            (effects->compositingType() == OpenGLCompositing);
#endif
}

bool ShaderEffect::isEnabled() const
{
    return mEnabled;
}

void ShaderEffect::setEnabled(bool enabled)
{
    mEnabled = enabled;
    // Everything needs to be repainted
    effects->addRepaintFull();
}

GLShader* ShaderEffect::shader() const
{
    return mShader;
}

void ShaderEffect::prePaintScreen( ScreenPrePaintData& data, int time )
{
    mTime += time / 1000.0f;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( mValid && mEnabled )
    {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        // Start rendering to texture
        effects->pushRenderTarget(mRenderTarget);
    }
#endif

    effects->prePaintScreen(data, time);
}

void ShaderEffect::postPaintScreen()
{
    // Call the next effect.
    effects->postPaintScreen();

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( mValid && mEnabled )
    {
        // Disable render texture
        assert( effects->popRenderTarget() == mRenderTarget );
        mTexture->bind();

        // Use the shader
        mShader->bind();
        mShader->setUniform("time", (float)mTime);
        mShader->setUniform("cursorX", (float)cursorPos().x());
        mShader->setUniform("cursorY", (float)cursorPos().y());

        // Render fullscreen quad with screen contents
        glBegin(GL_QUADS);
        glVertex2f(0.0, displayHeight());
        glVertex2f(displayWidth(), displayHeight());
        glVertex2f(displayWidth(), 0.0);
        glVertex2f(0.0, 0.0);
        glEnd();

        mShader->unbind();
        mTexture->unbind();
    }
#endif
}

} // namespace

