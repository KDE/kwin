/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "abstract_wayland_output.h"

#include <KWaylandServer/outputdevice_v2_interface.h>
#include <KWaylandServer/utils.h>

namespace KWin
{

class WaylandOutputDevice : public QObject
{
    Q_OBJECT

public:
    explicit WaylandOutputDevice(AbstractWaylandOutput *output, QObject *parent = nullptr);

    void setModes(AbstractWaylandOutput *output);
    
private Q_SLOTS:
    void handleGeometryChanged();
    void handleScaleChanged();
    void handleEnabledChanged();
    void handleTransformChanged();
    void handleModeChanged();
    void handleCapabilitiesChanged();
    void handleOverscanChanged();
    void handleVrrPolicyChanged();
    void handleModesChanged();

private:
    AbstractWaylandOutput *m_platformOutput;
    KWaylandServer::ScopedGlobalPointer<KWaylandServer::OutputDeviceV2Interface> m_outputDeviceV2;
};

} // namespace KWin
