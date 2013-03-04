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
#include <KDE/KStandardDirs>
#include <KDE/KLocalizedString>
#include <Plasma/ConfigLoader>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QLabel>
#include <QUiLoader>
#include <QVBoxLayout>

namespace KWin {

GenericScriptedConfigFactory::GenericScriptedConfigFactory()
    : KPluginFactory("kcm_kwin4_genericscripted")
{
}

QObject *GenericScriptedConfigFactory::create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args, const QString &keyword)
{
    Q_UNUSED(iface)
    Q_UNUSED(parent)
    if (keyword.startsWith("kwin4_effect_")) {
        return new ScriptedEffectConfig(componentData(), keyword, parentWidget, args);
    } else {
        return new ScriptingConfig(componentData(), keyword, parentWidget, args);
    }
}

GenericScriptedConfig::GenericScriptedConfig(const KComponentData &data, const QString &keyword, QWidget *parent, const QVariantList &args)
    : KCModule(data, parent, args)
    , m_packageName(keyword)
{
}

GenericScriptedConfig::~GenericScriptedConfig()
{
}

void GenericScriptedConfig::createUi()
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    const QString kconfigXTFile = KStandardDirs::locate("data", QLatin1String(KWIN_NAME) + '/' + typeName() + '/' + m_packageName + "/contents/config/main.xml");
    const QString uiPath = KStandardDirs::locate("data", QLatin1String(KWIN_NAME) + '/' + typeName() + '/' + m_packageName + "/contents/ui/config.ui");
    if (kconfigXTFile.isEmpty() || uiPath.isEmpty()) {
        layout->addWidget(new QLabel(i18nc("Error message", "Plugin does not provide configuration file in expected location")));
        return;
    }
    QFile xmlFile(kconfigXTFile);
    KConfigGroup cg = configGroup();
    Plasma::ConfigLoader *configLoader = new Plasma::ConfigLoader(&cg, &xmlFile, this);
    // load the ui file
    QUiLoader *loader = new QUiLoader(this);
    QFile uiFile(uiPath);
    uiFile.open(QFile::ReadOnly);
    QWidget *customConfigForm = loader->load(&uiFile, this);
    uiFile.close();
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

ScriptedEffectConfig::ScriptedEffectConfig(const KComponentData &data, const QString &keyword, QWidget *parent, const QVariantList &args)
    : GenericScriptedConfig(data, keyword, parent, args)
{
    createUi();
}

ScriptedEffectConfig::~ScriptedEffectConfig()
{
}

QString ScriptedEffectConfig::typeName() const
{
    return QString("effects");
}

KConfigGroup ScriptedEffectConfig::configGroup()
{
    return KSharedConfig::openConfig(KWIN_CONFIG)->group("Effect-" + packageName());
}

void ScriptedEffectConfig::reload()
{
    QDBusMessage message = QDBusMessage::createMethodCall("org.kde.kwin", "/KWin", "org.kde.KWin", "reconfigureEffect");
    message << QString(packageName());
    QDBusConnection::sessionBus().send(message);
}

ScriptingConfig::ScriptingConfig(const KComponentData &data, const QString &keyword, QWidget *parent, const QVariantList &args)
    : GenericScriptedConfig(data, keyword, parent, args)
{
    createUi();
}

ScriptingConfig::~ScriptingConfig()
{
}

KConfigGroup ScriptingConfig::configGroup()
{
    return KSharedConfig::openConfig(KWIN_CONFIG)->group("Script-" + packageName());
}

QString ScriptingConfig::typeName() const
{
    return QString("scripts");
}

void ScriptingConfig::reload()
{
    // TODO: what to call
}

K_EXPORT_PLUGIN(GenericScriptedConfigFactory())

} // namespace
