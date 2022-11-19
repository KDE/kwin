/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "libinputbackend.h"
#include "connection.h"
#include "device.h"

namespace KWin
{

LibinputBackend::LibinputBackend(Session *session, QObject *parent)
    : InputBackend(parent)
{
    m_thread.setObjectName(QStringLiteral("libinput-connection"));
    m_thread.start();

    m_connection = LibInput::Connection::create(session);
    m_connection->moveToThread(&m_thread);

    connect(
        m_connection.get(), &LibInput::Connection::eventsRead, this, [this]() {
            m_connection->processEvents();
        },
        Qt::QueuedConnection);

    // Direct connection because the deviceAdded() and the deviceRemoved() signals are emitted
    // from the main thread.
    connect(m_connection.get(), &LibInput::Connection::deviceAdded,
            this, &InputBackend::deviceAdded, Qt::DirectConnection);
    connect(m_connection.get(), &LibInput::Connection::deviceRemoved,
            this, &InputBackend::deviceRemoved, Qt::DirectConnection);
}

LibinputBackend::~LibinputBackend()
{
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
