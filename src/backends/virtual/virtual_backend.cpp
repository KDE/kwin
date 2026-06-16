/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_backend.h"

#include "core/drmdevice.h"
#include "core/gpumanager.h"
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

VirtualBackend::VirtualBackend(QObject *parent)
    : OutputBackend(parent)
{
    const auto &devices = GpuManager::self()->renderDevices();
    if (!devices.empty()) {
        m_renderDevice = devices.front().get();
    }
}

VirtualBackend::~VirtualBackend()
{
    for (BackendOutput *output : m_outputs) {
        output->unref();
    }
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
    return m_renderDevice;
}

void VirtualBackend::setOutputChangeCheck(const std::function<OutputConfigurationError(const OutputConfiguration &config)> &check)
{
    m_outputChangeCheck = check;
}

OutputConfigurationError VirtualBackend::applyOutputChanges(const OutputConfiguration &config)
{
    if (m_outputChangeCheck) {
        auto err = m_outputChangeCheck(config);
        if (err != OutputConfigurationError::None) {
            return err;
        }
    }
    return OutputBackend::applyOutputChanges(config);
}

} // namespace KWin

#include "moc_virtual_backend.cpp"
