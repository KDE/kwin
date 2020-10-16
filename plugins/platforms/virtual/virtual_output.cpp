/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_output.h"

namespace KWin
{

VirtualOutput::VirtualOutput(QObject *parent)
    : AbstractWaylandOutput()
{
    Q_UNUSED(parent);
    static int identifier = -1;
    identifier++;
    setName("Virtual-" + QString::number(identifier));
}

VirtualOutput::~VirtualOutput()
{
}

void VirtualOutput::init(const QPoint &logicalPosition, const QSize &pixelSize)
{
    KWaylandServer::OutputDeviceInterface::Mode mode;
    mode.id = 0;
    mode.size = pixelSize;
    mode.flags = KWaylandServer::OutputDeviceInterface::ModeFlag::Current;
    mode.refreshRate = 60000;  // TODO
    initInterfaces("model_TODO", "manufacturer_TODO", "UUID_TODO", pixelSize, { mode });
    setGeometry(QRect(logicalPosition, pixelSize));
}

void VirtualOutput::setGeometry(const QRect &geo)
{
    // TODO: set mode to have updated pixelSize
    setGlobalPos(geo.topLeft());
}

}
