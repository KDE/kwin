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

#include <KDE/KCModuleProxy>
#include <KDE/KPluginInfo>
#include <KDE/KServiceTypeTrader>

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QString>

namespace KWin {
namespace Compositing {

EffectConfig::EffectConfig(QObject *parent)
    : QObject(parent)
{
}

bool EffectConfig::effectUiConfigExists(const QString &serviceName) {

    //Our effect UI config is something like showfps_config.desktop
    QString tmp = serviceName;
    const QString effectConfig = tmp.remove("kwin4_effect_") + "_config.desktop";
    QString effectConfigFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kde5/services/kwin/" + effectConfig , QStandardPaths::LocateFile);
    return !effectConfigFile.isEmpty();
}

void EffectConfig::openConfig(const QString &effectName) {
    //setup the UI
    QDialog dialog;
    QVBoxLayout layout;
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    buttons->setCenterButtons(true);

    //Here we connect our buttons with the dialog
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    for(KService::Ptr service : offers) {
        KPluginInfo plugin(service);
        if (plugin.name() == effectName) {
            QString effectConfig = effectName.toLower().remove(" ") + "_config";
            KCModuleProxy *proxy = new KCModuleProxy(effectConfig);

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
}//end namespace Compositing
}//end namespace KWin
