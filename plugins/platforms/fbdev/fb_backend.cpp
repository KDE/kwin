/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "fb_backend.h"

#include "composite.h"
#include "logging.h"
#include "logind.h"
#include "scene_qpainter_fb_backend.h"
#include "outputscreens.h"
#include "virtual_terminal.h"
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

FramebufferOutput::FramebufferOutput(QObject *parent):
    AbstractWaylandOutput(parent)
{
    setName("FB-0");
}

void FramebufferOutput::init(const QSize &pixelSize, const QSize &physicalSize)
{
    KWaylandServer::OutputDeviceInterface::Mode mode;
    mode.id = 0;
    mode.size = pixelSize;
    mode.flags = KWaylandServer::OutputDeviceInterface::ModeFlag::Current;
    mode.refreshRate = 60000;  // TODO: get actual refresh rate of fb device?
    initInterfaces("model_TODO", "manufacturer_TODO", "UUID_TODO", physicalSize, { mode });
}

FramebufferBackend::FramebufferBackend(QObject *parent)
    : Platform(parent)
{
}

FramebufferBackend::~FramebufferBackend()
{
    unmap();
    if (m_fd >= 0) {
        close(m_fd);
    }
}

Screens *FramebufferBackend::createScreens(QObject *parent)
{
    return new OutputScreens(this, parent);
}

QPainterBackend *FramebufferBackend::createQPainterBackend()
{
    return new FramebufferQPainterBackend(this);
}

void FramebufferBackend::init()
{
    setSoftWareCursor(true);
    LogindIntegration *logind = LogindIntegration::self();
    auto takeControl = [logind, this]() {
        if (logind->hasSessionControl()) {
            openFrameBuffer();
        } else {
            logind->takeControl();
            connect(logind, &LogindIntegration::hasSessionControlChanged, this, &FramebufferBackend::openFrameBuffer);
        }
    };
    if (logind->isConnected()) {
        takeControl();
    } else {
        connect(logind, &LogindIntegration::connectedChanged, this, takeControl);
    }
    VirtualTerminal::create(this);
}

void FramebufferBackend::openFrameBuffer()
{
    VirtualTerminal::self()->init();
    QString framebufferDevice = deviceIdentifier().constData();
    if (framebufferDevice.isEmpty()) {
        framebufferDevice = QString(Udev().primaryFramebuffer()->devNode());
    }
    int fd = LogindIntegration::self()->takeDevice(framebufferDevice.toUtf8().constData());
    qCDebug(KWIN_FB) << "Using frame buffer device:" << framebufferDevice;
    if (fd < 0) {
        qCWarning(KWIN_FB) << "Failed to open frame buffer device:" << framebufferDevice << "through logind, trying without";
    }
    fd = open(framebufferDevice.toUtf8().constData(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        qCWarning(KWIN_FB) << "failed to open frame buffer device:" << framebufferDevice;
        emit initFailed();
        return;
    }
    m_fd = fd;
    if (!handleScreenInfo()) {
        qCWarning(KWIN_FB) << "failed to handle framebuffer information";
        emit initFailed();
        return;
    }
    initImageFormat();
    if (m_imageFormat == QImage::Format_Invalid) {
        emit initFailed();
        return;
    }
    setReady(true);
    emit screensQueried();
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

    auto *output = new FramebufferOutput(this);
    output->init(QSize(varinfo.xres, varinfo.yres), QSize(varinfo.width, varinfo.height));
    m_outputs << output;

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
