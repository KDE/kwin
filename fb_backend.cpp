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
#include "logind.h"
#include "scene_qpainter.h"
#include "screens_fb.h"
#include "virtual_terminal.h"
// Qt
#include <QDebug>
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
    : AbstractBackend(parent)
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
    return new FramebufferScreens(this, parent);
}

QPainterBackend *FramebufferBackend::createQPainterBackend()
{
    return new FramebufferQPainterBackend(this);
}

void FramebufferBackend::init()
{
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
    auto v = VirtualTerminal::create(this);
    v->init();
}

void FramebufferBackend::openFrameBuffer()
{
    int fd = LogindIntegration::self()->takeDevice(m_device.toUtf8().constData());
    if (fd < 0) {
        qCWarning(KWIN_CORE) << "Failed to open frame buffer device through logind, trying without";
    }
    fd = open(m_device.toUtf8().constData(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        qCWarning(KWIN_CORE) << "failed to open frame buffer device";
        return;
    }
    m_fd = fd;
    queryScreenInfo();
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
        qCWarning(KWIN_CORE) << "Failed to mmap frame buffer";
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
        qCWarning(KWIN_CORE) << "Failed to munmap frame buffer";
    }
    m_memory = nullptr;
}

QImage::Format FramebufferBackend::imageFormat() const
{
    if (m_fd < 0) {
        return QImage::Format_Invalid;
    }
    if (m_alpha.length == 0 &&
            m_red.length == 8 &&
            m_green.length == 8 &&
            m_blue.length == 8 &&
            m_blue.offset == 0 &&
            m_green.offset == 8 &&
            m_red.offset == 16) {
        return QImage::Format_RGB32;
    }
    return QImage::Format_Invalid;
}

}
