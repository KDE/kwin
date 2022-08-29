/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colordintegration.h"
#include "colorddevice.h"
#include "colordlogging.h"
#include "core/output.h"
#include "workspace.h"

#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>

namespace KWin
{

ColordIntegration::ColordIntegration()
{
    qDBusRegisterMetaType<CdStringMap>();

    auto watcher = new QDBusServiceWatcher(QStringLiteral("org.freedesktop.ColorManager"),
                                           QDBusConnection::systemBus(),
                                           QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &ColordIntegration::initialize);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &ColordIntegration::teardown);

    QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();
    if (interface->isServiceRegistered(QStringLiteral("org.freedesktop.ColorManager"))) {
        initialize();
    }
}

void ColordIntegration::initialize()
{
    m_colordInterface = new CdInterface(QStringLiteral("org.freedesktop.ColorManager"),
                                        QStringLiteral("/org/freedesktop/ColorManager"),
                                        QDBusConnection::systemBus(), this);

    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        handleOutputAdded(output);
    }

    connect(workspace(), &Workspace::outputAdded, this, &ColordIntegration::handleOutputAdded);
    connect(workspace(), &Workspace::outputRemoved, this, &ColordIntegration::handleOutputRemoved);
}

void ColordIntegration::teardown()
{
    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        handleOutputRemoved(output);
    }

    delete m_colordInterface;
    m_colordInterface = nullptr;

    disconnect(workspace(), &Workspace::outputAdded, this, &ColordIntegration::handleOutputAdded);
    disconnect(workspace(), &Workspace::outputRemoved, this, &ColordIntegration::handleOutputRemoved);
}

void ColordIntegration::handleOutputAdded(Output *output)
{
    if (output->isNonDesktop()) {
        return;
    }
    ColordDevice *device = new ColordDevice(output, this);

    CdStringMap properties;
    properties.insert(QStringLiteral("Kind"), QStringLiteral("display"));
    properties.insert(QStringLiteral("Colorspace"), QStringLiteral("RGB"));

    const QString vendor = output->manufacturer();
    if (!vendor.isEmpty()) {
        properties.insert(QStringLiteral("Vendor"), vendor);
    }

    const QString model = output->model();
    if (!model.isEmpty()) {
        properties.insert(QStringLiteral("Model"), model);
    }

    const QString serialNumber = output->serialNumber();
    if (!serialNumber.isEmpty()) {
        properties.insert(QStringLiteral("Serial"), serialNumber);
    }

    if (output->isInternal()) {
        properties.insert(QStringLiteral("Embedded"), QString());
    }

    QDBusPendingReply<QDBusObjectPath> reply =
        m_colordInterface->CreateDevice(output->name(), QStringLiteral("temp"), properties);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, device, watcher]() {
        watcher->deleteLater();

        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            qCDebug(KWIN_COLORD) << "Failed to add a colord device:" << reply.error();
            delete device;
            return;
        }

        const QDBusObjectPath objectPath = reply.value();
        if (!device->output()) {
            m_colordInterface->DeleteDevice(objectPath);
            delete device;
            return;
        }

        device->initialize(objectPath);
        m_outputToDevice.insert(device->output(), device);
    });
}

void ColordIntegration::handleOutputRemoved(Output *output)
{
    if (output->isNonDesktop()) {
        return;
    }
    ColordDevice *device = m_outputToDevice.take(output);
    if (device) {
        m_colordInterface->DeleteDevice(device->objectPath());
        delete device;
    }
}

} // namespace KWin
