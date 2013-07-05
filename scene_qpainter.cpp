/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "scene_qpainter.h"
// KWin
#include "client.h"
#include "composite.h"
#include "deleted.h"
#include "effects.h"
#include "main.h"
#include "paintredirector.h"
#include "toplevel.h"
#if HAVE_WAYLAND
#include "wayland_backend.h"
#endif
#include "workspace.h"
#include "xcbutils.h"
// Qt
#include <QDebug>
#include <QPainter>

namespace KWin
{

//****************************************
// QPainterBackend
//****************************************
QPainterBackend::QPainterBackend()
    : m_failed(false)
{
}

QPainterBackend::~QPainterBackend()
{
}

bool QPainterBackend::isLastFrameRendered() const
{
    return true;
}

OverlayWindow* QPainterBackend::overlayWindow()
{
    return NULL;
}

void QPainterBackend::showOverlay()
{
}

void QPainterBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}

void QPainterBackend::setFailed(const QString &reason)
{
    qWarning() << "Creating the XRender backend failed: " << reason;
    m_failed = true;
}

#if HAVE_WAYLAND
//****************************************
// WaylandQPainterBackend
//****************************************
static void handleFrameCallback(void *data, wl_callback *callback, uint32_t time)
{
    Q_UNUSED(data)
    Q_UNUSED(time)
    reinterpret_cast<WaylandQPainterBackend*>(data)->lastFrameRendered();

    if (callback) {
            wl_callback_destroy(callback);
    }
}

static const struct wl_callback_listener s_surfaceFrameListener = {
        handleFrameCallback
};

WaylandQPainterBackend::WaylandQPainterBackend()
    : QPainterBackend()
    , m_lastFrameRendered(true)
    , m_needsFullRepaint(true)
    , m_backBuffer(QImage(QSize(), QImage::Format_ARGB32_Premultiplied))
    , m_buffer(NULL)
{
    connect(Wayland::WaylandBackend::self()->shmPool(), SIGNAL(poolResized()), SLOT(remapBuffer()));
    connect(Wayland::WaylandBackend::self(), &Wayland::WaylandBackend::shellSurfaceSizeChanged,
            this, &WaylandQPainterBackend::screenGeometryChanged);
}

WaylandQPainterBackend::~WaylandQPainterBackend()
{
    if (m_buffer) {
        m_buffer->setUsed(false);
    }
}

bool WaylandQPainterBackend::isLastFrameRendered() const
{
    return m_lastFrameRendered;
}

bool WaylandQPainterBackend::usesOverlayWindow() const
{
    return false;
}

void WaylandQPainterBackend::present(int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Wayland::WaylandBackend *wl = Wayland::WaylandBackend::self();
    if (m_backBuffer.isNull()) {
        return;
    }
    m_lastFrameRendered = false;
    m_needsFullRepaint = false;
    wl_surface *surface = wl->surface();
    wl_callback *callback = wl_surface_frame(surface);
    wl_callback_add_listener(callback, &s_surfaceFrameListener, this);
    wl_surface_attach(surface, m_buffer->buffer(), 0, 0);
    Q_FOREACH (const QRect &rect, damage.rects()) {
        wl_surface_damage(surface, rect.x(), rect.y(), rect.width(), rect.height());
    }
    wl_surface_commit(surface);
    wl->dispatchEvents();
}

void WaylandQPainterBackend::lastFrameRendered()
{
    m_lastFrameRendered = true;
    Compositor::self()->lastFrameRendered();
}

void WaylandQPainterBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    m_buffer->setUsed(false);
    m_buffer = NULL;
}

QImage *WaylandQPainterBackend::buffer()
{
    return &m_backBuffer;
}

void WaylandQPainterBackend::prepareRenderingFrame()
{
    if (m_buffer) {
        if (m_buffer->isReleased()) {
            // we can re-use this buffer
            m_buffer->setReleased(false);
            return;
        } else {
            // buffer is still in use, get a new one
            m_buffer->setUsed(false);
        }
    }
    m_buffer = NULL;
    const QSize size(Wayland::WaylandBackend::self()->shellSurfaceSize());
    m_buffer = Wayland::WaylandBackend::self()->shmPool()->getBuffer(size, size.width() * 4);
    if (!m_buffer) {
        qDebug() << "Did not get a new Buffer from Shm Pool";
        m_backBuffer = QImage();
        return;
    }
    m_buffer->setUsed(true);
    m_backBuffer = QImage(m_buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    m_backBuffer.fill(Qt::transparent);
    m_needsFullRepaint = true;
    qDebug() << "Created a new back buffer";
}

void WaylandQPainterBackend::remapBuffer()
{
    if (!m_buffer || !m_buffer->isUsed()) {
        return;
    }
    const QSize size = m_backBuffer.size();
    m_backBuffer = QImage(m_buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    qDebug() << "Remapped our back buffer";
}

bool WaylandQPainterBackend::needsFullRepaint() const
{
    return m_needsFullRepaint;
}

#endif

//****************************************
// SceneQPainter
//****************************************
SceneQPainter *SceneQPainter::createScene()
{
    QScopedPointer<QPainterBackend> backend;
#if HAVE_WAYLAND
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        backend.reset(new WaylandQPainterBackend);
        if (backend->isFailed()) {
            return NULL;
        }
        return new SceneQPainter(backend.take());
    }
#endif
    return NULL;
}

SceneQPainter::SceneQPainter(QPainterBackend* backend)
    : Scene(Workspace::self())
    , m_backend(backend)
    , m_painter(new QPainter())
{
}

SceneQPainter::~SceneQPainter()
{
}

CompositingType SceneQPainter::compositingType() const
{
    return QPainterCompositing;
}

bool SceneQPainter::initFailed() const
{
    return false;
}

void SceneQPainter::paintGenericScreen(int mask, ScreenPaintData data)
{
    m_painter->save();
    m_painter->translate(data.xTranslation(), data.yTranslation());
    m_painter->scale(data.xScale(), data.yScale());
    Scene::paintGenericScreen(mask, data);
    m_painter->restore();
}

qint64 SceneQPainter::paint(QRegion damage, ToplevelList toplevels)
{
    QElapsedTimer renderTimer;
    renderTimer.start();

    createStackingOrder(toplevels);

    int mask = 0;
    m_backend->prepareRenderingFrame();
    m_painter->begin(m_backend->buffer());
    if (m_backend->needsFullRepaint()) {
        mask |= Scene::PAINT_SCREEN_BACKGROUND_FIRST;
        damage = QRegion(0, 0, displayWidth(), displayHeight());
    }
    QRegion updateRegion, validRegion;
    paintScreen(&mask, damage, QRegion(), &updateRegion, &validRegion);

    m_backend->showOverlay();

    m_painter->end();
    m_backend->present(mask, updateRegion);
    // do cleanup
    clearStackingOrder();

    return renderTimer.nsecsElapsed();
}

void SceneQPainter::paintBackground(QRegion region)
{
    m_painter->setBrush(Qt::black);
    m_painter->drawRects(region.rects());
}

Scene::Window *SceneQPainter::createWindow(Toplevel *toplevel)
{
    return new SceneQPainter::Window(this, toplevel);
}

Scene::EffectFrame *SceneQPainter::createEffectFrame(EffectFrameImpl *frame)
{
    return new QPainterEffectFrame(frame, this);
}

Shadow *SceneQPainter::createShadow(Toplevel *toplevel)
{
    return new SceneQPainterShadow(toplevel);
}

//****************************************
// SceneQPainter::Window
//****************************************
SceneQPainter::Window::Window(SceneQPainter *scene, Toplevel *c)
    : Scene::Window(c)
    , m_scene(scene)
{
}

SceneQPainter::Window::~Window()
{
    discardShape();
}

void SceneQPainter::Window::performPaint(int mask, QRegion region, WindowPaintData data)
{
    if (!(mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED)))
        region &= toplevel->visibleRect();

    if (region.isEmpty())
        return;
    QPainterWindowPixmap *pixmap = windowPixmap<QPainterWindowPixmap>();
    if (!pixmap || !pixmap->isValid()) {
        return;
    }
    if (!toplevel->damage().isEmpty()) {
        pixmap->update(toplevel->damage());
        toplevel->resetDamage();
    }

    QPainter *scenePainter = m_scene->painter();
    QPainter *painter = scenePainter;
    painter->setClipRegion(region);
    painter->setClipping(true);

    painter->save();
    painter->translate(x(), y());
    if (mask & PAINT_WINDOW_TRANSFORMED) {
        painter->translate(data.xTranslation(), data.yTranslation());
        painter->scale(data.xScale(), data.yScale());
    }

    const bool opaque = qFuzzyCompare(1.0, data.opacity());
    QImage tempImage;
    QPainter tempPainter;
    if (!opaque) {
        // need a temp render target which we later on blit to the screen
        tempImage = QImage(toplevel->visibleRect().size(), QImage::Format_ARGB32_Premultiplied);
        tempImage.fill(Qt::transparent);
        tempPainter.begin(&tempImage);
        tempPainter.save();
        tempPainter.translate(toplevel->geometry().topLeft() - toplevel->visibleRect().topLeft());
        painter = &tempPainter;
    }
    renderShadow(painter);
    renderWindowDecorations(painter);

    // render content
    const QRect src = QRect(toplevel->clientPos(), toplevel->clientSize());
    painter->drawImage(toplevel->clientPos(), pixmap->image(), src);

    if (!opaque) {
        tempPainter.restore();
        tempPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        QColor translucent(Qt::transparent);
        translucent.setAlphaF(data.opacity());
        tempPainter.fillRect(QRect(QPoint(0, 0), toplevel->visibleRect().size()), translucent);
        tempPainter.end();
        painter = scenePainter;
        painter->drawImage(toplevel->visibleRect().topLeft() - toplevel->geometry().topLeft(), tempImage);
    }

    painter->restore();

    painter->setClipRegion(QRegion());
    painter->setClipping(false);
}

void SceneQPainter::Window::renderShadow(QPainter* painter)
{
    if (!toplevel->shadow()) {
        return;
    }
    SceneQPainterShadow *shadow = static_cast<SceneQPainterShadow *>(toplevel->shadow());
    const QPixmap &topLeft     = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementTopLeft);
    const QPixmap &top         = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementTop);
    const QPixmap &topRight    = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementTopRight);
    const QPixmap &bottomLeft  = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementBottomLeft);
    const QPixmap &bottom      = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementBottom);
    const QPixmap &bottomRight = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementBottomRight);
    const QPixmap &left        = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementLeft);
    const QPixmap &right       = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementRight);

    const int leftOffset   = shadow->leftOffset();
    const int topOffset    = shadow->topOffset();
    const int rightOffset  = shadow->rightOffset();
    const int bottomOffset = shadow->bottomOffset();

    // top left
    painter->drawPixmap(-leftOffset, -topOffset, topLeft);
    // top right
    painter->drawPixmap(toplevel->width() - topRight.width() + rightOffset, -topOffset, topRight);
    // bottom left
    painter->drawPixmap(-leftOffset, toplevel->height() - bottomLeft.height() + bottomOffset, bottomLeft);
    // bottom right
    painter->drawPixmap(toplevel->width() - bottomRight.width() + rightOffset,
                        toplevel->height() - bottomRight.height() + bottomOffset,
                        bottomRight);
    // top
    painter->drawPixmap(topLeft.width() - leftOffset, -topOffset,
                        toplevel->width() - topLeft.width() - topRight.width() + leftOffset + rightOffset,
                        top.height(),
                        top);
    // left
    painter->drawPixmap(-leftOffset, topLeft.height() - topOffset, left.width(),
                        toplevel->height() - topLeft.height() - bottomLeft.height() + topOffset + bottomOffset,
                        left);
    // right
    painter->drawPixmap(toplevel->width() - right.width() + rightOffset,
                        topRight.height() - topOffset,
                        right.width(),
                        toplevel->height() - topRight.height() - bottomRight.height() + topOffset + bottomOffset,
                        right);
    // bottom
    painter->drawPixmap(bottomLeft.width() - leftOffset,
                        toplevel->height() - bottom.height() + bottomOffset,
                        toplevel->width() - bottomLeft.width() - bottomRight.width() + leftOffset + rightOffset,
                        bottom.height(),
                        bottom);
}

void SceneQPainter::Window::renderWindowDecorations(QPainter *painter)
{
    // TODO: custom decoration opacity
    Client *client = dynamic_cast<Client*>(toplevel);
    Deleted *deleted = dynamic_cast<Deleted*>(toplevel);
    if (!client && !deleted) {
        return;
    }

    bool noBorder = true;
    PaintRedirector *redirector = NULL;
    QRect dtr, dlr, drr, dbr;
    if (client && !client->noBorder()) {
        redirector = client->decorationPaintRedirector();
        client->layoutDecorationRects(dlr, dtr, drr, dbr, Client::WindowRelative);
        noBorder = false;
    } else if (deleted && !deleted->noBorder()) {
        noBorder = false;
        redirector = deleted->decorationPaintRedirector();
        deleted->layoutDecorationRects(dlr, dtr, drr, dbr);
    }
    if (noBorder || !redirector) {
        return;
    }

    redirector->ensurePixmapsPainted();
    const QImage *left   = redirector->leftDecoPixmap<const QImage *>();
    const QImage *top    = redirector->topDecoPixmap<const QImage *>();
    const QImage *right  = redirector->rightDecoPixmap<const QImage *>();
    const QImage *bottom = redirector->bottomDecoPixmap<const QImage *>();

    painter->drawImage(dtr, *top);
    painter->drawImage(dlr, *left);
    painter->drawImage(drr, *right);
    painter->drawImage(dbr, *bottom);

    redirector->markAsRepainted();
}

WindowPixmap *SceneQPainter::Window::createWindowPixmap()
{
    return new QPainterWindowPixmap(this);
}

//****************************************
// QPainterWindowPixmap
//****************************************
QPainterWindowPixmap::QPainterWindowPixmap(Scene::Window *window)
    : WindowPixmap(window)
    , m_shm(new Xcb::Shm)
{
}

QPainterWindowPixmap::~QPainterWindowPixmap()
{
}

void QPainterWindowPixmap::create()
{
    if (isValid() || !m_shm->isValid()) {
        return;
    }
    KWin::WindowPixmap::create();
    if (!isValid()) {
        return;
    }
    m_image = QImage((uchar*)m_shm->buffer(), size().width(), size().height(), QImage::Format_ARGB32_Premultiplied);
}

bool QPainterWindowPixmap::update(const QRegion &damage)
{
    Q_UNUSED(damage)
    if (!m_shm->isValid()) {
        return false;
    }

    // TODO: optimize by only updating the damaged areas
    xcb_shm_get_image_cookie_t cookie = xcb_shm_get_image_unchecked(connection(), pixmap(),
        0, 0, size().width(), size().height(),
        ~0, XCB_IMAGE_FORMAT_Z_PIXMAP, m_shm->segment(), 0);

    ScopedCPointer<xcb_shm_get_image_reply_t> image(xcb_shm_get_image_reply(connection(), cookie, NULL));
    if (image.isNull()) {
        return false;
    }
    return true;
}

QPainterEffectFrame::QPainterEffectFrame(EffectFrameImpl *frame, SceneQPainter *scene)
    : Scene::EffectFrame(frame)
    , m_scene(scene)
{
}

QPainterEffectFrame::~QPainterEffectFrame()
{
}

void QPainterEffectFrame::render(QRegion region, double opacity, double frameOpacity)
{
    Q_UNUSED(region)
    Q_UNUSED(opacity)
    // TODO: adjust opacity
    if (m_effectFrame->geometry().isEmpty()) {
        return; // Nothing to display
    }
    QPainter *painter = m_scene->painter();


    // Render the actual frame
    if (m_effectFrame->style() == EffectFrameUnstyled) {
        painter->save();
        painter->setPen(Qt::NoPen);
        QColor color(Qt::black);
        color.setAlphaF(frameOpacity);
        painter->setBrush(color);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRoundedRect(m_effectFrame->geometry().adjusted(-5, -5, 5, 5), 5.0, 5.0);
        painter->restore();
    } else if (m_effectFrame->style() == EffectFrameStyled) {
        qreal left, top, right, bottom;
        m_effectFrame->frame().getMargins(left, top, right, bottom);   // m_geometry is the inner geometry
        QRect geom = m_effectFrame->geometry().adjusted(-left, -top, right, bottom);
        painter->drawPixmap(geom, m_effectFrame->frame().framePixmap());
    }
    if (!m_effectFrame->selection().isNull()) {
        painter->drawPixmap(m_effectFrame->selection(), m_effectFrame->selectionFrame().framePixmap());
    }

    // Render icon
    if (!m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty()) {
        const QPoint topLeft(m_effectFrame->geometry().x(),
                             m_effectFrame->geometry().center().y() - m_effectFrame->iconSize().height() / 2);

        const QRect geom = QRect(topLeft, m_effectFrame->iconSize());
        painter->drawPixmap(geom, m_effectFrame->icon().pixmap(m_effectFrame->iconSize()));
    }

    // Render text
    if (!m_effectFrame->text().isEmpty()) {
        // Determine position on texture to paint text
        QRect rect(QPoint(0, 0), m_effectFrame->geometry().size());
        if (!m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty()) {
            rect.setLeft(m_effectFrame->iconSize().width());
        }

        // If static size elide text as required
        QString text = m_effectFrame->text();
        if (m_effectFrame->isStatic()) {
            QFontMetrics metrics(m_effectFrame->text());
            text = metrics.elidedText(text, Qt::ElideRight, rect.width());
        }

        painter->save();
        painter->setFont(m_effectFrame->font());
        if (m_effectFrame->style() == EffectFrameStyled) {
            painter->setPen(m_effectFrame->styledTextColor());
        } else {
            // TODO: What about no frame? Custom color setting required
            painter->setPen(Qt::white);
        }
        painter->drawText(rect.translated(m_effectFrame->geometry().topLeft()), m_effectFrame->alignment(), text);
        painter->restore();
    }
}

//****************************************
// QPainterShadow
//****************************************
SceneQPainterShadow::SceneQPainterShadow(Toplevel* toplevel)
    : Shadow(toplevel)
{
}

SceneQPainterShadow::~SceneQPainterShadow()
{
}

bool SceneQPainterShadow::prepareBackend()
{
    return true;
}

} // KWin
