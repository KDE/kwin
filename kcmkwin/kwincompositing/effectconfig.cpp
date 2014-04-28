/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
*                                                                        *
* This program is free software; you can redistribute it and/or modify   *
* it under the terms of the GNU General Public License as published by   *
* the Free Software Foundation; either version 2 of the License, or      *
* (at your option) any later version.                                    *
*                                                                        *
* This program is distributed in the hope that it will be useful,        *
* but WITHOUT ANY WARRANTY; without even the implied warranty of         *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
* GNU General Public License for more details.                           *
*                                                                        *
* You should have received a copy of the GNU General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
**************************************************************************/

#include "effectconfig.h"

#include <KCModule>
#include <KPluginTrader>

#include <KNS3/DownloadDialog>

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QString>

static const QString s_pluginDir = QStringLiteral("kwin/effects/configs/");

namespace KWin {
namespace Compositing {

EffectConfig::EffectConfig(QObject *parent)
    : QObject(parent)
{
}

void EffectConfig::openConfig(const QString &serviceName, bool scripted)
{
    //setup the UI
    QDialog dialog;

    // create the KCModule through the plugintrader
    KCModule *kcm = nullptr;
    if (scripted) {
        // try generic module for scripted
        const auto offers = KPluginTrader::self()->query(s_pluginDir, QString(),
                                                         QStringLiteral("[X-KDE-Library] == 'kcm_kwin4_genericscripted'"));
        if (!offers.isEmpty()) {
            const KPluginInfo &generic = offers.first();
            KPluginLoader loader(generic.libraryPath());
            KPluginFactory *factory = loader.factory();
            if (factory) {
                kcm = factory->create<KCModule>(serviceName, &dialog);
            }
        }
    } else {
        kcm = KPluginTrader::createInstanceFromQuery<KCModule>(s_pluginDir, QString(),
                                                               QStringLiteral("[X-KDE-ParentComponents] == '%1'").arg(serviceName),
                                                               &dialog);
    }
    if (!kcm) {
        return;
    }

    connect(&dialog, &QDialog::accepted, kcm, &KCModule::save);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                     QDialogButtonBox::Cancel |
                                                     QDialogButtonBox::Apply |
                                                     QDialogButtonBox::RestoreDefaults |
                                                     QDialogButtonBox::Reset,
                                                     &dialog);

    QPushButton *apply = buttons->button(QDialogButtonBox::Apply);
    QPushButton *reset = buttons->button(QDialogButtonBox::Reset);
    apply->setEnabled(false);
    reset->setEnabled(false);
    //Here we connect our buttons with the dialog
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(apply, &QPushButton::clicked, kcm, &KCModule::save);
    connect(reset, &QPushButton::clicked, kcm, &KCModule::load);
    auto changedSignal = static_cast<void(KCModule::*)(bool)>(&KCModule::changed);
    connect(kcm, changedSignal, apply, &QPushButton::setEnabled);
    connect(kcm, changedSignal, reset, &QPushButton::setEnabled);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, kcm, &KCModule::defaults);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(kcm);
    layout->addWidget(buttons);
    dialog.exec();
}

void EffectConfig::openGHNS()
{
    QPointer<KNS3::DownloadDialog> downloadDialog = new KNS3::DownloadDialog(QStringLiteral("kwineffect.knsrc"));
    if (downloadDialog->exec() == QDialog::Accepted) {
        emit effectListChanged();
    }

    delete downloadDialog;
}

}//end namespace Compositing
}//end namespace KWin
