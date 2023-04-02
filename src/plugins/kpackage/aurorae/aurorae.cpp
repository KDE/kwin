/*
    SPDX-FileCopyrightText: 2017 Demitrius Belai <demitriusbelai@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "aurorae.h"

void AuroraePackage::initPackage(KPackage::Package *package)
{
    package->setContentsPrefixPaths(QStringList());
    package->setDefaultPackageRoot(QStringLiteral("aurorae/themes/"));

    package->addFileDefinition("decoration", QStringLiteral("decoration.svgz"));
    package->setRequired("decoration", true);

    package->addFileDefinition("close", QStringLiteral("close.svgz"));

    package->addFileDefinition("minimize", QStringLiteral("minimize.svgz"));

    package->addFileDefinition("maximize", QStringLiteral("maximize.svgz"));

    package->addFileDefinition("restore", QStringLiteral("restore.svgz"));

    package->addFileDefinition("alldesktops", QStringLiteral("alldesktops.svgz"));

    package->addFileDefinition("keepabove", QStringLiteral("keepabove.svgz"));

    package->addFileDefinition("keepbelow", QStringLiteral("keepbelow.svgz"));

    package->addFileDefinition("shade", QStringLiteral("shade.svgz"));

    package->addFileDefinition("help", QStringLiteral("help.svgz"));

    package->addFileDefinition("configrc", QStringLiteral("configrc"));

    QStringList mimetypes;
    mimetypes << QStringLiteral("image/svg+xml-compressed");
    package->setDefaultMimeTypes(mimetypes);
}

void AuroraePackage::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }

    const QString configrc = package->metadata().pluginId() + "rc";
    package->addFileDefinition("configrc", configrc);
}

K_PLUGIN_CLASS_WITH_JSON(AuroraePackage, "kwin-packagestructure-aurorae.json")

#include "aurorae.moc"
