/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "openvr_backend.h"

#include "openvr_egl_backend.h"
#include "openvr_output.h"

#include <fcntl.h>
#include <gbm.h>
#include <ranges>
#include <xf86drm.h>

namespace KWin
{

OpenVRBackend::OpenVRBackend(QObject *parent)
    : OutputBackend(parent)
{
}

OpenVRBackend::~OpenVRBackend()
{
    for (BackendOutput *output : m_outputs) {
        output->unref();
    }
}

bool OpenVRBackend::initialize()
{
    return true;
}

QList<CompositingType> OpenVRBackend::supportedCompositors() const
{
    return {OpenGLCompositing};
}

std::unique_ptr<EglBackend> OpenVRBackend::createOpenGLBackend(RenderDevice *renderDevice)
{
    return std::make_unique<OpenVREglBackend>(this, renderDevice);
}

BackendOutput *OpenVRBackend::createVirtualOutput(const QString &name, const QString &description, const QSize &size, qreal scale)
{
    return addOutput(OutputInfo{
        .size = size,
        .scale = scale,
        .capabilities = BackendOutput::Capability::CustomModes,
        .connectorName = QStringLiteral("OpenVR-") + name,
    });
}

void OpenVRBackend::removeVirtualOutput(BackendOutput *output)
{
    if (auto openVROutput = qobject_cast<OpenVROutput *>(output)) {
        removeOutput(openVROutput);
    }
}

QList<BackendOutput *> OpenVRBackend::outputs() const
{
    return m_outputs | std::ranges::to<QList<BackendOutput *>>();
}

OpenVROutput *OpenVRBackend::createOutput(const OutputInfo &info)
{
    OpenVROutput *output = new OpenVROutput(this, info.internal, info.physicalSizeInMM, info.panelOrientation, info.edid, info.edidIdentifierOverride, info.connectorName, info.mstPath, info.capabilities);
    output->init(info.size * info.scale, info.scale, info.modes);
    m_outputs.append(output);
    Q_EMIT outputAdded(output);
    return output;
}

BackendOutput *OpenVRBackend::addOutput(const OutputInfo &info)
{
    OpenVROutput *output = createOutput(info);
    Q_EMIT outputsQueried();
    return output;
}

void OpenVRBackend::removeOutput(OpenVROutput *output)
{
    if (m_outputs.removeOne(output)) {
        Q_EMIT outputRemoved(output);
        Q_EMIT outputsQueried();
        output->unref();
    }
}

void OpenVRBackend::setOpenVROutputs(const QList<OutputInfo> &infos)
{
    const QList<OpenVROutput *> removed = m_outputs;

    for (const auto &info : infos) {
        createOutput(info);
    }

    for (OpenVROutput *output : removed) {
        m_outputs.removeOne(output);
        Q_EMIT outputRemoved(output);
        output->unref();
    }

    Q_EMIT outputsQueried();
}

} // namespace KWin

#include "moc_openvr_backend.cpp"
