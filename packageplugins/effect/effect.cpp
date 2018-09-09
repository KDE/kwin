/******************************************************************************
*   Copyright 2018 Vlad Zagorodniy <vladzzag@gmail.com>                       *
*                                                                             *
*   This library is free software; you can redistribute it and/or             *
*   modify it under the terms of the GNU Library General Public               *
*   License as published by the Free Software Foundation; either              *
*   version 2 of the License, or (at your option) any later version.          *
*                                                                             *
*   This library is distributed in the hope that it will be useful,           *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          *
*   Library General Public License for more details.                          *
*                                                                             *
*   You should have received a copy of the GNU Library General Public License *
*   along with this library; see the file COPYING.LIB.  If not, write to      *
*   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
*   Boston, MA 02110-1301, USA.                                               *
*******************************************************************************/

#include "effect.h"

#include <KLocalizedString>

EffectPackageStructure::EffectPackageStructure(QObject *parent, const QVariantList &args)
    : KPackage::PackageStructure(parent, args)
{
}

void EffectPackageStructure::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/effects/"));

    package->addDirectoryDefinition("code", QStringLiteral("code"), i18n("Executable Scripts"));
    package->setMimeTypes("code", {QStringLiteral("text/plain")});

    package->addFileDefinition("mainscript", QStringLiteral("code/main.js"), i18n("Main Script File"));
    package->setRequired("mainscript", true);

    package->addFileDefinition("config", QStringLiteral("config/main.xml"), i18n("Configuration Definition File"));
    package->setMimeTypes("config", {QStringLiteral("text/xml")});

    package->addFileDefinition("configui", QStringLiteral("ui/config.ui"), i18n("KCM User Interface File"));
    package->setMimeTypes("configui", {QStringLiteral("text/xml")});
}

void EffectPackageStructure::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }

    const KPluginMetaData md(package->metadata().metaDataFileName());
    const QString mainScript = md.value("X-Plasma-MainScript");
    if (mainScript.isEmpty()) {
        return;
    }

    package->addFileDefinition("mainscript", mainScript, i18n("Main Script File"));
}

K_EXPORT_KPACKAGE_PACKAGE_WITH_JSON(EffectPackageStructure, "kwin-packagestructure-effect.json")

#include "effect.moc"
