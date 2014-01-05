/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2011 Philipp Knechtges <philipp-dev@knechtges.com>
 *   Copyright 2014 Marco Martin <mart@kde.org>
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

    reconfigure(ReconfigureAll);

    // ### Hackish way to announce support.
    //     Should be included in _NET_SUPPORTED instead.
    if (shader && shader->isValid()) {
        net_wm_contrast_region = effects->announceSupportProperty("_KDE_NET_WM_BACKGROUND_CONTRAST_REGION", this);
    } else {
        net_wm_contrast_region = 0;
    }

    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(KWin::EffectWindow*,long)), this, SLOT(slotPropertyNotify(KWin::EffectWindow*,long)));
    connect(effects, SIGNAL(screenGeometryChanged(QSize)), this, SLOT(slotScreenGeometryChanged()));

    // Fetch the contrast regions for all windows
    foreach (EffectWindow *window, effects->stackingOrder())
        updateContrastRegion(window);
}

ContrastEffect::~ContrastEffect()
{
    delete shader;
}

void ContrastEffect::slotScreenGeometryChanged()
{
    effects->reloadEffect(this);
}

void ContrastEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    if (!shader || !shader->isValid())
        XDeleteProperty(display(), rootWindow(), net_wm_contrast_region);
}

void ContrastEffect::updateContrastRegion(EffectWindow *w) const
{
    QRegion region;
    float colorTransform[16];

    const QByteArray value = w->readProperty(net_wm_contrast_region, XA_CARDINAL, 32);
    if (value.size() > 0 && !((value.size() - (16 * sizeof(float))) % ((4 * sizeof(long))))) {
        const long *cardinals = reinterpret_cast<const long*>(value.constData());
        unsigned int i = 0;
        for (; i < ((value.size() - (16 * sizeof(long)))) / sizeof(long);) {
            int x = cardinals[i++];
            int y = cardinals[i++];
            int w = cardinals[i++];
            int h = cardinals[i++];
            region += QRect(x, y, w, h);
        }
        for (unsigned int j = 0; j < 16; ++j) {
             colorTransform[j] = (float)cardinals[i + j] / 100;
        }
        QMatrix4x4 colorMatrix(colorTransform);
        shader->setColorMatrix(colorMatrix);
    }

    if (region.isEmpty() && !value.isNull()) {
        // Set the data to a dummy value.
        // This is needed to be able to distinguish between the value not
        // being set, and being set to an empty region.
        w->setData(WindowBackgroundContrastRole, 1);
    } else
        w->setData(WindowBackgroundContrastRole, region);
}

void ContrastEffect::slotWindowAdded(EffectWindow *w)
{
    updateContrastRegion(w);
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
    bool supported = GLRenderTarget::supported() && GLTexture::NPOTTextureSupported() && ContrastShader::supported();

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

    const QVariant value = w->data(WindowBackgroundContrastRole);
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

void ContrastEffect::uploadGeometry(GLVertexBuffer *vbo, const QRegion &region)
{
    const int vertexCount = region.rectCount() * 6;

    QVector2D *map = (QVector2D *) vbo->map(vertexCount * sizeof(QVector2D));
    uploadRegion(map, region);
    vbo->unmap();

    const GLVertexAttrib layout[] = {
        { VA_Position, 2, GL_FLOAT, 0 },
        { VA_TexCoord, 2, GL_FLOAT, 0 }
    };

    vbo->setAttribLayout(layout, 2, sizeof(QVector2D));
}

bool ContrastEffect::shouldContrast(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    if (!shader || !shader->isValid())
        return false;

    if (effects->activeFullScreenEffect() && !w->data(WindowForceBackgroundContrastRole).toBool())
        return false;

    if (w->isDesktop())
        return false;

    bool scaled = !qFuzzyCompare(data.xScale(), 1.0) && !qFuzzyCompare(data.yScale(), 1.0);
    bool translated = data.xTranslation() || data.yTranslation();

    if (scaled || ((translated || (mask & PAINT_WINDOW_TRANSFORMED)) && !w->data(WindowForceBackgroundContrastRole).toBool()))
        return false;

    if (!w->hasAlpha())
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
            doContrast(shape, screen, data.opacity());
        }
    }

    // Draw the window over the contrast area
    effects->drawWindow(w, mask, region, data);
}

void ContrastEffect::paintEffectFrame(EffectFrame *frame, QRegion region, double opacity, double frameOpacity)
{
    const QRect screen(0, 0, displayWidth(), displayHeight());
    bool valid = shader && shader->isValid();
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
    uploadGeometry(vbo, actualShape);
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

    scratch.unbind();
    scratch.discard();

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

    shader->unbind();
}

} // namespace KWin

