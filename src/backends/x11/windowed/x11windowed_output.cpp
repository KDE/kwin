/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11windowed_output.h"
#include <config-kwin.h>
#include "renderloop_p.h"
#include "softwarevsyncmonitor.h"
#include "x11windowed_backend.h"

#include <NETWM>

#if HAVE_X11_XINPUT
#include <X11/extensions/XInput2.h>
#endif

#include <QIcon>

namespace KWin
{

X11WindowedOutput::X11WindowedOutput(X11WindowedBackend *backend)
    : AbstractWaylandOutput(backend)
    , m_renderLoop(new RenderLoop(this))
    , m_vsyncMonitor(SoftwareVsyncMonitor::create(this))
    , m_backend(backend)
{
    m_window = xcb_generate_id(m_backend->connection());

    static int identifier = -1;
    identifier++;
    setName("X11-" + QString::number(identifier));

    connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &X11WindowedOutput::vblank);
}

X11WindowedOutput::~X11WindowedOutput()
{
    xcb_unmap_window(m_backend->connection(), m_window);
    xcb_destroy_window(m_backend->connection(), m_window);
    delete m_winInfo;
    xcb_flush(m_backend->connection());
}

RenderLoop *X11WindowedOutput::renderLoop() const
{
    return m_renderLoop;
}

SoftwareVsyncMonitor *X11WindowedOutput::vsyncMonitor() const
{
    return m_vsyncMonitor;
}

void X11WindowedOutput::init(const QPoint &logicalPosition, const QSize &pixelSize)
{
    const int refreshRate = 60000; // TODO: get refresh rate via randr
    m_renderLoop->setRefreshRate(refreshRate);
    m_vsyncMonitor->setRefreshRate(refreshRate);

    Mode mode;
    mode.id = 0;
    mode.size = pixelSize;
    mode.flags = ModeFlag::Current;
    mode.refreshRate = refreshRate;

    // Physicial size must be adjusted, such that QPA calculates correct sizes of
    // internal elements.
    const QSize physicalSize = pixelSize / 96.0 * 25.4 / m_backend->initialOutputScale();
    initialize("model_TODO", "manufacturer_TODO", "eisa_TODO", "serial_TODO", physicalSize, { mode }, {});
    setGeometry(logicalPosition, pixelSize);
    setScale(m_backend->initialOutputScale());

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    const uint32_t values[] = {
        m_backend->screen()->black_pixel,
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_LEAVE_WINDOW |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_EXPOSURE
    };
    xcb_create_window(m_backend->connection(),
                      XCB_COPY_FROM_PARENT,
                      m_window,
                      m_backend->screen()->root,
                      0, 0,
                      pixelSize.width(), pixelSize.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                      mask, values);

    // select xinput 2 events
    initXInputForWindow();

    m_winInfo = new NETWinInfo(m_backend->connection(),
                               m_window,
                               m_backend->screen()->root,
                               NET::WMWindowType, NET::Properties2());

    m_winInfo->setWindowType(NET::Normal);
    m_winInfo->setPid(QCoreApplication::applicationPid());
    QIcon windowIcon = QIcon::fromTheme(QStringLiteral("kwin"));
    auto addIcon = [&windowIcon, this] (const QSize &size) {
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

void X11WindowedOutput::setGeometry(const QPoint &logicalPosition, const QSize &pixelSize)
{
    // TODO: set mode to have updated pixelSize
    Q_UNUSED(pixelSize);
    moveTo(logicalPosition);
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

void X11WindowedOutput::vblank(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    renderLoopPrivate->notifyFrameCompleted(timestamp);
}

bool X11WindowedOutput::usesSoftwareCursor() const
{
    return false;
}

}
