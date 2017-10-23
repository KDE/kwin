/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "fb_backend.h"
#include "composite.h"
#include "logging.h"
#include "logind.h"
#include "scene_qpainter_fb_backend.h"
#include "screens.h"
#include "virtual_terminal.h"
// system
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
// Linux
#include <linux/fb.h>

namespace KWin
{

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
    return new BasicScreens(this, parent);
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
    int fd = LogindIntegration::self()->takeDevice(deviceIdentifier().constData());
    if (fd < 0) {
        qCWarning(KWIN_FB) << "Failed to open frame buffer device through logind, trying without";
    }
    fd = open(deviceIdentifier().constData(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        qCWarning(KWIN_FB) << "failed to open frame buffer device";
        emit initFailed();
        return;
    }
    m_fd = fd;
    if (!queryScreenInfo()) {
        qCWarning(KWIN_FB) << "failed to query framebuffer information";
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

bool FramebufferBackend::queryScreenInfo()
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
    m_resolution = QSize(varinfo.xres, varinfo.yres);
    m_physicalSize = QSize(varinfo.width, varinfo.height);
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
    }
    qCWarning(KWIN_FB) << "Framebuffer format is unknown";
}

}
