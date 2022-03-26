/*
    SPDX-FileCopyrightText: 2017 Demitrius Belai <demitriusbelai@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "aurorae.h"

#include <KLocalizedString>

void AuroraePackage::initPackage(KPackage::Package *package)
{
    package->setContentsPrefixPaths(QStringList());
    package->setDefaultPackageRoot(QStringLiteral("aurorae/themes/"));

    package->addFileDefinition("decoration", QStringLiteral("decoration.svgz"),
                               i18n("Window Decoration"));
    package->setRequired("decoration", true);

    package->addFileDefinition("close", QStringLiteral("close.svgz"),
                               i18n("Close Button"));

    package->addFileDefinition("minimize", QStringLiteral("minimize.svgz"),
                               i18n("Minimize Button"));

    package->addFileDefinition("maximize", QStringLiteral("maximize.svgz"),
                               i18n("Maximize Button"));

    package->addFileDefinition("restore", QStringLiteral("restore.svgz"),
                               i18n("Restore Button"));

    package->addFileDefinition("alldesktops", QStringLiteral("alldesktops.svgz"),
                               i18n("Sticky Button"));

    package->addFileDefinition("keepabove", QStringLiteral("keepabove.svgz"),
                               i18n("Keepabove Button"));

    package->addFileDefinition("keepbelow", QStringLiteral("keepbelow.svgz"),
                               i18n("Keepbelow Button"));

    package->addFileDefinition("shade", QStringLiteral("shade.svgz"),
                               i18n("Shade Button"));

    package->addFileDefinition("help", QStringLiteral("help.svgz"),
                               i18n("Help Button"));

    package->addFileDefinition("configrc", QStringLiteral("configrc"),
                               i18n("Configuration file"));

    QStringList mimetypes;
    mimetypes << QStringLiteral("image/svg+xml-compressed");
    package->setDefaultMimeTypes(mimetypes);
}

void AuroraePackage::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }

    KPluginMetaData md(package->metadata().metaDataFileName());

    if (!md.pluginId().isEmpty()) {
        QString configrc = md.pluginId() + "rc";
        package->addFileDefinition("configrc", configrc, i18n("Configuration file"));
    }
}

K_PLUGIN_CLASS_WITH_JSON(AuroraePackage, "kwin-packagestructure-aurorae.json")

#include "aurorae.moc"
