/*
    SPDX-FileCopyrightText: 2017 Demitrius Belai <demitriusbelai@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KPackage/PackageStructure>

class DecorationPackage : public KPackage::PackageStructure
{
    Q_OBJECT
public:
    using KPackage::PackageStructure::PackageStructure;
    void initPackage(KPackage::Package *package) override
    {
        package->setDefaultPackageRoot(QStringLiteral("kwin/decorations/"));

        package->addDirectoryDefinition("config", QStringLiteral("config"));
        package->setMimeTypes("config", QStringList{QStringLiteral("text/xml")});

        package->addDirectoryDefinition("ui", QStringLiteral("ui"));

        package->addDirectoryDefinition("code", QStringLiteral("code"));

        package->addFileDefinition("mainscript", QStringLiteral("code/main.qml"));
        package->setRequired("mainscript", true);

        package->setMimeTypes("decoration", QStringList{QStringLiteral("text/plain")});
    }
};

K_PLUGIN_CLASS_WITH_JSON(DecorationPackage, "decoration.json")

#include "decoration.moc"
