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

#include <QtCore/QStringList>

#include <KDE/KAboutData>
#include <KDE/KPluginFactory>
#include <KDE/KStandardDirs>
#include <KDE/KMessageBox>
#include <KDE/KFileDialog>
#include <KDE/KMessageWidget>
#include <KDE/KPluginInfo>
#include <KDE/KServiceTypeTrader>
#include <KDE/Plasma/Package>
#include <KNS3/DownloadDialog>

#include "version.h"

K_PLUGIN_FACTORY_DECLARATION(KcmKWinScriptsFactory);

Module::Module(QWidget *parent, const QVariantList &args) :
    KCModule(KcmKWinScriptsFactory::componentData(), parent, args),
    ui(new Ui::Module),
    m_kwinConfig(KSharedConfig::openConfig("kwinrc"))
{
    KAboutData *about = new KAboutData("kwin-scripts", 0,
                                       ki18n("KWin Scripts"),
                                       global_s_versionStringFull,
                                       ki18n("Configure KWin scripts"),
                                       KAboutData::License_GPL_V2);

    about->addAuthor(ki18n("Tamás Krutki"));
    setAboutData(about);

    ui->setupUi(this);
    ui->ghnsButton->setIcon(KIcon("get-hot-new-stuff"));

    // TODO: remove once the category has been created.
    ui->ghnsButton->setVisible(false);

    connect(ui->scriptSelector, SIGNAL(changed(bool)), this, SLOT(changed()));
    connect(ui->importScriptButton, SIGNAL(clicked()), SLOT(importScript()));
    connect(ui->ghnsButton, SIGNAL(clicked(bool)), SLOT(slotGHNSClicked()));

    // We have no help and defaults and apply buttons.
    setButtons(buttons() ^ KCModule::Help ^ KCModule::Default ^ KCModule::Apply);

    updateListViewContents();
}

Module::~Module()
{
    delete ui;
}

void Module::importScript()
{
    QString path = KFileDialog::getOpenFileName(KUrl(), "*.kwinscript|KWin scripts (*.kwinscript)");

    if (path.isNull()) {
        return;
    }
    if (!Plasma::Package::installPackage(path, componentData().dirs()->saveLocation("data", "kwin/scripts/"), "kwin-script-")) {
        KMessageWidget* msgWidget = new KMessageWidget;
        msgWidget->setText(ki18n("Cannot import selected script: maybe a script already exists with the same name or there is a permission problem.").toString());
        msgWidget->setMessageType(KMessageWidget::Error);
        ui->verticalLayout2->insertWidget(0, msgWidget);
        msgWidget->animatedShow();
    }
    // TODO: reload list after successful import
}

void Module::updateListViewContents()
{
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Script", "not (exist [X-KWin-Exclude-Listing]) or [X-KWin-Exclude-Listing] == false");
    QList<KPluginInfo> scriptinfos = KPluginInfo::fromServices(offers);
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
    // TODO: reload scripts in KWin

    emit changed(false);
}

void Module::slotGHNSClicked()
{
    QPointer<KNS3::DownloadDialog> downloadDialog = new KNS3::DownloadDialog("kwinscripts.knsrc", this);
    if (downloadDialog->exec() == KDialog::Accepted) {
        if (!downloadDialog->changedEntries().isEmpty()) {
            updateListViewContents();
        }
    }
    delete downloadDialog;
}
