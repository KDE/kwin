/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "windowswitcher.h"

#include <KLocalizedString>

void SwitcherPackage::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/tabbox/"));

    package->addDirectoryDefinition("config", QStringLiteral("config"), i18n("Configuration Definitions"));
    QStringList mimetypes;
    mimetypes << QStringLiteral("text/xml");
    package->setMimeTypes("config", mimetypes);

    package->addDirectoryDefinition("ui", QStringLiteral("ui"), i18n("User Interface"));

    package->addDirectoryDefinition("code", QStringLiteral("code"), i18n("Executable windowswitcher"));

    package->addFileDefinition("mainscript", QStringLiteral("ui/main.qml"), i18n("Main Script File"));
    package->setRequired("mainscript", true);

    mimetypes.clear();
    mimetypes << QStringLiteral("text/plain");
    package->setMimeTypes("windowswitcher", mimetypes);
}

void SwitcherPackage::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }

    KPluginMetaData md(package->metadata().metaDataFileName());
    QString mainScript = md.value("X-Plasma-MainScript");

    if (!mainScript.isEmpty()) {
        package->addFileDefinition("mainscript", mainScript, i18n("Main Script File"));
    }
}

K_EXPORT_KPACKAGE_PACKAGE_WITH_JSON(SwitcherPackage, "kwin-packagestructure-windowswitcher.json")

#include "windowswitcher.moc"

