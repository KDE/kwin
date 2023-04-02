/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "scripts.h"

void ScriptsPackage::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/scripts/"));

    package->addDirectoryDefinition("config", QStringLiteral("config"));
    package->setMimeTypes("config", QStringList{QStringLiteral("text/xml")});

    package->addDirectoryDefinition("ui", QStringLiteral("ui"));

    package->addDirectoryDefinition("code", QStringLiteral("code"));

    package->addFileDefinition("mainscript", QStringLiteral("code/main.js"));
    package->setRequired("mainscript", true);

    package->setMimeTypes("scripts", QStringList{QStringLiteral("text/plain")});
}

void ScriptsPackage::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }

    const QString mainScript = package->metadata().value("X-Plasma-MainScript");
    if (!mainScript.isEmpty()) {
        package->addFileDefinition("mainscript", mainScript);
    }
}

K_PLUGIN_CLASS_WITH_JSON(ScriptsPackage, "kwin-packagestructure-scripts.json")

#include "scripts.moc"
