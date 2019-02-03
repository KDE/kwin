/*
 * Copyright (C) 2018 Eike Hein <hein@kde.org>
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
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
#include "animationsmodel.h"
#include "desktopsmodel.h"

#include <KAboutApplicationDialog>
#include <KAboutData>
#include <KConfigGroup>
#include <KLocalizedString>

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
    , m_animationsModel(new AnimationsModel(this))
{
    KAboutData *about = new KAboutData(QStringLiteral("kcm_kwin_virtualdesktops"),
        i18n("Configure Virtual Desktops"),
        QStringLiteral("2.0"), QString(), KAboutLicense::GPL);
    setAboutData(about);

    setButtons(Apply | Default);

    QObject::connect(m_desktopsModel, &KWin::DesktopsModel::userModifiedChanged,
        this, &VirtualDesktops::updateNeedsSave);
    connect(m_animationsModel, &AnimationsModel::enabledChanged,
        this, &VirtualDesktops::updateNeedsSave);
    connect(m_animationsModel, &AnimationsModel::currentIndexChanged,
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

QAbstractItemModel *VirtualDesktops::animationsModel() const
{
    return m_animationsModel;
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

    m_animationsModel->load();
}

void VirtualDesktops::save()
{
    m_desktopsModel->syncWithServer();
    m_animationsModel->save();

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
    m_animationsModel->defaults();

    setNavWraps(true);
    setOsdEnabled(false);
    setOsdDuration(1000);
    setOsdTextOnly(false);
}

void VirtualDesktops::configureAnimation()
{
    const QModelIndex index = m_animationsModel->index(m_animationsModel->currentIndex(), 0);
    if (!index.isValid()) {
        return;
    }

    m_animationsModel->requestConfigure(index, nullptr);
}

void VirtualDesktops::showAboutAnimation()
{
    const QModelIndex index = m_animationsModel->index(m_animationsModel->currentIndex(), 0);
    if (!index.isValid()) {
        return;
    }

    const QString name    = index.data(AnimationsModel::NameRole).toString();
    const QString comment = index.data(AnimationsModel::DescriptionRole).toString();
    const QString author  = index.data(AnimationsModel::AuthorNameRole).toString();
    const QString email   = index.data(AnimationsModel::AuthorEmailRole).toString();
    const QString website = index.data(AnimationsModel::WebsiteRole).toString();
    const QString version = index.data(AnimationsModel::VersionRole).toString();
    const QString license = index.data(AnimationsModel::LicenseRole).toString();
    const QString icon    = index.data(AnimationsModel::IconNameRole).toString();

    const KAboutLicense::LicenseKey licenseType = KAboutLicense::byKeyword(license).key();

    KAboutData aboutData(
        name,              // Plugin name
        name,              // Display name
        version,           // Version
        comment,           // Short description
        licenseType,       // License
        QString(),         // Copyright statement
        QString(),         // Other text
        website.toLatin1() // Home page
    );
    aboutData.setProgramLogo(icon);

    const QStringList authors = author.split(',');
    const QStringList emails = email.split(',');

    if (authors.count() == emails.count()) {
        int i = 0;
        for (const QString &author : authors) {
            if (!author.isEmpty()) {
                aboutData.addAuthor(i18n(author.toUtf8()), QString(), emails[i]);
            }
            i++;
        }
    }

    QPointer<KAboutApplicationDialog> aboutPlugin = new KAboutApplicationDialog(aboutData);
    aboutPlugin->exec();

    delete aboutPlugin;
}

void VirtualDesktops::updateNeedsSave()
{
    bool needsSave = false;

    if (m_desktopsModel->userModified()) {
        needsSave = true;
    }

    if (m_animationsModel->needsSave()) {
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
