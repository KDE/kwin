/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fakeinputbackend.h"
#include "fakeinputdevice.h"
#include "wayland_server.h"

#include <KWaylandServer/fakeinput_interface.h>

namespace KWin
{

FakeInputBackend::FakeInputBackend(QObject *parent)
    : InputBackend(parent)
{
}

void FakeInputBackend::initialize()
{
    auto fakeInput = new KWaylandServer::FakeInputInterface(waylandServer()->display(), this);
    connect(fakeInput, &KWaylandServer::FakeInputInterface::deviceCreated, this,
        [this] (KWaylandServer::FakeInputDevice *fakeDevice) {
            auto device = new FakeInputDevice(fakeDevice, this);
            Q_EMIT deviceAdded(device);

            connect(fakeDevice, &QObject::destroyed, this, [this, device]() {
                Q_EMIT deviceRemoved(device);
            });
        }
    );
}

} // namespace KWin
