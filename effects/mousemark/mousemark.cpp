/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "mousemark.h"

// KConfigSkeleton
#include "mousemarkconfig.h"

#include <kwinconfig.h>
#include <kwinglutils.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>

#include <math.h>

#include <kdebug.h>

namespace KWin
{

#define NULL_POINT (QPoint( -1, -1 )) // null point is (0,0), which is valid :-/

KWIN_EFFECT(mousemark, MouseMarkEffect)

MouseMarkEffect::MouseMarkEffect()
{
    KActionCollection* actionCollection = new KActionCollection(this);
    KAction* a = static_cast< KAction* >(actionCollection->addAction("ClearMouseMarks"));
    a->setText(i18n("Clear All Mouse Marks"));
    a->setGlobalShortcut(KShortcut(Qt::SHIFT + Qt::META + Qt::Key_F11));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(clear()));
    a = static_cast< KAction* >(actionCollection->addAction("ClearLastMouseMark"));
    a->setText(i18n("Clear Last Mouse Mark"));
    a->setGlobalShortcut(KShortcut(Qt::SHIFT + Qt::META + Qt::Key_F12));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(clearLast()));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            this, SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
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
    MouseMarkConfig::self()->readConfig();
    width = MouseMarkConfig::lineWidth();
    width_2 = width / 2;
    color = MouseMarkConfig::color();
    color.setAlphaF(1.0);
}

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
void MouseMarkEffect::addRect(const QPoint &p1, const QPoint &p2, XRectangle *r, XRenderColor *c)
{
    r->x = qMin(p1.x(), p2.x()) - width_2;
    r->y = qMin(p1.y(), p2.y()) - width_2;
    r->width = qAbs(p1.x()-p2.x()) + 1 + width_2;
    r->height = qAbs(p1.y()-p2.y()) + 1 + width_2;
    // fast move -> large rect, <strike>tess...</strike> interpolate a line
    if (r->width > 3*width/2 && r->height > 3*width/2) {
        const int n = sqrt(r->width*r->width + r->height*r->height) / width;
        XRectangle *rects = new XRectangle[n-1];
        const int w = p1.x() < p2.x() ? r->width : -r->width;
        const int h = p1.y() < p2.y() ? r->height : -r->height;
        for (int i = 1; i < n; ++i) {
            rects[i-1].x = p1.x() + i*w/n;
            rects[i-1].y = p1.y() + i*h/n;
            rects[i-1].width = rects[i-1].height = width;
        }
        XRenderFillRectangles(display(), PictOpSrc, effects->xrenderBufferPicture(), c, rects, n - 1);
        delete [] rects;
        r->x = p1.x();
        r->y = p1.y();
        r->width = r->height = width;
    }
}
#endif

void MouseMarkEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);   // paint normal screen
    if (marks.isEmpty() && drawing.isEmpty())
        return;
    if ( effects->isOpenGLCompositing()) {
#ifndef KWIN_HAVE_OPENGLES
        glEnable(GL_LINE_SMOOTH);
#endif
        glLineWidth(width);
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->reset();
        vbo->setUseColor(true);
        vbo->setColor(color);
        if (ShaderManager::instance()->isValid()) {
            ShaderManager::instance()->pushShader(ShaderManager::ColorShader);
        }
        QVector<float> verts;
        foreach (const Mark & mark, marks) {
            verts.clear();
            verts.reserve(mark.size() * 2);
            foreach (const QPoint & p, mark) {
                verts << p.x() << p.y();
            }
            vbo->setData(verts.size() / 2, 2, verts.data(), NULL);
            vbo->render(GL_LINE_STRIP);
        }
        if (!drawing.isEmpty()) {
            verts.clear();
            verts.reserve(drawing.size() * 2);
            foreach (const QPoint & p, drawing) {
                verts << p.x() << p.y();
            }
            vbo->setData(verts.size() / 2, 2, verts.data(), NULL);
            vbo->render(GL_LINE_STRIP);
        }
        if (ShaderManager::instance()->isValid()) {
            ShaderManager::instance()->popShader();
        }
        glLineWidth(1.0);
    #ifndef KWIN_HAVE_OPENGLES
        glDisable(GL_LINE_SMOOTH);
    #endif
    }
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if ( effects->compositingType() == XRenderCompositing) {
        XRenderColor c = preMultiply(color);
        for (int i = 0; i < marks.count(); ++i) {
            const int n = marks[i].count() - 1;
            if (n > 0) {
                XRectangle *rects = new XRectangle[n];
                for (int j = 0; j < marks[i].count()-1; ++j) {
                    addRect(marks[i][j], marks[i][j+1], &rects[j], &c);
                }
                XRenderFillRectangles(display(), PictOpSrc, effects->xrenderBufferPicture(), &c, rects, n);
                delete [] rects;
            }
        }
        const int n = drawing.count() - 1;
        if (n > 0) {
            XRectangle *rects = new XRectangle[n];
            for (int i = 0; i < n; ++i)
                addRect(drawing[i], drawing[i+1], &rects[i], &c);
            XRenderFillRectangles(display(), PictOpSrc, effects->xrenderBufferPicture(), &c, rects, n);
            delete [] rects;
        }
    }
#endif
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

bool MouseMarkEffect::isActive() const
{
    return !marks.isEmpty() || !drawing.isEmpty();
}


} // namespace

#include "mousemark.moc"
