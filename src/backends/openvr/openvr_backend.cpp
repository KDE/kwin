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
#include <openvr.h>
#include <ranges>
#include <xf86drm.h>

namespace KWin
{

namespace
{

QString getTrackedDeviceString(vr::IVRSystem *vrSystem, vr::TrackedDeviceIndex_t device, vr::TrackedDeviceProperty property)
{
    char buffer[128];
    vr::TrackedPropertyError error;
    vrSystem->GetStringTrackedDeviceProperty(device, property, buffer, sizeof(buffer), &error);
    if (error != vr::TrackedProp_Success) {
        return QStringLiteral("Error getting string: ") + QString::fromUtf8(vrSystem->GetPropErrorNameFromEnum(error));
    }
    return QString::fromUtf8(buffer);
}

} // namespace

OpenVRBackend::OpenVRBackend(QObject *parent)
    : OutputBackend(parent)
{
    m_vrRuntimeConnected = connectToVrRuntime();
}

OpenVRBackend::~OpenVRBackend()
{
    for (BackendOutput *output : m_outputs) {
        output->unref();
    }
    if (m_vrRuntimeConnected) {
        vr::VR_Shutdown();
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

bool OpenVRBackend::isVrRuntimeConnected() const
{
    return m_vrRuntimeConnected;
}

QString OpenVRBackend::vrDriver() const
{
    return m_vrDriver;
}

QString OpenVRBackend::vrDisplay() const
{
    return m_vrDisplay;
}

bool OpenVRBackend::connectToVrRuntime()
{
    vr::EVRInitError initError = vr::VRInitError_None;
    vr::IVRSystem *vrSystem = vr::VR_Init(&initError, vr::VRApplication_Overlay);
    if (initError != vr::VRInitError_None) {
        m_vrDriver = QStringLiteral("No Driver");
        m_vrDisplay = QStringLiteral("No Display");
        return false;
    }

    m_vrDriver = getTrackedDeviceString(vrSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
    m_vrDisplay = getTrackedDeviceString(vrSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
    return true;
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
