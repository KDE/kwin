/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_backend.h"

#include "core/drmdevice.h"
#include "core/renderdevice.h"
#include "virtual_egl_backend.h"
#include "virtual_output.h"
#include "virtual_qpainter_backend.h"

#include <fcntl.h>
#include <gbm.h>
#include <ranges>
#include <xf86drm.h>

namespace KWin
{

static std::unique_ptr<RenderDevice> findRenderDevice()
{
#if !HAVE_LIBDRM_FAUX
#if defined(Q_OS_LINUX)
    // Workaround for libdrm being unaware of faux bus.
    if (qEnvironmentVariableIsSet("CI")) {
        return RenderDevice::open(QStringLiteral("/dev/dri/card1"));
    }
#endif
#endif

    const int deviceCount = drmGetDevices2(0, nullptr, 0);
    if (deviceCount <= 0) {
        return nullptr;
    }

    QList<drmDevice *> devices(deviceCount);
    if (drmGetDevices2(0, devices.data(), devices.size()) < 0) {
        return nullptr;
    }
    auto deviceCleanup = qScopeGuard([&devices]() {
        drmFreeDevices(devices.data(), devices.size());
    });

    for (drmDevice *device : std::as_const(devices)) {
        // If it's a vgem device, prefer the primary node because gbm will attempt to allocate
        // dumb buffers and they can be allocated only on the primary node.
        int nodeType = DRM_NODE_RENDER;
        if (device->bustype == DRM_BUS_PLATFORM) {
            if (strcmp(device->businfo.platform->fullname, "vgem") == 0) {
                nodeType = DRM_NODE_PRIMARY;
            }
        }
#if HAVE_LIBDRM_FAUX
        if (device->bustype == DRM_BUS_FAUX) {
            if (strcmp(device->businfo.faux->name, "vgem") == 0) {
                nodeType = DRM_NODE_PRIMARY;
            }
        }
#endif

        if (device->available_nodes & (1 << nodeType)) {
            if (auto ret = RenderDevice::open(device->nodes[nodeType])) {
                return ret;
            }
        }
    }

    return nullptr;
}

VirtualBackend::VirtualBackend(QObject *parent)
    : OutputBackend(parent)
    , m_renderDevice(findRenderDevice())
{
}

VirtualBackend::~VirtualBackend()
{
}

bool VirtualBackend::initialize()
{
    return true;
}

QList<CompositingType> VirtualBackend::supportedCompositors() const
{
    QList<CompositingType> compositingTypes;
    if (m_renderDevice) {
        compositingTypes.append(OpenGLCompositing);
    }
    compositingTypes.append(QPainterCompositing);
    return compositingTypes;
}

DrmDevice *VirtualBackend::drmDevice() const
{
    return m_renderDevice ? m_renderDevice->drmDevice() : nullptr;
}

std::unique_ptr<QPainterBackend> VirtualBackend::createQPainterBackend()
{
    return std::make_unique<VirtualQPainterBackend>(this);
}

std::unique_ptr<EglBackend> VirtualBackend::createOpenGLBackend()
{
    return std::make_unique<VirtualEglBackend>(this);
}

BackendOutput *VirtualBackend::createVirtualOutput(const QString &name, const QString &description, const QSize &size, qreal scale)
{
    return addOutput(OutputInfo{
        .size = size,
        .scale = scale,
        .connectorName = QStringLiteral("Virtual-") + name,
    });
}

void VirtualBackend::removeVirtualOutput(BackendOutput *output)
{
    if (auto virtualOutput = qobject_cast<VirtualOutput *>(output)) {
        removeOutput(virtualOutput);
    }
}

QList<BackendOutput *> VirtualBackend::outputs() const
{
    return m_outputs | std::ranges::to<QList<BackendOutput *>>();
}

VirtualOutput *VirtualBackend::createOutput(const OutputInfo &info)
{
    VirtualOutput *output = new VirtualOutput(this, info.internal, info.physicalSizeInMM, info.panelOrientation, info.edid, info.edidIdentifierOverride, info.connectorName, info.mstPath);
    output->init(info.size * info.scale, info.scale, info.modes);
    m_outputs.append(output);
    Q_EMIT outputAdded(output);
    return output;
}

BackendOutput *VirtualBackend::addOutput(const OutputInfo &info)
{
    VirtualOutput *output = createOutput(info);
    Q_EMIT outputsQueried();
    return output;
}

void VirtualBackend::removeOutput(VirtualOutput *output)
{
    if (m_outputs.removeOne(output)) {
        Q_EMIT outputRemoved(output);
        Q_EMIT outputsQueried();
        output->unref();
    }
}

void VirtualBackend::setVirtualOutputs(const QList<OutputInfo> &infos)
{
    const QList<VirtualOutput *> removed = m_outputs;

    for (const auto &info : infos) {
        createOutput(info);
    }

    for (VirtualOutput *output : removed) {
        m_outputs.removeOne(output);
        Q_EMIT outputRemoved(output);
        output->unref();
    }

    Q_EMIT outputsQueried();
}

RenderDevice *VirtualBackend::renderDevice() const
{
    return m_renderDevice.get();
}

EglDisplay *VirtualBackend::sceneEglDisplayObject() const
{
    return m_renderDevice->eglDisplay();
}

} // namespace KWin

#include "moc_virtual_backend.cpp"
