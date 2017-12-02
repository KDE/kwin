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
#include "effects.h"
#include "blurshader.h"
// KConfigSkeleton
#include "blurconfig.h"

#include <QMatrix4x4>
#include <QLinkedList>

#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/blur_interface.h>
#include <KWayland/Server/shadow_interface.h>
#include <KWayland/Server/display.h>

namespace KWin
{

static const QByteArray s_blurAtomName = QByteArrayLiteral("_KDE_NET_WM_BLUR_BEHIND_REGION");

BlurEffect::BlurEffect()
{
    initConfig<BlurConfig>();
    shader = BlurShader::create();
    m_simpleShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("logout-blur.frag"));
    if (!m_simpleShader->isValid()) {
        qCDebug(KWINEFFECTS) << "Simple blur shader failed to load";
    }

    updateTexture();
    reconfigure(ReconfigureAll);

    // ### Hackish way to announce support.
    //     Should be included in _NET_SUPPORTED instead.
    if (shader && shader->isValid() && target->valid()) {
        net_wm_blur_region = effects->announceSupportProperty(s_blurAtomName, this);
        KWayland::Server::Display *display = effects->waylandDisplay();
        if (display) {
            m_blurManager = display->createBlurManager(this);
            m_blurManager->create();
        }
    } else {
        net_wm_blur_region = 0;
    }

    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(KWin::EffectWindow*,long)), this, SLOT(slotPropertyNotify(KWin::EffectWindow*,long)));
    connect(effects, SIGNAL(screenGeometryChanged(QSize)), this, SLOT(slotScreenGeometryChanged()));
    connect(effects, &EffectsHandler::xcbConnectionChanged, this,
        [this] {
            if (shader && shader->isValid() && target->valid()) {
                net_wm_blur_region = effects->announceSupportProperty(s_blurAtomName, this);
            }
        }
    );

    // Fetch the blur regions for all windows
    foreach (EffectWindow *window, effects->stackingOrder())
        updateBlurRegion(window);
}

BlurEffect::~BlurEffect()
{
    windows.clear();

    delete m_simpleShader;
    delete shader;
    delete target;
}

void BlurEffect::slotScreenGeometryChanged()
{
    effects->makeOpenGLContextCurrent();
    updateTexture();
    // Fetch the blur regions for all windows
    foreach (EffectWindow *window, effects->stackingOrder())
        updateBlurRegion(window);
    effects->doneOpenGLContextCurrent();
}

void BlurEffect::updateTexture() {
    delete target;
    // Offscreen texture that's used as the target for the horizontal blur pass
    // and the source for the vertical pass.
    tex = GLTexture(GL_RGBA8, effects->virtualScreenSize());
    tex.setFilter(GL_LINEAR);
    tex.setWrapMode(GL_CLAMP_TO_EDGE);

    target = new GLRenderTarget(tex);
}

void BlurEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    BlurConfig::self()->read();
    int radius = qBound(2, BlurConfig::blurRadius(), 14);
    if (shader)
        shader->setRadius(radius);

    m_shouldCache = BlurConfig::cacheTexture();

    windows.clear();

    if (!shader || !shader->isValid()) {
        effects->removeSupportProperty(s_blurAtomName, this);
        delete m_blurManager;
        m_blurManager = nullptr;
    }
}

void BlurEffect::updateBlurRegion(EffectWindow *w) const
{
    QRegion region;
    QByteArray value;

    if (net_wm_blur_region != XCB_ATOM_NONE) {
        value = w->readProperty(net_wm_blur_region, XCB_ATOM_CARDINAL, 32);
        if (value.size() > 0 && !(value.size() % (4 * sizeof(uint32_t)))) {
            const uint32_t *cardinals = reinterpret_cast<const uint32_t*>(value.constData());
            for (unsigned int i = 0; i < value.size() / sizeof(uint32_t);) {
                int x = cardinals[i++];
                int y = cardinals[i++];
                int w = cardinals[i++];
                int h = cardinals[i++];
                region += QRect(x, y, w, h);
            }
        }
    }

    KWayland::Server::SurfaceInterface *surf = w->surface();

    if (surf && surf->blur()) {
        region = surf->blur()->region();
    }

    //!value.isNull() full window in X11 case, surf->blur()
    //valid, full window in wayland case
    if (region.isEmpty() && (!value.isNull() || (surf && surf->blur()))) {
        // Set the data to a dummy value.
        // This is needed to be able to distinguish between the value not
        // being set, and being set to an empty region.
        w->setData(WindowBlurBehindRole, 1);
    } else
        w->setData(WindowBlurBehindRole, region);
}

void BlurEffect::slotWindowAdded(EffectWindow *w)
{
    KWayland::Server::SurfaceInterface *surf = w->surface();

    if (surf) {
        windows[w].blurChangedConnection = connect(surf, &KWayland::Server::SurfaceInterface::blurChanged, this, [this, w] () {

            if (w) {
                updateBlurRegion(w);
            }
        });
    }
    updateBlurRegion(w);
}

void BlurEffect::slotWindowDeleted(EffectWindow *w)
{
    if (windows.contains(w)) {
        disconnect(windows[w].blurChangedConnection);
        windows.remove(w);
    }
}

void BlurEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == net_wm_blur_region && net_wm_blur_region != XCB_ATOM_NONE) {
        updateBlurRegion(w);
        CacheEntry it = windows.find(w);
        if (it != windows.end()) {
            const QRect screen = effects->virtualScreenGeometry();
            it->damagedRegion = expand(blurRegion(w).translated(w->pos())) & screen;
        }
    }
}

bool BlurEffect::enabledByDefault()
{
    GLPlatform *gl = GLPlatform::instance();

    if (gl->isIntel() && gl->chipClass() < SandyBridge)
        return false;
    if (gl->isSoftwareEmulation()) {
        return false;
    }

    return true;
}

bool BlurEffect::supported()
{
    bool supported = effects->isOpenGLCompositing() && GLRenderTarget::supported();

    if (supported) {
        int maxTexSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

        const QSize screenSize = effects->virtualScreenSize();
        if (screenSize.width() > maxTexSize || screenSize.height() > maxTexSize)
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

    for (const QRect &rect : region) {
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

void BlurEffect::uploadRegion(QVector2D *&map, const QRegion &region)
{
    for (const QRect &r : region) {
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

void BlurEffect::uploadGeometry(GLVertexBuffer *vbo, const QRegion &horizontal, const QRegion &vertical)
{
    const int vertexCount = (horizontal.rectCount() + vertical.rectCount()) * 6;
    if (!vertexCount)
        return;

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
    if (!shader || !shader->isValid()) {
        return;
    }

    // to blur an area partially we have to shrink the opaque area of a window
    QRegion newClip;
    const QRegion oldClip = data.clip;
    const int radius = shader->radius();
    for (const QRect &rect : data.clip) {
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
    const QRect screen = effects->virtualScreenGeometry();
    const QRegion blurArea = blurRegion(w).translated(w->pos()) & screen;
    const QRegion expandedBlur = expand(blurArea) & screen;

    if (m_shouldCache && !w->isDeleted()) {
        // we are caching the horizontally blurred background texture

        // if a window underneath the blurred area is damaged we have to
        // update the cached texture
        QRegion damagedCache;
        CacheEntry it = windows.find(w);
        if (it != windows.end() && !it->dropCache &&
            it->windowPos == w->pos() &&
            it->blurredBackground.size() == expandedBlur.boundingRect().size()) {
            damagedCache = (expand(expandedBlur & m_damagedArea) |
                            (it->damagedRegion & data.paint)) & expandedBlur;
        } else {
            damagedCache = expandedBlur;
        }
        if (!damagedCache.isEmpty()) {
            // This is the area of the blurry window which really can change.
            const QRegion damagedArea = damagedCache & blurArea;
            // In order to be able to recalculate this area we have to make sure the
            // background area is painted before.
            data.paint |= expand(damagedArea);
            if (it != windows.end()) {
                // In case we already have a texture cache mark the dirty regions invalid.
                it->damagedRegion &= expandedBlur;
                it->damagedRegion |= damagedCache;
                // The valid part of the cache can be considered as being opaque
                // as long as we don't need to update a bordering part
                data.clip |= blurArea - expand(it->damagedRegion);
                it->dropCache = false;
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
    if (!target->valid() || !shader || !shader->isValid())
        return false;

    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool())
        return false;

    if (w->isDesktop())
        return false;

    bool scaled = !qFuzzyCompare(data.xScale(), 1.0) && !qFuzzyCompare(data.yScale(), 1.0);
    bool translated = data.xTranslation() || data.yTranslation();

    if ((scaled || (translated || (mask & PAINT_WINDOW_TRANSFORMED))) && !w->data(WindowForceBlurRole).toBool())
        return false;

    bool blurBehindDecos = effects->decorationsHaveAlpha() &&
                effects->decorationSupportsBlurBehind();

    if (!w->hasAlpha() && w->opacity() >= 1.0 && !(blurBehindDecos && w->hasDecoration()))
        return false;

    return true;
}

void BlurEffect::drawWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    const QRect screen = GLRenderTarget::virtualScreenGeometry();
    if (shouldBlur(w, mask, data)) {
        QRegion shape = region & blurRegion(w).translated(w->pos()) & screen;

        // let's do the evil parts - someone wants to blur behind a transformed window
        const bool translated = data.xTranslation() || data.yTranslation();
        const bool scaled = data.xScale() != 1 || data.yScale() != 1;
        if (scaled) {
            QPoint pt = shape.boundingRect().topLeft();
            QVector<QRect> shapeRects = shape.rects();
            shape = QRegion(); // clear
            foreach (QRect r, shapeRects) {
                r.moveTo(pt.x() + (r.x() - pt.x()) * data.xScale() + data.xTranslation(),
                            pt.y() + (r.y() - pt.y()) * data.yScale() + data.yTranslation());
                r.setWidth(r.width() * data.xScale());
                r.setHeight(r.height() * data.yScale());
                shape |= r;
            }
            shape = shape & region;

        //Only translated, not scaled
        } else if (translated) {
            shape = shape.translated(data.xTranslation(), data.yTranslation());
            shape = shape & region;
        }

        if (!shape.isEmpty()) {
            if (w->isFullScreen() && GLRenderTarget::blitSupported() && m_simpleShader->isValid()
                    && !GLPlatform::instance()->supports(LimitedNPOT) && shape.boundingRect() == w->geometry()) {
                doSimpleBlur(w, data.opacity(), data.screenProjectionMatrix());
            } else if (m_shouldCache && !translated && !w->isDeleted()) {
                doCachedBlur(w, region, data.opacity(), data.screenProjectionMatrix());
            } else {
                doBlur(shape, screen, data.opacity(), data.screenProjectionMatrix());
            }
        }
    }

    // Draw the window over the blurred area
    effects->drawWindow(w, mask, region, data);
}

void BlurEffect::paintEffectFrame(EffectFrame *frame, QRegion region, double opacity, double frameOpacity)
{
    const QRect screen = effects->virtualScreenGeometry();
    bool valid = target->valid() && shader && shader->isValid();
    QRegion shape = frame->geometry().adjusted(-5, -5, 5, 5) & screen;
    if (valid && !shape.isEmpty() && region.intersects(shape.boundingRect()) && frame->style() != EffectFrameNone) {
        doBlur(shape, screen, opacity * frameOpacity, frame->screenProjectionMatrix());
    }
    effects->paintEffectFrame(frame, region, opacity, frameOpacity);
}

void BlurEffect::doSimpleBlur(EffectWindow *w, const float opacity, const QMatrix4x4 &screenProjection)
{
    // The fragment shader uses a LOD bias of 1.75, so we need 3 mipmap levels.
    GLTexture blurTexture = GLTexture(GL_RGBA8, w->size(), 3);
    blurTexture.setFilter(GL_LINEAR_MIPMAP_LINEAR);
    blurTexture.setWrapMode(GL_CLAMP_TO_EDGE);

    target->attachTexture(blurTexture);
    target->blitFromFramebuffer(w->geometry(), QRect(QPoint(0, 0), w->size()));

    // Unmodified base image
    ShaderBinder binder(m_simpleShader);
    QMatrix4x4 mvp = screenProjection;
    mvp.translate(w->x(), w->y());
    m_simpleShader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
    m_simpleShader->setUniform("u_alphaProgress", opacity);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    blurTexture.bind();
    blurTexture.generateMipmaps();
    blurTexture.render(infiniteRegion(), w->geometry());
    blurTexture.unbind();
    glDisable(GL_BLEND);
}

void BlurEffect::doBlur(const QRegion& shape, const QRect& screen, const float opacity, const QMatrix4x4 &screenProjection)
{
    const QRegion expanded = expand(shape) & screen;
    const QRect r = expanded.boundingRect();

    // Upload geometry for the horizontal and vertical passes
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    uploadGeometry(vbo, expanded, shape);
    vbo->bindArrays();

    const qreal scale = GLRenderTarget::virtualScreenScale();

    // Create a scratch texture and copy the area in the back buffer that we're
    // going to blur into it
    // for HIGH DPI scratch is captured in native resolution, it is then implicitly downsampled
    // when rendering into tex
    GLTexture scratch(GL_RGBA8, r.width() * scale, r.height() * scale);
    scratch.setFilter(GL_LINEAR);
    scratch.setWrapMode(GL_CLAMP_TO_EDGE);
    scratch.bind();

    const QRect sg = GLRenderTarget::virtualScreenGeometry();
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (r.x() - sg.x()) * scale, (sg.height() - sg.y() - r.y() - r.height()) * scale,
                        scratch.width(), scratch.height());

    // Draw the texture on the offscreen framebuffer object, while blurring it horizontally
    target->attachTexture(tex);
    GLRenderTarget::pushRenderTarget(target);

    shader->bind();
    shader->setDirection(Qt::Horizontal);
    shader->setPixelDistance(1.0 / r.width());

    QMatrix4x4 modelViewProjectionMatrix;
    modelViewProjectionMatrix.ortho(0, tex.width(), tex.height(), 0 , 0, 65535);
    shader->setModelViewProjectionMatrix(modelViewProjectionMatrix);

    // Set up the texture matrix to transform from screen coordinates
    // to texture coordinates.
    QMatrix4x4 textureMatrix;
    textureMatrix.scale(1.0 / r.width(), -1.0 / r.height(), 1);
    textureMatrix.translate(-r.x(), (-r.height() - r.y()), 0);
    shader->setTextureMatrix(textureMatrix);

    vbo->draw(GL_TRIANGLES, 0, expanded.rectCount() * 6);

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
        glEnable(GL_BLEND);
#if 1 // bow shape, always above y = x
        float o = 1.0f-opacity;
        o = 1.0f - o*o;
#else // sigmoid shape, above y = x for x > 0.5, below y = x for x < 0.5
        float o = 2.0f*opacity - 1.0f;
        o = 0.5f + o / (1.0f + qAbs(o));
#endif
        glBlendColor(0, 0, 0, o);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    }

    // Set the up the texture matrix to transform from screen coordinates
    // to texture coordinates.
    textureMatrix.setToIdentity();
    textureMatrix.scale(1.0 / tex.width(), -1.0 / tex.height(), 1);
    textureMatrix.translate(0, -tex.height(), 0);
    shader->setTextureMatrix(textureMatrix);
    shader->setModelViewProjectionMatrix(screenProjection);

    vbo->draw(GL_TRIANGLES, expanded.rectCount() * 6, shape.rectCount() * 6);
    vbo->unbindArrays();

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
    }

    tex.unbind();
    shader->unbind();
}

void BlurEffect::doCachedBlur(EffectWindow *w, const QRegion& region, const float opacity, const QMatrix4x4 &screenProjection)
{
    const QRect screen = effects->virtualScreenGeometry();
    const QRegion blurredRegion = blurRegion(w).translated(w->pos()) & screen;
    const QRegion expanded = expand(blurredRegion) & screen;
    const QRect r = expanded.boundingRect();

    // The background texture we get is only partially valid.

    CacheEntry it = windows.find(w);
    if (it == windows.end()) {
        BlurWindowInfo bwi;
        bwi.blurredBackground = GLTexture(GL_RGBA8, r.width(),r.height());
        bwi.damagedRegion = expanded;
        bwi.dropCache = false;
        bwi.windowPos = w->pos();
        it = windows.insert(w, bwi);
    } else if (it->blurredBackground.size() != r.size()) {
        it->blurredBackground = GLTexture(GL_RGBA8, r.width(),r.height());
        it->dropCache = false;
        it->windowPos = w->pos();
    } else if (it->windowPos != w->pos()) {
        it->dropCache = false;
        it->windowPos = w->pos();
    }

    GLTexture targetTexture = it->blurredBackground;
    targetTexture.setFilter(GL_LINEAR);
    targetTexture.setWrapMode(GL_CLAMP_TO_EDGE);
    shader->bind();
    QMatrix4x4 textureMatrix;
    QMatrix4x4 modelViewProjectionMatrix;

    /**
     * Which part of the background texture can be updated ?
     *
     * Well this is a rather difficult question. We kind of rely on the fact, that
     * we need a bigger background region being painted before, more precisely if we want to
     * blur region A we need the background region expand(A). This business logic is basically
     * done in prePaintWindow:
     *          data.paint |= expand(damagedArea);
     *
     * Now "data.paint" gets clipped and becomes what we receive as the "region" variable
     * in this function. In theory there is now only one function that does this clipping
     * and this is paintSimpleScreen. The clipping has the effect that "damagedRegion"
     * is no longer a subset of "region" and we cannot fully validate the cache within one
     * rendering pass. If we would now update the "damageRegion & region" part of the cache
     * we would wrongly update the part of the cache that is next to the "region" border and
     * which lies within "damagedRegion", just because we cannot assume that the framebuffer
     * outside of "region" is valid. Therefore the maximal damaged region of the cache that can
     * be repainted is given by:
     *          validUpdate = damagedRegion - expand(damagedRegion - region);
     *
     * Now you may ask what is with the rest of "damagedRegion & region" that is not part
     * of "validUpdate" but also might end up on the screen. Well under the assumption
     * that only the occlusion culling can shrink "data.paint", we can control this by reducing
     * the opaque area of every window by a margin of the blurring radius (c.f. prePaintWindow).
     * This way we are sure that this area is overpainted by a higher opaque window.
     *
     * Apparently paintSimpleScreen is not the only function that can influence "region".
     * In fact every effect's paintWindow that is called before Blur::paintWindow
     * can do so (e.g. SlidingPopups). Hence we have to make the compromise that we update
     * "damagedRegion & region" of the cache but only mark "validUpdate" as valid.
     **/
    const QRegion damagedRegion = it->damagedRegion;
    const QRegion updateBackground = damagedRegion & region;
    const QRegion validUpdate = damagedRegion - expand(damagedRegion - region);

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
        const QRect updateRect = (expand(updateBackground) & expanded).boundingRect();
        // First we have to copy the background from the frontbuffer
        // into a scratch texture (in this case "tex").
        tex.bind();

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, updateRect.x(), effects->virtualScreenSize().height() - updateRect.y() - updateRect.height(),
                            updateRect.width(), updateRect.height());

        // Draw the texture on the offscreen framebuffer object, while blurring it horizontally
        target->attachTexture(targetTexture);
        GLRenderTarget::pushRenderTarget(target);

        shader->setDirection(Qt::Horizontal);
        shader->setPixelDistance(1.0 / tex.width());

        modelViewProjectionMatrix.ortho(0, r.width(), r.height(), 0 , 0, 65535);
        modelViewProjectionMatrix.translate(-r.x(), -r.y(), 0);
        shader->setModelViewProjectionMatrix(modelViewProjectionMatrix);

        // Set up the texture matrix to transform from screen coordinates
        // to texture coordinates.
        textureMatrix.scale(1.0 / tex.width(), -1.0 / tex.height(), 1);
        textureMatrix.translate(-updateRect.x(), -updateRect.height() - updateRect.y(), 0);
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

    shader->setDirection(Qt::Vertical);
    shader->setPixelDistance(1.0 / targetTexture.height());

    // Modulate the blurred texture with the window opacity if the window isn't opaque
    if (opacity < 1.0) {
        glEnable(GL_BLEND);
        glBlendColor(0, 0, 0, opacity);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    }

    shader->setModelViewProjectionMatrix(screenProjection);

    // Set the up the texture matrix to transform from screen coordinates
    // to texture coordinates.
    textureMatrix.setToIdentity();
    textureMatrix.scale(1.0 / targetTexture.width(), -1.0 / targetTexture.height(), 1);
    textureMatrix.translate(-r.x(), -targetTexture.height() - r.y(), 0);
    shader->setTextureMatrix(textureMatrix);

    vbo->draw(GL_TRIANGLES, verticalOffset, verticalCount);
    vbo->unbindArrays();

    if (opacity < 1.0) {
        glDisable(GL_BLEND);
    }

    targetTexture.unbind();
    shader->unbind();
}

int BlurEffect::blurRadius() const
{
    if (!shader) {
        return 0;
    }
    return shader->radius();
}

} // namespace KWin

