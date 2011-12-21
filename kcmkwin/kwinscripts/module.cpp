/*
 * Copyright (c) 2011 Tamas Krutki <ktamasw@gmail.com>
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

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStringList>
#include <QtGui/QPaintEngine>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <KDE/KAboutData>
#include <KDE/KPluginFactory>
#include <KDE/KStandardDirs>
#include <KDE/KMessageBox>
#include <KDE/KFileDialog>
#include <KDE/KMessageWidget>

#include "version.h"

K_PLUGIN_FACTORY_DECLARATION(KcmKWinScriptsFactory);

Module::Module(QWidget *parent, const QVariantList &args) :
    KCModule(KcmKWinScriptsFactory::componentData(), parent, args),
    ui(new Ui::Module)
{
    KAboutData *about = new KAboutData("kwin-scripts", 0,
                                       ki18n("KWin Scripts"),
                                       global_s_versionStringFull,
                                       ki18n("Configure KWin scripts"),
                                       KAboutData::License_GPL_V2);

    about->addAuthor(ki18n("Tamás Krutki"));
    setAboutData(about);

    ui->setupUi(this);

    connect(ui->listWidget, SIGNAL(itemSelectionChanged()), SLOT(updateButtons()));
    connect(ui->exportSelectedButton, SIGNAL(clicked()), SLOT(exportScript()));
    connect(ui->importScriptButton, SIGNAL(clicked()), SLOT(importScript()));
    connect(ui->removeScriptButton, SIGNAL(clicked()), SLOT(removeScript()));

    // We have no help and defaults and apply buttons.
    setButtons(buttons() ^ KCModule::Help ^ KCModule::Default ^ KCModule::Apply);

    ui->listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    updateListViewContents();
    updateButtons();
}

Module::~Module()
{
    delete ui;
}

void Module::updateButtons()
{
    if (ui->listWidget->selectedItems().isEmpty()) {
        ui->exportSelectedButton->setEnabled(false);
        ui->removeScriptButton->setEnabled(false);
    } else {
        ui->exportSelectedButton->setEnabled(true);
        ui->removeScriptButton->setEnabled(true);
    }
}

void Module::exportScript()
{
    QString path = KFileDialog::getSaveFileName(KUrl(), "*.kwinscript *.kws *.kwinqs|KWin scripts (*.kwinscript, *.kws, *.kwinqs)");

    if (!path.isNull()) {
        QFile f(componentData().dirs()->findResource("data", "kwin/scripts/" + ui->listWidget->currentItem()->text()));

        QFileInfo pathInfo(path);
        QDir dir(pathInfo.absolutePath());

        if (dir.exists(pathInfo.fileName())) {
            dir.remove(pathInfo.fileName());
        }

        if (f.copy(path)) {
            KMessageWidget* msgWidget = new KMessageWidget;
            msgWidget->setText(ki18n("The selected script was exported successfully!").toString());
            msgWidget->setMessageType(KMessageWidget::Positive);
            ui->verticalLayout2->insertWidget(0, msgWidget);
            msgWidget->animatedShow();
        } else {
            KMessageWidget* msgWidget = new KMessageWidget;
            msgWidget->setText(ki18n("An error occurred, the selected script could not be exported!").toString());
            msgWidget->setMessageType(KMessageWidget::Error);
            ui->verticalLayout2->insertWidget(0, msgWidget);
            msgWidget->animatedShow();
        }
    }

    updateListViewContents();
    updateButtons();
}

void Module::importScript()
{
    QString path = KFileDialog::getOpenFileName(KUrl(), "*.kwinscript *.kws *.kwinqs|KWin scripts (*.kwinscript, *.kws, *.kwinqs)");

    if (!path.isNull()) {
        QFileInfo pathInfo(path);
        QString fileName(pathInfo.fileName());

        QFile f(path);
        if (!f.copy(componentData().dirs()->saveLocation("data", "kwin/scripts/") + fileName)) {
            KMessageWidget* msgWidget = new KMessageWidget;
            msgWidget->setText(ki18n("Cannot import selected script: maybe a script already exists with the same name or there is a permission problem.").toString());
            msgWidget->setMessageType(KMessageWidget::Error);
            ui->verticalLayout2->insertWidget(0, msgWidget);
            msgWidget->animatedShow();
        } else {
            f.setFileName(componentData().dirs()->saveLocation("data", "kwin/scripts/") + fileName);
            f.open(QIODevice::ReadWrite);
            f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                             QFile::ReadGroup | QFile::ExeGroup |
                             QFile::ReadUser | QFile::ExeUser | QFile::WriteUser |
                             QFile::ReadOther | QFile::ExeOther);
            f.close();

            KMessageWidget* msgWidget = new KMessageWidget;
            msgWidget->setText(ki18n("The selected script was imported successfully!").toString());
            msgWidget->setMessageType(KMessageWidget::Positive);
            ui->verticalLayout2->insertWidget(0, msgWidget);
            msgWidget->animatedShow();
        }
    }

    updateListViewContents();
    updateButtons();
}

void Module::removeScript()
{
    if (KMessageBox::questionYesNo(this, ki18n("Do you really want to delete the selected script?").toString(), ki18n("Remove KWin script").toString()) == KMessageBox::Yes) {
        QDir dir(QFileInfo(componentData().dirs()->findResource("data", "kwin/scripts/" + ui->listWidget->currentItem()->text())).absolutePath());
        dir.remove(ui->listWidget->currentItem()->text());
        updateListViewContents();
        updateButtons();
    }
}

void Module::updateListViewContents()
{
    ui->listWidget->clear();

    QStringList dirList = componentData().dirs()->findDirs("data", "kwin/scripts");
    for (int i = 0; i < dirList.size(); i++) {
        QDir dir(dirList[i]);
        QStringList fileNameList = dir.entryList(QStringList() << "*.kws" << "*.kwinscript" << "*.kwinqs", QDir::Files, QDir::Name);

        for (int j = 0; j < fileNameList.size(); j++) {
            ui->listWidget->addItem(fileNameList[j]);
        }
    }
}
