/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mousemark.h"

// KConfigSkeleton
#include "mousemarkconfig.h"

#include <QAction>
#include <kwinconfig.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>
#include <KGlobalAccel>
#include <KLocalizedString>

#include <QPainter>

#include <cmath>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <xcb/render.h>
#endif

namespace KWin
{

#define NULL_POINT (QPoint( -1, -1 )) // null point is (0,0), which is valid :-/

MouseMarkEffect::MouseMarkEffect()
{
    initConfig<MouseMarkConfig>();
    QAction* a = new QAction(this);
    a->setObjectName(QStringLiteral("ClearMouseMarks"));
    a->setText(i18n("Clear All Mouse Marks"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::SHIFT + Qt::META + Qt::Key_F11);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::SHIFT + Qt::META + Qt::Key_F11);
    effects->registerGlobalShortcut(Qt::SHIFT + Qt::META + Qt::Key_F11, a);
    connect(a, &QAction::triggered, this, &MouseMarkEffect::clear);
    a = new QAction(this);
    a->setObjectName(QStringLiteral("ClearLastMouseMark"));
    a->setText(i18n("Clear Last Mouse Mark"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::SHIFT + Qt::META + Qt::Key_F12);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::SHIFT + Qt::META + Qt::Key_F12);
    effects->registerGlobalShortcut(Qt::SHIFT + Qt::META + Qt::Key_F12, a);
    connect(a, &QAction::triggered, this, &MouseMarkEffect::clearLast);

    connect(effects, &EffectsHandler::mouseChanged, this, &MouseMarkEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::screenLockingChanged, this, &MouseMarkEffect::screenLockingChanged);
    reconfigure(ReconfigureAll);
    arrow_start = NULL_POINT;
    effects->startMousePolling(); // We require it to detect activation as well
}

MouseMarkEffect::~MouseMarkEffect()
{
    effects->stopMousePolling();
}

static int width_2 = 1;
void MouseMarkEffect::reconfigure(ReconfigureFlags)
{
    MouseMarkConfig::self()->read();
    width = MouseMarkConfig::lineWidth();
    width_2 = width / 2;
    color = MouseMarkConfig::color();
    color.setAlphaF(1.0);
}

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
void MouseMarkEffect::addRect(const QPoint &p1, const QPoint &p2, xcb_rectangle_t *r, xcb_render_color_t *c)
{
    r->x = qMin(p1.x(), p2.x()) - width_2;
    r->y = qMin(p1.y(), p2.y()) - width_2;
    r->width = qAbs(p1.x()-p2.x()) + 1 + width_2;
    r->height = qAbs(p1.y()-p2.y()) + 1 + width_2;
    // fast move -> large rect, <strike>tess...</strike> interpolate a line
    if (r->width > 3*width/2 && r->height > 3*width/2) {
        const int n = sqrt(r->width*r->width + r->height*r->height) / width;
        xcb_rectangle_t *rects = new xcb_rectangle_t[n-1];
        const int w = p1.x() < p2.x() ? r->width : -r->width;
        const int h = p1.y() < p2.y() ? r->height : -r->height;
        for (int i = 1; i < n; ++i) {
            rects[i-1].x = p1.x() + i*w/n;
            rects[i-1].y = p1.y() + i*h/n;
            rects[i-1].width = rects[i-1].height = width;
        }
        xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, effects->xrenderBufferPicture(), *c, n - 1, rects);
        delete [] rects;
        r->x = p1.x();
        r->y = p1.y();
        r->width = r->height = width;
    }
}
#endif

void MouseMarkEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);   // paint normal screen
    if (marks.isEmpty() && drawing.isEmpty())
        return;
    if ( effects->isOpenGLCompositing()) {
        if (!GLPlatform::instance()->isGLES()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glEnable(GL_LINE_SMOOTH);
            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        }
        glLineWidth(width);
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->reset();
        vbo->setUseColor(true);
        vbo->setColor(color);
        ShaderBinder binder(ShaderTrait::UniformColor);
        binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());
        QVector<float> verts;
        foreach (const Mark & mark, marks) {
            verts.clear();
            verts.reserve(mark.size() * 2);
            foreach (const QPoint & p, mark) {
                verts << p.x() << p.y();
            }
            vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
            vbo->render(GL_LINE_STRIP);
        }
        if (!drawing.isEmpty()) {
            verts.clear();
            verts.reserve(drawing.size() * 2);
            foreach (const QPoint & p, drawing) {
                verts << p.x() << p.y();
            }
            vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
            vbo->render(GL_LINE_STRIP);
        }
        glLineWidth(1.0);
        if (!GLPlatform::instance()->isGLES()) {
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_BLEND);
        }
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if ( effects->compositingType() == XRenderCompositing) {
        xcb_render_color_t c = preMultiply(color);
        for (int i = 0; i < marks.count(); ++i) {
            const int n = marks[i].count() - 1;
            if (n > 0) {
                xcb_rectangle_t *rects = new xcb_rectangle_t[n];
                for (int j = 0; j < marks[i].count()-1; ++j) {
                    addRect(marks[i][j], marks[i][j+1], &rects[j], &c);
                }
                xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, effects->xrenderBufferPicture(), c, n, rects);
                delete [] rects;
            }
        }
        const int n = drawing.count() - 1;
        if (n > 0) {
            xcb_rectangle_t *rects = new xcb_rectangle_t[n];
            for (int i = 0; i < n; ++i)
                addRect(drawing[i], drawing[i+1], &rects[i], &c);
            xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_SRC, effects->xrenderBufferPicture(), c, n, rects);
            delete [] rects;
        }
    }
#endif
    if (effects->compositingType() == QPainterCompositing) {
        QPainter *painter = effects->scenePainter();
        painter->save();
        QPen pen(color);
        pen.setWidth(width);
        painter->setPen(pen);
        foreach (const Mark &mark, marks) {
            drawMark(painter, mark);
        }
        drawMark(painter, drawing);
        painter->restore();
    }
}

void MouseMarkEffect::drawMark(QPainter *painter, const Mark &mark)
{
    if (mark.count() <= 1) {
        return;
    }
    for (int i = 0; i < mark.count() - 1; ++i) {
        painter->drawLine(mark[i], mark[i+1]);
    }
}

void MouseMarkEffect::slotMouseChanged(const QPoint& pos, const QPoint&,
                                   Qt::MouseButtons, Qt::MouseButtons,
                                   Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (modifiers == (Qt::META | Qt::SHIFT | Qt::CTRL)) {  // start/finish arrow
        if (arrow_start != NULL_POINT) {
            marks.append(createArrow(arrow_start, pos));
            arrow_start = NULL_POINT;
            effects->addRepaintFull();
            return;
        } else
            arrow_start = pos;
    }
    if (arrow_start != NULL_POINT)
        return;
    // TODO the shortcuts now trigger this right before they're activated
    if (modifiers == (Qt::META | Qt::SHIFT)) {  // activated
        if (drawing.isEmpty())
            drawing.append(pos);
        if (drawing.last() == pos)
            return;
        QPoint pos2 = drawing.last();
        drawing.append(pos);
        QRect repaint = QRect(qMin(pos.x(), pos2.x()), qMin(pos.y(), pos2.y()),
                              qMax(pos.x(), pos2.x()), qMax(pos.y(), pos2.y()));
        repaint.adjust(-width, -width, width, width);
        effects->addRepaint(repaint);
    } else if (!drawing.isEmpty()) {
        marks.append(drawing);
        drawing.clear();
    }
}

void MouseMarkEffect::clear()
{
    drawing.clear();
    marks.clear();
    effects->addRepaintFull();
}

void MouseMarkEffect::clearLast()
{
    if (arrow_start != NULL_POINT) {
        arrow_start = NULL_POINT;
    } else if (!drawing.isEmpty()) {
        drawing.clear();
        effects->addRepaintFull();
    } else if (!marks.isEmpty()) {
        marks.pop_back();
        effects->addRepaintFull();
    }
}

MouseMarkEffect::Mark MouseMarkEffect::createArrow(QPoint arrow_start, QPoint arrow_end)
{
    Mark ret;
    double angle = atan2((double)(arrow_end.y() - arrow_start.y()), (double)(arrow_end.x() - arrow_start.x()));
    ret += arrow_start + QPoint(50 * cos(angle + M_PI / 6),
                                50 * sin(angle + M_PI / 6));   // right one
    ret += arrow_start;
    ret += arrow_end;
    ret += arrow_start; // it's connected lines, so go back with the middle one
    ret += arrow_start + QPoint(50 * cos(angle - M_PI / 6),
                                50 * sin(angle - M_PI / 6));   // left one
    return ret;
}

void MouseMarkEffect::screenLockingChanged(bool locked)
{
    if (!marks.isEmpty() || !drawing.isEmpty()) {
        effects->addRepaintFull();
    }
    // disable mouse polling while screen is locked.
    if (locked) {
        effects->stopMousePolling();
    } else {
        effects->startMousePolling();
    }
}

bool MouseMarkEffect::isActive() const
{
    return (!marks.isEmpty() || !drawing.isEmpty()) && !effects->isScreenLocked();
}


} // namespace

