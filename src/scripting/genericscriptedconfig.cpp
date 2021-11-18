/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    Q_UNUSED(keyword)

    // the plugin id is in the args when created by desktop effects kcm or EffectsModel in general
    QString pluginId = args.isEmpty() ? QString() : args.first().toString();

    // If we do not get the id of the effect we want to load from the args, we have to check our metadata.
    // This can be the case if the factory gets loaded from a KPluginSelector
    // the plugin id is in plugin factory metadata when created by scripts kcm
    // (because it uses kpluginselector, which doesn't pass the plugin id as the first arg),
    // can be dropped once the scripts kcm is ported to qtquick (because then we could pass the plugin id via the args)
    if (pluginId.isEmpty()) {
        pluginId = metaData().pluginId();
    }
    if (pluginId.startsWith(QLatin1String("kwin4_effect_"))) {
        return new ScriptedEffectConfig(pluginId, parentWidget, args);
    } else {
        return new ScriptingConfig(pluginId, parentWidget, args);
    }
}

GenericScriptedConfig::GenericScriptedConfig(const QString &keyword, QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
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
    const QString metaDataPath =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral(KWIN_NAME "/%1/%2/metadata.desktop").arg(typeName(), m_packageName));
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

ScriptedEffectConfig::ScriptedEffectConfig(const QString &keyword, QWidget *parent, const QVariantList &args)
    : GenericScriptedConfig(keyword, parent, args)
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

ScriptingConfig::ScriptingConfig(const QString &keyword, QWidget *parent, const QVariantList &args)
    : GenericScriptedConfig(keyword, parent, args)
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
