/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/backendoutput.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "utils/filedescriptor.h"

#include <openvr.h>

namespace KWin
{

class OpenVRBackend;
class OpenVROutput;
class RenderDevice;

class KWIN_EXPORT OpenVRBackend : public OutputBackend
{
    Q_OBJECT

public:
    OpenVRBackend(QObject *parent = nullptr);
    ~OpenVRBackend() override;

    bool initialize() override;

    std::unique_ptr<EglBackend> createOpenGLBackend(RenderDevice *renderDevice) override;

    BackendOutput *createVirtualOutput(const QString &name, const QString &description, const QSize &size, qreal scale) override;
    void removeVirtualOutput(BackendOutput *output) override;

    struct OutputInfo
    {
        QSize size;
        double scale = 1;
        bool internal = false;
        QSize physicalSizeInMM;
        BackendOutput::Capabilities capabilities = BackendOutput::Capability::CustomModes | BackendOutput::Capability::Disable;
        QList<OutputModeline> modes;
        OutputTransform panelOrientation = OutputTransform::Kind::Normal;
        QByteArray edid;
        std::optional<QByteArray> edidIdentifierOverride;
        std::optional<QString> connectorName;
        std::optional<QByteArray> mstPath;
    };
    BackendOutput *addOutput(const OutputInfo &info);
    void setOpenVROutputs(const QList<OutputInfo> &infos);

    QList<BackendOutput *> outputs() const override;

    QList<CompositingType> supportedCompositors() const override;

    bool isVrRuntimeConnected() const;
    QString vrDriver() const;
    QString vrDisplay() const;

Q_SIGNALS:
    void openVROutputsSet(bool countChanged);

private:
    bool connectToVrRuntime();

    OpenVROutput *createOutput(const OutputInfo &info);
    void removeOutput(OpenVROutput *output);

    QList<OpenVROutput *> m_outputs;
    vr::EVRInitError m_lastHmdError = vr::VRInitError_None;
    QString m_vrDriver;
    QString m_vrDisplay;
    bool m_vrRuntimeConnected = false;
};

} // namespace KWin
