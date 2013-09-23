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
#include <kwinglplatform.h>
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

static int align(int value, int align)
{
    return (value + align - 1) & ~(align - 1);
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
        int w = align(size.width(), 128);
        int h = align(size.height(), 128);
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

    updatePixmaps(rects, pending);

    pending = QRegion();
    scheduled = QRegion();

    xcb_flush(connection());
}

void PaintRedirector::updatePixmaps(const QRect *rects, const QRegion &region)
{
    for (int i = 0; i < PixmapCount; ++i) {
        if (!rects[i].isValid())
            continue;

        const QRect bounding = region.boundingRect();
        const QRegion reg = region & rects[i];

        if (reg.isEmpty())
            continue;

        paint(DecorationPixmap(i), rects[i], bounding, reg);
    }
}

void PaintRedirector::preparePaint(const QPixmap &pending)
{
    Q_UNUSED(pending)
}

void PaintRedirector::resizePixmaps()
{
    QRect rects[PixmapCount];
    m_client->layoutDecorationRects(rects[LeftPixmap], rects[TopPixmap], rects[RightPixmap], rects[BottomPixmap], Client::DecorationRelative);

    resizePixmaps(rects);

    // repaint
    if (widget) {
        widget->update();
    }
}

void PaintRedirector::resizePixmaps(const QRect *rects)
{
    for (int i = 0; i < PixmapCount; ++i) {
        resize(DecorationPixmap(i), rects[i].size());
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

void PaintRedirector::resize(DecorationPixmap border, const QSize &size)
{
    Q_UNUSED(border)
    Q_UNUSED(size)
}

void PaintRedirector::paint(DecorationPixmap border, const QRect& r, const QRect &b, const QRegion &reg)
{
    Q_UNUSED(border)
    Q_UNUSED(r)
    Q_UNUSED(b)
    Q_UNUSED(reg)
}




// ------------------------------------------------------------------




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



// ------------------------------------------------------------------


OpenGLPaintRedirector::OpenGLPaintRedirector(Client *c, QWidget *widget)
    : ImageBasedPaintRedirector(c, widget)
{
    for (int i = 0; i < TextureCount; ++i)
        m_textures[i] = NULL;

    PaintRedirector::resizePixmaps();
}

OpenGLPaintRedirector::~OpenGLPaintRedirector()
{
    for (int i = 0; i < TextureCount; ++i)
        delete m_textures[i];
}

void OpenGLPaintRedirector::resizePixmaps(const QRect *rects)
{
    QSize size[2];
    size[LeftRight] = QSize(rects[LeftPixmap].width() + rects[RightPixmap].width(),
                            align(qMax(rects[LeftPixmap].height(), rects[RightPixmap].height()), 128));
    size[TopBottom] = QSize(align(qMax(rects[TopPixmap].width(), rects[BottomPixmap].width()), 128),
                            rects[TopPixmap].height() + rects[BottomPixmap].height());

    if (!GLTexture::NPOTTextureSupported()) {
        for (int i = 0; i < 2; i++) {
            size[i].rwidth()  = nearestPowerOfTwo(size[i].width());
            size[i].rheight() = nearestPowerOfTwo(size[i].height());
        }
    }

    for (int i = 0; i < 2; i++) {
        if (m_textures[i] && m_textures[i]->size() == size[i])
            continue;

        delete m_textures[i];
        m_textures[i] = NULL;

        if (size[i].isEmpty())
            continue;

        m_textures[i] = new GLTexture(size[i].width(), size[i].height());
        m_textures[i]->setYInverted(true);
        m_textures[i]->setWrapMode(GL_CLAMP_TO_EDGE);
        m_textures[i]->clear();
    }
}

void OpenGLPaintRedirector::preparePaint(const QPixmap &pending)
{
    m_tempImage = pending.toImage();
}

void OpenGLPaintRedirector::updatePixmaps(const QRect *rects, const QRegion &region)
{
    const QImage &image = scratchImage();
    const QRect bounding = region.boundingRect();

    const int leftWidth = rects[LeftPixmap].width();
    const int topHeight = rects[TopPixmap].height();

    // Top, Right, Bottom, Left
    GLTexture *textures[4] = { m_textures[TopBottom], m_textures[LeftRight], m_textures[TopBottom], m_textures[LeftRight] };
    QPoint offsets[4] = { QPoint(0, 0), QPoint(leftWidth, 0), QPoint(0, topHeight), QPoint(0, 0) };

    for (int i = 0; i < 4; i++) {
        const QRect dirty = (region & rects[i]).boundingRect();
        if (!textures[i] || dirty.isEmpty())
            continue;

        const QPoint dst = dirty.topLeft() - rects[i].topLeft() + offsets[i];
        const QRect src(dirty.topLeft() - bounding.topLeft(), dirty.size());

        textures[i]->update(image, dst, src);
    }
}




// ------------------------------------------------------------------




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
