/*
    SPDX-FileCopyrightText: 2011 Tamas Krutki <ktamasw@gmail.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "module.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFileDialog>
#include <QStandardPaths>
#include <QStringList>

#include <KAboutData>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KNSWidgets/Button>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <KPackage/PackageStructure>
#include <KPluginFactory>
#include <KSharedConfig>

#include <KCMultiDialog>

#include "config-kwin.h"
#include "kwinscriptsdata.h"

Module::Module(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : KQuickAddons::ConfigModule(parent, data, args)
    , m_kwinScriptsData(new KWinScriptsData(this))
    , m_model(new KPluginModel(this))
{
    // Hide the help button, because there is no help
    setButtons(Apply | Default);
    connect(m_model, &KPluginModel::isSaveNeededChanged, this, [this]() {
        setNeedsSave(m_model->isSaveNeeded() || !m_pendingDeletions.isEmpty());
    });
    connect(m_model, &KPluginModel::defaulted, this, [this](bool defaulted) {
        setRepresentsDefaults(defaulted);
    });
    m_model->setConfig(KSharedConfig::openConfig("kwinrc")->group("Plugins"));
}

void Module::onGHNSEntriesChanged()
{
    m_model->clear();
    m_model->addPlugins(m_kwinScriptsData->pluginMetaDataList(), QString());
}

void Module::importScript()
{
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

void Module::configure(const KPluginMetaData &data)
{
    auto dialog = new KCMultiDialog();
    dialog->addModule(data);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void Module::togglePendingDeletion(const KPluginMetaData &data)
{
    if (m_pendingDeletions.contains(data)) {
        m_pendingDeletions.removeOne(data);
    } else {
        m_pendingDeletions.append(data);
    }
    setNeedsSave(m_model->isSaveNeeded() || !m_pendingDeletions.isEmpty());
    Q_EMIT pendingDeletionsChanged();
}

void Module::importScriptInstallFinished(KJob *job)
{
    // if the applet is already installed, just add it to the containment
    if (job->error() != KJob::NoError) {
        setErrorMessage(i18nc("Placeholder is error message returned from the install service", "Cannot import selected script.\n%1", job->errorString()));
        return;
    }

    using namespace KPackage;

    // so we can show the name of the package we just imported
    PackageStructure *structure = PackageLoader::self()->loadPackageStructure(QStringLiteral("KWin/Script"));
    Package package(structure);
    package.setPath(job->property("packagePath").toString());
    Q_ASSERT(package.isValid());

    m_infoMessage = i18nc("Placeholder is name of the script that was imported", "The script \"%1\" was successfully imported.", package.metadata().name());
    m_errorMessage.clear();
    Q_EMIT messageChanged();

    m_model->clear();
    m_model->addPlugins(m_kwinScriptsData->pluginMetaDataList(), QString());

    setNeedsSave(false);
}

void Module::defaults()
{
    m_model->defaults();
    m_pendingDeletions.clear();
    Q_EMIT pendingDeletionsChanged();

    setNeedsSave(m_model->isSaveNeeded());
}

void Module::load()
{
    m_model->clear();
    m_model->addPlugins(m_kwinScriptsData->pluginMetaDataList(), QString());
    m_pendingDeletions.clear();
    Q_EMIT pendingDeletionsChanged();

    setNeedsSave(false);
}

void Module::save()
{
    using namespace KPackage;
    PackageStructure *structure = PackageLoader::self()->loadPackageStructure(QStringLiteral("KWin/Script"));
    for (const KPluginMetaData &info : std::as_const(m_pendingDeletions)) {
        // We can get the package root from the entry path
        QDir root = QFileInfo(info.metaDataFileName()).dir();
        root.cdUp();
        KJob *uninstallJob = Package(structure).uninstall(info.pluginId(), root.absolutePath());
        connect(uninstallJob, &KJob::result, this, [this, uninstallJob]() {
            if (!uninstallJob->errorString().isEmpty()) {
                setErrorMessage(i18n("Error when uninstalling KWin Script: %1", uninstallJob->errorString()));
            } else {
                load(); // Make sure to reload the KCM to deleted entries to disappear
            }
        });
    }

    m_infoMessage.clear();
    Q_EMIT messageChanged();
    m_pendingDeletions.clear();
    Q_EMIT pendingDeletionsChanged();

    m_model->save();
    QDBusMessage message = QDBusMessage::createMethodCall("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "start");
    QDBusConnection::sessionBus().asyncCall(message);

    setNeedsSave(false);
}

K_PLUGIN_FACTORY_WITH_JSON(KcmKWinScriptsFactory, "kcm_kwin_scripts.json",
                           registerPlugin<Module>();
                           registerPlugin<KWinScriptsData>();)

#include "module.moc"
