/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "output.h"
#include "wayland/outputdevice_v2_interface.h"
#include "wayland/utils.h"

namespace KWin
{

class WaylandOutputDevice : public QObject
{
    Q_OBJECT

public:
    explicit WaylandOutputDevice(Output *output, QObject *parent = nullptr);

private Q_SLOTS:
    void handleGeometryChanged();
    void handleScaleChanged();
    void handleEnabledChanged();
    void handleTransformChanged();
    void handleCurrentModeChanged();
    void handleCapabilitiesChanged();
    void handleOverscanChanged();
    void handleVrrPolicyChanged();
    void handleModesChanged();
    void handleRgbRangeChanged();

private:
    void updateModes(Output *output);

    Output *m_platformOutput;
    KWaylandServer::ScopedGlobalPointer<KWaylandServer::OutputDeviceV2Interface> m_outputDeviceV2;
};

} // namespace KWin
