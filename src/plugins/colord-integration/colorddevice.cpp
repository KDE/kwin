/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colorddevice.h"
#include "abstract_output.h"
#include "colordevice.h"
#include "colordlogging.h"
#include "colormanager.h"
#include "colordprofileinterface.h"

namespace KWin
{

ColordDevice::ColordDevice(AbstractOutput *output, QObject *parent)
    : QObject(parent)
    , m_output(output)
{
}

AbstractOutput *ColordDevice::output() const
{
    return m_output;
}

QDBusObjectPath ColordDevice::objectPath() const
{
    return m_colordInterface ? QDBusObjectPath(m_colordInterface->path()) : QDBusObjectPath();
}

void ColordDevice::initialize(const QDBusObjectPath &devicePath)
{
    m_colordInterface = new CdDeviceInterface(QStringLiteral("org.freedesktop.ColorManager"),
                                              devicePath.path(), QDBusConnection::systemBus(), this);
    connect(m_colordInterface, &CdDeviceInterface::Changed, this, &ColordDevice::updateProfile);

    updateProfile();
}

void ColordDevice::updateProfile()
{
    const QList<QDBusObjectPath> profiles = m_colordInterface->profiles();
    if (profiles.isEmpty()) {
        qCDebug(KWIN_COLORD) << m_output->name() << "has no any color profile assigned";
        return;
    }

    CdProfileInterface profile(QStringLiteral("org.freedesktop.ColorManager"),
                               profiles.first().path(), QDBusConnection::systemBus());
    if (!profile.isValid()) {
        qCWarning(KWIN_COLORD) << profiles.first() << "is an invalid color profile";
        return;
    }

    ColorDevice *device = ColorManager::self()->findDevice(m_output);
    if (device) {
        device->setProfile(profile.filename());
    }
}

} // namespace KWin
