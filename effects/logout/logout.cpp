/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>
Copyright (C) 2009, 2010 Lucas Murray <lmurray@undefinedfire.com>

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

#include "logout.h"

#include "kwinglutils.h"

#include <math.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <KDE/KStandardDirs>

#include <QtGui/QMatrix4x4>
#include <QtGui/QVector2D>

namespace KWin
{

KWIN_EFFECT(logout, LogoutEffect)

LogoutEffect::LogoutEffect()
    : progress(0.0)
    , displayEffect(false)
    , logoutWindow(NULL)
    , logoutWindowClosed(true)
    , logoutWindowPassed(false)
    , canDoPersistent(false)
    , ignoredWindows()
    , m_vignettingShader(NULL)
    , m_blurShader(NULL)
{
    // Persistent effect
    logoutAtom = XInternAtom(display(), "_KDE_LOGGING_OUT", False);
    effects->registerPropertyType(logoutAtom, true);

    // Block KSMServer's effect
    char net_wm_cm_name[ 100 ];
    sprintf(net_wm_cm_name, "_NET_WM_CM_S%d", DefaultScreen(display()));
    Atom net_wm_cm = XInternAtom(display(), net_wm_cm_name, False);
    Window sel = XGetSelectionOwner(display(), net_wm_cm);
    Atom hack = XInternAtom(display(), "_KWIN_LOGOUT_EFFECT", False);
    XChangeProperty(display(), sel, hack, hack, 8, PropModeReplace, (unsigned char*)&hack, 1);
    // the atom is not removed when effect is destroyed, this is temporary anyway

    blurTexture = NULL;
    blurTarget = NULL;
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(KWin::EffectWindow*,long)), this, SLOT(slotPropertyNotify(KWin::EffectWindow*,long)));
}

LogoutEffect::~LogoutEffect()
{
    delete blurTexture;
    delete blurTarget;
    delete m_vignettingShader;
    delete m_blurShader;
}

void LogoutEffect::reconfigure(ReconfigureFlags)
{
    frameDelay = 0;
    KConfigGroup conf = effects->effectConfig("Logout");
    useBlur = conf.readEntry("UseBlur", true);
    delete blurTexture;
    blurTexture = NULL;
    delete blurTarget;
    blurTarget = NULL;
    blurSupported = false;
    delete m_blurShader;
    m_blurShader = NULL;
}

void LogoutEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (!displayEffect && progress == 0.0) {
        if (blurTexture) {
            delete blurTexture;
            blurTexture = NULL;
            delete blurTarget;
            blurTarget = NULL;
            blurSupported = false;
        }
    } else if (!blurTexture) {
        blurSupported = false;
        delete blurTarget; // catch as we just tested the texture ;-P
        if (effects->isOpenGLCompositing() && GLTexture::NPOTTextureSupported() && GLRenderTarget::blitSupported() && useBlur) {
            // TODO: It seems that it is not possible to create a GLRenderTarget that has
            //       a different size than the display right now. Most likely a KWin core bug.
            // Create texture and render target
            blurTexture = new GLTexture(displayWidth(), displayHeight());
            blurTexture->setFilter(GL_LINEAR_MIPMAP_LINEAR);
            blurTexture->setWrapMode(GL_CLAMP_TO_EDGE);

            blurTarget = new GLRenderTarget(*blurTexture);
            if (blurTarget->valid())
                blurSupported = true;

            // As creating the render target takes time it can cause the first two frames of the
            // blur animation to be jerky. For this reason we only start the animation after the
            // third frame.
            frameDelay = 2;
        }
    }

    if (frameDelay)
        --frameDelay;
    else {
        if (displayEffect)
            progress = qMin(1.0, progress + time / animationTime(2000.0));
        else if (progress > 0.0)
            progress = qMax(0.0, progress - time / animationTime(500.0));
    }

    if (blurSupported && progress > 0.0) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }

    effects->prePaintScreen(data, time);
}

void LogoutEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (progress > 0.0) {
        if (effects->isOpenGLCompositing()) {
            // In OpenGL mode we add vignetting and, if supported, a slight blur
            if (blurSupported) {
                // When using blur we render everything to an FBO and as such don't do the vignetting
                // until after we render the FBO to the screen.
                if (w == logoutWindow) {
                    // Window is rendered after the FBO
                    windowOpacity = data.opacity();
                    data.setOpacity(0.0); // Cheat, we need the opacity for later but don't want to blur it
                } else {
                    if (logoutWindowPassed || ignoredWindows.contains(w)) {
                        // Window is rendered after the FBO
                        windows.append(w);
                        windowsOpacities[ w ] = data.opacity();
                        data.setOpacity(0.0);
                    } else // Window is added to the FBO
                        data.multiplySaturation((1.0 - progress * 0.2));
                }
            } else {
                // If we are not blurring then we are not rendering to an FBO
                if (w == logoutWindow)
                    // This is the logout window don't alter it but render our vignetting now
                    renderVignetting();
                else if (!logoutWindowPassed && !ignoredWindows.contains(w))
                    // Window is in the background, desaturate
                    data.multiplySaturation((1.0 - progress * 0.2));
                // All other windows are unaltered
            }
        }
        if (effects->compositingType() == KWin::XRenderCompositing) {
            // Since we can't do vignetting in XRender just do a stronger desaturation and darken
            if (w != logoutWindow && !logoutWindowPassed && !ignoredWindows.contains(w)) {
                data.multiplySaturation((1.0 - progress * 0.8));
                data.multiplyBrightness((1.0 - progress * 0.3));
            }
        }
        if (w == logoutWindow ||
                ignoredWindows.contains(w))   // HACK: All windows past the first ignored one should not be
            //       blurred as it affects the stacking order.
            // All following windows are on top of the logout window and should not be altered either
            logoutWindowPassed = true;
    }
    effects->paintWindow(w, mask, region, data);
}

void LogoutEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);

    if (effects->isOpenGLCompositing() && progress > 0.0) {
        if (!blurSupported) {
            if (!logoutWindowPassed)
                // The logout window has been deleted but we still want to fade out the vignetting, thus
                // render it on the top of everything if still animating. We don't check if logoutWindow
                // is set as it may still be even if it wasn't rendered.
                renderVignetting();
        } else {
            GLRenderTarget::pushRenderTarget(blurTarget);
            blurTarget->blitFromFramebuffer();
            GLRenderTarget::popRenderTarget();

            //--------------------------
            // Render the screen effect
            renderBlurTexture();

            // Vignetting (Radial gradient with transparent middle and black edges)
            renderVignetting();
            //--------------------------

            // Render the logout window
            if (logoutWindow) {
                int winMask = logoutWindow->hasAlpha() ? PAINT_WINDOW_TRANSLUCENT : PAINT_WINDOW_OPAQUE;
                WindowPaintData winData(logoutWindow);
                winData.setOpacity(windowOpacity);
                effects->drawWindow(logoutWindow, winMask, region, winData);
            }

            // Render all windows on top of logout window
            foreach (EffectWindow * w, windows) {
                int winMask = w->hasAlpha() ? PAINT_WINDOW_TRANSLUCENT : PAINT_WINDOW_OPAQUE;
                WindowPaintData winData(w);
                winData.setOpacity(windowsOpacities[ w ]);
                effects->drawWindow(w, winMask, region, winData);
            }

            windows.clear();
            windowsOpacities.clear();
        }
    }
}

void LogoutEffect::postPaintScreen()
{
    if ((progress != 0.0 && progress != 1.0) || frameDelay)
        effects->addRepaintFull();

    if (progress > 0.0)
        logoutWindowPassed = false;
    effects->postPaintScreen();
}

void LogoutEffect::slotWindowAdded(EffectWindow* w)
{
    if (isLogoutDialog(w)) {
        logoutWindow = w;
        logoutWindowClosed = false; // So we don't blur the window on close
        progress = 0.0;
        displayEffect = true;
        ignoredWindows.clear();
        effects->addRepaintFull();
    } else if (canDoPersistent)
        // TODO: Add parent
        ignoredWindows.append(w);
}

void LogoutEffect::slotWindowClosed(EffectWindow* w)
{
    if (w == logoutWindow) {
        logoutWindowClosed = true;
        if (!canDoPersistent)
            displayEffect = false; // Fade back to normal
        effects->addRepaintFull();
    }
}

void LogoutEffect::slotWindowDeleted(EffectWindow* w)
{
    windows.removeAll(w);
    ignoredWindows.removeAll(w);
    if (w == logoutWindow)
        logoutWindow = NULL;
}

bool LogoutEffect::isLogoutDialog(EffectWindow* w)
{
    // TODO there should be probably a better way (window type?)
    if (w->windowClass() == "ksmserver ksmserver"
            && (w->windowRole() == "logoutdialog" || w->windowRole() == "logouteffect")) {
        return true;
    }
    return false;
}

void LogoutEffect::renderVignetting()
{
    if (effects->compositingType() == OpenGL1Compositing) {
        renderVignettingLegacy();
        return;
    }
    if (!m_vignettingShader) {
        m_vignettingShader = ShaderManager::instance()->loadFragmentShader(KWin::ShaderManager::ColorShader,
                                                                           KGlobal::dirs()->findResource("data", "kwin/vignetting.frag"));
        if (!m_vignettingShader->isValid()) {
            kDebug(1212) << "Vignetting Shader failed to load";
            return;
        }
    } else if (!m_vignettingShader->isValid()) {
        // shader broken
        return;
    }
    // need to get the projection matrix from the ortho shader for the vignetting shader
    QMatrix4x4 projection = ShaderManager::instance()->pushShader(KWin::ShaderManager::SimpleShader)->getUniformMatrix4x4("projection");
    ShaderManager::instance()->popShader();

    ShaderBinder binder(m_vignettingShader);
    m_vignettingShader->setUniform(KWin::GLShader::ProjectionMatrix, projection);
    m_vignettingShader->setUniform("u_progress", (float)progress * 0.9f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_SCISSOR_TEST);
    const QRect fullArea = effects->clientArea(FullArea, 0, 0);
    for (int screen = 0; screen < effects->numScreens(); screen++) {
        const QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
        glScissor(screenGeom.x(), displayHeight() - screenGeom.y() - screenGeom.height(),
                  screenGeom.width(), screenGeom.height());  // GL coords are flipped
        const float cenX = screenGeom.x() + screenGeom.width() / 2;
        const float cenY = fullArea.height() - screenGeom.y() - screenGeom.height() / 2;
        const float r = float((screenGeom.width() > screenGeom.height())
                              ? screenGeom.width() : screenGeom.height()) * 0.8f;  // Radius
        m_vignettingShader->setUniform("u_center", QVector2D(cenX, cenY));
        m_vignettingShader->setUniform("u_radius", r);
        QVector<float> vertices;
        vertices << screenGeom.x() << screenGeom.y();
        vertices << screenGeom.x() << screenGeom.y() + screenGeom.height();
        vertices << screenGeom.x() + screenGeom.width() << screenGeom.y();
        vertices << screenGeom.x() + screenGeom.width() << screenGeom.y() + screenGeom.height();
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->setData(vertices.count()/2, 2, vertices.constData(), NULL);
        vbo->render(GL_TRIANGLE_STRIP);
    }
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
}

void LogoutEffect::renderVignettingLegacy()
{
#ifndef KWIN_HAVE_OPENGLES
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
    glEnable(GL_BLEND);   // If not already (Such as when rendered straight to the screen)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int screen = 0; screen < effects->numScreens(); screen++) {
        QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
        glScissor(screenGeom.x(), displayHeight() - screenGeom.y() - screenGeom.height(),
                  screenGeom.width(), screenGeom.height());  // GL coords are flipped
        glEnable(GL_SCISSOR_TEST);   // Geom must be set before enable
        const float cenX = screenGeom.x() + screenGeom.width() / 2;
        const float cenY = screenGeom.y() + screenGeom.height() / 2;
        const float a = M_PI / 16.0f; // Angle of increment
        const float r = float((screenGeom.width() > screenGeom.height())
                              ? screenGeom.width() : screenGeom.height()) * 0.8f;  // Radius
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        glVertex3f(cenX, cenY, 0.0f);
        glColor4f(0.0f, 0.0f, 0.0f, progress * 0.9f);
        for (float i = 0.0f; i <= M_PI * 2.01f; i += a)
            glVertex3f(cenX + r * cos(i), cenY + r * sin(i), 0.0f);
        glEnd();
        glDisable(GL_SCISSOR_TEST);
    }
    glPopAttrib();
#endif
}

void LogoutEffect::renderBlurTexture()
{
    if (effects->compositingType() == OpenGL1Compositing) {
        renderBlurTextureLegacy();
        return;
    }
    if (!m_blurShader) {
        m_blurShader = ShaderManager::instance()->loadFragmentShader(KWin::ShaderManager::SimpleShader,
                                                                     KGlobal::dirs()->findResource("data", "kwin/logout-blur.frag"));
        if (!m_blurShader->isValid()) {
            kDebug(1212) << "Logout blur shader failed to load";
        }
    } else if (!m_blurShader->isValid()) {
        // shader is broken - no need to continue here
        return;
    }
    // Unmodified base image
    ShaderBinder binder(m_blurShader);
    m_blurShader->setUniform(GLShader::Offset, QVector2D(0, 0));
    m_blurShader->setUniform(GLShader::ModulationConstant, QVector4D(1.0, 1.0, 1.0, 1.0));
    m_blurShader->setUniform(GLShader::Saturation, 1.0);
    m_blurShader->setUniform(GLShader::AlphaToOne, 1);
    m_blurShader->setUniform("u_alphaProgress", (float)progress * 0.4f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    blurTexture->bind();
    blurTexture->render(infiniteRegion(), QRect(0, 0, displayWidth(), displayHeight()));
    blurTexture->unbind();
    glDisable(GL_BLEND);
    checkGLError("Render blur texture");
}

void LogoutEffect::renderBlurTextureLegacy()
{
#ifndef KWIN_HAVE_OPENGLES
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
    // Unmodified base image
    blurTexture->bind();
    blurTexture->render(infiniteRegion(), QRect(0, 0, displayWidth(), displayHeight()));

    // Blurred image
    GLfloat bias[1];
    glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, bias);
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 1.75);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, progress * 0.4);

    blurTexture->render(infiniteRegion(), QRect(0, 0, displayWidth(), displayHeight()));

    glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, bias[0]);
    blurTexture->unbind();
    glPopAttrib();
#endif
}

void LogoutEffect::slotPropertyNotify(EffectWindow* w, long a)
{
    if (w || a != logoutAtom)
        return; // Not our atom

    QByteArray byteData = effects->readRootProperty(logoutAtom, logoutAtom, 8);
    if (byteData.length() < 1) {
        // Property was deleted
        displayEffect = false;
        return;
    }

    // We are using a compatible KSMServer therefore only terminate the effect when the
    // atom is deleted, not when the dialog is closed.
    canDoPersistent = true;
    effects->addRepaintFull();
}

bool LogoutEffect::isActive() const
{
    return progress != 0 || logoutWindow;
}

} // namespace
