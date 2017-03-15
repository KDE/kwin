/******************************************************************************
*   Copyright 2017 by Demitrius Belai <demitriusbelai@gmail.com>              *
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

K_EXPORT_KPACKAGE_PACKAGE_WITH_JSON(AuroraePackage, "kwin-packagestructure-aurorae.json")

#include "aurorae.moc"

