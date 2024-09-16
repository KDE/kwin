/*
    SPDX-FileCopyrightText: 2017 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KPackage/PackageStructure>

class ScriptsPackage : public KPackage::PackageStructure
{
    Q_OBJECT
public:
    using KPackage::PackageStructure::PackageStructure;
    void initPackage(KPackage::Package *package) override
    {
        package->setDefaultPackageRoot(QStringLiteral("kwin/scripts/"));

        package->addDirectoryDefinition("config", QStringLiteral("config"));
        package->setMimeTypes("config", QStringList{QStringLiteral("text/xml")});

        package->addDirectoryDefinition("ui", QStringLiteral("ui"));

        package->addDirectoryDefinition("code", QStringLiteral("code"));

        package->setMimeTypes("scripts", QStringList{QStringLiteral("text/plain")});
    }

    void pathChanged(KPackage::Package *package) override
    {
        if (!package->metadata().isValid()) {
            return;
        }

        const QString api = package->metadata().value(QStringLiteral("X-Plasma-API"));
        if (api == QStringLiteral("javascript")) {
            package->addFileDefinition("mainscript", QStringLiteral("code/main.js"));
            package->setRequired("mainscript", true);
        } else if (api == QStringLiteral("declarativescript")) {
            package->addFileDefinition("mainscript", QStringLiteral("ui/main.qml"));
            package->setRequired("mainscript", true);
        }
    }
};

K_PLUGIN_CLASS_WITH_JSON(ScriptsPackage, "scripts.json")

#include "scripts.moc"
