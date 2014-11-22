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
#include <QPaintEngine>
#include <qevent.h>
#include <qpainter.h>
#include <qmath.h>

namespace KWin
{

PaintRedirector *PaintRedirector::create(Client *c, KDecoration *deco)
{
    if (effects->isOpenGLCompositing()) {
        return new OpenGLPaintRedirector(c, deco);
    } else if (effects->compositingType() == XRenderCompositing) {
        return new RasterXRenderPaintRedirector(c, deco);
    } else if (effects->compositingType() == QPainterCompositing) {
        return new QImagePaintRedirector(c, deco);
    }
    return NULL;
}

PaintRedirector::PaintRedirector(Client *c, KDecoration *deco)
    : QObject(deco)
    , widget(deco->widget())
    , recursionCheck(false)
    , m_client(c)
    , m_decoration(deco)
    , m_requiresRepaint(false)
{
    added(deco->widget());
}

PaintRedirector::~PaintRedirector()
{
}

void PaintRedirector::reparent(Deleted *d)
{
    setParent(d);
    widget = NULL;
    m_client = NULL;
    m_decoration = NULL;
}

static int align(int value, int align)
{
    return (value + align - 1) & ~(align - 1);
}

void PaintRedirector::performPendingPaint()
{
    if (!widget && !m_decoration->window()) {
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
    m_decoration->render(scratch, pending.boundingRect());
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

void PaintRedirector::addRepaint(const QRegion &region)
{
    pending |= region;
    scheduled = pending;

    // schedule repaint
    const int paddingLeft = m_client->paddingLeft();
    const int paddingTop = m_client->paddingTop();
    const bool needsTranslate = (paddingLeft != 0 || paddingTop != 0);
    m_client->addRepaint(needsTranslate ? pending.translated(-paddingLeft, -paddingTop) : pending);
    m_requiresRepaint = true;
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
    if (!w) {
        return;
    }
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

const QImage *PaintRedirector::image(PaintRedirector::DecorationPixmap border) const
{
    Q_UNUSED(border)
    return NULL;
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




ImageBasedPaintRedirector::ImageBasedPaintRedirector(Client *c, KDecoration *deco)
    : PaintRedirector(c, deco)
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


OpenGLPaintRedirector::OpenGLPaintRedirector(Client *c, KDecoration *deco)
    : ImageBasedPaintRedirector(c, deco),
      m_texture(nullptr)
{
    PaintRedirector::resizePixmaps();
}

OpenGLPaintRedirector::~OpenGLPaintRedirector()
{
    delete m_texture;
}

void OpenGLPaintRedirector::resizePixmaps(const QRect *rects)
{
    QSize size;

    size.rwidth() = qMax(qMax(rects[TopPixmap].width(), rects[BottomPixmap].width()),
                         qMax(rects[LeftPixmap].height(), rects[RightPixmap].height()));
    size.rheight() = rects[TopPixmap].height() + rects[BottomPixmap].height() +
                     rects[LeftPixmap].width() + rects[RightPixmap].width() + 3;

    size.rwidth() = align(size.width(), 128);

    effects->makeOpenGLContextCurrent();

    if (m_texture && m_texture->size() == size)
        return;

    delete m_texture;
    m_texture = 0;

    if (!size.isEmpty()) {
        m_texture = new GLTexture(size.width(), size.height());
        m_texture->setYInverted(true);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_texture->clear();
    }
}

// Rotates the given source rect 90° counter-clockwise,
// and flips it vertically
static QImage rotate(const QImage &srcImage, const QRect &srcRect)
{
    QImage image(srcRect.height(), srcRect.width(), srcImage.format());

    const uint32_t *src = reinterpret_cast<const uint32_t *>(srcImage.bits());
    uint32_t *dst = reinterpret_cast<uint32_t *>(image.bits());

    for (int x = 0; x < image.width(); x++) {
        const uint32_t *s = src + (srcRect.y() + x) * srcImage.width() + srcRect.x();
        uint32_t *d = dst + x;

        for (int y = 0; y < image.height(); y++) {
            *d = s[y];
            d += image.width();
        }
    }

    return image;
}

void OpenGLPaintRedirector::updatePixmaps(const QRect *rects, const QRegion &region)
{
    const QImage &image = scratchImage();
    const QRect bounding = region.boundingRect();

    if (!m_texture)
        return;

    // Top, Right, Bottom, Left
    Qt::Orientation orientations[4] = { Qt::Horizontal, Qt::Vertical, Qt::Horizontal, Qt::Vertical };

    const int topHeight    = rects[TopPixmap].height();
    const int bottomHeight = rects[BottomPixmap].height();
    const int leftWidth    = rects[LeftPixmap].width();

    const QPoint offsets[4] = {
        QPoint(0, 0),                                         // Top
        QPoint(0, topHeight + bottomHeight + leftWidth + 3),  // Right
        QPoint(0, topHeight + 1),                             // Bottom
        QPoint(0, topHeight + bottomHeight + 2)               // Left
    };

    for (int i = 0; i < 4; i++) {
        const QRect dirty = (region & rects[i]).boundingRect();
        if (dirty.isEmpty())
            continue;

        const QPoint dst = dirty.topLeft() - rects[i].topLeft() + offsets[i];
        const QRect src(dirty.topLeft() - bounding.topLeft(), dirty.size());

        if (orientations[i] == Qt::Horizontal) {
            m_texture->update(image, dst, src);
        } else {
            // We have to rotate the src image
            const QImage im = rotate(image, src);
            m_texture->update(im, dst);
        }
    }
}




// ------------------------------------------------------------------




RasterXRenderPaintRedirector::RasterXRenderPaintRedirector(Client *c, KDecoration *deco)
    : ImageBasedPaintRedirector(c, deco)
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
    XRenderPicture *picture = m_pictures[border];
    if (!picture) {
        return XCB_RENDER_PICTURE_NONE;
    }
    return *picture;
}

void RasterXRenderPaintRedirector::resize(PaintRedirector::DecorationPixmap border, const QSize &size)
{
    if (m_sizes[border] != size) {
        m_sizes[border] = size;
        if (m_pixmaps[border] != XCB_PIXMAP_NONE) {
            xcb_free_pixmap(connection(), m_pixmaps[border]);
        }
        delete m_pictures[border];
        if (!size.isEmpty()) {
            m_pixmaps[border] = xcb_generate_id(connection());
            xcb_create_pixmap(connection(), 32, m_pixmaps[border], rootWindow(), size.width(), size.height());
            m_pictures[border] = new XRenderPicture(m_pixmaps[border], 32);
        } else {
            m_pixmaps[border] = XCB_PIXMAP_NONE;
            m_pictures[border] = nullptr;
        }
    }
    if (!m_pictures[border]) {
        return;
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

QImagePaintRedirector::QImagePaintRedirector(Client *c, KDecoration *deco)
    : ImageBasedPaintRedirector(c, deco)
{
    resizePixmaps();
}

QImagePaintRedirector::~QImagePaintRedirector()
{
}

void QImagePaintRedirector::paint(PaintRedirector::DecorationPixmap border, const QRect &r, const QRect &b, const QRegion &reg)
{
    // clip the sub area
    const QRect bounding = reg.boundingRect();
    const QPoint offset = bounding.topLeft() - r.topLeft();

    QPainter painter(&m_images[border]);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(offset, scratchImage(), QRect(bounding.topLeft() - b.topLeft(), bounding.size()));
}

void QImagePaintRedirector::resize(PaintRedirector::DecorationPixmap border, const QSize &size)
{
    m_images[border] = QImage(size, QImage::Format_ARGB32_Premultiplied);
    m_images[border].fill(Qt::transparent);
}

const QImage *QImagePaintRedirector::image(PaintRedirector::DecorationPixmap border) const
{
    return &m_images[border];
}

} // namespace

#include "paintredirector.moc"
