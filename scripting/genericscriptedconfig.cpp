/*
 *  KWin - the KDE window manager
 *  This file is part of the KDE project.
 *
 * Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "genericscriptedconfig.h"
#include "config-kwin.h"
#include <kwineffects_interface.h>
#include <KAboutData>
#define TRANSLATION_DOMAIN "kwin_scripting"
#include <KLocalizedString>
#include <KLocalizedTranslator>
#include <kconfigloader.h>
#include <KDesktopFile>

#include <QFile>
#include <QLabel>
#include <QUiLoader>
#include <QVBoxLayout>
#include <QStandardPaths>

namespace KWin {

QObject *GenericScriptedConfigFactory::create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args, const QString &keyword)
{
    Q_UNUSED(iface)
    Q_UNUSED(parent)
    if (keyword.startsWith(QLatin1String("kwin4_effect_"))) {
        return new ScriptedEffectConfig(QStringLiteral("kcm_kwin4_genericscripted"), keyword, parentWidget, args);
    } else {
        return new ScriptingConfig(QStringLiteral("kcm_kwin4_genericscripted"), keyword, parentWidget, args);
    }
}

GenericScriptedConfig::GenericScriptedConfig(const QString &componentName, const QString &keyword, QWidget *parent, const QVariantList &args)
    : KCModule(KAboutData::pluginData(componentName), parent, args)
    , m_packageName(keyword)
    , m_translator(new KLocalizedTranslator(this))
{
    QCoreApplication::instance()->installTranslator(m_translator);
}

GenericScriptedConfig::~GenericScriptedConfig()
{
}

void GenericScriptedConfig::createUi()
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    const QString kconfigXTFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                        QLatin1String(KWIN_NAME) +
                                                        QLatin1Char('/') +
                                                        typeName() +
                                                        QLatin1Char('/') +
                                                        m_packageName +
                                                        QLatin1String("/contents/config/main.xml"));
    const QString uiPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                 QLatin1String(KWIN_NAME) +
                                                 QLatin1Char('/') +
                                                 typeName() +
                                                 QLatin1Char('/') +
                                                 m_packageName +
                                                 QLatin1String("/contents/ui/config.ui"));
    if (kconfigXTFile.isEmpty() || uiPath.isEmpty()) {
        layout->addWidget(new QLabel(i18nc("Error message", "Plugin does not provide configuration file in expected location")));
        return;
    }
    QFile xmlFile(kconfigXTFile);
    KConfigGroup cg = configGroup();
    KConfigLoader *configLoader = new KConfigLoader(cg, &xmlFile, this);
    // load the ui file
    QUiLoader *loader = new QUiLoader(this);
    loader->setLanguageChangeEnabled(true);
    QFile uiFile(uiPath);
    // try getting a translation domain
    const QString metaDataPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                        QStringLiteral(KWIN_NAME"/%1/%2/metadata.desktop").arg(typeName()).arg(m_packageName));
    if (!metaDataPath.isNull()) {
        KDesktopFile metaData(metaDataPath);
        m_translator->setTranslationDomain(metaData.desktopGroup().readEntry("X-KWin-Config-TranslationDomain", QString()));
    }

    uiFile.open(QFile::ReadOnly);
    QWidget *customConfigForm = loader->load(&uiFile, this);
    m_translator->addContextToMonitor(customConfigForm->objectName());
    uiFile.close();

    // send a custom event to the translator to retranslate using our translator
    QEvent le(QEvent::LanguageChange);
    QCoreApplication::sendEvent(customConfigForm, &le);

    layout->addWidget(customConfigForm);
    addConfig(configLoader, customConfigForm);
}

void GenericScriptedConfig::save()
{
    KCModule::save();
    reload();
}

void GenericScriptedConfig::reload()
{
}

ScriptedEffectConfig::ScriptedEffectConfig(const QString &componentName, const QString &keyword, QWidget *parent, const QVariantList &args)
    : GenericScriptedConfig(componentName, keyword, parent, args)
{
    createUi();
}

ScriptedEffectConfig::~ScriptedEffectConfig()
{
}

QString ScriptedEffectConfig::typeName() const
{
    return QStringLiteral("effects");
}

KConfigGroup ScriptedEffectConfig::configGroup()
{
    return KSharedConfig::openConfig(QStringLiteral(KWIN_CONFIG))->group(QLatin1String("Effect-") + packageName());
}

void ScriptedEffectConfig::reload()
{
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(packageName());
}

ScriptingConfig::ScriptingConfig(const QString &componentName, const QString &keyword, QWidget *parent, const QVariantList &args)
    : GenericScriptedConfig(componentName, keyword, parent, args)
{
    createUi();
}

ScriptingConfig::~ScriptingConfig()
{
}

KConfigGroup ScriptingConfig::configGroup()
{
    return KSharedConfig::openConfig(QStringLiteral(KWIN_CONFIG))->group(QLatin1String("Script-") + packageName());
}

QString ScriptingConfig::typeName() const
{
    return QStringLiteral("scripts");
}

void ScriptingConfig::reload()
{
    // TODO: what to call
}


} // namespace
