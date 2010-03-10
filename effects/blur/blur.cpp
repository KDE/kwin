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


namespace KWin
{

enum {
    BlurRegionRole = 0x1C50A74B 
};


KWIN_EFFECT(blur, BlurEffect)
KWIN_EFFECT_SUPPORTED(blur, BlurEffect::supported())


BlurEffect::BlurEffect()
    : radius(12)
{
    shader = BlurShader::create();
    shader->setRadius(radius);

    // Offscreen texture that's used as the target for the horizontal blur pass
    // and the source for the vertical pass.
    tex = new GLTexture(displayWidth(), displayHeight());
    tex->setFilter(GL_LINEAR);
    tex->setWrapMode(GL_CLAMP_TO_EDGE);

    target = new GLRenderTarget(tex);

    net_wm_blur_region = XInternAtom(display(), "_KDE_NET_WM_BLUR_REGION", False);
    effects->registerPropertyType(net_wm_blur_region, true);

    // ### Hackish way to announce support.
    //     Should be included in _NET_SUPPORTED instead.
    XChangeProperty(display(), rootWindow(), net_wm_blur_region, net_wm_blur_region,
                    32, PropModeReplace, 0, 0);
}

BlurEffect::~BlurEffect()
{
    effects->registerPropertyType(net_wm_blur_region, false);
    XDeleteProperty(display(), rootWindow(), net_wm_blur_region);

    delete shader;
    delete target;
    delete tex;
}

void BlurEffect::updateBlurRegion(EffectWindow *w) const
{
    QRegion region;

    const QByteArray value = w->readProperty(net_wm_blur_region, XA_CARDINAL, 32);
    if (value.size() > 0 && !(value.size() % (4 * sizeof(quint32)))) {
        const quint32 *cardinals = reinterpret_cast<const quint32*>(value.constData());
        for (unsigned int i = 0; i < value.size() / sizeof(quint32);) {
            int x = cardinals[i++];
            int y = cardinals[i++];
            int w = cardinals[i++];
            int h = cardinals[i++];
            region += QRect(x, y, w, h);
        }
    }

    w->setData(BlurRegionRole, value.isNull() ? QVariant() : region);
}

void BlurEffect::windowAdded(EffectWindow *w)
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
    return GLRenderTarget::supported() && GLTexture::NPOTTextureSupported() &&
           ((GLShader::vertexShaderSupported() && GLShader::fragmentShaderSupported()) ||
            hasGLExtension("GL_ARB_fragment_program"));
}

QRect BlurEffect::expand(const QRect &rect) const
{
    return rect.adjusted(-radius, -radius, radius, radius);
}

QRegion BlurEffect::expand(const QRegion &region) const
{
    QRegion expanded;

    if (region.rectCount() < 10) {
        foreach (const QRect &rect, region.rects())
            expanded += expand(rect);
    } else
        expanded += expand(region.boundingRect());

    return expanded;
}

QRegion BlurEffect::blurRegion(const EffectWindow *w) const
{
    QRegion region;

    const QVariant value = w->data(BlurRegionRole);
    if (value.isValid()) {
        const QRegion appRegion = qvariant_cast<QRegion>(value);
        if (!appRegion.isEmpty()) {
            if (w->hasDecoration()) {
                region = w->shape();
                region -= w->contentsRect();
                region |= appRegion.translated(w->contentsRect().topLeft()) &
                                        w->contentsRect();
            } else
                region = appRegion & w->contentsRect();
        } else
            region = w->shape();
    } else
        region = w->shape();

    return region;
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
    bool transformed = scaled || translated;
    bool hasAlpha = w->hasAlpha() || (w->hasDecoration() && effects->decorationsHaveAlpha());

    QRegion shape;
    if (!effects->activeFullScreenEffect() && hasAlpha && !w->isDesktop() && !transformed)
        shape = blurRegion(w).translated(w->geometry().topLeft()) & screen;

    if (!shape.isEmpty() && region.intersects(shape.boundingRect()))
    {
        const QRect r = expand(shape.boundingRect()) & screen;
        const QPoint offset = -shape.boundingRect().topLeft() +
                        (shape.boundingRect().topLeft() - r.topLeft());

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
        shader->setOpacity(1.0);

        glBegin(GL_QUADS);
        glTexCoord2f(0, 1);  glVertex2i(0, 0);
        glTexCoord2f(1, 1);  glVertex2i(r.width(), 0);
        glTexCoord2f(1, 0);  glVertex2i(r.width(), r.height());
        glTexCoord2f(0, 0);  glVertex2i(0, r.height());
        glEnd();

        effects->popRenderTarget();
        scratch.unbind();
        scratch.discard();

        // Now draw the horizontally blurred area back to the backbuffer, while
        // blurring it vertically and clipping it to the window shape.
        tex->bind();

        shader->setDirection(Qt::Vertical);
        shader->setPixelDistance(1.0 / tex->height());

        // Modulate the blurred texture with the window opacity if the window isn't opaque
        const float opacity = data.opacity * data.contents_opacity;
        if (opacity < 1.0) {
            shader->setOpacity(opacity);

            glPushAttrib(GL_COLOR_BUFFER_BIT);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }

        int vertexCount = shape.rectCount() * 4;
        if (vertices.size() < vertexCount)
            vertices.resize(vertexCount);

        // Set the up the texture matrix to transform from screen coordinates
        // to texture coordinates.
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();
        glScalef(1.0 / tex->width(), -1.0 / tex->height(), 1);
        glTranslatef(offset.x(), -tex->height() + offset.y(), 0); 

        int i = 0;
        foreach (const QRect &r, shape.rects()) {
            vertices[i++] = QVector2D(r.x(),             r.y()); 
            vertices[i++] = QVector2D(r.x() + r.width(), r.y()); 
            vertices[i++] = QVector2D(r.x() + r.width(), r.y() + r.height()); 
            vertices[i++] = QVector2D(r.x(),             r.y() + r.height()); 
        }

        if (vertexCount > 1000) {
            glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0, (float*)vertices.constData());
            glVertexPointer(2, GL_FLOAT, 0, (float*)vertices.constData());
            glDrawArrays(GL_QUADS, 0, vertexCount);
            glPopClientAttrib();
        } else {
            glBegin(GL_QUADS);
            for (int i = 0; i < vertexCount; i++) {
                glTexCoord2fv((const float*)&vertices[i]);
                glVertex2fv((const float*)&vertices[i]);
            }
            glEnd();
        }
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        if (opacity < 1.0)
            glPopAttrib();

        tex->unbind(); 
        shader->unbind(); 
    }

    // Draw the window over the blurred area
    effects->drawWindow(w, mask, region, data);
}

} // namespace KWin

