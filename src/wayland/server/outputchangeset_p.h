/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "outputchangeset.h"

namespace KWaylandServer
{

class OutputChangeSetPrivate
{
public:
    OutputChangeSetPrivate(OutputDeviceInterface *outputdevice, OutputChangeSet *parent);

    OutputChangeSet *q;
    OutputDeviceInterface *outputDevice;

    OutputDeviceInterface::Enablement enabled;
    int modeId;
    OutputDeviceInterface::Transform transform;
    QPoint position;
    qreal scale;
    OutputDeviceInterface::ColorCurves colorCurves;
    uint32_t overscan;
    OutputDeviceInterface::VrrPolicy vrrPolicy = OutputDeviceInterface::VrrPolicy::Automatic;
};

}
