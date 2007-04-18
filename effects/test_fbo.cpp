/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "test_fbo.h"

#include <kwinglutils.h>


namespace KWin
{

KWIN_EFFECT( Test_FBO, TestFBOEffect );
KWIN_EFFECT_SUPPORTED( Test_FBO, TestFBOEffect::supported() );


TestFBOEffect::TestFBOEffect() : Effect()
    {
    mRot = 0.0f;

    // Create texture and render target
    mTexture = new GLTexture(displayWidth(), displayHeight());
    mTexture->setFilter(GL_LINEAR_MIPMAP_LINEAR);

    mRenderTarget = new GLRenderTarget(mTexture);

    mValid = mRenderTarget->valid();
    }

bool TestFBOEffect::supported()
    {
    return hasGLExtension("GL_EXT_framebuffer_object") &&
            (effects->compositingType() == OpenGLCompositing);
    }


void TestFBOEffect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if(mValid)
        {
        *mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        // Start rendering to texture
        mRenderTarget->enable();
        }

    effects->prePaintScreen(mask, region, time);
    }

void TestFBOEffect::postPaintScreen()
    {
    // Call the next effect.
    effects->postPaintScreen();

    if(mValid)
        {
        // Disable render texture
        mRenderTarget->disable();
        mTexture->bind();

        // Render fullscreen quad with screen contents
        glBegin(GL_QUADS);
            glTexCoord2f(0.0, 0.0); glVertex2f(0.0, displayHeight());
            glTexCoord2f(1.0, 0.0); glVertex2f(displayWidth(), displayHeight());
            glTexCoord2f(1.0, 1.0); glVertex2f(displayWidth(), 0.0);
            glTexCoord2f(0.0, 1.0); glVertex2f(0.0, 0.0);
        glEnd();

        // Render rotated screen thumbnail
        mRot += 0.5f;
        glTranslatef(displayWidth()/2.0f, displayHeight()/2.0f, 0.0f);
        glRotatef(mRot, 0.0, 0.0, 1.0);
        glScalef(0.2, 0.2, 0.2);
        glTranslatef(-displayWidth()/2.0f, -displayHeight()/2.0f, 0.0f);

        glBegin(GL_QUADS);
            glTexCoord2f(0.0, 0.0); glVertex2f(0.0, displayHeight());
            glTexCoord2f(1.0, 0.0); glVertex2f(displayWidth(), displayHeight());
            glTexCoord2f(1.0, 1.0); glVertex2f(displayWidth(), 0.0);
            glTexCoord2f(0.0, 1.0); glVertex2f(0.0, 0.0);
        glEnd();

        // Reset matrix and unbind texture
        glLoadIdentity();

        mTexture->unbind();

        // Make sure the animation continues
        effects->addRepaintFull();
        }

    }


} // namespace

