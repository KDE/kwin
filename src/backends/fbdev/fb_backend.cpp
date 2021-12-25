/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "fb_backend.h"

#include "backends/libinput/libinputbackend.h"
#include "composite.h"
#include "logging.h"
#include "main.h"
#include "platform.h"
#include "renderloop_p.h"
#include "scene_qpainter_fb_backend.h"
#include "session.h"
#include "softwarevsyncmonitor.h"
#include "udev.h"
// system
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
// Linux
#include <linux/fb.h>

namespace KWin
{
FramebufferOutput::FramebufferOutput(QObject *parent)
    : AbstractWaylandOutput(parent)
    , m_renderLoop(new RenderLoop(this))
{
    setName("FB-0");

    if (!m_vsyncMonitor) {
        SoftwareVsyncMonitor *monitor = SoftwareVsyncMonitor::create(this);
        monitor->setRefreshRate(m_renderLoop->refreshRate());
        connect(m_renderLoop, &RenderLoop::refreshRateChanged, this, [this, monitor]() {
            monitor->setRefreshRate(m_renderLoop->refreshRate());
        });
        m_vsyncMonitor = monitor;
    }

    connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &FramebufferOutput::vblank);
}

RenderLoop *FramebufferOutput::renderLoop() const
{
    return m_renderLoop;
}

VsyncMonitor *FramebufferOutput::vsyncMonitor() const
{
    return m_vsyncMonitor;
}

void FramebufferOutput::init(const QSize &pixelSize, const QSize &physicalSize)
{
    const int refreshRate = 60000; // TODO: get actual refresh rate of fb device?
    m_renderLoop->setRefreshRate(refreshRate);

    Mode mode;
    mode.id = 0;
    mode.size = pixelSize;
    mode.flags = ModeFlag::Current;
    mode.refreshRate = refreshRate;
    initialize("model_TODO", "manufacturer_TODO", "eisa_TODO", "serial_TODO", physicalSize, { mode }, {});
}

void FramebufferOutput::vblank(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_renderLoop);
    renderLoopPrivate->notifyFrameCompleted(timestamp);
}

FramebufferBackend::FramebufferBackend(QObject *parent)
    : Platform(parent)
    , m_session(Session::create(this))
{
    setPerScreenRenderingEnabled(true);
    setSupportsPointerWarping(true);
}

FramebufferBackend::~FramebufferBackend()
{
    unmap();
    if (m_fd >= 0) {
        close(m_fd);
    }
}

InputBackend *FramebufferBackend::createInputBackend()
{
    return new LibinputBackend(this);
}

QPainterBackend *FramebufferBackend::createQPainterBackend()
{
    return new FramebufferQPainterBackend(this);
}

Session *FramebufferBackend::session() const
{
    return m_session;
}

bool FramebufferBackend::initialize()
{
    QString framebufferDevice = deviceIdentifier();
    if (framebufferDevice.isEmpty()) {
        const auto fbs = Udev().listFramebuffers();
        if (fbs.size() > 0) {
            framebufferDevice = fbs.at(0)->devNode();
        }
    }
    int fd = open(framebufferDevice.toUtf8().constData(), O_RDWR | O_CLOEXEC);
    qCDebug(KWIN_FB) << "Using frame buffer device:" << framebufferDevice;
    if (fd < 0) {
        qCWarning(KWIN_FB) << "failed to open frame buffer device:" << framebufferDevice;
        return false;
    }
    m_fd = fd;
    if (!handleScreenInfo()) {
        qCWarning(KWIN_FB) << "failed to handle framebuffer information";
        return false;
    }
    initImageFormat();
    if (m_imageFormat == QImage::Format_Invalid) {
        return false;
    }
    setReady(true);
    Q_EMIT screensQueried();
    return true;
}

int FramebufferBackend::fileDescriptor() const
{
    return m_fd;
}

bool FramebufferBackend::handleScreenInfo()
{
    if (m_fd < 0) {
        return false;
    }

    fb_var_screeninfo varinfo;
    fb_fix_screeninfo fixinfo;

    // Probe the device for screen information.
    if (ioctl(m_fd, FBIOGET_FSCREENINFO, &fixinfo) < 0 || ioctl(m_fd, FBIOGET_VSCREENINFO, &varinfo) < 0) {
        return false;
    }

    // Activate the framebuffer device, assuming this is a non-primary framebuffer device
    varinfo.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
    ioctl(m_fd, FBIOPUT_VSCREENINFO, &varinfo);

    // Probe the device for new screen information.
    if (ioctl(m_fd, FBIOGET_VSCREENINFO, &varinfo) < 0) {
        return false;
    }

    auto *output = new FramebufferOutput;
    output->init(QSize(varinfo.xres, varinfo.yres), QSize(varinfo.width, varinfo.height));
    m_outputs << output;
    Q_EMIT outputAdded(output);
    Q_EMIT outputEnabled(output);

    m_id = QByteArray(fixinfo.id);
    m_red = {varinfo.red.offset, varinfo.red.length};
    m_green = {varinfo.green.offset, varinfo.green.length};
    m_blue = {varinfo.blue.offset, varinfo.blue.length};
    m_alpha = {varinfo.transp.offset, varinfo.transp.length};
    m_bitsPerPixel = varinfo.bits_per_pixel;
    m_bufferLength = fixinfo.smem_len;
    m_bytesPerLine = fixinfo.line_length;

    return true;
}

void FramebufferBackend::map()
{
    if (m_memory) {
        // already mapped;
        return;
    }
    if (m_fd < 0) {
        // not valid
        return;
    }
    void *mem = mmap(nullptr, m_bufferLength, PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (mem == MAP_FAILED) {
        qCWarning(KWIN_FB) << "Failed to mmap frame buffer";
        return;
    }
    m_memory = mem;
}

void FramebufferBackend::unmap()
{
    if (!m_memory) {
        return;
    }
    if (munmap(m_memory, m_bufferLength) < 0) {
        qCWarning(KWIN_FB) << "Failed to munmap frame buffer";
    }
    m_memory = nullptr;
}

QSize FramebufferBackend::screenSize() const
{
    if (m_outputs.isEmpty()) {
        return QSize();
    }
    return m_outputs[0]->pixelSize();
}

QImage::Format FramebufferBackend::imageFormat() const
{
    return m_imageFormat;
}

void FramebufferBackend::initImageFormat()
{
    if (m_fd < 0) {
        return;
    }

    qCDebug(KWIN_FB) << "Bits Per Pixel: " << m_bitsPerPixel;
    qCDebug(KWIN_FB) << "Buffer Length: " << m_bufferLength;
    qCDebug(KWIN_FB) << "Bytes Per Line: " << m_bytesPerLine;
    qCDebug(KWIN_FB) << "Alpha Length: " << m_alpha.length;
    qCDebug(KWIN_FB) << "Red Length: "   << m_red.length;
    qCDebug(KWIN_FB) << "Green Length: " << m_green.length;
    qCDebug(KWIN_FB) << "Blue Length: "  << m_blue.length;
    qCDebug(KWIN_FB) << "Blue Offset: "  << m_blue.offset;
    qCDebug(KWIN_FB) << "Green Offset: " << m_green.offset;
    qCDebug(KWIN_FB) << "Red Offset: "   << m_red.offset;
    qCDebug(KWIN_FB) << "Alpha Offset: " << m_alpha.offset;

    if (m_bitsPerPixel == 32 &&
            m_red.length == 8 &&
            m_green.length == 8 &&
            m_blue.length == 8 &&
            m_blue.offset == 0 &&
            m_green.offset == 8 &&
            m_red.offset == 16) {
        qCDebug(KWIN_FB) << "Framebuffer format is RGB32";
        m_imageFormat = QImage::Format_RGB32;
    } else if (m_bitsPerPixel == 32 &&
            m_red.length == 8 &&
            m_green.length == 8 &&
            m_blue.length == 8 &&
            m_alpha.length == 8 &&
            m_red.offset == 0 &&
            m_green.offset == 8 &&
            m_blue.offset == 16 &&
            m_alpha.offset == 24) {
        qCDebug(KWIN_FB) << "Framebuffer format is RGBA8888";
        m_imageFormat = QImage::Format_RGBA8888;
    } else if (m_bitsPerPixel == 24 &&
            m_red.length == 8 &&
            m_green.length == 8 &&
            m_blue.length == 8 &&
            m_blue.offset == 0 &&
            m_green.offset == 8 &&
            m_red.offset == 16) {
        qCDebug(KWIN_FB) << "Framebuffer Format is RGB888";
        m_bgr = true;
        m_imageFormat = QImage::Format_RGB888;
    } else if (m_bitsPerPixel == 16 &&
            m_red.length == 5 &&
            m_green.length == 6 &&
            m_blue.length == 5 &&
            m_blue.offset == 0 &&
            m_green.offset == 5 &&
            m_red.offset == 11) {
        qCDebug(KWIN_FB) << "Framebuffer Format is RGB16";
        m_imageFormat = QImage::Format_RGB16;
    } else {
        qCWarning(KWIN_FB) << "Framebuffer format is unknown";
    }
}

Outputs FramebufferBackend::outputs() const
{
    return m_outputs;
}

Outputs FramebufferBackend::enabledOutputs() const
{
    return m_outputs;
}

}
