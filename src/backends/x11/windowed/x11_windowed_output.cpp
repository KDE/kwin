/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_output.h"
#include "../common/kwinxrenderutils.h"
#include "x11_windowed_backend.h"
#include "x11_windowed_egl_backend.h"
#include "x11_windowed_qpainter_backend.h"

#include <config-kwin.h>

#include "composite.h"
#include "core/renderlayer.h"
#include "core/renderloop_p.h"
#include "cursorsource.h"
#include "scene/cursorscene.h"

#include <NETWM>

#if HAVE_X11_XINPUT
#include <X11/extensions/XInput2.h>
#endif

#include <QIcon>
#include <QPainter>

namespace KWin
{

X11WindowedCursor::X11WindowedCursor(X11WindowedOutput *output)
    : m_output(output)
{
}

X11WindowedCursor::~X11WindowedCursor()
{
    if (m_handle != XCB_CURSOR_NONE) {
        xcb_free_cursor(m_output->backend()->connection(), m_handle);
        m_handle = XCB_CURSOR_NONE;
    }
}

void X11WindowedCursor::update(const QImage &image, const QPoint &hotspot)
{
    X11WindowedBackend *backend = m_output->backend();

    xcb_connection_t *connection = backend->connection();
    xcb_pixmap_t pix = XCB_PIXMAP_NONE;
    xcb_gcontext_t gc = XCB_NONE;
    xcb_cursor_t cid = XCB_CURSOR_NONE;

    if (!image.isNull()) {
        pix = xcb_generate_id(connection);
        gc = xcb_generate_id(connection);
        cid = xcb_generate_id(connection);

        // right now on X we only have one scale between all screens, and we know we will have at least one screen
        const qreal outputScale = 1;
        const QSize targetSize = image.size() * outputScale / image.devicePixelRatio();
        const QImage img = image.scaled(targetSize, Qt::KeepAspectRatio);

        xcb_create_pixmap(connection, 32, pix, backend->screen()->root, img.width(), img.height());
        xcb_create_gc(connection, gc, pix, 0, nullptr);

        xcb_put_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc, img.width(), img.height(), 0, 0, 0, 32, img.sizeInBytes(), img.constBits());

        XRenderPicture pic(pix, 32);
        xcb_render_create_cursor(connection, cid, pic, qRound(hotspot.x() * outputScale), qRound(hotspot.y() * outputScale));
    }

    xcb_change_window_attributes(connection, m_output->window(), XCB_CW_CURSOR, &cid);

    if (pix) {
        xcb_free_pixmap(connection, pix);
    }
    if (gc) {
        xcb_free_gc(connection, gc);
    }

    if (m_handle) {
        xcb_free_cursor(connection, m_handle);
    }
    m_handle = cid;
    xcb_flush(connection);
}

X11WindowedOutput::X11WindowedOutput(X11WindowedBackend *backend)
    : Output(backend)
    , m_renderLoop(std::make_unique<RenderLoop>())
    , m_backend(backend)
{
    m_window = xcb_generate_id(m_backend->connection());

    static int identifier = -1;
    identifier++;
    setInformation(Information{
        .name = QStringLiteral("X11-%1").arg(identifier),
    });
}

X11WindowedOutput::~X11WindowedOutput()
{
    xcb_present_select_input(m_backend->connection(), m_presentEvent, m_window, 0);
    xcb_unmap_window(m_backend->connection(), m_window);
    xcb_destroy_window(m_backend->connection(), m_window);
    xcb_flush(m_backend->connection());
}

QRegion X11WindowedOutput::exposedArea() const
{
    return m_exposedArea;
}

void X11WindowedOutput::addExposedArea(const QRect &rect)
{
    m_exposedArea += rect;
}

void X11WindowedOutput::clearExposedArea()
{
    m_exposedArea = QRegion();
}

RenderLoop *X11WindowedOutput::renderLoop() const
{
    return m_renderLoop.get();
}

X11WindowedBackend *X11WindowedOutput::backend() const
{
    return m_backend;
}

X11WindowedCursor *X11WindowedOutput::cursor() const
{
    return m_cursor.get();
}

xcb_window_t X11WindowedOutput::window() const
{
    return m_window;
}

int X11WindowedOutput::depth() const
{
    return m_backend->screen()->root_depth;
}

QPoint X11WindowedOutput::hostPosition() const
{
    return m_hostPosition;
}

void X11WindowedOutput::init(const QSize &pixelSize, qreal scale)
{
    const int refreshRate = 60000; // TODO: get refresh rate via randr
    m_renderLoop->setRefreshRate(refreshRate);

    auto mode = std::make_shared<OutputMode>(pixelSize, m_renderLoop->refreshRate());

    State initialState;
    initialState.modes = {mode};
    initialState.currentMode = mode;
    initialState.scale = scale;
    setState(initialState);

    const uint32_t eventMask = XCB_EVENT_MASK_KEY_PRESS
        | XCB_EVENT_MASK_KEY_RELEASE
        | XCB_EVENT_MASK_BUTTON_PRESS
        | XCB_EVENT_MASK_BUTTON_RELEASE
        | XCB_EVENT_MASK_POINTER_MOTION
        | XCB_EVENT_MASK_ENTER_WINDOW
        | XCB_EVENT_MASK_LEAVE_WINDOW
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY
        | XCB_EVENT_MASK_EXPOSURE;

    const uint32_t values[] = {
        m_backend->screen()->black_pixel,
        eventMask,
    };

    uint32_t valueMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    xcb_create_window(m_backend->connection(),
                      XCB_COPY_FROM_PARENT,
                      m_window,
                      m_backend->screen()->root,
                      0, 0,
                      pixelSize.width(), pixelSize.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                      valueMask, values);

    // select xinput 2 events
    initXInputForWindow();

    const uint32_t presentEventMask = XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY;
    m_presentEvent = xcb_generate_id(m_backend->connection());
    xcb_present_select_input(m_backend->connection(), m_presentEvent, m_window, presentEventMask);

    m_winInfo = std::make_unique<NETWinInfo>(m_backend->connection(), m_window, m_backend->screen()->root,
                                             NET::WMWindowType, NET::Properties2());

    m_winInfo->setWindowType(NET::Normal);
    m_winInfo->setPid(QCoreApplication::applicationPid());
    QIcon windowIcon = QIcon::fromTheme(QStringLiteral("kwin"));
    auto addIcon = [&windowIcon, this](const QSize &size) {
        if (windowIcon.actualSize(size) != size) {
            return;
        }
        NETIcon icon;
        QImage windowImage = windowIcon.pixmap(size).toImage();
        icon.data = windowImage.bits();
        icon.size.width = size.width();
        icon.size.height = size.height();
        m_winInfo->setIcon(icon, false);
    };
    addIcon(QSize(16, 16));
    addIcon(QSize(32, 32));
    addIcon(QSize(48, 48));

    m_cursor = std::make_unique<X11WindowedCursor>(this);

    xcb_map_window(m_backend->connection(), m_window);
}

void X11WindowedOutput::initXInputForWindow()
{
    if (!m_backend->hasXInput()) {
        return;
    }
#if HAVE_X11_XINPUT
    XIEventMask evmasks[1];
    unsigned char mask1[XIMaskLen(XI_LASTEVENT)];

    memset(mask1, 0, sizeof(mask1));
    XISetMask(mask1, XI_TouchBegin);
    XISetMask(mask1, XI_TouchUpdate);
    XISetMask(mask1, XI_TouchOwnership);
    XISetMask(mask1, XI_TouchEnd);
    evmasks[0].deviceid = XIAllMasterDevices;
    evmasks[0].mask_len = sizeof(mask1);
    evmasks[0].mask = mask1;
    XISelectEvents(m_backend->display(), m_window, evmasks, 1);
#endif
}

void X11WindowedOutput::resize(const QSize &pixelSize)
{
    auto mode = std::make_shared<OutputMode>(pixelSize, m_renderLoop->refreshRate());

    State next = m_state;
    next.modes = {mode};
    next.currentMode = mode;
    setState(next);
}

void X11WindowedOutput::handlePresentCompleteNotify(xcb_present_complete_notify_event_t *event)
{
    std::chrono::microseconds timestamp(event->ust);
    RenderLoopPrivate::get(m_renderLoop.get())->notifyFrameCompleted(timestamp);
}

void X11WindowedOutput::setWindowTitle(const QString &title)
{
    m_winInfo->setName(title.toUtf8().constData());
}

QPoint X11WindowedOutput::internalPosition() const
{
    return geometry().topLeft();
}

void X11WindowedOutput::setHostPosition(const QPoint &pos)
{
    m_hostPosition = pos;
}

QPointF X11WindowedOutput::mapFromGlobal(const QPointF &pos) const
{
    return (pos - hostPosition() + internalPosition()) / scale();
}

bool X11WindowedOutput::setCursor(CursorSource *source)
{
    if (X11WindowedEglBackend *backend = qobject_cast<X11WindowedEglBackend *>(Compositor::self()->backend())) {
        renderCursorOpengl(backend, source);
    } else if (X11WindowedQPainterBackend *backend = qobject_cast<X11WindowedQPainterBackend *>(Compositor::self()->backend())) {
        renderCursorQPainter(backend, source);
    }

    return true;
}

bool X11WindowedOutput::moveCursor(const QPoint &position)
{
    // The cursor position is controlled by the host compositor.
    return true;
}

void X11WindowedOutput::renderCursorOpengl(X11WindowedEglBackend *backend, CursorSource *source)
{
    X11WindowedEglCursorLayer *cursorLayer = backend->cursorLayer(this);
    if (source) {
        cursorLayer->setSize(source->size());
        cursorLayer->setHotspot(source->hotspot());
    } else {
        cursorLayer->setSize(QSize());
        cursorLayer->setHotspot(QPoint());
    }

    std::optional<OutputLayerBeginFrameInfo> beginInfo = cursorLayer->beginFrame();
    if (!beginInfo) {
        return;
    }

    RenderTarget *renderTarget = &beginInfo->renderTarget;
    renderTarget->setDevicePixelRatio(scale());

    RenderLayer renderLayer(m_renderLoop.get());
    renderLayer.setDelegate(std::make_unique<SceneDelegate>(Compositor::self()->cursorScene()));

    renderLayer.delegate()->prePaint();
    renderLayer.delegate()->paint(renderTarget, infiniteRegion());
    renderLayer.delegate()->postPaint();

    cursorLayer->endFrame(infiniteRegion(), infiniteRegion());
}

void X11WindowedOutput::renderCursorQPainter(X11WindowedQPainterBackend *backend, CursorSource *source)
{
    X11WindowedQPainterCursorLayer *cursorLayer = backend->cursorLayer(this);
    if (source) {
        cursorLayer->setSize(source->size());
        cursorLayer->setHotspot(source->hotspot());
    } else {
        cursorLayer->setSize(QSize());
        cursorLayer->setHotspot(QPoint());
    }

    std::optional<OutputLayerBeginFrameInfo> beginInfo = cursorLayer->beginFrame();
    if (!beginInfo) {
        return;
    }

    RenderTarget *renderTarget = &beginInfo->renderTarget;
    renderTarget->setDevicePixelRatio(scale());

    RenderLayer renderLayer(m_renderLoop.get());
    renderLayer.setDelegate(std::make_unique<SceneDelegate>(Compositor::self()->cursorScene()));

    renderLayer.delegate()->prePaint();
    renderLayer.delegate()->paint(renderTarget, infiniteRegion());
    renderLayer.delegate()->postPaint();

    cursorLayer->endFrame(infiniteRegion(), infiniteRegion());
}

void X11WindowedOutput::updateEnabled(bool enabled)
{
    State next = m_state;
    next.enabled = enabled;
    setState(next);
}

} // namespace KWin
