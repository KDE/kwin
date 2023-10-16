/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KPackage/PackageStructure>

class SwitcherPackage : public KPackage::PackageStructure
{
    Q_OBJECT
public:
    using KPackage::PackageStructure::PackageStructure;
    void initPackage(KPackage::Package *package) override
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
};

K_PLUGIN_CLASS_WITH_JSON(SwitcherPackage, "windowswitcher.json")

#include "windowswitcher.moc"
