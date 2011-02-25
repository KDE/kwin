/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
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
#include <KConfigGroup>
#include <KDebug>

namespace KWin
{
KWIN_EFFECT(blur, BlurEffect)
KWIN_EFFECT_SUPPORTED(blur, BlurEffect::supported())


BlurEffect::BlurEffect()
{
    shader = BlurShader::create();

    // Offscreen texture that's used as the target for the horizontal blur pass
    // and the source for the vertical pass.
    tex = new GLTexture(displayWidth(), displayHeight());
    tex->setFilter(GL_LINEAR);
    tex->setWrapMode(GL_CLAMP_TO_EDGE);

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
}

BlurEffect::~BlurEffect()
{
    effects->registerPropertyType(net_wm_blur_region, false);
    XDeleteProperty(display(), rootWindow(), net_wm_blur_region);

    delete shader;
    delete target;
    delete tex;
}

void BlurEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    KConfigGroup cg = EffectsHandler::effectConfig("Blur");
    int radius = qBound(2, cg.readEntry("BlurRadius", 12), 14);
    shader->setRadius(radius);

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

void BlurEffect::propertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == net_wm_blur_region)
        updateBlurRegion(w);
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

    if (supported) {
        // check the blacklist
        KSharedConfigPtr config = KSharedConfig::openConfig("kwinrc");
        KConfigGroup blacklist = config->group("Blacklist").group("Blur");
        if (effects->checkDriverBlacklist(blacklist)) {
            kDebug() << "Blur effect disabled by driver blacklist";
            supported = false;
        }
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

    if (region.rectCount() < 20) {
        foreach (const QRect & rect, region.rects())
        expanded += expand(rect);
    } else
        expanded += expand(region.boundingRect());

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
                region |= appRegion.translated(w->contentsRect().topLeft()) &
                          w->contentsRect();
            } else
                region = appRegion & w->contentsRect();
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

void BlurEffect::paintScreen(int mask, QRegion region, ScreenPaintData &data)
{
    // Force the scene to call paintGenericScreen() so the windows are painted bottom -> top
    if (!effects->activeFullScreenEffect())
        mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->paintScreen(mask, region, data);
}

void BlurEffect::drawWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    bool scaled = !qFuzzyCompare(data.xScale, 1.0) && !qFuzzyCompare(data.yScale, 1.0);
    bool translated = data.xTranslate || data.yTranslate;
    bool transformed = scaled || translated || mask & PAINT_WINDOW_TRANSFORMED;
    bool hasAlpha = w->hasAlpha() || (w->hasDecoration() && effects->decorationsHaveAlpha() && effects->decorationSupportsBlurBehind());
    bool valid = target->valid() && shader->isValid();

    QRegion shape;
    const QVariant forceBlur = w->data(WindowForceBlurRole);
    if ((!effects->activeFullScreenEffect() || (forceBlur.isValid() && forceBlur.toBool()))
            && hasAlpha && !w->isDesktop() && !transformed)
        shape = blurRegion(w).translated(w->geometry().topLeft()) & screen;

    if (valid && !shape.isEmpty() && region.intersects(shape.boundingRect())) {
        doBlur(shape, screen, data.opacity * data.contents_opacity);
    }

    // Draw the window over the blurred area
    effects->drawWindow(w, mask, region, data);
}

void BlurEffect::paintEffectFrame(EffectFrame *frame, QRegion region, double opacity, double frameOpacity)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    bool valid = target->valid() && shader->isValid();
    QRegion shape = frame->geometry().adjusted(-5, -5, 5, 5) & screen;
    if (valid && !shape.isEmpty() && region.intersects(shape.boundingRect())) {
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
    effects->pushRenderTarget(target);

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

    effects->popRenderTarget();
    scratch.unbind();
    scratch.discard();

    // Now draw the horizontally blurred area back to the backbuffer, while
    // blurring it vertically and clipping it to the window shape.
    tex->bind();

    shader->setDirection(Qt::Vertical);
    shader->setPixelDistance(1.0 / tex->height());

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
    textureMatrix.scale(1.0 / tex->width(), -1.0 / tex->height(), 1);
    textureMatrix.translate(0, -tex->height(), 0);
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

    tex->unbind();
    shader->unbind();
}

} // namespace KWin

