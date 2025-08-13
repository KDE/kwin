/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_output.h"
#include "virtual_backend.h"

#include "compositor.h"
#include "core/outputconfiguration.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "core/renderloop.h"
#include "utils/softwarevsyncmonitor.h"

namespace KWin
{

VirtualOutput::VirtualOutput(VirtualBackend *parent, bool internal, const QSize &physicalSizeInMM, OutputTransform panelOrientation, const QByteArray &edid, std::optional<QByteArray> edidIdentifierOverride, const std::optional<QString> &connectorName, const std::optional<QByteArray> &mstPath)
    : BackendOutput()
    , m_backend(parent)
    , m_renderLoop(std::make_unique<RenderLoop>(this))
    , m_vsyncMonitor(SoftwareVsyncMonitor::create())
{
    connect(m_vsyncMonitor.get(), &VsyncMonitor::vblankOccurred, this, &VirtualOutput::vblank);

    static int identifier = -1;
    m_identifier = ++identifier;
    setInformation(Information{
        .name = connectorName.value_or(QStringLiteral("Virtual-%1").arg(identifier)),
        .physicalSize = physicalSizeInMM,
        .edid = Edid{edid, edidIdentifierOverride},
        .panelOrientation = panelOrientation,
        .internal = internal,
        .mstPath = mstPath.value_or(QByteArray()),
    });
}

VirtualOutput::~VirtualOutput()
{
}

RenderLoop *VirtualOutput::renderLoop() const
{
    return m_renderLoop.get();
}

bool VirtualOutput::testPresentation(const std::shared_ptr<OutputFrame> &frame)
{
    return true;
}

bool VirtualOutput::present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    m_frame = frame;
    m_vsyncMonitor->arm();
    return true;
}

void VirtualOutput::init(const QPoint &logicalPosition, const QSize &pixelSize, qreal scale, const QList<std::tuple<QSize, uint64_t, OutputMode::Flags>> &modes)
{
    QList<std::shared_ptr<OutputMode>> modeList;
    for (const auto &mode : modes) {
        const auto &[size, refresh, flags] = mode;
        modeList.push_back(std::make_shared<OutputMode>(size, refresh, flags));
    }
    if (modeList.empty()) {
        modeList.push_back(std::make_shared<OutputMode>(pixelSize, 60000, OutputMode::Flag::Preferred));
    }

    m_renderLoop->setRefreshRate(modeList.front()->refreshRate());
    m_vsyncMonitor->setRefreshRate(modeList.front()->refreshRate());
    setState(State{
        .position = logicalPosition,
        .scale = scale,
        .modes = modeList,
        .currentMode = modeList.front(),
        .scaleSetting = scale,
    });
}

void VirtualOutput::applyChanges(const OutputConfiguration &config)
{
    auto props = config.constChangeSet(this);
    if (!props) {
        return;
    }
    Q_EMIT aboutToChange(props.get());

    State next = m_state;
    next.enabled = props->enabled.value_or(m_state.enabled);
    next.transform = props->transform.value_or(m_state.transform);
    next.position = props->pos.value_or(m_state.position);
    next.scale = props->scale.value_or(m_state.scale);
    next.scaleSetting = props->scaleSetting.value_or(m_state.scaleSetting);
    next.desiredModeSize = props->desiredModeSize.value_or(m_state.desiredModeSize);
    next.desiredModeRefreshRate = props->desiredModeRefreshRate.value_or(m_state.desiredModeRefreshRate);
    next.currentMode = props->mode.value_or(m_state.currentMode).lock();
    if (!next.currentMode) {
        next.currentMode = next.modes.front();
    }
    next.uuid = props->uuid.value_or(m_state.uuid);
    next.replicationSource = props->replicationSource.value_or(m_state.replicationSource);
    next.priority = props->priority.value_or(m_state.priority);
    next.deviceOffset = props->deviceOffset.value_or(m_state.deviceOffset);
    setState(next);
    m_renderLoop->setRefreshRate(next.currentMode->refreshRate());
    m_vsyncMonitor->setRefreshRate(next.currentMode->refreshRate());

    Q_EMIT changed();
}

void VirtualOutput::vblank(std::chrono::nanoseconds timestamp)
{
    if (m_frame) {
        m_frame->presented(timestamp, PresentationMode::VSync);
        m_frame.reset();
    }
}

void VirtualOutput::setOutputLayer(std::unique_ptr<OutputLayer> &&layer)
{
    m_layer = std::move(layer);
}

OutputLayer *VirtualOutput::outputLayer() const
{
    return m_layer.get();
}
}

#include "moc_virtual_output.cpp"
