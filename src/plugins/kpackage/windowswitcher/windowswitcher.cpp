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

    void pathChanged(KPackage::Package *package) override
    {
        if (package->path().isEmpty()) {
            return;
        }

        const QString mainScript = package->metadata().value("X-Plasma-MainScript");
        if (!mainScript.isEmpty()) {
            package->addFileDefinition("mainscript", mainScript);
        }
    }
};

K_PLUGIN_CLASS_WITH_JSON(SwitcherPackage, "windowswitcher.json")

#include "windowswitcher.moc"
