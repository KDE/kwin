/*
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KPackage/PackageStructure>

class EffectPackageStructure : public KPackage::PackageStructure
{
    Q_OBJECT
public:
    using KPackage::PackageStructure::PackageStructure;
    void initPackage(KPackage::Package *package) override
    {
        package->setDefaultPackageRoot(QStringLiteral("kwin/effects/"));

        package->addDirectoryDefinition("code", QStringLiteral("code"));
        package->setMimeTypes("code", QStringList{QStringLiteral("text/plain")});

        package->addDirectoryDefinition("ui", QStringLiteral("ui"));
        package->setMimeTypes("ui", QStringList{QStringLiteral("text/plain")});

        package->addFileDefinition("config", QStringLiteral("config/main.xml"));
        package->setMimeTypes("config", QStringList{QStringLiteral("text/xml")});

        package->addFileDefinition("configui", QStringLiteral("ui/config.ui"));
        package->setMimeTypes("configui", QStringList{QStringLiteral("text/xml")});
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

K_PLUGIN_CLASS_WITH_JSON(EffectPackageStructure, "effect.json")

#include "effect.moc"
