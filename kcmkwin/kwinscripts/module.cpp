/*
 * Copyright (c) 2011 Tamas Krutki <ktamasw@gmail.com>
 * Copyright (c) 2012 Martin Gräßlin <mgraesslin@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "module.h"
#include "ui_module.h"

#include <QFileDialog>
#include <QStringList>
#include <QStandardPaths>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>

#include <KAboutData>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KMessageBox>
#include <KMessageWidget>
#include <KPluginInfo>
#include <KPackage/PackageLoader>
#include <KPackage/Package>

#include <KNewStuff3/KNS3/Button>

#include "version.h"

Module::Module(QWidget *parent, const QVariantList &args) :
    KCModule(parent, args),
    ui(new Ui::Module),
    m_kwinConfig(KSharedConfig::openConfig("kwinrc"))
{
    KAboutData *about = new KAboutData("kwin-scripts",
                                       i18n("KWin Scripts"),
                                       global_s_versionStringFull,
                                       i18n("Configure KWin scripts"),
                                       KAboutLicense::GPL_V2);

    about->addAuthor(i18n("Tamás Krutki"));
    setAboutData(about);

    ui->setupUi(this);

    ui->messageWidget->hide();

    ui->ghnsButton->setConfigFile(QStringLiteral("kwinscripts.knsrc"));
    connect(ui->ghnsButton, &KNS3::Button::dialogFinished, this, [this](const KNS3::Entry::List &changedEntries) {
        if (!changedEntries.isEmpty()) {
            updateListViewContents();
        }
    });

    connect(ui->scriptSelector, SIGNAL(changed(bool)), this, SLOT(changed()));
    connect(ui->importScriptButton, SIGNAL(clicked()), SLOT(importScript()));

    updateListViewContents();
}

Module::~Module()
{
    delete ui;
}

void Module::importScript()
{
    ui->messageWidget->animatedHide();

    QString path = QFileDialog::getOpenFileName(nullptr, i18n("Import KWin Script"), QDir::homePath(),
                                                i18n("*.kwinscript|KWin scripts (*.kwinscript)"));

    if (path.isNull()) {
        return;
    }

    using namespace KPackage;
    PackageStructure *structure = PackageLoader::self()->loadPackageStructure(QStringLiteral("KWin/Script"));
    Package package(structure);

    KJob *installJob = package.update(path);
    installJob->setProperty("packagePath", path); // so we can retrieve it later for showing the script's name
    connect(installJob, &KJob::result, this, &Module::importScriptInstallFinished);
}

void Module::importScriptInstallFinished(KJob *job)
{
    // if the applet is already installed, just add it to the containment
    if (job->error() != KJob::NoError) {
        ui->messageWidget->setText(i18nc("Placeholder is error message returned from the install service", "Cannot import selected script.\n%1", job->errorString()));
        ui->messageWidget->setMessageType(KMessageWidget::Error);
        ui->messageWidget->animatedShow();
        return;
    }

    using namespace KPackage;

    // so we can show the name of the package we just imported
    PackageStructure *structure = PackageLoader::self()->loadPackageStructure(QStringLiteral("KWin/Script"));
    Package package(structure);
    package.setPath(job->property("packagePath").toString());
    Q_ASSERT(package.isValid());

    ui->messageWidget->setText(i18nc("Placeholder is name of the script that was imported", "The script \"%1\" was successfully imported.", package.metadata().name()));
    ui->messageWidget->setMessageType(KMessageWidget::Information);
    ui->messageWidget->animatedShow();

    updateListViewContents();

    emit changed(true);
}

void Module::updateListViewContents()
{
    auto filter =  [](const KPluginMetaData &md) {
        if (md.value(QStringLiteral("X-KWin-Exclude-Listing")) == QLatin1String("true") ) {
            return false;
        }
        return true;
    };

    const QString scriptFolder = QStringLiteral("kwin/scripts/");
    const auto scripts = KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Script"), scriptFolder, filter);

    QList<KPluginInfo> scriptinfos = KPluginInfo::fromMetaData(scripts.toVector());

    ui->scriptSelector->addPlugins(scriptinfos, KPluginSelector::ReadConfigFile, QString(), QString(), m_kwinConfig);
}

void Module::defaults()
{
    ui->scriptSelector->defaults();
    emit changed(true);
}

void Module::load()
{
    updateListViewContents();
    ui->scriptSelector->load();

    emit changed(false);
}

void Module::save()
{
    ui->scriptSelector->save();
    m_kwinConfig->sync();
    QDBusMessage message = QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "start");
    QDBusConnection::sessionBus().asyncCall(message);

    emit changed(false);
}
