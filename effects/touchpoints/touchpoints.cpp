/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "touchpoints.h"

#include <QAction>
#include <kwinglutils.h>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <kwinxrenderutils.h>
#include <xcb/xcb.h>
#include <xcb/render.h>
#endif

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QPainter>

#include <cmath>

namespace KWin
{

TouchPointsEffect::TouchPointsEffect()
    : Effect()
{
}

TouchPointsEffect::~TouchPointsEffect() = default;

static const Qt::GlobalColor s_colors[] = {
    Qt::blue,
    Qt::red,
    Qt::green,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::gray,
    Qt::darkBlue,
    Qt::darkRed,
    Qt::darkGreen
};

Qt::GlobalColor TouchPointsEffect::colorForId(quint32 id)
{
    auto it = m_colors.constFind(id);
    if (it != m_colors.constEnd()) {
        return it.value();
    }
    static int s_colorIndex = -1;
    s_colorIndex = (s_colorIndex + 1) % 10;
    m_colors.insert(id, s_colors[s_colorIndex]);
    return s_colors[s_colorIndex];
}

bool TouchPointsEffect::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(time)
    TouchPoint point;
    point.pos = pos;
    point.press = true;
    point.color = colorForId(id);
    m_points << point;
    m_latestPositions.insert(id, pos);
    repaint();
    return false;
}

bool TouchPointsEffect::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(time)
    TouchPoint point;
    point.pos = pos;
    point.press = true;
    point.color = colorForId(id);
    m_points << point;
    m_latestPositions.insert(id, pos);
    repaint();
    return false;
}

bool TouchPointsEffect::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(time)
    auto it = m_latestPositions.constFind(id);
    if (it != m_latestPositions.constEnd()) {
        TouchPoint point;
        point.pos = it.value();
        point.press = false;
        point.color = colorForId(id);
        m_points << point;
    }
    return false;
}

void TouchPointsEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    auto it = m_points.begin();
    while (it != m_points.end()) {
        it->time += time;
        if (it->time > m_ringLife) {
            it = m_points.erase(it);
        } else {
            it++;
        }
    }

    effects->prePaintScreen(data, time);
}

void TouchPointsEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);

    paintScreenSetup(mask, region, data);
    for (auto it = m_points.constBegin(), end = m_points.constEnd(); it != end; ++it) {
        for (int i = 0; i < m_ringCount; ++i) {
            float alpha = computeAlpha(it->time, i);
            float size = computeRadius(it->time, it->press, i);
            if (size > 0 && alpha > 0) {
                QColor color = it->color;
                color.setAlphaF(alpha);
                drawCircle(color, it->pos.x(), it->pos.y(), size);
            }
        }
    }
    paintScreenFinish(mask, region, data);
}

void TouchPointsEffect::postPaintScreen()
{
    effects->postPaintScreen();
    repaint();
}

float TouchPointsEffect::computeRadius(int time, bool press, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    if (press) {
        return ((time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
    }
    return ((m_ringLife - time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
}

float TouchPointsEffect::computeAlpha(int time, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    return (m_ringLife - (float)time - ringDistance * (ring)) / m_ringLife;
}

void TouchPointsEffect::repaint()
{
    if (!m_points.isEmpty()) {
        QRegion dirtyRegion;
        const int radius = m_ringMaxSize + m_lineWidth;
        for (auto it = m_points.constBegin(), end = m_points.constEnd(); it != end; ++it) {
            dirtyRegion |= QRect(it->pos.x() - radius, it->pos.y() - radius, 2*radius, 2*radius);
        }
        effects->addRepaint(dirtyRegion);
    }
}

bool TouchPointsEffect::isActive() const
{
    return !m_points.isEmpty();
}

void TouchPointsEffect::drawCircle(const QColor& color, float cx, float cy, float r)
{
    if (effects->isOpenGLCompositing())
        drawCircleGl(color, cx, cy, r);
    if (effects->compositingType() == XRenderCompositing)
        drawCircleXr(color, cx, cy, r);
    if (effects->compositingType() == QPainterCompositing)
        drawCircleQPainter(color, cx, cy, r);
}

void TouchPointsEffect::paintScreenSetup(int mask, QRegion region, ScreenPaintData& data)
{
    if (effects->isOpenGLCompositing())
        paintScreenSetupGl(mask, region, data);
}

void TouchPointsEffect::paintScreenFinish(int mask, QRegion region, ScreenPaintData& data)
{
    if (effects->isOpenGLCompositing())
        paintScreenFinishGl(mask, region, data);
}

void TouchPointsEffect::drawCircleGl(const QColor& color, float cx, float cy, float r)
{
    static const int num_segments = 80;
    static const float theta = 2 * 3.1415926 / float(num_segments);
    static const float c = cosf(theta); //precalculate the sine and cosine
    static const float s = sinf(theta);
    float t;

    float x = r;//we start at angle = 0
    float y = 0;

    GLVertexBuffer* vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    vbo->setColor(color);
    QVector<float> verts;
    verts.reserve(num_segments * 2);

    for (int ii = 0; ii < num_segments; ++ii) {
        verts << x + cx << y + cy;//output vertex
        //apply the rotation matrix
        t = x;
        x = c * x - s * y;
        y = s * t + c * y;
    }
    vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
    vbo->render(GL_LINE_LOOP);
}

void TouchPointsEffect::drawCircleXr(const QColor& color, float cx, float cy, float r)
{
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (r <= m_lineWidth)
        return;

    int num_segments = r+8;
    float theta = 2.0 * 3.1415926 / num_segments;
    float cos = cosf(theta); //precalculate the sine and cosine
    float sin = sinf(theta);
    float x[2] = {r, r-m_lineWidth};
    float y[2] = {0, 0};

#define DOUBLE_TO_FIXED(d) ((xcb_render_fixed_t) ((d) * 65536))
    QVector<xcb_render_pointfix_t> strip;
    strip.reserve(2*num_segments+2);

    xcb_render_pointfix_t point;
    point.x = DOUBLE_TO_FIXED(x[1]+cx);
    point.y = DOUBLE_TO_FIXED(y[1]+cy);
    strip << point;

    for (int i = 0; i < num_segments; ++i) {
        //apply the rotation matrix
        const float h[2] = {x[0], x[1]};
        x[0] = cos * x[0] - sin * y[0];
        x[1] = cos * x[1] - sin * y[1];
        y[0] = sin * h[0] + cos * y[0];
        y[1] = sin * h[1] + cos * y[1];

        point.x = DOUBLE_TO_FIXED(x[0]+cx);
        point.y = DOUBLE_TO_FIXED(y[0]+cy);
        strip << point;

        point.x = DOUBLE_TO_FIXED(x[1]+cx);
        point.y = DOUBLE_TO_FIXED(y[1]+cy);
        strip << point;
    }

    const float h = x[0];
    x[0] = cos * x[0] - sin * y[0];
    y[0] = sin * h    + cos * y[0];

    point.x = DOUBLE_TO_FIXED(x[0]+cx);
    point.y = DOUBLE_TO_FIXED(y[0]+cy);
    strip << point;

    XRenderPicture fill = xRenderFill(color);
    xcb_render_tri_strip(xcbConnection(), XCB_RENDER_PICT_OP_OVER,
                          fill, effects->xrenderBufferPicture(), 0,
                          0, 0, strip.count(), strip.constData());
#undef DOUBLE_TO_FIXED
#else
    Q_UNUSED(color)
    Q_UNUSED(cx)
    Q_UNUSED(cy)
    Q_UNUSED(r)
#endif
}

void TouchPointsEffect::drawCircleQPainter(const QColor &color, float cx, float cy, float r)
{
    QPainter *painter = effects->scenePainter();
    painter->save();
    painter->setPen(color);
    painter->drawArc(cx - r, cy - r, r * 2, r * 2, 0, 5760);
    painter->restore();
}

void TouchPointsEffect::paintScreenSetupGl(int, QRegion, ScreenPaintData &data)
{
    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::UniformColor);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());

    glLineWidth(m_lineWidth);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TouchPointsEffect::paintScreenFinishGl(int, QRegion, ScreenPaintData&)
{
    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}

} // namespace

