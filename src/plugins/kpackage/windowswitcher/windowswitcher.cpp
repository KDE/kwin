/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "windowswitcher.h"

void SwitcherPackage::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/tabbox/"));

    package->addDirectoryDefinition("config", QStringLiteral("config"));
    package->setMimeTypes("config", QStringList{QStringLiteral("text/xml")});

    package->addDirectoryDefinition("ui", QStringLiteral("ui"));

    package->addDirectoryDefinition("code", QStringLiteral("code"));

    package->addFileDefinition("mainscript", QStringLiteral("ui/main.qml"));
    package->setRequired("mainscript", true);

    package->setMimeTypes("windowswitcher", QStringList(QStringLiteral("text/plain")));
}

void SwitcherPackage::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }

    const QString mainScript = package->metadata().value("X-Plasma-MainScript");
    if (!mainScript.isEmpty()) {
        package->addFileDefinition("mainscript", mainScript);
    }
}

K_PLUGIN_CLASS_WITH_JSON(SwitcherPackage, "kwin-packagestructure-windowswitcher.json")

#include "windowswitcher.moc"
