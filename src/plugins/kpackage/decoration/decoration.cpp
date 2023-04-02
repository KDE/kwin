/*
    SPDX-FileCopyrightText: 2017 Demitrius Belai <demitriusbelai@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "decoration.h"

void DecorationPackage::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/decorations/"));

    package->addDirectoryDefinition("config", QStringLiteral("config"));
    QStringList mimetypes;
    mimetypes << QStringLiteral("text/xml");
    package->setMimeTypes("config", mimetypes);

    package->addDirectoryDefinition("ui", QStringLiteral("ui"));

    package->addDirectoryDefinition("code", QStringLiteral("code"));

    package->addFileDefinition("mainscript", QStringLiteral("code/main.qml"));
    package->setRequired("mainscript", true);

    mimetypes.clear();
    mimetypes << QStringLiteral("text/plain");
    package->setMimeTypes("decoration", mimetypes);
}

void DecorationPackage::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }

    const QString mainScript = package->metadata().value("X-Plasma-MainScript");
    if (!mainScript.isEmpty()) {
        package->addFileDefinition("mainscript", mainScript);
    }
}

K_PLUGIN_CLASS_WITH_JSON(DecorationPackage, "kwin-packagestructure-decoration.json")

#include "decoration.moc"
