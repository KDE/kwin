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
#include "virtual_backend.h"
#include "scene_qpainter_virtual_backend.h"
#include "screens_virtual.h"
#include "wayland_server.h"
#include "egl_gbm_backend.h"
// Qt
#include <QTemporaryDir>
// KWayland
#include <KWayland/Server/seat_interface.h>
// system
#include <fcntl.h>
#include <unistd.h>
#include <config-kwin.h>
#if HAVE_GBM
#include <gbm.h>
#endif
#include <colorcorrection/manager.h>

namespace KWin
{

VirtualBackend::VirtualBackend(QObject *parent)
    : Platform(parent)
{
    if (qEnvironmentVariableIsSet("KWIN_WAYLAND_VIRTUAL_SCREENSHOTS")) {
        m_screenshotDir.reset(new QTemporaryDir);
        if (!m_screenshotDir->isValid()) {
            m_screenshotDir.reset();
        }
        if (!m_screenshotDir.isNull()) {
            qDebug() << "Screenshots saved to: " << m_screenshotDir->path();
        }
    }
    setSupportsPointerWarping(true);
    setSupportsGammaControl(true);
}

VirtualBackend::~VirtualBackend()
{
#if HAVE_GBM
    if (m_gbmDevice) {
        gbm_device_destroy(m_gbmDevice);
    }
#endif
    if (m_drmFd != -1) {
        close(m_drmFd);
    }
}

void VirtualBackend::init()
{
    setSoftWareCursor(true);
    m_size = initialWindowSize();
    setReady(true);
    waylandServer()->seat()->setHasPointer(true);
    waylandServer()->seat()->setHasKeyboard(true);
    waylandServer()->seat()->setHasTouch(true);
    emit screensQueried();
}

QString VirtualBackend::screenshotDirPath() const
{
    if (m_screenshotDir.isNull()) {
        return QString();
    }
    return m_screenshotDir->path();
}

Screens *VirtualBackend::createScreens(QObject *parent)
{
    return new VirtualScreens(this, parent);
}

QPainterBackend *VirtualBackend::createQPainterBackend()
{
    return new VirtualQPainterBackend(this);
}

OpenGLBackend *VirtualBackend::createOpenGLBackend()
{
    return new EglGbmBackend(this);
}

int VirtualBackend::gammaRampSize(int screen) const {
    return m_gammaSizes[screen];
}

bool VirtualBackend::setGammaRamp(int screen, ColorCorrect::GammaRamp &gamma) {
    Q_UNUSED(gamma);
    return m_gammaResults[screen];
}

}
