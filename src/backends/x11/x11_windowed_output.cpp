/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_windowed_output.h"
#include "config-kwin.h"

#include "x11_windowed_backend.h"
#include "x11_windowed_logging.h"

#include "compositor.h"
#include "core/graphicsbuffer.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "core/renderloop_p.h"

#include <NETWM>

#if HAVE_X11_XINPUT
#include <X11/extensions/XInput2.h>
#endif

#include <QIcon>
#include <QPainter>

#include <drm_fourcc.h>
#include <ranges>
#include <xcb/dri3.h>
#include <xcb/shm.h>
#include <xcb/xinput.h>

namespace KWin
{

X11WindowedBuffer::X11WindowedBuffer(X11WindowedOutput *output, xcb_pixmap_t pixmap, GraphicsBuffer *graphicsBuffer)
    : m_output(output)
    , m_buffer(graphicsBuffer)
    , m_pixmap(pixmap)
{
    connect(graphicsBuffer, &GraphicsBuffer::destroyed, this, &X11WindowedBuffer::defunct);
}

X11WindowedBuffer::~X11WindowedBuffer()
{
    m_buffer->disconnect(this);
    xcb_free_pixmap(m_output->backend()->connection(), m_pixmap);
    unlock();
}

GraphicsBuffer *X11WindowedBuffer::buffer() const
{
    return m_buffer;
}

xcb_pixmap_t X11WindowedBuffer::pixmap() const
{
    return m_pixmap;
}

void X11WindowedBuffer::lock()
{
    if (!m_locked) {
        m_locked = true;
        m_buffer->ref();
    }
}

void X11WindowedBuffer::unlock()
{
    if (m_locked) {
        m_locked = false;
        m_buffer->unref();
    }
}

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

void X11WindowedCursor::update(const QImage &image, const QPointF &hotspot)
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
        const QImage img = image.scaled(targetSize, Qt::KeepAspectRatio).convertedTo(QImage::Format_ARGB32_Premultiplied);

        xcb_create_pixmap(connection, 32, pix, backend->screen()->root, img.width(), img.height());
        xcb_create_gc(connection, gc, pix, 0, nullptr);

        xcb_put_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc, img.width(), img.height(), 0, 0, 0, 32, img.sizeInBytes(), img.constBits());

        xcb_render_picture_t pic = xcb_generate_id(connection);
        xcb_render_create_picture(connection, pic, pix, backend->pictureFormatForDepth(32), 0, nullptr);
        xcb_render_create_cursor(connection, cid, pic, qRound(hotspot.x() * outputScale), qRound(hotspot.y() * outputScale));
        xcb_render_free_picture(connection, pic);
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
    : BackendOutput()
    , m_renderLoop(std::make_unique<RenderLoop>(this))
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
    m_buffers.clear();

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

void X11WindowedOutput::init(const QSize &pixelSize, qreal scale, bool fullscreen)
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

    const uint32_t presentEventMask = XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY | XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY;
    m_presentEvent = xcb_generate_id(m_backend->connection());
    xcb_present_select_input(m_backend->connection(), m_presentEvent, m_window, presentEventMask);

    m_winInfo = std::make_unique<NETWinInfo>(m_backend->connection(), m_window, m_backend->screen()->root,
                                             NET::WMWindowType, NET::Properties2());
    if (fullscreen) {
        m_winInfo->setState(NET::FullScreen, NET::FullScreen);
    }

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
    struct
    {
        xcb_input_event_mask_t head;
        xcb_input_xi_event_mask_t mask;
    } mask;
    mask.head.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
    mask.head.mask_len = 1;
    mask.mask = static_cast<xcb_input_xi_event_mask_t>(XCB_INPUT_XI_EVENT_MASK_RAW_TOUCH_BEGIN | XCB_INPUT_XI_EVENT_MASK_RAW_TOUCH_UPDATE | XCB_INPUT_XI_EVENT_MASK_TOUCH_END | XCB_INPUT_XI_EVENT_MASK_TOUCH_OWNERSHIP);
    xcb_input_xi_select_events(m_backend->connection(), m_window, 1, &mask.head);
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
    m_frame->presented(timestamp, PresentationMode::VSync);
    m_frame.reset();
}

void X11WindowedOutput::handlePresentIdleNotify(xcb_present_idle_notify_event_t *event)
{
    for (auto &[graphicsBuffer, x11Buffer] : m_buffers) {
        if (x11Buffer->pixmap() == event->pixmap) {
            x11Buffer->unlock();
            return;
        }
    }
}

void X11WindowedOutput::setWindowTitle(const QString &title)
{
    m_winInfo->setName(title.toUtf8().constData());
}

QPoint X11WindowedOutput::internalPosition() const
{
    return BackendOutput::position();
}

void X11WindowedOutput::setHostPosition(const QPoint &pos)
{
    m_hostPosition = pos;
}

QPointF X11WindowedOutput::mapFromGlobal(const QPointF &pos) const
{
    return (pos - hostPosition() + internalPosition()) / scale();
}

bool X11WindowedOutput::presentAsync(OutputLayer *layer, std::optional<std::chrono::nanoseconds> allowedVrrDelay)
{
    // Xorg moves the cursor, there's nothing to do
    return layer->type() == OutputLayerType::CursorOnly;
}

xcb_pixmap_t X11WindowedOutput::importDmaBufBuffer(const DmaBufAttributes *attributes)
{
    uint8_t depth;
    uint8_t bpp;
    switch (attributes->format) {
    case DRM_FORMAT_ARGB8888:
        depth = 32;
        bpp = 32;
        break;
    case DRM_FORMAT_XRGB8888:
        depth = 24;
        bpp = 32;
        break;
    default:
        qCWarning(KWIN_X11WINDOWED) << "Cannot import a buffer with unsupported format";
        return XCB_PIXMAP_NONE;
    }

    xcb_pixmap_t pixmap = xcb_generate_id(m_backend->connection());
    if (m_backend->driMajorVersion() >= 1 || m_backend->driMinorVersion() >= 2) {
        // xcb_dri3_pixmap_from_buffers() takes the ownership of the file descriptors.
        int fds[4] = {
            attributes->fd[0].duplicate().take(),
            attributes->fd[1].duplicate().take(),
            attributes->fd[2].duplicate().take(),
            attributes->fd[3].duplicate().take(),
        };
        xcb_dri3_pixmap_from_buffers(m_backend->connection(), pixmap, m_window, attributes->planeCount,
                                     attributes->width, attributes->height,
                                     attributes->pitch[0], attributes->offset[0],
                                     attributes->pitch[1], attributes->offset[1],
                                     attributes->pitch[2], attributes->offset[2],
                                     attributes->pitch[3], attributes->offset[3],
                                     depth, bpp, attributes->modifier, fds);
    } else {
        // xcb_dri3_pixmap_from_buffer() takes the ownership of the file descriptor.
        xcb_dri3_pixmap_from_buffer(m_backend->connection(), pixmap, m_window,
                                    attributes->height * attributes->pitch[0], attributes->width, attributes->height,
                                    attributes->pitch[0], depth, bpp, attributes->fd[0].duplicate().take());
    }

    return pixmap;
}

xcb_pixmap_t X11WindowedOutput::importShmBuffer(const ShmAttributes *attributes)
{
    // xcb_shm_attach_fd() takes the ownership of the passed shm file descriptor.
    FileDescriptor poolFileDescriptor = attributes->fd.duplicate();
    if (!poolFileDescriptor.isValid()) {
        qCWarning(KWIN_X11WINDOWED) << "Failed to duplicate shm file descriptor";
        return XCB_PIXMAP_NONE;
    }

    xcb_shm_seg_t segment = xcb_generate_id(m_backend->connection());
    xcb_shm_attach_fd(m_backend->connection(), segment, poolFileDescriptor.take(), 0);

    xcb_pixmap_t pixmap = xcb_generate_id(m_backend->connection());
    xcb_shm_create_pixmap(m_backend->connection(), pixmap, m_window, attributes->size.width(), attributes->size.height(), depth(), segment, 0);
    xcb_shm_detach(m_backend->connection(), segment);

    return pixmap;
}

xcb_pixmap_t X11WindowedOutput::importBuffer(GraphicsBuffer *graphicsBuffer)
{
    std::unique_ptr<X11WindowedBuffer> &x11Buffer = m_buffers[graphicsBuffer];
    if (!x11Buffer) {
        xcb_pixmap_t pixmap = XCB_PIXMAP_NONE;
        if (const DmaBufAttributes *attributes = graphicsBuffer->dmabufAttributes()) {
            pixmap = importDmaBufBuffer(attributes);
        } else if (const ShmAttributes *attributes = graphicsBuffer->shmAttributes()) {
            pixmap = importShmBuffer(attributes);
        }
        if (pixmap == XCB_PIXMAP_NONE) {
            return XCB_PIXMAP_NONE;
        }

        x11Buffer = std::make_unique<X11WindowedBuffer>(this, pixmap, graphicsBuffer);
        connect(x11Buffer.get(), &X11WindowedBuffer::defunct, this, [this, graphicsBuffer]() {
            m_buffers.erase(graphicsBuffer);
        });
    }

    x11Buffer->lock();
    return x11Buffer->pixmap();
}

void X11WindowedOutput::setPrimaryBuffer(GraphicsBuffer *buffer)
{
    m_pendingBuffer = importBuffer(buffer);
}

bool X11WindowedOutput::testPresentation(const std::shared_ptr<OutputFrame> &frame)
{
    return true;
}

bool X11WindowedOutput::present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    auto cursorLayers = Compositor::self()->backend()->compatibleOutputLayers(this) | std::views::filter([](OutputLayer *layer) {
        return layer->type() == OutputLayerType::CursorOnly;
    });
    if (!cursorLayers.empty()) {
        if (cursorLayers.front()->isEnabled()) {
            xcb_xfixes_show_cursor(m_backend->connection(), m_window);
            // the cursor layers update the image on their own already
        } else {
            xcb_xfixes_hide_cursor(m_backend->connection(), m_window);
        }
    }
    // we still present the window, even if the primary layer isn't in the list
    // in order to get presentation feedback
    if (!m_pendingBuffer) {
        return false;
    }

    xcb_xfixes_region_t valid = 0;
    xcb_xfixes_region_t update = 0;
    uint32_t serial = 0;
    uint32_t options = 0;
    uint64_t targetMsc = 0;

    xcb_present_pixmap(backend()->connection(),
                       window(),
                       m_pendingBuffer,
                       serial,
                       valid,
                       update,
                       0,
                       0,
                       XCB_NONE,
                       XCB_NONE,
                       XCB_NONE,
                       options,
                       targetMsc,
                       0,
                       0,
                       0,
                       nullptr);
    m_frame = frame;
    return true;
}

void X11WindowedOutput::setOutputLayers(std::vector<std::unique_ptr<OutputLayer>> &&layers)
{
    m_layers = std::move(layers);
}

QList<OutputLayer *> X11WindowedOutput::outputLayers() const
{
    return m_layers | std::views::transform(&std::unique_ptr<OutputLayer>::get) | std::ranges::to<QList>();
}

} // namespace KWin

#include "moc_x11_windowed_output.cpp"
