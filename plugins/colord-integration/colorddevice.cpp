/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colorddevice.h"
#include "abstract_output.h"
#include "colordlogging.h"
#include "colordprofileinterface.h"

#include <lcms2.h>

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

    cmsHPROFILE handle = cmsOpenProfileFromFile(profile.filename().toUtf8(), "r");
    if (!handle) {
        qCWarning(KWIN_COLORD) << "Failed to open profile file" << profile.filename();
        return;
    }

    GammaRamp ramp(m_output->gammaRampSize());
    uint16_t *redChannel = ramp.red();
    uint16_t *greenChannel = ramp.green();
    uint16_t *blueChannel = ramp.blue();

    cmsToneCurve **vcgt = static_cast<cmsToneCurve **>(cmsReadTag(handle, cmsSigVcgtTag));
    if (!vcgt || !vcgt[0]) {
        qCDebug(KWIN_COLORD) << "Profile" << profile.filename() << "has no VCGT tag";

        for (uint32_t i = 0; i < ramp.size(); ++i) {
            const uint16_t value = (i * 0xffff) / (ramp.size() - 1);

            redChannel[i] = value;
            greenChannel[i] = value;
            blueChannel[i] = value;
        }
    } else {
        for (uint32_t i = 0; i < ramp.size(); ++i) {
            const uint16_t index = (i * 0xffff) / (ramp.size() - 1);

            redChannel[i] = cmsEvalToneCurve16(vcgt[0], index);
            greenChannel[i] = cmsEvalToneCurve16(vcgt[1], index);
            blueChannel[i] = cmsEvalToneCurve16(vcgt[2], index);
        }
    }

    cmsCloseProfile(handle);

    if (!m_output->setGammaRamp(ramp)) {
        qCWarning(KWIN_COLORD) << "Failed to apply color profilie on output" << m_output->name();
    }
}

} // namespace KWin
