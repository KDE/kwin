/*
 * Copyright (C) 2018 Eike Hein <hein@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "virtualdesktops.h"
#include "desktopsmodel.h"

#include <KAboutData>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(VirtualDesktopsFactory, "kcm_kwin_virtualdesktops.json", registerPlugin<KWin::VirtualDesktops>();)

namespace KWin
{

VirtualDesktops::VirtualDesktops(QObject *parent, const QVariantList &args)
    : KQuickAddons::ConfigModule(parent, args)
    , m_kwinConfig(KSharedConfig::openConfig("kwinrc"))
    , m_desktopsModel(new KWin::DesktopsModel(this))
    , m_navWraps(true)
    , m_osdEnabled(false)
    , m_osdDuration(1000)
    , m_osdTextOnly(false)
{
    KAboutData *about = new KAboutData(QStringLiteral("kcm_kwin_virtualdesktops"),
        i18n("Configure Virtual Desktops"),
        QStringLiteral("2.0"), QString(), KAboutLicense::GPL);
    setAboutData(about);

    setButtons(Apply | Default);

    QObject::connect(m_desktopsModel, &KWin::DesktopsModel::userModifiedChanged,
        this, &VirtualDesktops::updateNeedsSave);
}

VirtualDesktops::~VirtualDesktops()
{
}

QAbstractItemModel *VirtualDesktops::desktopsModel() const
{
    return m_desktopsModel;
}

bool VirtualDesktops::navWraps() const
{
    return m_navWraps;
}

void VirtualDesktops::setNavWraps(bool wraps)
{
    if (m_navWraps != wraps) {
        m_navWraps = wraps;

        emit navWrapsChanged();

        updateNeedsSave();
    }
}

bool VirtualDesktops::osdEnabled() const
{
    return m_osdEnabled;
}

void VirtualDesktops::setOsdEnabled(bool enabled)
{
    if (m_osdEnabled != enabled) {
        m_osdEnabled = enabled;

        emit osdEnabledChanged();

        updateNeedsSave();
    }
}

int VirtualDesktops::osdDuration() const
{
    return m_osdDuration;
}

void VirtualDesktops::setOsdDuration(int duration)
{
    if (m_osdDuration != duration) {
        m_osdDuration = duration;

        emit osdDurationChanged();

        updateNeedsSave();
    }
}

int VirtualDesktops::osdTextOnly() const
{
    return m_osdTextOnly;
}

void VirtualDesktops::setOsdTextOnly(bool textOnly)
{
    if (m_osdTextOnly != textOnly) {
        m_osdTextOnly = textOnly;

        emit osdTextOnlyChanged();

        updateNeedsSave();
    }
}

void VirtualDesktops::load()
{
    KConfigGroup navConfig(m_kwinConfig, "Windows");
    setNavWraps(navConfig.readEntry<bool>("RollOverDesktops", true));

    KConfigGroup osdConfig(m_kwinConfig, "Plugins");
    setOsdEnabled(osdConfig.readEntry("desktopchangeosdEnabled", false));

    KConfigGroup osdSettings(m_kwinConfig, "Script-desktopchangeosd");
    setOsdDuration(osdSettings.readEntry("PopupHideDelay", 1000));
    setOsdTextOnly(osdSettings.readEntry("TextOnly", false));
}

void VirtualDesktops::save()
{
    m_desktopsModel->syncWithServer();

    KConfigGroup navConfig(m_kwinConfig, "Windows");
    navConfig.writeEntry("RollOverDesktops", m_navWraps);

    KConfigGroup osdConfig(m_kwinConfig, "Plugins");
    osdConfig.writeEntry("desktopchangeosdEnabled", m_osdEnabled);

    KConfigGroup osdSettings(m_kwinConfig, "Script-desktopchangeosd");
    osdSettings.writeEntry("PopupHideDelay", m_osdDuration);
    osdSettings.writeEntry("TextOnly", m_osdTextOnly);

    m_kwinConfig->sync();

    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"), QStringLiteral("reloadConfig"));
    QDBusConnection::sessionBus().send(message);

    setNeedsSave(false);
}

void VirtualDesktops::defaults()
{
    m_desktopsModel->setRows(1);

    setNavWraps(true);
    setOsdEnabled(false);
    setOsdDuration(1000);
    setOsdTextOnly(false);
}

void VirtualDesktops::updateNeedsSave()
{
    bool needsSave = false;

    if (m_desktopsModel->userModified()) {
        needsSave = true;
    }

    KConfigGroup navConfig(m_kwinConfig, "Windows");

    if (m_navWraps != navConfig.readEntry<bool>("RollOverDesktops", true)) {
        needsSave = true;
    }

    KConfigGroup osdConfig(m_kwinConfig, "Plugins");

    if (m_osdEnabled != osdConfig.readEntry("desktopchangeosdEnabled", false)) {
        needsSave = true;
    }

    KConfigGroup osdSettings(m_kwinConfig, "Script-desktopchangeosd");

    if (m_osdDuration != osdSettings.readEntry("PopupHideDelay", 1000)) {
        needsSave = true;
    }

    if (m_osdTextOnly != osdSettings.readEntry("TextOnly", false)) {
        needsSave = true;
    }

    setNeedsSave(needsSave);
}

}

#include "virtualdesktops.moc"
