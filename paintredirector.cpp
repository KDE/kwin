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
#include <kwinglutils.h>
#include <kwinxrenderutils.h>
#include <kdebug.h>
#include <QPaintEngine>
#include <qevent.h>
#include <qpainter.h>
#include <qmath.h>

namespace KWin
{

PaintRedirector *PaintRedirector::create(Client *c, QWidget *widget)
{
    if (effects->isOpenGLCompositing()) {
        return new OpenGLPaintRedirector(c, widget);
    } else {
        if (!Extensions::nonNativePixmaps()) {
            return new NativeXRenderPaintRedirector(c, widget);
        }
        return new RasterXRenderPaintRedirector(c, widget);
    }
}

PaintRedirector::PaintRedirector(Client *c, QWidget* w)
    : QObject(w)
    , widget(w)
    , recursionCheck(false)
    , m_client(c)
    , m_requiresRepaint(false)
{
    added(w);
}

PaintRedirector::~PaintRedirector()
{
}

void PaintRedirector::reparent(Deleted *d)
{
    setParent(d);
    widget = NULL;
    m_client = NULL;
}

void PaintRedirector::performPendingPaint()
{
    if (!widget) {
        return;
    }
    //qDebug() << "### performing paint, pending:" << pending.boundingRect();
    const QSize size = pending.boundingRect().size();
    QPaintDevice *scratch = this->scratch();
    if (scratch->width() < size.width() || scratch->height() < size.height()) {
        int w = (size.width() + 128) & ~128;
        int h = (size.height() + 128) & ~128;
        scratch = recreateScratch(QSize(qMax(scratch->width(), w), qMax(scratch->height(), h)));
    }
    fillScratch(Qt::transparent);
    recursionCheck = true;
    // do not use DrawWindowBackground, it's ok to be transparent
    widget->render(scratch, QPoint(), pending.boundingRect(), QWidget::DrawChildren);
    recursionCheck = false;
    cleanupTimer.start(2000, this);
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
        discardScratch();
    }
}

void PaintRedirector::ensurePixmapsPainted()
{
    if (pending.isEmpty() || !m_client)
        return;

    performPendingPaint();

    QRect rects[PixmapCount];
    m_client->layoutDecorationRects(rects[LeftPixmap], rects[TopPixmap], rects[RightPixmap], rects[BottomPixmap], Client::DecorationRelative);

    for (int i=0; i<PixmapCount; ++i) {
        repaintPixmap(DecorationPixmap(i), rects[i], pending);
    }

    pending = QRegion();
    scheduled = QRegion();

    xcb_flush(connection());
}

void PaintRedirector::preparePaint(const QPixmap &pending)
{
    Q_UNUSED(pending)
}

void PaintRedirector::repaintPixmap(DecorationPixmap border, const QRect &r, QRegion reg)
{
    if (!r.isValid())
        return;
    const QRect b = reg.boundingRect();
    reg &= r;
    if (reg.isEmpty())
        return;
    paint(border, r, b, reg);
}

void PaintRedirector::resizePixmaps()
{
    QRect rects[PixmapCount];
    m_client->layoutDecorationRects(rects[LeftPixmap], rects[TopPixmap], rects[RightPixmap], rects[BottomPixmap], Client::DecorationRelative);

    for (int i=0; i<PixmapCount; ++i) {
        resize(DecorationPixmap(i), rects[i].size());
    }

    // repaint
    if (widget) {
        widget->update();
    }
}

GLTexture *PaintRedirector::texture(PaintRedirector::DecorationPixmap border) const
{
    Q_UNUSED(border)
    return NULL;
}

xcb_render_picture_t PaintRedirector::picture(PaintRedirector::DecorationPixmap border) const
{
    Q_UNUSED(border)
    return XCB_RENDER_PICTURE_NONE;
}

ImageBasedPaintRedirector::ImageBasedPaintRedirector(Client *c, QWidget *widget)
    : PaintRedirector(c, widget)
{
}

ImageBasedPaintRedirector::~ImageBasedPaintRedirector()
{
}

QPaintDevice *ImageBasedPaintRedirector::recreateScratch(const QSize &size)
{
    m_scratchImage = QImage(size, QImage::Format_ARGB32_Premultiplied);
    return &m_scratchImage;
}

QPaintDevice *ImageBasedPaintRedirector::scratch()
{
    return &m_scratchImage;
}

void ImageBasedPaintRedirector::fillScratch(Qt::GlobalColor color)
{
    m_scratchImage.fill(color);
}

void ImageBasedPaintRedirector::discardScratch()
{
    m_scratchImage = QImage();
}

OpenGLPaintRedirector::OpenGLPaintRedirector(Client *c, QWidget *widget)
    : ImageBasedPaintRedirector(c, widget)
{
    for (int i=0; i<PixmapCount; ++i) {
        m_textures[i] = NULL;
    }
    resizePixmaps();
}

OpenGLPaintRedirector::~OpenGLPaintRedirector()
{
    for (int i=0; i<PixmapCount; ++i) {
        delete m_textures[i];
    }
}

GLTexture *OpenGLPaintRedirector::texture(PaintRedirector::DecorationPixmap border) const
{
    return m_textures[border];
}

void OpenGLPaintRedirector::resize(PaintRedirector::DecorationPixmap border, const QSize &size)
{
    if (!m_textures[border] || m_textures[border]->size() != size) {
        delete m_textures[border];
        m_textures[border] = new GLTexture(size.width(), size.height());
        m_textures[border]->setYInverted(true);
    }
}

void OpenGLPaintRedirector::preparePaint(const QPixmap &pending)
{
    m_tempImage = pending.toImage();
}

void OpenGLPaintRedirector::paint(PaintRedirector::DecorationPixmap border, const QRect &r, const QRect &b, const QRegion &reg)
{
    // clip the sub area
    const QRect bounding = reg.boundingRect();

    m_textures[border]->update(scratchImage(), bounding.topLeft() - r.topLeft(), QRect(bounding.topLeft() - b.topLeft(), bounding.size()));
}

RasterXRenderPaintRedirector::RasterXRenderPaintRedirector(Client *c, QWidget *widget)
    : ImageBasedPaintRedirector(c, widget)
    , m_gc(0)
{
    for (int i=0; i<PixmapCount; ++i) {
        m_pixmaps[i] = XCB_PIXMAP_NONE;
        m_pictures[i] = NULL;
    }
    resizePixmaps();
}

RasterXRenderPaintRedirector::~RasterXRenderPaintRedirector()
{
    for (int i=0; i<PixmapCount; ++i) {
        if (m_pixmaps[i] != XCB_PIXMAP_NONE) {
            xcb_free_pixmap(connection(), m_pixmaps[i]);
        }
        delete m_pictures[i];
    }
    if (m_gc != 0) {
        xcb_free_gc(connection(), m_gc);
    }
}

xcb_render_picture_t RasterXRenderPaintRedirector::picture(PaintRedirector::DecorationPixmap border) const
{
    return *m_pictures[border];
}

void RasterXRenderPaintRedirector::resize(PaintRedirector::DecorationPixmap border, const QSize &size)
{
    if (m_sizes[border] != size) {
        if (m_pixmaps[border] != XCB_PIXMAP_NONE) {
            xcb_free_pixmap(connection(), m_pixmaps[border]);
        }
        m_pixmaps[border] = xcb_generate_id(connection());
        xcb_create_pixmap(connection(), 32, m_pixmaps[border], rootWindow(), size.width(), size.height());
        delete m_pictures[border];
        m_pictures[border] = new XRenderPicture(m_pixmaps[border], 32);
    }
    // fill transparent
    xcb_rectangle_t rect = {0, 0, uint16_t(size.width()), uint16_t(size.height())};
    xcb_render_fill_rectangles(connection(), XCB_RENDER_PICT_OP_SRC, *m_pictures[border], preMultiply(Qt::transparent), 1, &rect);
}

void RasterXRenderPaintRedirector::preparePaint(const QPixmap &pending)
{
    m_tempImage = pending.toImage();
}

void RasterXRenderPaintRedirector::paint(PaintRedirector::DecorationPixmap border, const QRect &r, const QRect &b, const QRegion &reg)
{
    // clip the sub area
    const QRect bounding = reg.boundingRect();
    const QPoint offset = bounding.topLeft() - r.topLeft();
    if (m_gc == 0) {
        m_gc = xcb_generate_id(connection());
        xcb_create_gc(connection(), m_gc, m_pixmaps[border], 0, NULL);
    }

    const QImage img(scratchImage().copy(QRect(bounding.topLeft() - b.topLeft(), bounding.size())));
    xcb_put_image(connection(), XCB_IMAGE_FORMAT_Z_PIXMAP, m_pixmaps[border], m_gc,
                  img.width(), img.height(), offset.x(), offset.y(), 0, 32, img.byteCount(), img.constBits());
}

NativeXRenderPaintRedirector::NativeXRenderPaintRedirector(Client *c, QWidget *widget)
    : PaintRedirector(c, widget)
{
    resizePixmaps();
}

NativeXRenderPaintRedirector::~NativeXRenderPaintRedirector()
{
}

xcb_render_picture_t NativeXRenderPaintRedirector::picture(PaintRedirector::DecorationPixmap border) const
{
    return m_pixmaps[border].x11PictureHandle();
}

void NativeXRenderPaintRedirector::resize(PaintRedirector::DecorationPixmap border, const QSize &size)
{
    if (m_pixmaps[border].size() != size) {
        m_pixmaps[border] = QPixmap(size);
    }
    m_pixmaps[border].fill(Qt::transparent);
}

void NativeXRenderPaintRedirector::paint(PaintRedirector::DecorationPixmap border, const QRect &r, const QRect &b, const QRegion &reg)
{
    QPainter pt(&m_pixmaps[border]);
    pt.translate(-r.topLeft());
    pt.setCompositionMode(QPainter::CompositionMode_Source);
    pt.setClipRegion(reg);
    pt.drawPixmap(b.topLeft(), m_scratch);
    pt.end();
}

void NativeXRenderPaintRedirector::fillScratch(Qt::GlobalColor color)
{
    m_scratch.fill(color);
}

QPaintDevice *NativeXRenderPaintRedirector::recreateScratch(const QSize &size)
{
    m_scratch = QPixmap(size);
    return &m_scratch;
}

QPaintDevice *NativeXRenderPaintRedirector::scratch()
{
    return &m_scratch;
}

void NativeXRenderPaintRedirector::discardScratch()
{
    m_scratch = QPixmap();
}

} // namespace

#include "paintredirector.moc"
