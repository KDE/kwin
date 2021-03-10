/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "outputchangeset.h"

namespace KWaylandServer
{

class OutputChangeSet::Private
{
public:
    Private(OutputDeviceInterface *outputdevice, OutputChangeSet *parent);
    ~Private();

    OutputChangeSet *q;
    OutputDeviceInterface *o;

    OutputDeviceInterface::Enablement enabled;
    int modeId;
    OutputDeviceInterface::Transform transform;
    QPoint position;
    qreal scale;
    OutputDeviceInterface::ColorCurves colorCurves;
};

}
