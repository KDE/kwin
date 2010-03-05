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


namespace KWin
{

KWIN_EFFECT(blur, BlurEffect)
KWIN_EFFECT_SUPPORTED(blur, BlurEffect::supported())


BlurEffect::BlurEffect()
    : radius(12)
{
    shader = new BlurShader;
    shader->setRadius(radius);

    // Offscreen texture that's used as the target for the horizontal blur pass
    // and the source for the vertical pass.
    tex = new GLTexture(displayWidth(), displayHeight());
    tex->setFilter(GL_LINEAR);
    tex->setWrapMode(GL_CLAMP_TO_EDGE);

    target = new GLRenderTarget(tex);
}

BlurEffect::~BlurEffect()
{
    delete shader;
    delete target;
    delete tex;
}

bool BlurEffect::supported()
{
    return GLRenderTarget::supported() && GLTexture::NPOTTextureSupported() &&
           hasGLExtension("GL_ARB_fragment_program");
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

void BlurEffect::paintScreen(int mask, QRegion region, ScreenPaintData &data)
{
    // Force the scene to call paintGenericScreen() so the windows are painted bottom -> top
    if (!effects->activeFullScreenEffect())
        mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->paintScreen(mask, region, data);
}

void BlurEffect::drawWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    bool scaled = !qFuzzyCompare(data.xScale, 1.0) && !qFuzzyCompare(data.yScale, 1.0);
    bool translated = data.xTranslate || data.yTranslate;
    bool hasAlpha = w->hasAlpha() || (w->hasDecoration() && effects->decorationsHaveAlpha());

    if (!effects->activeFullScreenEffect() && hasAlpha && !w->isDesktop() &&
        !scaled && !translated /* && region.intersects(w->geometry())*/)
    {
        const QRect screen(0, 0, displayWidth(), displayHeight());
        const QRegion shape = w->shape().translated(w->geometry().topLeft()) & screen;
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

        const float tw = tex->width();
        const float th = tex->height();
        int vertexCount = shape.rectCount() * 4;

        if (vertices.size() < vertexCount) {
            vertices.resize(vertexCount);
            texCoords.resize(vertexCount);
        }

        int i = 0;
        foreach (const QRect &r, shape.rects()) {
            vertices[i + 0] = QVector2D(r.x(),             r.y()); 
            vertices[i + 1] = QVector2D(r.x() + r.width(), r.y()); 
            vertices[i + 2] = QVector2D(r.x() + r.width(), r.y() + r.height()); 
            vertices[i + 3] = QVector2D(r.x(),             r.y() + r.height()); 

            const QRect sr = r.translated(offset);
            texCoords[i + 0] = QVector2D(sr.x() / tw,                1 - sr.y() / th);
            texCoords[i + 1] = QVector2D((sr.x() + sr.width()) / tw, 1 - sr.y() / th);
            texCoords[i + 2] = QVector2D((sr.x() + sr.width()) / tw, 1 - (sr.y() + sr.height()) / th);
            texCoords[i + 3] = QVector2D(sr.x() / tw,                1 - (sr.y() + sr.height()) / th);
            i += 4;
        }

        if (vertexCount > 1000) {
            glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0, (float*)texCoords.constData());
            glVertexPointer(2, GL_FLOAT, 0, (float*)vertices.constData());
            glDrawArrays(GL_QUADS, 0, vertexCount);
            glPopClientAttrib();
        } else {
            glBegin(GL_QUADS);
            for (int i = 0; i < vertexCount; i++) {
                glTexCoord2fv((const float*)&texCoords[i]);
                glVertex2fv((const float*)&vertices[i]);
            }
            glEnd();
        }

        if (opacity < 1.0)
            glPopAttrib();

        tex->unbind(); 
        shader->unbind(); 
    }

    // Draw the window over the blurred area
    effects->drawWindow(w, mask, region, data);
}

} // namespace KWin

