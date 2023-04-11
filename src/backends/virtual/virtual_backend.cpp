/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_backend.h"

#include "virtual_egl_backend.h"
#include "virtual_logging.h"
#include "virtual_output.h"
#include "virtual_qpainter_backend.h"

#include <fcntl.h>
#include <gbm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>

namespace KWin
{

static FileDescriptor findRenderNode()
{
    const int deviceCount = drmGetDevices2(0, nullptr, 0);
    if (!deviceCount) {
        return FileDescriptor{};
    }

    QVector<drmDevice *> devices(deviceCount);
    if (drmGetDevices2(0, devices.data(), devices.size()) < 0) {
        return FileDescriptor{};
    }
    auto deviceCleanup = qScopeGuard([&devices]() {
        drmFreeDevices(devices.data(), devices.size());
    });

    for (drmDevice *device : std::as_const(devices)) {
        if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
            FileDescriptor fd{open(device->nodes[DRM_NODE_RENDER], O_RDWR | O_CLOEXEC)};
            if (fd.isValid()) {
                return fd;
            }
        }
    }

    return FileDescriptor{};
}

VirtualBackend::VirtualBackend(QObject *parent)
    : OutputBackend(parent)
{
    m_drmFileDescriptor = findRenderNode();
    if (m_drmFileDescriptor.isValid()) {
        struct stat buf;
        if (fstat(m_drmFileDescriptor.get(), &buf) == -1) {
            qCWarning(KWIN_VIRTUAL) << "fstat() failed" << strerror(errno);
            return;
        }
        m_gbmDevice = gbm_create_device(m_drmFileDescriptor.get());
        m_renderDeviceId = buf.st_rdev;
    }
}

VirtualBackend::~VirtualBackend()
{
    if (sceneEglDisplay() != EGL_NO_DISPLAY) {
        eglTerminate(sceneEglDisplay());
    }
    if (m_gbmDevice) {
        gbm_device_destroy(m_gbmDevice);
    }
}

QVector<CompositingType> VirtualBackend::supportedCompositors() const
{
    QVector<CompositingType> compositingTypes;
    if (m_gbmDevice) {
        compositingTypes.append(OpenGLCompositing);
    }
    compositingTypes.append(QPainterCompositing);
    return compositingTypes;
}

gbm_device *VirtualBackend::gbmDevice() const
{
    return m_gbmDevice;
}

dev_t VirtualBackend::renderDeviceId() const
{
    return m_renderDeviceId;
}

bool VirtualBackend::initialize()
{
    return true;
}

std::unique_ptr<QPainterBackend> VirtualBackend::createQPainterBackend()
{
    return std::make_unique<VirtualQPainterBackend>(this);
}

std::unique_ptr<OpenGLBackend> VirtualBackend::createOpenGLBackend()
{
    return std::make_unique<VirtualEglBackend>(this);
}

Outputs VirtualBackend::outputs() const
{
    return m_outputs;
}

VirtualOutput *VirtualBackend::createOutput(const QPoint &position, const QSize &size, qreal scale)
{
    VirtualOutput *output = new VirtualOutput(this);
    output->init(position, size, scale);
    m_outputs.append(output);
    Q_EMIT outputAdded(output);
    output->updateEnabled(true);
    return output;
}

Output *VirtualBackend::addOutput(const QSize &size, qreal scale)
{
    VirtualOutput *output = createOutput(QPoint(), size * scale, scale);
    Q_EMIT outputsQueried();
    return output;
}

void VirtualBackend::setVirtualOutputs(const QVector<QRect> &geometries, QVector<qreal> scales)
{
    Q_ASSERT(scales.size() == 0 || scales.size() == geometries.size());

    const QVector<VirtualOutput *> removed = m_outputs;

    for (int i = 0; i < geometries.size(); i++) {
        createOutput(geometries[i].topLeft(), geometries[i].size(), scales.value(i, 1.0));
    }

    for (VirtualOutput *output : removed) {
        output->updateEnabled(false);
        m_outputs.removeOne(output);
        Q_EMIT outputRemoved(output);
        output->unref();
    }

    Q_EMIT outputsQueried();
}

} // namespace KWin
