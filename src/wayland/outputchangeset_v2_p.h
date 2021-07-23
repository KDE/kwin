/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "outputchangeset_v2.h"

namespace KWaylandServer
{

class OutputChangeSetV2Private
{
public:
    OutputChangeSetV2Private(OutputDeviceV2Interface *outputdevice, OutputChangeSetV2 *parent);

    OutputChangeSetV2 *q;
    OutputDeviceV2Interface *outputDevice;

    bool enabled;
    QSize size;
    int refreshRate;
    OutputDeviceV2Interface::Transform transform;
    QPoint position;
    qreal scale;
    uint32_t overscan;
    OutputDeviceV2Interface::VrrPolicy vrrPolicy = OutputDeviceV2Interface::VrrPolicy::Automatic;
};

}
