/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "libinputbackend.h"
#include "connection.h"
#include "device.h"
#include "input.h"
#include "keyboard_input.h"

namespace KWin
{

LibinputBackend::LibinputBackend(Session *session, QObject *parent)
    : InputBackend(parent)
{
    m_thread.setObjectName(QStringLiteral("libinput-connection"));
    m_thread.start();

    m_connection = LibInput::Connection::create(session);
    m_connection->moveToThread(&m_thread);

    connect(m_connection, &LibInput::Connection::eventsRead, this, [this]() {
        m_connection->processEvents();
    }, Qt::QueuedConnection);

    connect(m_connection, &LibInput::Connection::deviceAdded, this, [this](LibInput::Device *device) {
        if (device->isKeyboard() && !device->keyboard()) {
            device->setKeyboard(std::make_unique<KeyboardInput>(input()));
        }
        Q_EMIT deviceAdded(device);
    });
    connect(m_connection, &LibInput::Connection::deviceRemoved,
            this, &InputBackend::deviceRemoved);
}

LibinputBackend::~LibinputBackend()
{
    // Notify the main thread about removed input devices. The deviceRemoved() signal cannot
    // be emitted from the connection thread otherwise it will be queued, which we don't want.
    const auto devices = m_connection->devices();
    for (const auto device : devices) {
        Q_EMIT deviceRemoved(device);
    }

    m_connection->deleteLater();
    m_thread.quit();
    m_thread.wait();
}

void LibinputBackend::initialize()
{
    m_connection->setInputConfig(config());
    m_connection->setup();
}

void LibinputBackend::updateScreens()
{
    m_connection->updateScreens();
}

} // namespace KWin

#include "moc_libinputbackend.cpp"
