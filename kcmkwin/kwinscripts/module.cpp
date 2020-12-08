/*
    SPDX-FileCopyrightText: 2011 Tamas Krutki <ktamasw@gmail.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "module.h"
#include "ui_module.h"

#include <QFileDialog>
#include <QStringList>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include <KAboutData>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KMessageBox>
#include <KMessageWidget>
#include <KPluginInfo>
#include <KPackage/PackageLoader>
#include <KPackage/Package>
#include <KPackage/PackageStructure>

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
            ui->scriptSelector->clearPlugins();
            updateListViewContents();
        }
    });

    connect(ui->scriptSelector, &KPluginSelector::changed, this, qOverload<bool>(&KCModule::changed));
    connect(ui->scriptSelector, &KPluginSelector::defaulted, this, qOverload<bool>(&KCModule::defaulted));
    connect(ui->importScriptButton, &QPushButton::clicked, this, &Module::importScript);

    ui->scriptSelector->setAdditionalButtonHandler([this](const KPluginInfo &info) {
        QPushButton *button = new QPushButton(ui->scriptSelector);
        button->setIcon(QIcon::fromTheme(QStringLiteral("delete")));
        button->setEnabled(QFileInfo(info.entryPath()).isWritable());
        connect(button, &QPushButton::clicked, this, [this, info](){
            using namespace KPackage;
            PackageStructure *structure = PackageLoader::self()->loadPackageStructure(QStringLiteral("KWin/Script"));
            Package package(structure);
            // We can get the package root from the entry path
            QDir root = QFileInfo(info.entryPath()).dir();
            root.cdUp();
            KJob *uninstallJob = Package(structure).uninstall(info.pluginName(), root.absolutePath());
            connect(uninstallJob, &KJob::result, this, [this, uninstallJob](){
                ui->scriptSelector->clearPlugins();
                updateListViewContents();
                // If the uninstallation is successful the entry will be immediately removed
                if (!uninstallJob->errorString().isEmpty()) {
                    ui->messageWidget->setText(i18n("Error when uninstalling KWin Script: %1", uninstallJob->errorString()));
                    ui->messageWidget->setMessageType(KMessageWidget::Error);
                    ui->messageWidget->animatedShow();
                }
            });
       });
       return button;
    });

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
        return md.isValid() && !md.rawData().value("X-KWin-Exclude-Listing").toBool();
    };

    const QString scriptFolder = QStringLiteral("kwin/scripts/");
    const auto scripts = KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Script"), scriptFolder, filter);

    QList<KPluginInfo> scriptinfos = KPluginInfo::fromMetaData(scripts.toVector());

    ui->scriptSelector->addPlugins(scriptinfos, KPluginSelector::ReadConfigFile, QString(), QString(), m_kwinConfig);
}

void Module::defaults()
{
    ui->scriptSelector->defaults();
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
