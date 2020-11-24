/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colordintegration.h"
#include "abstract_output.h"
#include "colorddevice.h"
#include "colordlogging.h"
#include "main.h"
#include "platform.h"

#include <QDBusPendingCallWatcher>

namespace KWin
{

ColordIntegration::ColordIntegration(QObject *parent)
    : Plugin(parent)
{
    qDBusRegisterMetaType<CdStringMap>();

    const Platform *platform = kwinApp()->platform();

    m_colordInterface = new CdInterface(QStringLiteral("org.freedesktop.ColorManager"),
                                        QStringLiteral("/org/freedesktop/ColorManager"),
                                        QDBusConnection::systemBus(), this);

    const QVector<AbstractOutput *> outputs = platform->outputs();
    for (AbstractOutput *output : outputs) {
        handleOutputAdded(output);
    }

    connect(platform, &Platform::outputAdded, this, &ColordIntegration::handleOutputAdded);
    connect(platform, &Platform::outputRemoved, this, &ColordIntegration::handleOutputRemoved);
}

void ColordIntegration::handleOutputAdded(AbstractOutput *output)
{
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

void ColordIntegration::handleOutputRemoved(AbstractOutput *output)
{
    ColordDevice *device = m_outputToDevice.take(output);
    if (device) {
        m_colordInterface->DeleteDevice(device->objectPath());
        delete device;
    }
}

} // namespace KWin
