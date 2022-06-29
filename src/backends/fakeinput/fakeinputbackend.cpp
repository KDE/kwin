/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fakeinputbackend.h"
#include "fakeinputdevice.h"
#include "wayland/fakeinput_interface.h"
#include "wayland_server.h"

namespace KWin
{

FakeInputBackend::FakeInputBackend() = default;
FakeInputBackend::~FakeInputBackend() = default;

void FakeInputBackend::initialize()
{
    m_interface = std::make_unique<KWaylandServer::FakeInputInterface>(waylandServer()->display());
    connect(m_interface.get(), &KWaylandServer::FakeInputInterface::deviceCreated, this, [this](KWaylandServer::FakeInputDevice *fakeDevice) {
        m_devices[fakeDevice] = std::make_unique<FakeInputDevice>(fakeDevice);
        Q_EMIT deviceAdded(m_devices[fakeDevice].get());
    });
    connect(m_interface.get(), &KWaylandServer::FakeInputInterface::deviceDestroyed, this, [this](KWaylandServer::FakeInputDevice *fakeDevice) {
        auto it = m_devices.find(fakeDevice);
        if (it != m_devices.end()) {
            const std::unique_ptr<FakeInputDevice> device = std::move(it->second);
            m_devices.erase(it);
            Q_EMIT deviceRemoved(device.get());
        }
    });
}

} // namespace KWin
