/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_backend.h"

#include "virtual_egl_backend.h"
#include "virtual_output.h"

#include <fcntl.h>
#include <gbm.h>
#include <ranges>
#include <xf86drm.h>

namespace KWin
{

VirtualBackend::VirtualBackend(QObject *parent)
    : OutputBackend(parent)
{
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
    return {OpenGLCompositing};
}

std::unique_ptr<EglBackend> VirtualBackend::createOpenGLBackend(RenderDevice *renderDevice)
{
    return std::make_unique<VirtualEglBackend>(this, renderDevice);
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

} // namespace KWin

#include "moc_virtual_backend.cpp"
