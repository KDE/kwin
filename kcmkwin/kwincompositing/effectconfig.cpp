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

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QString>

#include <KCModuleProxy>
#include <KPluginInfo>
#include <KServiceTypeTrader>

EffectConfig::EffectConfig(QObject *parent)
    : QObject(parent)
{
}

QString EffectConfig::serviceName(const QString &effectName) {
    //The effect name is something like "Show Fps" and
    //we want something like "showfps"
    return effectName.toLower().replace(" ", "");
}

bool EffectConfig::effectUiConfigExists(const QString &effectName) {

    const QString effectConfig = serviceName(effectName) + "_config";
    QString effectConfigFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kde5/services/kwin/" + effectConfig +".desktop", QStandardPaths::LocateFile);

    return !effectConfigFile.isEmpty();
}

void EffectConfig::openConfig(const QString &effectName) {
    //setup the UI
    QDialog dialog;
    QVBoxLayout layout;
    KCModuleProxy *proxy;
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    buttons->setCenterButtons(true);

    //Here we connect our buttons with the dialog
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    for(KService::Ptr service : offers) {
        KPluginInfo plugin(service);
        if (plugin.name() == effectName) {
            QString effectConfig = serviceName(plugin.name() + "_config");
            proxy = new KCModuleProxy(effectConfig);

            //setup the Layout of our UI
            layout.addWidget(proxy);
            layout.addWidget(buttons);
            dialog.setLayout(&layout);

            //open the dialog
            if (dialog.exec() == QDialog::Accepted) {
                proxy->save();
            }
        }
    }
}

