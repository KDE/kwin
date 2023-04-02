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

        package->addFileDefinition("mainscript", QStringLiteral("code/main.js"));
        package->setRequired("mainscript", true);

        package->addFileDefinition("config", QStringLiteral("config/main.xml"));
        package->setMimeTypes("config", QStringList{QStringLiteral("text/xml")});

        package->addFileDefinition("configui", QStringLiteral("ui/config.ui"));
        package->setMimeTypes("configui", QStringList{QStringLiteral("text/xml")});
    }

    void pathChanged(KPackage::Package *package) override
    {
        if (package->path().isEmpty()) {
            return;
        }

        const QString mainScript = package->metadata().value("X-Plasma-MainScript");
        if (mainScript.isEmpty()) {
            return;
        }

        package->addFileDefinition("mainscript", mainScript);
    }
};

K_PLUGIN_CLASS_WITH_JSON(EffectPackageStructure, "effect.json")

#include "effect.moc"
