/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2011 Philipp Knechtges <philipp-dev@knechtges.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  if not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "blur.h"
#include "blurshader.h"

#include <X11/Xatom.h>

#include <QMatrix4x4>
#include <QLinkedList>
#include <KConfigGroup>
#include <KDebug>

namespace KWin
{

KWIN_EFFECT(blur, BlurEffect)
KWIN_EFFECT_SUPPORTED(blur, BlurEffect::supported())
KWIN_EFFECT_ENABLEDBYDEFAULT(blur, BlurEffect::enabledByDefault())

BlurEffect::BlurEffect()
{
    shader = BlurShader::create();

    // Offscreen texture that's used as the target for the horizontal blur pass
    // and the source for the vertical pass.
    tex = GLTexture(displayWidth(), displayHeight());
    tex.setFilter(GL_LINEAR);
    tex.setWrapMode(GL_CLAMP_TO_EDGE);

    target = new GLRenderTarget(tex);

    net_wm_blur_region = XInternAtom(display(), "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
    effects->registerPropertyType(net_wm_blur_region, true);

    reconfigure(ReconfigureAll);

    // ### Hackish way to announce support.
    //     Should be included in _NET_SUPPORTED instead.
    if (shader->isValid() && target->valid()) {
        XChangeProperty(display(), rootWindow(), net_wm_blur_region, net_wm_blur_region,
                        32, PropModeReplace, 0, 0);
    } else {
        XDeleteProperty(display(), rootWindow(), net_wm_blur_region);
    }
    connect(effects, SIGNAL(windowAdded(EffectWindow*)), this, SLOT(slotWindowAdded(EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(EffectWindow*)), this, SLOT(slotWindowDeleted(EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(EffectWindow*,long)), this, SLOT(slotPropertyNotify(EffectWindow*,long)));
}

BlurEffect::~BlurEffect()
{
    effects->registerPropertyType(net_wm_blur_region, false);
    XDeleteProperty(display(), rootWindow(), net_wm_blur_region);

    windows.clear();

    delete shader;
    delete target;
}

void BlurEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    KConfigGroup cg = EffectsHandler::effectConfig("Blur");
    int radius = qBound(2, cg.readEntry("BlurRadius", 12), 14);
    shader->setRadius(radius);

    m_shouldCache = cg.readEntry("CacheTexture", true);

    windows.clear();

    if (!shader->isValid())
        XDeleteProperty(display(), rootWindow(), net_wm_blur_region);
}

void BlurEffect::updateBlurRegion(EffectWindow *w) const
{
    QRegion region;

    const QByteArray value = w->readProperty(net_wm_blur_region, XA_CARDINAL, 32);
    if (value.size() > 0 && !(value.size() % (4 * sizeof(unsigned long)))) {
        const unsigned long *cardinals = reinterpret_cast<const unsigned long*>(value.constData());
        for (unsigned int i = 0; i < value.size() / sizeof(unsigned long);) {
            int x = cardinals[i++];
            int y = cardinals[i++];
            int w = cardinals[i++];
            int h = cardinals[i++];
            region += QRect(x, y, w, h);
        }
    }

    if (region.isEmpty() && !value.isNull()) {
        // Set the data to a dummy value.
        // This is needed to be able to distinguish between the value not
        // being set, and being set to an empty region.
        w->setData(WindowBlurBehindRole, 1);
    } else
        w->setData(WindowBlurBehindRole, region);
}

void BlurEffect::slotWindowAdded(EffectWindow *w)
{
    updateBlurRegion(w);
}

void BlurEffect::slotWindowDeleted(EffectWindow *w)
{
    if (windows.contains(w)) {
        windows.remove(w);
    }
}

void BlurEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == net_wm_blur_region) {
        updateBlurRegion(w);
        if (windows.contains(w)) {
            const QRect screen(0, 0, displayWidth(), displayHeight());
            windows[w].damagedRegion = expand(blurRegion(w).translated(w->pos())) & screen;
        }
    }
}

bool BlurEffect::enabledByDefault()
{
    GLPlatform *gl = GLPlatform::instance();

    if (gl->isIntel())
        return false;

    return true;
}

bool BlurEffect::supported()
{
    bool supported = GLRenderTarget::supported() && GLTexture::NPOTTextureSupported() &&
                     (GLSLBlurShader::supported() || ARBBlurShader::supported());

    if (supported) {
        int maxTexSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

        if (displayWidth() > maxTexSize || displayHeight() > maxTexSize)
            supported = false;
    }
    return supported;
}

QRect BlurEffect::expand(const QRect &rect) const
{
    const int radius = shader->radius();
    return rect.adjusted(-radius, -radius, radius, radius);
}

QRegion BlurEffect::expand(const QRegion &region) const
{
    QRegion expanded;

    foreach (const QRect & rect, region.rects()) {
        expanded += expand(rect);
    }

    return expanded;
}

QRegion BlurEffect::blurRegion(const EffectWindow *w) const
{
    QRegion region;

    const QVariant value = w->data(WindowBlurBehindRole);
    if (value.isValid()) {
        const QRegion appRegion = qvariant_cast<QRegion>(value);
        if (!appRegion.isEmpty()) {
            if (w->hasDecoration() && effects->decorationSupportsBlurBehind()) {
                region = w->shape();
                region -= w->decorationInnerRect();
            }
            region |= appRegion.translated(w->contentsRect().topLeft()) &
                      w->decorationInnerRect();
        } else {
            // An empty region means that the blur effect should be enabled
            // for the whole window.
            region = w->shape();
        }
    } else if (w->hasDecoration() && effects->decorationSupportsBlurBehind()) {
        // If the client hasn't specified a blur region, we'll only enable
        // the effect behind the decoration.
        region = w->shape();
        region -= w->decorationInnerRect();
    }

    return region;
}

void BlurEffect::drawRegion(const QRegion &region)
{
    const int vertexCount = region.rectCount() * 6;
    if (vertices.size() < vertexCount)
        vertices.resize(vertexCount);

    int i = 0;
    foreach (const QRect & r, region.rects()) {
        vertices[i++] = QVector2D(r.x() + r.width(), r.y());
        vertices[i++] = QVector2D(r.x(),             r.y());
        vertices[i++] = QVector2D(r.x(),             r.y() + r.height());
        vertices[i++] = QVector2D(r.x(),             r.y() + r.height());
        vertices[i++] = QVector2D(r.x() + r.width(), r.y() + r.height());
        vertices[i++] = QVector2D(r.x() + r.width(), r.y());
    }
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setData(vertexCount, 2, (float*)vertices.constData(), (float*)vertices.constData());
    vbo->render(GL_TRIANGLES);
}

void BlurEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    m_damagedArea = QRegion();
    m_paintedArea = QRegion();
    m_currentBlur = QRegion();

    effects->prePaintScreen(data, time);
}

void BlurEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    // this effect relies on prePaintWindow being called in the bottom to top order

    effects->prePaintWindow(w, data, time);

    if (!w->isPaintingEnabled()) {
        return;
    }

    // to blur an area partially we have to shrink the opaque area of a window
    QRegion newClip;
    const QRegion oldClip = data.clip;
    const int radius = shader->radius();
    foreach (const QRect& rect, data.clip.rects()) {
        newClip |= rect.adjusted(radius,radius,-radius,-radius);
    }
    data.clip = newClip;

    const QRegion oldPaint = data.paint;

    // we don't have to blur a region we don't see
    m_currentBlur -= newClip;
    // if we have to paint a non-opaque part of this window that intersects with the
    // currently blurred region (which is not cached) we have to redraw the whole region
    if ((data.paint-oldClip).intersects(m_currentBlur)) {
        data.paint |= m_currentBlur;
    }

    // in case this window has regions to be blurred
    const QRegion blurArea = blurRegion(w).translated(w->pos());
    const QRegion expandedBlur = expand(blurArea);

    if (m_shouldCache) {
        // we are caching the horizontally blurred background texture

        // if a window underneath the blurred area is damaged we have to
        // update the cached texture
        QRegion damagedCache;
        if (windows.contains(w) && !windows[w].dropCache) {
            damagedCache = expand(expandedBlur & m_damagedArea) & expandedBlur;
        } else {
            damagedCache = expandedBlur;
        }
        if (!damagedCache.isEmpty()) {
            // This is the area of the blurry window which really can change.
            const QRegion damagedArea = damagedCache & blurArea;
            // In order to be able to recalculate this area we have to make sure the
            // background area is painted before.
            data.paint |= expand(damagedArea);
            if (windows.contains(w)) {
                // In case we already have a texture cache mark the dirty regions invalid.
                windows[w].damagedRegion |= damagedCache;
                windows[w].dropCache = false;
            }
            // we keep track of the "damage propagation"
            m_damagedArea |= damagedArea;
            // we have to check again whether we do not damage a blurred area
            // of a window we do not cache
            if (expandedBlur.intersects(m_currentBlur)) {
                data.paint |= m_currentBlur;
            }
        }
    } else {
        // we are not caching the window

        // if this window or an window underneath the blurred area is painted again we have to
        // blur everything
        if (m_paintedArea.intersects(expandedBlur) || data.paint.intersects(blurArea)) {
            data.paint |= expandedBlur;
            // we keep track of the "damage propagation"
            m_damagedArea |= expand(expandedBlur & m_damagedArea) & blurArea;
            // we have to check again whether we do not damage a blurred area
            // of a window we do not cache
            if (expandedBlur.intersects(m_currentBlur)) {
                data.paint |= m_currentBlur;
            }
        }

        m_currentBlur |= expandedBlur;
    }

    // we don't consider damaged areas which are occluded and are not
    // explicitly damaged by this window
    m_damagedArea -= data.clip;
    m_damagedArea |= oldPaint;

    // in contrast to m_damagedArea does m_paintedArea keep track of all repainted areas
    m_paintedArea -= data.clip;
    m_paintedArea |= data.paint;
}

bool BlurEffect::shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    if (!target->valid() || !shader->isValid())
        return false;

    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool())
        return false;

    if (w->isDesktop())
        return false;

    bool scaled = !qFuzzyCompare(data.xScale, 1.0) && !qFuzzyCompare(data.yScale, 1.0);
    bool translated = data.xTranslate || data.yTranslate;

    if (scaled || translated || (mask & PAINT_WINDOW_TRANSFORMED))
        return false;

    bool blurBehindDecos = effects->decorationsHaveAlpha() &&
                effects->decorationSupportsBlurBehind();

    if (!w->hasAlpha() && !(blurBehindDecos && w->hasDecoration()))
        return false;

    return true;
}

void BlurEffect::drawWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    if (shouldBlur(w, mask, data)) {
        const QRegion shape = region & blurRegion(w).translated(w->pos()) & screen;

        if (!shape.isEmpty()) {
            if (m_shouldCache) {
                doCachedBlur(w, region, data.opacity * data.contents_opacity);
            } else {
                doBlur(shape, screen, data.opacity * data.contents_opacity);
            }
        }
    }

    // Draw the window over the blurred area
    effects->drawWindow(w, mask, region, data);
}

void BlurEffect::paintEffectFrame(EffectFrame *frame, QRegion region, double opacity, double frameOpacity)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    bool valid = target->valid() && shader->isValid();
    QRegion shape = frame->geometry().adjusted(-5, -5, 5, 5) & screen;
    if (valid && !shape.isEmpty() && region.intersects(shape.boundingRect()) && frame->style() != EffectFrameNone) {
        doBlur(shape, screen, opacity * frameOpacity);
    }
    effects->paintEffectFrame(frame, region, opacity, frameOpacity);
}

void BlurEffect::doBlur(const QRegion& shape, const QRect& screen, const float opacity)
{
    const QRegion expanded = expand(shape) & screen;
    const QRect r = expanded.boundingRect();

    // Create a scratch texture and copy the area in the back buffer that we're
    // going to blur into it
    GLTexture scratch(r.width(), r.height());
    scratch.setFilter(GL_LINEAR);
    scratch.setWrapMode(GL_CLAMP_TO_EDGE);
    scratch.bind();

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r.x(), displayHeight() - r.y() - r.height(),
                        r.width(), r.height());

    // Draw the texture on the offscreen framebuffer object, while blurring it horizontally
    target->attachTexture(tex);
    GLRenderTarget::pushRenderTarget(target);

    shader->bind();
    shader->setDirection(Qt::Horizontal);
    shader->setPixelDistance(1.0 / r.width());

    // Set up the texture matrix to transform from screen coordinates
    // to texture coordinates.
#ifndef KWIN_HAVE_OPENGLES
    glMatrixMode(GL_TEXTURE);
#endif
    pushMatrix();
    QMatrix4x4 textureMatrix;
    textureMatrix.scale(1.0 / scratch.width(), -1.0 / scratch.height(), 1);
    textureMatrix.translate(-r.x(), -scratch.height() - r.y(), 0);
    loadMatrix(textureMatrix);
    shader->setTextureMatrix(textureMatrix);

    drawRegion(expanded);

    GLRenderTarget::popRenderTarget();
    scratch.unbind();
    scratch.discard();

    // Now draw the horizontally blurred area back to the backbuffer, while
    // blurring it vertically and clipping it to the window shape.
    tex.bind();

    shader->setDirection(Qt::Vertical);
    shader->setPixelDistance(1.0 / tex.height());

    // Modulate the blurred texture with the window opacity if the window isn't opaque
    if (opacity < 1.0) {
#ifndef KWIN_HAVE_OPENGLES
        glPushAttrib(GL_COLOR_BUFFER_BIT);
#endif
        glEnable(GL_BLEND);
        glBlendColor(0, 0, 0, opacity);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    }

    // Set the up the texture matrix to transform from screen coordinates
    // to texture coordinates.
    textureMatrix.setToIdentity();
    textureMatrix.scale(1.0 / tex.width(), -1.0 / tex.height(), 1);
    textureMatrix.translate(0, -tex.height(), 0);
    loadMatrix(textureMatrix);
    shader->setTextureMatrix(textureMatrix);

    drawRegion(shape);

    popMatrix();
#ifndef KWIN_HAVE_OPENGLES
    glMatrixMode(GL_MODELVIEW);
#endif

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
#ifndef KWIN_HAVE_OPENGLES
        glPopAttrib();
#endif
    }

    tex.unbind();
    shader->unbind();
}

void BlurEffect::doCachedBlur(EffectWindow *w, const QRegion& region, const float opacity)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    const QRegion blurredRegion = blurRegion(w).translated(w->pos()) & screen;
    const QRegion expanded = expand(blurredRegion) & screen;
    const QRect r = expanded.boundingRect();

    // The background texture we get is only partially valid.

    if (!windows.contains(w)) {
        BlurWindowInfo bwi;
        bwi.blurredBackground = GLTexture(r.width(),r.height());
        bwi.damagedRegion = expanded;
        bwi.dropCache = false;
        windows[w] = bwi;
    }

    if (windows[w].blurredBackground.size() != r.size()) {
        windows[w].blurredBackground = GLTexture(r.width(),r.height());
        windows[w].damagedRegion = expanded;
        windows[w].dropCache = false;
    }

    GLTexture targetTexture = windows[w].blurredBackground;
    targetTexture.setFilter(GL_LINEAR);
    targetTexture.setWrapMode(GL_CLAMP_TO_EDGE);
    shader->bind();
    QMatrix4x4 textureMatrix;
    QMatrix4x4 modelViewProjectionMatrix;
#ifndef KWIN_HAVE_OPENGLES
    glMatrixMode(GL_MODELVIEW);
    pushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    pushMatrix();
    glMatrixMode(GL_PROJECTION);
    pushMatrix();
#endif

    // We only update that part of the background texture that is visible and marked as dirty.
    const QRegion updateBackground = windows[w].damagedRegion & region;

    if (!updateBackground.isEmpty()) {
        const QRect updateRect = (expand(updateBackground) & expanded).boundingRect();
        // Create a scratch texture and copy the area in the back buffer that we're
        // going to blur into it
        GLTexture scratch(updateRect.width(), updateRect.height());
        scratch.setFilter(GL_LINEAR);
        scratch.setWrapMode(GL_CLAMP_TO_EDGE);
        scratch.bind();

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, updateRect.x(), displayHeight() - updateRect.y() - updateRect.height(),
                            updateRect.width(), updateRect.height());

        // Draw the texture on the offscreen framebuffer object, while blurring it horizontally
        target->attachTexture(targetTexture);
        GLRenderTarget::pushRenderTarget(target);

        shader->setDirection(Qt::Horizontal);
        shader->setPixelDistance(1.0 / updateRect.width());

        modelViewProjectionMatrix.ortho(0, r.width(), r.height(), 0 , 0, 65535);
        modelViewProjectionMatrix.translate(-r.x(), -r.y(), 0);
        loadMatrix(modelViewProjectionMatrix);
        shader->setModelViewProjectionMatrix(modelViewProjectionMatrix);

        // Set up the texture matrix to transform from screen coordinates
        // to texture coordinates.
        textureMatrix.scale(1.0 / scratch.width(), -1.0 / scratch.height(), 1);
        textureMatrix.translate(-updateRect.x(), -scratch.height() - updateRect.y(), 0);
#ifndef KWIN_HAVE_OPENGLES
        glMatrixMode(GL_TEXTURE);
        loadMatrix(textureMatrix);
        glMatrixMode(GL_PROJECTION);
#endif
        shader->setTextureMatrix(textureMatrix);

        drawRegion(updateBackground & screen);

        GLRenderTarget::popRenderTarget();
        scratch.unbind();
        // mark the updated region as valid
        windows[w].damagedRegion -= updateBackground;
    }

    // Now draw the horizontally blurred area back to the backbuffer, while
    // blurring it vertically and clipping it to the window shape.
    targetTexture.bind();

    shader->setDirection(Qt::Vertical);
    shader->setPixelDistance(1.0 / targetTexture.height());

    // Modulate the blurred texture with the window opacity if the window isn't opaque
    if (opacity < 1.0) {
#ifndef KWIN_HAVE_OPENGLES
        glPushAttrib(GL_COLOR_BUFFER_BIT);
#endif
        glEnable(GL_BLEND);
        glBlendColor(0, 0, 0, opacity);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    }

    modelViewProjectionMatrix.setToIdentity();
    modelViewProjectionMatrix.ortho(0, displayWidth(), displayHeight(), 0, 0, 65535);
    loadMatrix(modelViewProjectionMatrix);
    shader->setModelViewProjectionMatrix(modelViewProjectionMatrix);

    // Set the up the texture matrix to transform from screen coordinates
    // to texture coordinates.
    textureMatrix.setToIdentity();
    textureMatrix.scale(1.0 / targetTexture.width(), -1.0 / targetTexture.height(), 1);
    textureMatrix.translate(-r.x(), -targetTexture.height() - r.y(), 0);
#ifndef KWIN_HAVE_OPENGLES
    glMatrixMode(GL_TEXTURE);
    loadMatrix(textureMatrix);
    glMatrixMode(GL_PROJECTION);
#endif
    shader->setTextureMatrix(textureMatrix);

    drawRegion(blurredRegion & region);

#ifndef KWIN_HAVE_OPENGLES
    popMatrix();
    glMatrixMode(GL_TEXTURE);
    popMatrix();
    glMatrixMode(GL_MODELVIEW);
    popMatrix();
#endif

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
#ifndef KWIN_HAVE_OPENGLES
        glPopAttrib();
#endif
    }

    targetTexture.unbind();
    shader->unbind();
}

} // namespace KWin

