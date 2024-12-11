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

VirtualOutput::VirtualOutput(VirtualBackend *parent, bool internal, const QSize &physicalSizeInMM, OutputTransform panelOrientation)
    : Output(parent)
    , m_backend(parent)
    , m_renderLoop(std::make_unique<RenderLoop>(this))
    , m_vsyncMonitor(SoftwareVsyncMonitor::create())
{
    connect(m_vsyncMonitor.get(), &VsyncMonitor::vblankOccurred, this, &VirtualOutput::vblank);

    static int identifier = -1;
    m_identifier = ++identifier;
    setInformation(Information{
        .name = QStringLiteral("Virtual-%1").arg(identifier),
        .physicalSize = physicalSizeInMM,
        .panelOrientation = panelOrientation,
        .internal = internal,
    });
}

VirtualOutput::~VirtualOutput()
{
}

RenderLoop *VirtualOutput::renderLoop() const
{
    return m_renderLoop.get();
}

void VirtualOutput::present(const std::shared_ptr<OutputFrame> &frame)
{
    m_frame = frame;
    m_vsyncMonitor->arm();
    Q_EMIT outputChange(frame->damage());
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
    m_nextState.position = logicalPosition;
    m_nextState.scale = scale;
    m_nextState.modes = modeList;
    m_nextState.currentMode = modeList.front();
    applyNextState();
}

void VirtualOutput::applyChanges(const OutputConfiguration &config)
{
    auto props = config.constChangeSet(this);
    if (!props) {
        return;
    }
    Q_EMIT aboutToChange(props.get());

    m_nextState.enabled = props->enabled.value_or(m_nextState.enabled);
    m_nextState.transform = props->transform.value_or(m_nextState.transform);
    m_nextState.position = props->pos.value_or(m_nextState.position);
    m_nextState.scale = props->scale.value_or(m_nextState.scale);
    m_nextState.desiredModeSize = props->desiredModeSize.value_or(m_nextState.desiredModeSize);
    m_nextState.desiredModeRefreshRate = props->desiredModeRefreshRate.value_or(m_nextState.desiredModeRefreshRate);
    m_nextState.currentMode = props->mode.value_or(m_nextState.currentMode).lock();
    if (!m_nextState.currentMode) {
        m_nextState.currentMode = m_nextState.modes.front();
    }
    applyNextState();
    m_renderLoop->setRefreshRate(m_nextState.currentMode->refreshRate());
    m_vsyncMonitor->setRefreshRate(m_nextState.currentMode->refreshRate());

    Q_EMIT changed();
}

void VirtualOutput::updateEnabled(bool enabled)
{
    m_nextState.enabled = enabled;
    applyNextState();
}

void VirtualOutput::vblank(std::chrono::nanoseconds timestamp)
{
    if (m_frame) {
        m_frame->presented(timestamp, PresentationMode::VSync);
        m_frame.reset();
    }
}
}

#include "moc_virtual_output.cpp"
