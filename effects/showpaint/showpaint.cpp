/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showpaint.h"

#include <kwinglutils.h>
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <xcb/render.h>
#endif

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QPainter>

namespace KWin
{

static const qreal s_alpha = 0.2;
static const QVector<QColor> s_colors {
    Qt::red,
    Qt::green,
    Qt::blue,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::gray
};

ShowPaintEffect::ShowPaintEffect()
{
    auto *toggleAction = new QAction(this);
    toggleAction->setObjectName(QStringLiteral("Toggle"));
    toggleAction->setText(i18n("Toggle Show Paint"));
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {});
    KGlobalAccel::self()->setShortcut(toggleAction, {});
    effects->registerGlobalShortcut({}, toggleAction);

    connect(toggleAction, &QAction::triggered, this, &ShowPaintEffect::toggle);
}

void ShowPaintEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_painted = QRegion();
    effects->paintScreen(mask, region, data);
    if (effects->isOpenGLCompositing()) {
        paintGL(data.projectionMatrix());
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if (effects->compositingType() == XRenderCompositing) {
        paintXrender();
    }
#endif
    if (effects->compositingType() == QPainterCompositing) {
        paintQPainter();
    }
    if (++m_colorIndex == s_colors.count()) {
        m_colorIndex = 0;
    }
}

void ShowPaintEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    m_painted |= region;
    effects->paintWindow(w, mask, region, data);
}

void ShowPaintEffect::paintGL(const QMatrix4x4 &projection)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);
    ShaderBinder binder(ShaderTrait::UniformColor);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projection);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    QColor color = s_colors[m_colorIndex];
    color.setAlphaF(s_alpha);
    vbo->setColor(color);
    QVector<float> verts;
    verts.reserve(m_painted.rectCount() * 12);
    for (const QRect &r : m_painted) {
        verts << r.x() + r.width() << r.y();
        verts << r.x() << r.y();
        verts << r.x() << r.y() + r.height();
        verts << r.x() << r.y() + r.height();
        verts << r.x() + r.width() << r.y() + r.height();
        verts << r.x() + r.width() << r.y();
    }
    vbo->setData(verts.count() / 2, 2, verts.data(), nullptr);
    vbo->render(GL_TRIANGLES);
    glDisable(GL_BLEND);
}

void ShowPaintEffect::paintXrender()
{
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    xcb_render_color_t col;
    const QColor &color = s_colors[m_colorIndex];
    col.alpha = int(s_alpha * 0xffff);
    col.red = int(s_alpha * 0xffff * color.red() / 255);
    col.green = int(s_alpha * 0xffff * color.green() / 255);
    col.blue = int(s_alpha * 0xffff * color.blue() / 255);
    QVector<xcb_rectangle_t> rects;
    rects.reserve(m_painted.rectCount());
    for (const QRect &r : m_painted) {
        xcb_rectangle_t rect = {int16_t(r.x()), int16_t(r.y()), uint16_t(r.width()), uint16_t(r.height())};
        rects << rect;
    }
    xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_OVER, effects->xrenderBufferPicture(), col, rects.count(), rects.constData());
#endif
}

void ShowPaintEffect::paintQPainter()
{
    QColor color = s_colors[m_colorIndex];
    color.setAlphaF(s_alpha);
    for (const QRect &r : m_painted) {
        effects->scenePainter()->fillRect(r, color);
    }
}

bool ShowPaintEffect::isActive() const
{
    return m_active;
}

void ShowPaintEffect::toggle()
{
    m_active = !m_active;
    effects->addRepaintFull();
}

} // namespace KWin
