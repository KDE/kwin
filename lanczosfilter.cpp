/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 by Fredrik Höglund <fredrik@kde.org>
Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>

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

#include "lanczosfilter.h"
#include "effects.h"
#include "options.h"

#include <kwinglutils.h>
#include <kwinglplatform.h>

#include <kwineffects.h>
#include <KDE/KGlobalSettings>

#include <qmath.h>
#include <cmath>

namespace KWin
{

LanczosFilter::LanczosFilter(QObject* parent)
    : QObject(parent)
    , m_offscreenTex(0)
    , m_offscreenTarget(0)
    , m_shader(0)
    , m_inited(false)
{
}

LanczosFilter::~LanczosFilter()
{
    delete m_offscreenTarget;
    delete m_offscreenTex;
}

void LanczosFilter::init()
{
    if (m_inited)
        return;
    m_inited = true;
    const bool force = (qstrcmp(qgetenv("KWIN_FORCE_LANCZOS"), "1") == 0);
    if (force) {
        kWarning(1212) << "Lanczos Filter forced on by environment variable";
    }

    if (!force && options->glSmoothScale() != 2)
        return; // disabled by config

    // The lanczos filter is reported to be broken with the Intel driver and Mesa 7.10
    GLPlatform *gl = GLPlatform::instance();
    if (!force && gl->driver() == Driver_Intel && gl->mesaVersion() >= kVersionNumber(7, 10) && gl->chipClass() < SandyBridge)
        return;
    // With fglrx the ARB Shader crashes KWin (see Bug #270818 and #286795) and GLSL Shaders are not functional
    if (!force && gl->driver() == Driver_Catalyst) {
        return;
    }

    m_shader = new LanczosShader(this);
    if (!m_shader->init()) {
        delete m_shader;
        m_shader = 0;
    }
}


void LanczosFilter::updateOffscreenSurfaces()
{
    int w = displayWidth();
    int h = displayHeight();
    if (!GLTexture::NPOTTextureSupported()) {
        w = nearestPowerOfTwo(w);
        h = nearestPowerOfTwo(h);
    }
    if (!m_offscreenTex || m_offscreenTex->width() != w || m_offscreenTex->height() != h) {
        if (m_offscreenTex) {
            delete m_offscreenTex;
            delete m_offscreenTarget;
        }
        m_offscreenTex = new GLTexture(w, h);
        m_offscreenTex->setFilter(GL_LINEAR);
        m_offscreenTex->setWrapMode(GL_CLAMP_TO_EDGE);
        m_offscreenTarget = new GLRenderTarget(*m_offscreenTex);
    }
}

static float sinc(float x)
{
    return std::sin(x * M_PI) / (x * M_PI);
}

static float lanczos(float x, float a)
{
    if (qFuzzyCompare(x + 1.0, 1.0))
        return 1.0;

    if (qAbs(x) >= a)
        return 0.0;

    return sinc(x) * sinc(x / a);
}

void LanczosShader::createKernel(float delta, int *size)
{
    const float a = 2.0;

    // The two outermost samples always fall at points where the lanczos
    // function returns 0, so we'll skip them.
    const int sampleCount = qBound(3, qCeil(delta * a) * 2 + 1 - 2, 29);
    const int center = sampleCount / 2;
    const int kernelSize = center + 1;
    const float factor = 1.0 / delta;

    QVector<float> values(kernelSize);
    float sum = 0;

    for (int i = 0; i < kernelSize; i++) {
        const float val = lanczos(i * factor, a);
        sum += i > 0 ? val * 2 : val;
        values[i] = val;
    }

    memset(m_kernel, 0, 16 * sizeof(QVector4D));

    // Normalize the kernel
    for (int i = 0; i < kernelSize; i++) {
        const float val = values[i] / sum;
        m_kernel[i] = QVector4D(val, val, val, val);
    }

    *size = kernelSize;
}

void LanczosShader::createOffsets(int count, float width, Qt::Orientation direction)
{
    memset(m_offsets, 0, 16 * sizeof(QVector2D));
    for (int i = 0; i < count; i++) {
        m_offsets[i] = (direction == Qt::Horizontal) ?
                       QVector2D(i / width, 0) : QVector2D(0, i / width);
    }
}

void LanczosFilter::performPaint(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data)
{
    if (effects->compositingType() == KWin::OpenGLCompositing && (data.xScale() < 0.9 || data.yScale() < 0.9) &&
            KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects) {
        if (!m_inited)
            init();
        const QRect screenRect = Workspace::self()->clientArea(ScreenArea, w->screen(), w->desktop());
        // window geometry may not be bigger than screen geometry to fit into the FBO
        if (m_shader && w->width() <= screenRect.width() && w->height() <= screenRect.height()) {
            double left = 0;
            double top = 0;
            double right = w->width();
            double bottom = w->height();
            foreach (const WindowQuad & quad, data.quads) {
                // we need this loop to include the decoration padding
                left   = qMin(left, quad.left());
                top    = qMin(top, quad.top());
                right  = qMax(right, quad.right());
                bottom = qMax(bottom, quad.bottom());
            }
            double width = right - left;
            double height = bottom - top;
            if (width > screenRect.width() || height > screenRect.height()) {
                // window with padding does not fit into the framebuffer
                // so cut of the shadow
                left = 0;
                top = 0;
                width = w->width();
                height = w->height();
            }
            int tx = data.xTranslation() + w->x() + left * data.xScale();
            int ty = data.yTranslation() + w->y() + top * data.yScale();
            int tw = width * data.xScale();
            int th = height * data.yScale();
            const QRect textureRect(tx, ty, tw, th);
            const bool hardwareClipping = !(QRegion(textureRect)-region).isEmpty();

            int sw = width;
            int sh = height;

            GLTexture *cachedTexture = static_cast< GLTexture*>(w->data(LanczosCacheRole).value<void*>());
            if (cachedTexture) {
                if (cachedTexture->width() == tw && cachedTexture->height() == th) {
                    cachedTexture->bind();
                    if (hardwareClipping) {
                        glEnable(GL_SCISSOR_TEST);
                    }
                    if (ShaderManager::instance()->isValid()) {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                        const qreal rgb = data.brightness() * data.opacity();
                        const qreal a = data.opacity();

                        GLShader *shader = ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
                        shader->setUniform(GLShader::Offset, QVector2D(0, 0));
                        shader->setUniform(GLShader::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
                        shader->setUniform(GLShader::Saturation, data.saturation());
                        shader->setUniform(GLShader::AlphaToOne, 0);

                        cachedTexture->render(region, textureRect, hardwareClipping);

                        ShaderManager::instance()->popShader();
                        glDisable(GL_BLEND);
                    } else {
                        prepareRenderStates(cachedTexture, data.opacity(), data.brightness(), data.saturation());
                        cachedTexture->render(region, textureRect, hardwareClipping);
                        restoreRenderStates(cachedTexture, data.opacity(), data.brightness(), data.saturation());
                    }
                    if (hardwareClipping) {
                        glDisable(GL_SCISSOR_TEST);
                    }
                    cachedTexture->unbind();
                    m_timer.start(5000, this);
                    return;
                } else {
                    // offscreen texture not matching - delete
                    delete cachedTexture;
                    cachedTexture = 0;
                    w->setData(LanczosCacheRole, QVariant());
                }
            }

            WindowPaintData thumbData = data;
            thumbData.setXScale(1.0);
            thumbData.setYScale(1.0);
            thumbData.setXTranslation(-w->x() - left);
            thumbData.setYTranslation(-w->y() - top);
            thumbData.setBrightness(1.0);
            thumbData.setOpacity(1.0);
            thumbData.setSaturation(1.0);

            // Bind the offscreen FBO and draw the window on it unscaled
            updateOffscreenSurfaces();
            GLRenderTarget::pushRenderTarget(m_offscreenTarget);

            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);
            w->sceneWindow()->performPaint(mask, infiniteRegion(), thumbData);

            // Create a scratch texture and copy the rendered window into it
            GLTexture tex(sw, sh);
            tex.setFilter(GL_LINEAR);
            tex.setWrapMode(GL_CLAMP_TO_EDGE);
            tex.bind();

            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, m_offscreenTex->height() - sh, sw, sh);

            // Set up the shader for horizontal scaling
            float dx = sw / float(tw);
            int kernelSize;
            m_shader->createKernel(dx, &kernelSize);
            m_shader->createOffsets(kernelSize, sw, Qt::Horizontal);

            m_shader->bind();
            m_shader->setUniforms();

            // Draw the window back into the FBO, this time scaled horizontally
            glClear(GL_COLOR_BUFFER_BIT);
            QVector<float> verts;
            QVector<float> texCoords;
            verts.reserve(12);
            texCoords.reserve(12);

            texCoords << 1.0 << 0.0; verts << tw  << 0.0; // Top right
            texCoords << 0.0 << 0.0; verts << 0.0 << 0.0; // Top left
            texCoords << 0.0 << 1.0; verts << 0.0 << sh;  // Bottom left
            texCoords << 0.0 << 1.0; verts << 0.0 << sh;  // Bottom left
            texCoords << 1.0 << 1.0; verts << tw  << sh;  // Bottom right
            texCoords << 1.0 << 0.0; verts << tw  << 0.0; // Top right
            GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
            vbo->reset();
            vbo->setData(6, 2, verts.constData(), texCoords.constData());
            vbo->render(GL_TRIANGLES);

            // At this point we don't need the scratch texture anymore
            tex.unbind();
            tex.discard();

            // create scratch texture for second rendering pass
            GLTexture tex2(tw, sh);
            tex2.setFilter(GL_LINEAR);
            tex2.setWrapMode(GL_CLAMP_TO_EDGE);
            tex2.bind();

            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, m_offscreenTex->height() - sh, tw, sh);

            // Set up the shader for vertical scaling
            float dy = sh / float(th);
            m_shader->createKernel(dy, &kernelSize);
            m_shader->createOffsets(kernelSize, m_offscreenTex->height(), Qt::Vertical);
            m_shader->setUniforms();

            // Now draw the horizontally scaled window in the FBO at the right
            // coordinates on the screen, while scaling it vertically and blending it.
            glClear(GL_COLOR_BUFFER_BIT);

            verts.clear();

            verts << tw  << 0.0; // Top right
            verts << 0.0 << 0.0; // Top left
            verts << 0.0 << th;  // Bottom left
            verts << 0.0 << th;  // Bottom left
            verts << tw  << th;  // Bottom right
            verts << tw  << 0.0; // Top right
            vbo->setData(6, 2, verts.constData(), texCoords.constData());
            vbo->render(GL_TRIANGLES);

            tex2.unbind();
            tex2.discard();
            m_shader->unbind();

            // create cache texture
            GLTexture *cache = new GLTexture(tw, th);

            cache->setFilter(GL_LINEAR);
            cache->setWrapMode(GL_CLAMP_TO_EDGE);
            cache->bind();
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, m_offscreenTex->height() - th, tw, th);
            GLRenderTarget::popRenderTarget();

            if (hardwareClipping) {
                glEnable(GL_SCISSOR_TEST);
            }
            if (ShaderManager::instance()->isValid()) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                const qreal rgb = data.brightness() * data.opacity();
                const qreal a = data.opacity();

                GLShader *shader = ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
                shader->setUniform(GLShader::Offset, QVector2D(0, 0));
                shader->setUniform(GLShader::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
                shader->setUniform(GLShader::Saturation, data.saturation());
                shader->setUniform(GLShader::AlphaToOne, 0);

                cache->render(region, textureRect, hardwareClipping);

                ShaderManager::instance()->popShader();
                glDisable(GL_BLEND);
            } else {
                prepareRenderStates(cache, data.opacity(), data.brightness(), data.saturation());
                cache->render(region, textureRect, hardwareClipping);
                restoreRenderStates(cache, data.opacity(), data.brightness(), data.saturation());
            }
            if (hardwareClipping) {
                glDisable(GL_SCISSOR_TEST);
            }

            cache->unbind();
            w->setData(LanczosCacheRole, QVariant::fromValue(static_cast<void*>(cache)));

            // Delete the offscreen surface after 5 seconds
            m_timer.start(5000, this);
            return;
        }
    } // if ( effects->compositingType() == KWin::OpenGLCompositing )
    w->sceneWindow()->performPaint(mask, region, data);
} // End of function

void LanczosFilter::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timer.timerId()) {
        m_timer.stop();

        delete m_offscreenTarget;
        delete m_offscreenTex;
        m_offscreenTarget = 0;
        m_offscreenTex = 0;
        foreach (EffectWindow * w, effects->stackingOrder()) {
            QVariant cachedTextureVariant = w->data(LanczosCacheRole);
            if (cachedTextureVariant.isValid()) {
                GLTexture *cachedTexture = static_cast< GLTexture*>(cachedTextureVariant.value<void*>());
                delete cachedTexture;
                cachedTexture = 0;
                w->setData(LanczosCacheRole, QVariant());
            }
        }
    }
}

void LanczosFilter::prepareRenderStates(GLTexture* tex, double opacity, double brightness, double saturation)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(tex)
    Q_UNUSED(opacity)
    Q_UNUSED(brightness)
    Q_UNUSED(saturation)
#else
    const bool alpha = true;
    // setup blending of transparent windows
    glPushAttrib(GL_ENABLE_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (saturation != 1.0 && tex->saturationSupported()) {
        // First we need to get the color from [0; 1] range to [0.5; 1] range
        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
        const float scale_constant[] = { 1.0, 1.0, 1.0, 0.5};
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, scale_constant);
        tex->bind();

        // Then we take dot product of the result of previous pass and
        //  saturation_constant. This gives us completely unsaturated
        //  (greyscale) image
        // Note that both operands have to be in range [0.5; 1] since opengl
        //  automatically substracts 0.5 from them
        glActiveTexture(GL_TEXTURE1);
        float saturation_constant[] = { 0.5 + 0.5 * 0.30, 0.5 + 0.5 * 0.59, 0.5 + 0.5 * 0.11, saturation };
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant);
        tex->bind();

        // Finally we need to interpolate between the original image and the
        //  greyscale image to get wanted level of saturation
        glActiveTexture(GL_TEXTURE2);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant);
        // Also replace alpha by primary color's alpha here
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        // And make primary color contain the wanted opacity
        glColor4f(opacity, opacity, opacity, opacity);
        tex->bind();

        if (alpha || brightness != 1.0f) {
            glActiveTexture(GL_TEXTURE3);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            // The color has to be multiplied by both opacity and brightness
            float opacityByBrightness = opacity * brightness;
            glColor4f(opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity);
            if (alpha) {
                // Multiply original texture's alpha by our opacity
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
            } else {
                // Alpha will be taken from previous stage
                glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
            }
            tex->bind();
        }

        glActiveTexture(GL_TEXTURE0);
    } else if (opacity != 1.0 || brightness != 1.0) {
        // the window is additionally configured to have its opacity adjusted,
        // do it
        float opacityByBrightness = opacity * brightness;
        if (alpha) {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(opacityByBrightness, opacityByBrightness, opacityByBrightness,
                      opacity);
        } else {
            // Multiply color by brightness and replace alpha by opacity
            float constant[] = { opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity };
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT);
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);
        }
    }
#endif
}

void LanczosFilter::restoreRenderStates(GLTexture* tex, double opacity, double brightness, double saturation)
{
#ifdef KWIN_HAVE_OPENGLES
    Q_UNUSED(tex)
    Q_UNUSED(opacity)
    Q_UNUSED(brightness)
    Q_UNUSED(saturation)
#else
    if (opacity != 1.0 || saturation != 1.0 || brightness != 1.0f) {
        if (saturation != 1.0 && tex->saturationSupported()) {
            glActiveTexture(GL_TEXTURE3);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE2);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE1);
            glDisable(tex->target());
            glActiveTexture(GL_TEXTURE0);
        }
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(0, 0, 0, 0);

    glPopAttrib();  // ENABLE_BIT
#endif
}

/************************************************
* LanczosShader
************************************************/
LanczosShader::LanczosShader(QObject* parent)
    : QObject(parent)
    , m_shader(0)
    , m_uTexUnit(0)
    , m_uOffsets(0)
    , m_uKernel(0)
    , m_arbProgram(0)
{
}

LanczosShader::~LanczosShader()
{
    delete m_shader;
#ifndef KWIN_HAVE_OPENGLES
    if (m_arbProgram) {
        glDeleteProgramsARB(1, &m_arbProgram);
        m_arbProgram = 0;
    }
#endif
}

void LanczosShader::bind()
{
    if (m_shader)
        ShaderManager::instance()->pushShader(m_shader);
#ifndef KWIN_HAVE_OPENGLES
    else {
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_arbProgram);
    }
#endif
}

void LanczosShader::unbind()
{
    if (m_shader)
        ShaderManager::instance()->popShader();
#ifndef KWIN_HAVE_OPENGLES
    else {
        int boundObject;
        glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &boundObject);
        if (boundObject == (int)m_arbProgram) {
            glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
            glDisable(GL_FRAGMENT_PROGRAM_ARB);
        }
    }
#endif
}

void LanczosShader::setUniforms()
{
    if (m_shader) {
        glUniform1i(m_uTexUnit, 0);
        glUniform2fv(m_uOffsets, 16, (const GLfloat*)m_offsets);
        glUniform4fv(m_uKernel, 16, (const GLfloat*)m_kernel);
    }
#ifndef KWIN_HAVE_OPENGLES
    else {
        for (int i = 0; i < 16; ++i) {
            glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, i, m_offsets[i].x(), m_offsets[i].y(), 0, 0);
        }
        for (int i = 0; i < 16; ++i) {
            glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, i + 16, m_kernel[i].x(), m_kernel[i].y(), m_kernel[i].z(), m_kernel[i].w());
        }
    }
#endif
}

bool LanczosShader::init()
{
    GLPlatform *gl = GLPlatform::instance();
    if (gl->supports(GLSL) &&
            ShaderManager::instance()->isValid() &&
            GLRenderTarget::supported() &&
            !(gl->isRadeon() && gl->chipClass() < R600)) {
        m_shader = ShaderManager::instance()->loadFragmentShader(ShaderManager::SimpleShader, ":/resources/lanczos-fragment.glsl");
        if (m_shader->isValid()) {
            ShaderManager::instance()->pushShader(m_shader);
            m_uTexUnit    = m_shader->uniformLocation("texUnit");
            m_uKernel     = m_shader->uniformLocation("kernel");
            m_uOffsets    = m_shader->uniformLocation("offsets");
            ShaderManager::instance()->popShader();
            return true;
        } else {
            kDebug(1212) << "Shader is not valid";
            m_shader = 0;
            // try ARB shader
        }
    }

#ifdef KWIN_HAVE_OPENGLES
    // no ARB shader in GLES
    return false;
#else
    // try to create an ARB Shader
    if (!hasGLExtension("GL_ARB_fragment_program"))
        return false;

    // We allow Lanczos for SandyBridge or later, but only GLSL shaders are supported, see BUG 301729
    if (gl->isIntel())
        return false;

    QByteArray text;
    QTextStream stream(&text);

    // Note: This program uses 31 temporaries, 61 ALU instructions, 31 texture
    //       fetches, 3 texture indirections and 93 instructions.
    //       The R300 limitations are 32, 64, 32, 4 and 96 respectively.
    stream << "!!ARBfp1.0\n";
    stream << "TEMP sum;\n";

    // Declare 30 temporaries for holding texcoords and TEX results
    for (int i = 0; i < 30; i++)
        stream << "TEMP temp" << i << ";\n";

    // Compute the texture coordinates
    for (int i = 0, j = 0; i < 30 / 2; i++) {
        stream << "ADD temp" << j++ << ", fragment.texcoord, program.local[" << i + 1 << "];\n";
        stream << "SUB temp" << j++ << ", fragment.texcoord, program.local[" << i + 1 << "];\n";
    }

    // Sample the texture coordinates
    stream << "TEX sum, fragment.texcoord, texture[0], 2D;\n";
    for (int i = 0; i < 30; i++)
        stream << "TEX temp" << i << ", temp" << i << ", texture[0], 2D;\n";

    // Process the results
    stream << "MUL sum, sum, program.local[16];\n"; // sum = sum * kernel[0]
    for (int i = 0, j = 0; i < 30 / 2; i++) {
        stream << "MAD sum, temp" << j++ << ", program.local[" << (17 + i) << "], sum;\n";
        stream << "MAD sum, temp" << j++ << ", program.local[" << (17 + i) << "], sum;\n";
    }

    stream << "MOV result.color, sum;\n";
    stream << "END\n";
    stream.flush();

    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glGenProgramsARB(1, &m_arbProgram);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_arbProgram);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, text.length(), text.constData());

    if (glGetError()) {
        const char *error = (const char*)glGetString(GL_PROGRAM_ERROR_STRING_ARB);
        kError() << "Failed to compile fragment program:" << error;
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
        glDeleteProgramsARB(1, &m_arbProgram);
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
        m_arbProgram = 0;
        return false;
    }

    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    kDebug(1212) << "ARB Shader compiled, id: " << m_arbProgram;
    return true;
#endif
}

} // namespace

