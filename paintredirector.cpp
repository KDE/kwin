/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2009 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#include "paintredirector.h"

#include "client.h"
#include "deleted.h"
#include "effects.h"
#include <kdebug.h>
#include <QPaintEngine>
#include <qevent.h>
#include <qpainter.h>
#include <qmath.h>

namespace KWin
{

PaintRedirector::PaintRedirector(Client *c, QWidget* w)
    : QObject(w)
    , widget(w)
    , recursionCheck(false)
    , m_client(c)
    , m_responsibleForPixmap(!effects->isOpenGLCompositing())
    , m_requiresRepaint(false)
{
    added(w);
    resizePixmaps();
}

PaintRedirector::~PaintRedirector()
{
    if (m_responsibleForPixmap) {
        for (int i=0; i<PixmapCount; ++i) {
            XFreePixmap(display(), m_pixmaps[i].handle());
        }
    }
}

void PaintRedirector::reparent(Deleted *d)
{
    setParent(d);
    widget = NULL;
    m_client = NULL;
}

QPixmap PaintRedirector::performPendingPaint()
{
    if (!widget) {
        return QPixmap();
    }
    //qDebug() << "### performing paint, pending:" << pending.boundingRect();
    const QSize size = pending.boundingRect().size();
    if (scratch.width() < size.width() || scratch.height() < size.height()) {
        int w = (size.width() + 128) & ~128;
        int h = (size.height() + 128) & ~128;
        scratch = QPixmap(qMax(scratch.width(), w), qMax(scratch.height(), h));
    }
    scratch.fill(Qt::transparent);
    recursionCheck = true;
    // do not use DrawWindowBackground, it's ok to be transparent
    widget->render(&scratch, QPoint(), pending.boundingRect(), QWidget::DrawChildren);
    recursionCheck = false;
    cleanupTimer.start(2000, this);
    return scratch;
}

bool PaintRedirector::isToolTip(QWidget *object) const
{
    // ### We need a more reliable way of doing this
    return object->windowFlags() & Qt::ToolTip;
}

bool PaintRedirector::eventFilter(QObject* o, QEvent* e)
{
    if (!widget || !m_client) {
        return false;
    }
    switch(e->type()) {
    case QEvent::ChildAdded: {
        QChildEvent* c = static_cast< QChildEvent* >(e);
        if (c->child()->isWidgetType() && !isToolTip(static_cast< QWidget* >(c->child())))
            added(static_cast< QWidget* >(c->child()));
        break;
    }
    case QEvent::ChildRemoved: {
        QChildEvent* c = static_cast< QChildEvent* >(e);
        if (c->child()->isWidgetType())
            removed(static_cast< QWidget* >(c->child()));
        break;
    }
    case QEvent::Paint: {
        if (!recursionCheck) {
            QPaintEvent* pe = static_cast< QPaintEvent* >(e);
            QWidget* w = static_cast< QWidget* >(o);
            pending |= pe->region().translated(w->mapTo(widget, QPoint(0, 0)));
            scheduled = pending;

            // schedule repaint
            const int paddingLeft = m_client->paddingLeft();
            const int paddingTop = m_client->paddingTop();
            const bool needsTranslate = (paddingLeft != 0 || paddingTop != 0);
            m_client->addRepaint(needsTranslate ? pending.translated(-paddingLeft, -paddingTop) : pending);
            m_requiresRepaint = true;
            return true; // filter out
        }
    }
    default:
        break;
    }
    return false;
}

QRegion PaintRedirector::pendingRegion() const
{
    return pending;
}

QRegion PaintRedirector::scheduledRepaintRegion()
{
    QRegion tempRegion = scheduled;
    scheduled = QRegion();
    return tempRegion;
}

void PaintRedirector::added(QWidget* w)
{
    w->installEventFilter(this);
    foreach (QObject * o, w->children()) {
        if (o->isWidgetType() && !isToolTip(static_cast< QWidget* >(o)))
            added(static_cast< QWidget* >(o));
    }
}

void PaintRedirector::removed(QWidget* w)
{
    foreach (QObject * o, w->children()) {
        if (o->isWidgetType())
            removed(static_cast< QWidget* >(o));
    }
    w->installEventFilter(this);
}

void PaintRedirector::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == cleanupTimer.timerId()) {
        cleanupTimer.stop();
        scratch = QPixmap();
    }
}

void PaintRedirector::ensurePixmapsPainted()
{
    if (pending.isEmpty() || !m_client)
        return;

    QPixmap p = performPendingPaint();

    QRect rects[PixmapCount];
    m_client->layoutDecorationRects(rects[LeftPixmap], rects[TopPixmap], rects[RightPixmap], rects[BottomPixmap], Client::DecorationRelative);

    for (int i=0; i<PixmapCount; ++i) {
        repaintPixmap(m_pixmaps[i], rects[i], p, pending);
    }

    pending = QRegion();
    scheduled = QRegion();

    XSync(display(), false);
}

void PaintRedirector::repaintPixmap(QPixmap &pix, const QRect &r, const QPixmap &src, QRegion reg)
{
    if (!r.isValid())
        return;
    QRect b = reg.boundingRect();
    reg &= r;
    if (reg.isEmpty())
        return;
    QPainter pt(&pix);
    pt.translate(-r.topLeft());
    pt.setCompositionMode(QPainter::CompositionMode_Source);
    pt.setClipRegion(reg);
    pt.drawPixmap(b.topLeft(), src);
    pt.end();
}

void PaintRedirector::resizePixmaps()
{
    QRect rects[PixmapCount];
    m_client->layoutDecorationRects(rects[LeftPixmap], rects[TopPixmap], rects[RightPixmap], rects[BottomPixmap], Client::DecorationRelative);

    for (int i=0; i<PixmapCount; ++i) {
        if (m_pixmaps[i].size() == rects[i].size()) {
            // size unchanged, no need to recreate, but repaint
            m_pixmaps[i].fill(Qt::transparent);
            continue;
        }
        if (!m_responsibleForPixmap) {
            m_pixmaps[i] = QPixmap(rects[i].size());
        } else {
            if (!m_pixmaps[i].isNull() && m_pixmaps[i].paintEngine()->type() == QPaintEngine::X11) {
                XFreePixmap(display(), m_pixmaps[i].handle());
            }
            Pixmap xpix = XCreatePixmap(QX11Info::display(), rootWindow(),
                                        rects[i].size().width(), rects[i].height(),
                                        32);
            m_pixmaps[i] = QPixmap::fromX11Pixmap(xpix, QPixmap::ExplicitlyShared);
        }
        // Make sure the pixmaps are created with alpha channels
        m_pixmaps[i].fill(Qt::transparent);
    }

    // repaint
    if (widget) {
        widget->update();
    }
}

} // namespace

#include "paintredirector.moc"
