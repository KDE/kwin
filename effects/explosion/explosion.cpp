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

#include "explosion.h"

#include <kwinglutils.h>
#include <kwinglplatform.h>

#include <QMatrix4x4>
#include <QVector2D>

#include <KStandardDirs>
#include <kdebug.h>

#include <math.h>


namespace KWin
{

KWIN_EFFECT(explosion, ExplosionEffect)
KWIN_EFFECT_SUPPORTED(explosion, ExplosionEffect::supported())

ExplosionEffect::ExplosionEffect() : Effect()
{
    mShader = 0;
    mStartOffsetTex = 0;
    mEndOffsetTex = 0;

    mActiveAnimations = 0;
    mValid = true;
    mInited = false;
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
}

ExplosionEffect::~ExplosionEffect()
{
    delete mShader;
    delete mStartOffsetTex;
    delete mEndOffsetTex;
}

bool ExplosionEffect::supported()
{
    return GLPlatform::instance()->supports(GLSL) &&
           (effects->compositingType() == OpenGLCompositing);
}

bool ExplosionEffect::loadData()
{
    mInited = true;
    if (!ShaderManager::instance()->isValid()) {
        return false;
    }
    QString shadername("explosion");
    const QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/explosion.frag");
    QString starttexture =  KGlobal::dirs()->findResource("data", "kwin/explosion-start.png");
    QString endtexture =  KGlobal::dirs()->findResource("data", "kwin/explosion-end.png");
    if (starttexture.isEmpty() || endtexture.isEmpty()) {
        kError(1212) << "Couldn't locate texture files" << endl;
        return false;
    }

    mShader = ShaderManager::instance()->loadFragmentShader(ShaderManager::GenericShader, fragmentshader);
    if (!mShader->isValid()) {
        kError(1212) << "The shader failed to load!" << endl;
        return false;
    } else {
        ShaderManager::instance()->pushShader(mShader);
        mShader->setUniform("startOffsetTexture", 4);
        mShader->setUniform("endOffsetTexture", 5);
        ShaderManager::instance()->popShader();
    }

    mStartOffsetTex = new GLTexture(starttexture);
    mEndOffsetTex = new GLTexture(endtexture);
    if (mStartOffsetTex->isNull() || mEndOffsetTex->isNull()) {
        kError(1212) << "The textures failed to load!" << endl;
        return false;
    } else {
        mStartOffsetTex->setFilter(GL_LINEAR);
        mEndOffsetTex->setFilter(GL_LINEAR);
    }

    return true;
}

void ExplosionEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (mActiveAnimations > 0)
        // We need to mark the screen as transformed. Otherwise the whole screen
        //  won't be repainted, resulting in artefacts
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, time);
}

void ExplosionEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (mWindows.contains(w)) {
        if (mValid) {
            mWindows[ w  ] += time / animationTime(700.0);   // complete change in 700ms
            if (mWindows[ w  ] < 1) {
                data.setTranslucent();
                data.setTransformed();
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
            } else {
                mWindows.remove(w);
                w->unrefWindow();
                mActiveAnimations--;
            }
        }
    }

    effects->prePaintWindow(w, data, time);
}

void ExplosionEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    // Make sure we have OpenGL compositing and the window is vidible and not a
    //  special window
    bool useshader = (mValid && mWindows.contains(w));
    if (useshader) {
        double maxscaleadd = 1.5f;
        double scale = 1 + maxscaleadd * mWindows[w];
        data.xScale = scale;
        data.yScale = scale;
        data.xTranslate += int(w->width() / 2 * (1 - scale));
        data.yTranslate += int(w->height() / 2 * (1 - scale));
        data.opacity *= 0.99;  // Force blending
        ShaderManager *manager = ShaderManager::instance();
        GLShader *shader = manager->pushShader(ShaderManager::GenericShader);
        QMatrix4x4 screenTransformation = shader->getUniformMatrix4x4("screenTransformation");
        manager->popShader();
        ShaderManager::instance()->pushShader(mShader);
        mShader->setUniform("screenTransformation", screenTransformation);
        mShader->setUniform("factor", (float)mWindows[w]);
        mShader->setUniform("scale", (float)scale);
        mShader->setUniform("windowSize", QVector2D(w->width(), w->height()));
        glActiveTexture(GL_TEXTURE4);
        mStartOffsetTex->bind();
        glActiveTexture(GL_TEXTURE5);
        mEndOffsetTex->bind();
        glActiveTexture(GL_TEXTURE0);
        data.shader = mShader;
    }

    // Call the next effect.
    effects->paintWindow(w, mask, region, data);

    if (useshader) {
        ShaderManager::instance()->popShader();
        glActiveTexture(GL_TEXTURE4);
        mStartOffsetTex->unbind();
        glActiveTexture(GL_TEXTURE5);
        mEndOffsetTex->unbind();
        glActiveTexture(GL_TEXTURE0);
    }
}

void ExplosionEffect::postPaintScreen()
{
    if (mActiveAnimations > 0)
        effects->addRepaintFull();

    // Call the next effect.
    effects->postPaintScreen();
}

void ExplosionEffect::slotWindowClosed(EffectWindow* c)
{
    const void* e = c->data(WindowClosedGrabRole).value<void*>();
    if (e && e != this)
        return;
    if (c->isOnCurrentDesktop() && !c->isMinimized()) {
        if (mValid && !mInited)
            mValid = loadData();
        if (!mValid) {
            // don't add to list as we cannot animate this window;
            return;
        }
        mWindows[ c ] = 0; // count up to 1
        c->addRepaintFull();
        c->refWindow();
        mActiveAnimations++;
    }
}

void ExplosionEffect::slotWindowDeleted(EffectWindow* c)
{
    mWindows.remove(c);
}

bool ExplosionEffect::isActive() const
{
    return mActiveAnimations > 0;
}

} // namespace

