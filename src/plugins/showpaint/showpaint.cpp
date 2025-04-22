/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showpaint.h"

#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"

#include <QAction>
#include <QPainter>

namespace KWin
{

static const qreal s_alpha = 0.2;
static const QList<QColor> s_colors{
    Qt::red,
    Qt::green,
    Qt::blue,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::gray};

ShowPaintEffect::ShowPaintEffect() = default;

void ShowPaintEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen)
{
    m_painted = QRegion();
    effects->paintScreen(renderTarget, viewport, mask, region, screen);
    if (effects->isOpenGLCompositing()) {
        paintGL(renderTarget, viewport.projectionMatrix(), viewport.scale());
    } else if (effects->compositingType() == QPainterCompositing) {
        paintQPainter();
    }
    if (++m_colorIndex == s_colors.count()) {
        m_colorIndex = 0;
    }
}

void ShowPaintEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    m_painted += region;
    effects->paintWindow(renderTarget, viewport, w, mask, region, data);
}

void ShowPaintEffect::paintGL(const RenderTarget &renderTarget, const QMatrix4x4 &projection, qreal scale)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    ShaderBinder binder(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projection);
    binder.shader()->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    QColor color = s_colors[m_colorIndex];
    color.setAlphaF(s_alpha);
    binder.shader()->setUniform(GLShader::ColorUniform::Color, color);
    QList<QVector2D> verts;
    verts.reserve(m_painted.rectCount() * 12);
    for (const QRect &r : m_painted) {
        const auto deviceRect = snapToPixelGridF(scaledRect(r, scale));
        verts.push_back(QVector2D(deviceRect.x() + deviceRect.width(), deviceRect.y()));
        verts.push_back(QVector2D(deviceRect.x(), deviceRect.y()));
        verts.push_back(QVector2D(deviceRect.x(), deviceRect.y() + deviceRect.height()));
        verts.push_back(QVector2D(deviceRect.x(), deviceRect.y() + deviceRect.height()));
        verts.push_back(QVector2D(deviceRect.x() + deviceRect.width(), deviceRect.y() + deviceRect.height()));
        verts.push_back(QVector2D(deviceRect.x() + deviceRect.width(), deviceRect.y()));
    }
    vbo->setVertices(verts);
    vbo->render(GL_TRIANGLES);
    glDisable(GL_BLEND);
}

void ShowPaintEffect::paintQPainter()
{
    QColor color = s_colors[m_colorIndex];
    color.setAlphaF(s_alpha);
    for (const QRect &r : m_painted) {
        effects->scenePainter()->fillRect(r, color);
    }
}

} // namespace KWin

#include "moc_showpaint.cpp"
