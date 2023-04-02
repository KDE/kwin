/*
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "effect.h"

EffectPackageStructure::EffectPackageStructure(QObject *parent, const QVariantList &args)
    : KPackage::PackageStructure(parent, args)
{
}

void EffectPackageStructure::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/effects/"));

    package->addDirectoryDefinition("code", QStringLiteral("code"));
    package->setMimeTypes("code", {QStringLiteral("text/plain")});

    package->addFileDefinition("mainscript", QStringLiteral("code/main.js"));
    package->setRequired("mainscript", true);

    package->addFileDefinition("config", QStringLiteral("config/main.xml"));
    package->setMimeTypes("config", {QStringLiteral("text/xml")});

    package->addFileDefinition("configui", QStringLiteral("ui/config.ui"));
    package->setMimeTypes("configui", {QStringLiteral("text/xml")});
}

void EffectPackageStructure::pathChanged(KPackage::Package *package)
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

K_PLUGIN_CLASS_WITH_JSON(EffectPackageStructure, "kwin-packagestructure-effect.json")

#include "effect.moc"
