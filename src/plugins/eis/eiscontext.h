/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QFlag>
#include <QSocketNotifier>
#include <QString>

#include <libeis.h>

#include <memory>
#include <vector>

namespace KWin
{

class EisBackend;
struct EisClient;

class EisContext
{
public:
    EisContext(EisBackend *backend, QFlags<eis_device_capability> allowedCapabilities);
    virtual ~EisContext();

    void updateScreens();
    void updateKeymap();

protected:
    eis *m_eisContext;
    void connectToClient(eis_client *client);
    virtual void connectionRequested(eis_client *client) = 0;

private:
    void handleEvents();

    EisBackend *m_backend;
    QFlags<eis_device_capability> m_allowedCapabilities;
    QSocketNotifier m_socketNotifier;
    std::vector<std::unique_ptr<EisClient>> m_clients;
};

class DbusEisContext : public EisContext
{
public:
    DbusEisContext(EisBackend *backend, QFlags<eis_device_capability> allowedCapabilities, int cookie, const QString &dbusService);

    int addClient();

    const int cookie;
    const QString dbusService;

private:
    void connectionRequested(eis_client *client) override;
};
}
