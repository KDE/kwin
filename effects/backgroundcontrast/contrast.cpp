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

#include "contrast.h"
#include "contrastshader.h"
// KConfigSkeleton
#include "contrastconfig.h"

#include <X11/Xatom.h>

#include <QMatrix4x4>
#include <QLinkedList>
#include <KDebug>

namespace KWin
{

KWIN_EFFECT(contrast, ContrastEffect)
KWIN_EFFECT_SUPPORTED(contrast, ContrastEffect::supported())
KWIN_EFFECT_ENABLEDBYDEFAULT(contrast, ContrastEffect::enabledByDefault())

ContrastEffect::ContrastEffect()
{
    shader = ContrastShader::create();

    // Offscreen texture that's used as the target for the horizontal blur pass
    // and the source for the vertical pass.
    tex = GLTexture(displayWidth(), displayHeight());
    tex.setFilter(GL_LINEAR);
    tex.setWrapMode(GL_CLAMP_TO_EDGE);

    target = new GLRenderTarget(tex);

    reconfigure(ReconfigureAll);

    // ### Hackish way to announce support.
    //     Should be included in _NET_SUPPORTED instead.
    if (shader && shader->isValid() && target->valid()) {
        net_wm_blur_region = effects->announceSupportProperty("_KDE_NET_WM_BLUR_BEHIND_REGION", this);
    } else {
        net_wm_blur_region = 0;
    }

    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(KWin::EffectWindow*,long)), this, SLOT(slotPropertyNotify(KWin::EffectWindow*,long)));
    connect(effects, SIGNAL(screenGeometryChanged(QSize)), this, SLOT(slotScreenGeometryChanged()));

    // Fetch the blur regions for all windows
    foreach (EffectWindow *window, effects->stackingOrder())
        updateContrastRegion(window);
}

ContrastEffect::~ContrastEffect()
{
    windows.clear();

    delete shader;
    delete target;
}

void ContrastEffect::slotScreenGeometryChanged()
{
    effects->reloadEffect(this);
}

void ContrastEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    ContrastConfig::self()->readConfig();
    int strength = 4;
    if (shader)
        shader->setStrength(strength);

    m_shouldCache = ContrastConfig::cacheTexture();

    windows.clear();

    if (!shader || !shader->isValid())
        XDeleteProperty(display(), rootWindow(), net_wm_blur_region);
}

void ContrastEffect::updateContrastRegion(EffectWindow *w) const
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

void ContrastEffect::slotWindowAdded(EffectWindow *w)
{
    updateContrastRegion(w);
}

void ContrastEffect::slotWindowDeleted(EffectWindow *w)
{
    if (windows.contains(w)) {
        windows.remove(w);
    }
}

void ContrastEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == net_wm_blur_region) {
        updateContrastRegion(w);
        CacheEntry it = windows.find(w);
        if (it != windows.end()) {
            const QRect screen(0, 0, displayWidth(), displayHeight());
            it->damagedRegion = contrastRegion(w).translated(w->pos()) & screen;
        }
    }
}

bool ContrastEffect::enabledByDefault()
{
    GLPlatform *gl = GLPlatform::instance();

    if (gl->isIntel() && gl->chipClass() < SandyBridge)
        return false;
    if (gl->driver() == Driver_Catalyst && effects->compositingType() == OpenGL1Compositing) {
        // fglrx supports only ARB shaders and those tend to crash KWin (see Bug #270818 and #286795)
        return false;
    }

    return true;
}

bool ContrastEffect::supported()
{
    bool supported = GLRenderTarget::supported() && GLTexture::NPOTTextureSupported() && GLSLContrastShader::supported();

    if (supported) {
        int maxTexSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

        if (displayWidth() > maxTexSize || displayHeight() > maxTexSize)
            supported = false;
    }
    return supported;
}

QRegion ContrastEffect::contrastRegion(const EffectWindow *w) const
{
    QRegion region;

    const QVariant value = w->data(WindowBlurBehindRole);
    if (value.isValid()) {
        const QRegion appRegion = qvariant_cast<QRegion>(value);
        if (!appRegion.isEmpty()) {
            if (w->decorationHasAlpha() && effects->decorationSupportsBlurBehind()) {
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
    } else if (w->decorationHasAlpha() && effects->decorationSupportsBlurBehind()) {
        // If the client hasn't specified a blur region, we'll only enable
        // the effect behind the decoration.
        region = w->shape();
        region -= w->decorationInnerRect();
    }

    return region;
}

void ContrastEffect::uploadRegion(QVector2D *&map, const QRegion &region)
{
    foreach (const QRect &r, region.rects()) {
        const QVector2D topLeft(r.x(), r.y());
        const QVector2D topRight(r.x() + r.width(), r.y());
        const QVector2D bottomLeft(r.x(), r.y() + r.height());
        const QVector2D bottomRight(r.x() + r.width(), r.y() + r.height());

        // First triangle
        *(map++) = topRight;
        *(map++) = topLeft;
        *(map++) = bottomLeft;

        // Second triangle
        *(map++) = bottomLeft;
        *(map++) = bottomRight;
        *(map++) = topRight;
    }
}

void ContrastEffect::uploadGeometry(GLVertexBuffer *vbo, const QRegion &horizontal, const QRegion &vertical)
{
    const int vertexCount = (horizontal.rectCount() + vertical.rectCount()) * 6;

    QVector2D *map = (QVector2D *) vbo->map(vertexCount * sizeof(QVector2D));
    uploadRegion(map, horizontal);
    uploadRegion(map, vertical);
    vbo->unmap();

    const GLVertexAttrib layout[] = {
        { VA_Position, 2, GL_FLOAT, 0 },
        { VA_TexCoord, 2, GL_FLOAT, 0 }
    };

    vbo->setAttribLayout(layout, 2, sizeof(QVector2D));
}

void ContrastEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    m_damagedArea = QRegion();
    m_paintedArea = QRegion();
    m_currentBlur = QRegion();

    effects->prePaintScreen(data, time);
}

void ContrastEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    // this effect relies on prePaintWindow being called in the bottom to top order

    effects->prePaintWindow(w, data, time);

    if (!w->isPaintingEnabled()) {
        return;
    }
    if (!shader || !shader->isValid()) {
        return;
    }

    const QRegion oldPaint = data.paint;

    // we don't have to blur a region we don't see
    m_currentBlur -= data.clip;
    // if we have to paint a non-opaque part of this window that intersects with the
    // currently blurred region (which is not cached) we have to redraw the whole region
    if ((data.paint-data.clip).intersects(m_currentBlur)) {
        data.paint |= m_currentBlur;
    }

    // in case this window has regions to be blurred
    const QRect screen(0, 0, displayWidth(), displayHeight());
    const QRegion blurArea = contrastRegion(w).translated(w->pos()) & screen;

    if (m_shouldCache) {
        // we are caching the horizontally blurred background texture

        // if a window underneath the blurred area is damaged we have to
        // update the cached texture
        QRegion damagedCache;
        CacheEntry it = windows.find(w);
        if (it != windows.end() && !it->dropCache &&
            it->windowPos == w->pos() &&
            it->coloredBackground.size() == blurArea.boundingRect().size()) {
            damagedCache = ((blurArea & m_damagedArea) |
                            (it->damagedRegion & data.paint)) & blurArea;
        } else {
            damagedCache = blurArea;
        }
        if (!damagedCache.isEmpty()) {
            // This is the area of the blurry window which really can change.
            const QRegion damagedArea = damagedCache & blurArea;
            // In order to be able to recalculate this area we have to make sure the
            // background area is painted before.
            data.paint |= damagedArea;
            if (it != windows.end()) {
                // In case we already have a texture cache mark the dirty regions invalid.
                it->damagedRegion &= blurArea;
                it->damagedRegion |= damagedCache;
                // The valid part of the cache can be considered as being opaque
                // as long as we don't need to update a bordering part
                data.clip |= blurArea - it->damagedRegion;
                it->dropCache = false;
            }
            // we keep track of the "damage propagation"
            m_damagedArea |= damagedArea;
            // we have to check again whether we do not damage a blurred area
            // of a window we do not cache
            if (blurArea.intersects(m_currentBlur)) {
                data.paint |= m_currentBlur;
            }
        }
    } else {
        // we are not caching the window

        // if this window or an window underneath the modified area is painted again we have to
        // do everything
        if (m_paintedArea.intersects(blurArea) || data.paint.intersects(blurArea)) {
            data.paint |= blurArea;
            // we keep track of the "damage propagation"
            m_damagedArea |= blurArea;
            // we have to check again whether we do not damage a blurred area
            // of a window we do not cache
            if (blurArea.intersects(m_currentBlur)) {
                data.paint |= m_currentBlur;
            }
        }

        m_currentBlur |= blurArea;
    }

    // we don't consider damaged areas which are occluded and are not
    // explicitly damaged by this window
    m_damagedArea -= data.clip;
    m_damagedArea |= oldPaint;

    // in contrast to m_damagedArea does m_paintedArea keep track of all repainted areas
    m_paintedArea -= data.clip;
    m_paintedArea |= data.paint;
}

bool ContrastEffect::shouldContrast(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    if (!target->valid() || !shader || !shader->isValid())
        return false;

    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool())
        return false;

    if (w->isDesktop())
        return false;

    bool scaled = !qFuzzyCompare(data.xScale(), 1.0) && !qFuzzyCompare(data.yScale(), 1.0);
    bool translated = data.xTranslation() || data.yTranslation();

    if (scaled || ((translated || (mask & PAINT_WINDOW_TRANSFORMED)) && !w->data(WindowForceBlurRole).toBool()))
        return false;

    bool blurBehindDecos = effects->decorationsHaveAlpha() &&
                effects->decorationSupportsBlurBehind();

    if (!w->hasAlpha() && !(blurBehindDecos && w->hasDecoration()))
        return false;

    return true;
}

void ContrastEffect::drawWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    if (shouldContrast(w, mask, data)) {
        QRegion shape = region & contrastRegion(w).translated(w->pos()) & screen;

        const bool translated = data.xTranslation() || data.yTranslation();
        // let's do the evil parts - someone wants to blur behind a transformed window
        if (translated) {
            shape = shape.translated(data.xTranslation(), data.yTranslation());
            shape = shape & region;
        }

        if (!shape.isEmpty()) {
            if (m_shouldCache && !translated) {
                doCachedContrast(w, region, data.opacity());
            } else {
                doContrast(shape, screen, data.opacity());
            }
        }
    }

    // Draw the window over the blurred area
    effects->drawWindow(w, mask, region, data);
}

void ContrastEffect::paintEffectFrame(EffectFrame *frame, QRegion region, double opacity, double frameOpacity)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    bool valid = target->valid() && shader && shader->isValid();
    QRegion shape = frame->geometry().adjusted(-5, -5, 5, 5) & screen;
    if (valid && !shape.isEmpty() && region.intersects(shape.boundingRect()) && frame->style() != EffectFrameNone) {
        doContrast(shape, screen, opacity * frameOpacity);
    }
    effects->paintEffectFrame(frame, region, opacity, frameOpacity);
}

void ContrastEffect::doContrast(const QRegion& shape, const QRect& screen, const float opacity)
{
    const QRegion actualShape = shape & screen;
    const QRect r = actualShape.boundingRect();

    // Upload geometry for the horizontal and vertical passes
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    uploadGeometry(vbo, actualShape, shape);
    vbo->bindArrays();

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

    // Set up the texture matrix to transform from screen coordinates
    // to texture coordinates.
#ifdef KWIN_HAVE_OPENGL_1
    if (effects->compositingType() == OpenGL1Compositing) {
        glMatrixMode(GL_TEXTURE);
        pushMatrix();
    }
#endif
    QMatrix4x4 textureMatrix;
    textureMatrix.scale(1.0 / scratch.width(), -1.0 / scratch.height(), 1);
    textureMatrix.translate(-r.x(), -scratch.height() - r.y(), 0);
    loadMatrix(textureMatrix);
    shader->setTextureMatrix(textureMatrix);

    vbo->draw(GL_TRIANGLES, 0, actualShape.rectCount() * 6);

    GLRenderTarget::popRenderTarget();
    scratch.unbind();
    scratch.discard();

    // Now draw the horizontally blurred area back to the backbuffer, while
    // blurring it vertically and clipping it to the window shape.
    tex.bind();

    // Modulate the blurred texture with the window opacity if the window isn't opaque
    if (opacity < 1.0) {
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

    vbo->draw(GL_TRIANGLES, actualShape.rectCount() * 6, shape.rectCount() * 6);
    vbo->unbindArrays();

#ifdef KWIN_HAVE_OPENGL_1
    if (effects->compositingType() == OpenGL1Compositing) {
        popMatrix();
        glMatrixMode(GL_MODELVIEW);
    }
#endif

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
    }

    tex.unbind();
    shader->unbind();
}

void ContrastEffect::doCachedContrast(EffectWindow *w, const QRegion& region, const float opacity)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    const QRegion blurredRegion = contrastRegion(w).translated(w->pos()) & screen;
    const QRegion actualRegion = blurredRegion & screen;
    const QRect r = actualRegion.boundingRect();

    // The background texture we get is only partially valid.

    CacheEntry it = windows.find(w);
    if (it == windows.end()) {
        BlurWindowInfo bwi;
        bwi.coloredBackground = GLTexture(r.width(),r.height());
        bwi.damagedRegion = actualRegion;
        bwi.dropCache = false;
        bwi.windowPos = w->pos();
        it = windows.insert(w, bwi);
    } else if (it->coloredBackground.size() != r.size()) {
        it->coloredBackground = GLTexture(r.width(),r.height());
        it->dropCache = false;
        it->windowPos = w->pos();
    } else if (it->windowPos != w->pos()) {
        it->dropCache = false;
        it->windowPos = w->pos();
    }

    GLTexture targetTexture = it->coloredBackground;
    targetTexture.setFilter(GL_LINEAR);
    targetTexture.setWrapMode(GL_CLAMP_TO_EDGE);
    shader->bind();
    QMatrix4x4 textureMatrix;
    QMatrix4x4 modelViewProjectionMatrix;
#ifdef KWIN_HAVE_OPENGL_1
    if (effects->compositingType() == OpenGL1Compositing) {
        glMatrixMode(GL_MODELVIEW);
        pushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_TEXTURE);
        pushMatrix();
        glMatrixMode(GL_PROJECTION);
        pushMatrix();
    }
#endif

    const QRegion damagedRegion = it->damagedRegion;
    const QRegion updateBackground = damagedRegion & region;
    const QRegion validUpdate = damagedRegion;

    const QRegion horizontal = validUpdate.isEmpty() ? QRegion() : (updateBackground & screen);
    const QRegion vertical   = blurredRegion & region;

    const int horizontalOffset = 0;
    const int horizontalCount = horizontal.rectCount() * 6;

    const int verticalOffset = horizontalCount;
    const int verticalCount = vertical.rectCount() * 6;

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    uploadGeometry(vbo, horizontal, vertical);

    vbo->bindArrays();

    if (!validUpdate.isEmpty()) {
        const QRect updateRect = (updateBackground & actualRegion).boundingRect();
        // First we have to copy the background from the frontbuffer
        // into a scratch texture (in this case "tex").
        tex.bind();

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, updateRect.x(), displayHeight() - updateRect.y() - updateRect.height(),
                            updateRect.width(), updateRect.height());

        // Draw the texture on the offscreen framebuffer object, while blurring it horizontally
        target->attachTexture(targetTexture);
        GLRenderTarget::pushRenderTarget(target);

        modelViewProjectionMatrix.ortho(0, r.width(), r.height(), 0 , 0, 65535);
        modelViewProjectionMatrix.translate(-r.x(), -r.y(), 0);
        loadMatrix(modelViewProjectionMatrix);
        shader->setModelViewProjectionMatrix(modelViewProjectionMatrix);

        // Set up the texture matrix to transform from screen coordinates
        // to texture coordinates.
        textureMatrix.scale(1.0 / tex.width(), -1.0 / tex.height(), 1);
        textureMatrix.translate(-updateRect.x(), -updateRect.height() - updateRect.y(), 0);
#ifdef KWIN_HAVE_OPENGL_1
        if (effects->compositingType() == OpenGL1Compositing) {
            glMatrixMode(GL_TEXTURE);
            loadMatrix(textureMatrix);
            glMatrixMode(GL_PROJECTION);
        }
#endif
        shader->setTextureMatrix(textureMatrix);

        vbo->draw(GL_TRIANGLES, horizontalOffset, horizontalCount);

        GLRenderTarget::popRenderTarget();
        tex.unbind();
        // mark the updated region as valid
        it->damagedRegion -= validUpdate;
    }

    // Now draw the horizontally blurred area back to the backbuffer, while
    // blurring it vertically and clipping it to the window shape.
    targetTexture.bind();

    // Modulate the blurred texture with the window opacity if the window isn't opaque
    if (opacity < 1.0) {
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
#ifdef KWIN_HAVE_OPENGL_1
    if (effects->compositingType() == OpenGL1Compositing) {
        glMatrixMode(GL_TEXTURE);
        loadMatrix(textureMatrix);
        glMatrixMode(GL_PROJECTION);
    }
#endif
    shader->setTextureMatrix(textureMatrix);

    vbo->draw(GL_TRIANGLES, verticalOffset, verticalCount);
    vbo->unbindArrays();

#ifdef KWIN_HAVE_OPENGL_1
    if (effects->compositingType() == OpenGL1Compositing) {
        popMatrix();
        glMatrixMode(GL_TEXTURE);
        popMatrix();
        glMatrixMode(GL_MODELVIEW);
        popMatrix();
    }
#endif

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
    }

    targetTexture.unbind();
    shader->unbind();
}

} // namespace KWin

