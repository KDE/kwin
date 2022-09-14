/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "decorationbridge.h"

#include <config-kwin.h>

#include "decoratedclient.h"
#include "decorations_logging.h"
#include "settings.h"
// KWin core
#include "wayland/server_decoration_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

// KDecoration
#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationSettings>

// Frameworks
#include <KPluginFactory>
#include <KPluginMetaData>

// Qt
#include <QMetaProperty>
#include <QPainter>

namespace KWin
{
namespace Decoration
{

static const QString s_aurorae = QStringLiteral("org.kde.kwin.aurorae");
static const QString s_pluginName = QStringLiteral("org.kde.kdecoration2");
#if HAVE_BREEZE_DECO
static const QString s_defaultPlugin = QStringLiteral(BREEZE_KDECORATION_PLUGIN_ID);
#else
static const QString s_defaultPlugin = s_aurorae;
#endif

DecorationBridge::DecorationBridge()
    : m_factory(nullptr)
    , m_showToolTips(false)
    , m_settings()
    , m_noPlugin(false)
{
    readDecorationOptions();
}

QString DecorationBridge::readPlugin()
{
    return kwinApp()->config()->group(s_pluginName).readEntry("library", s_defaultPlugin);
}

static bool readNoPlugin()
{
    return kwinApp()->config()->group(s_pluginName).readEntry("NoPlugin", false);
}

QString DecorationBridge::readTheme() const
{
    return kwinApp()->config()->group(s_pluginName).readEntry("theme", m_defaultTheme);
}

void DecorationBridge::readDecorationOptions()
{
    m_showToolTips = kwinApp()->config()->group(s_pluginName).readEntry("ShowToolTips", true);
}

bool DecorationBridge::hasPlugin()
{
    const DecorationBridge *bridge = workspace()->decorationBridge();
    if (!bridge) {
        return false;
    }
    return !bridge->m_noPlugin && bridge->m_factory;
}

void DecorationBridge::init()
{
    using namespace KWaylandServer;
    m_noPlugin = readNoPlugin();
    if (m_noPlugin) {
        if (waylandServer()) {
            waylandServer()->decorationManager()->setDefaultMode(ServerSideDecorationManagerInterface::Mode::None);
        }
        return;
    }
    m_plugin = readPlugin();
    m_settings = QSharedPointer<KDecoration2::DecorationSettings>::create(this);
    if (!initPlugin()) {
        if (m_plugin != s_defaultPlugin) {
            // try loading default plugin
            m_plugin = s_defaultPlugin;
            initPlugin();
        }
        // default plugin failed to load, try fallback
        if (!m_factory) {
            m_plugin = s_aurorae;
            initPlugin();
        }
    }
    if (waylandServer()) {
        waylandServer()->decorationManager()->setDefaultMode(m_factory ? ServerSideDecorationManagerInterface::Mode::Server : ServerSideDecorationManagerInterface::Mode::None);
    }
}

bool DecorationBridge::initPlugin()
{
    const KPluginMetaData metaData = KPluginMetaData::findPluginById(s_pluginName, m_plugin);
    if (!metaData.isValid()) {
        qCWarning(KWIN_DECORATIONS) << "Could not locate decoration plugin" << m_plugin;
        return false;
    }
    qCDebug(KWIN_DECORATIONS) << "Trying to load decoration plugin: " << metaData.fileName();
    auto factoryResult = KPluginFactory::loadFactory(metaData);
    if (factoryResult) {
        m_factory.reset(factoryResult.plugin);
        loadMetaData(metaData.rawData());
        return true;
    } else {
        qCWarning(KWIN_DECORATIONS) << "Error loading plugin:" << factoryResult.errorText;
        return false;
    }
}

static void recreateDecorations()
{
    Workspace::self()->forEachAbstractClient([](Window *window) {
        window->invalidateDecoration();
    });
}

void DecorationBridge::reconfigure()
{
    readDecorationOptions();

    if (m_noPlugin != readNoPlugin()) {
        m_noPlugin = !m_noPlugin;
        // no plugin setting changed
        if (m_noPlugin) {
            // decorations disabled now
            m_plugin = QString();
            m_factory.reset();
            m_settings.reset();
        } else {
            // decorations enabled now
            init();
        }
        recreateDecorations();
        return;
    }

    const QString newPlugin = readPlugin();
    if (newPlugin != m_plugin) {
        // plugin changed, recreate everything
        auto oldFactory = std::move(m_factory);
        const auto oldPluginName = m_plugin;
        m_plugin = newPlugin;
        if (!initPlugin()) {
            // loading new plugin failed
            m_factory = std::move(oldFactory);
            m_plugin = oldPluginName;
        } else {
            recreateDecorations();
            // TODO: unload and destroy old plugin
        }
    } else {
        // same plugin, but theme might have changed
        const QString oldTheme = m_theme;
        m_theme = readTheme();
        if (m_theme != oldTheme) {
            recreateDecorations();
        }
    }
}

void DecorationBridge::loadMetaData(const QJsonObject &object)
{
    // reset all settings
    m_recommendedBorderSize = QString();
    m_theme = QString();
    m_defaultTheme = QString();

    // load the settings
    const QJsonValue decoSettings = object.value(s_pluginName);
    if (decoSettings.isUndefined()) {
        // no settings
        return;
    }
    const QVariantMap decoSettingsMap = decoSettings.toObject().toVariantMap();
    auto recBorderSizeIt = decoSettingsMap.find(QStringLiteral("recommendedBorderSize"));
    if (recBorderSizeIt != decoSettingsMap.end()) {
        m_recommendedBorderSize = recBorderSizeIt.value().toString();
    }
    findTheme(decoSettingsMap);

    Q_EMIT metaDataLoaded();
}

void DecorationBridge::findTheme(const QVariantMap &map)
{
    auto it = map.find(QStringLiteral("themes"));
    if (it == map.end()) {
        return;
    }
    if (!it.value().toBool()) {
        return;
    }
    it = map.find(QStringLiteral("defaultTheme"));
    m_defaultTheme = it != map.end() ? it.value().toString() : QString();
    m_theme = readTheme();
}

std::unique_ptr<KDecoration2::DecoratedClientPrivate> DecorationBridge::createClient(KDecoration2::DecoratedClient *client, KDecoration2::Decoration *decoration)
{
    return std::unique_ptr<DecoratedClientImpl>(new DecoratedClientImpl(static_cast<Window *>(decoration->parent()), client, decoration));
}

std::unique_ptr<KDecoration2::DecorationSettingsPrivate> DecorationBridge::settings(KDecoration2::DecorationSettings *parent)
{
    return std::unique_ptr<SettingsImpl>(new SettingsImpl(parent));
}

KDecoration2::Decoration *DecorationBridge::createDecoration(Window *window)
{
    if (m_noPlugin) {
        return nullptr;
    }
    if (!m_factory) {
        return nullptr;
    }
    QVariantMap args({{QStringLiteral("bridge"), QVariant::fromValue(this)}});

    if (!m_theme.isEmpty()) {
        args.insert(QStringLiteral("theme"), m_theme);
    }
    auto deco = m_factory->create<KDecoration2::Decoration>(window, QVariantList({args}));
    deco->setSettings(m_settings);
    deco->init();
    return deco;
}

static QString settingsProperty(const QVariant &variant)
{
    if (QLatin1String(variant.typeName()) == QLatin1String("KDecoration2::BorderSize")) {
        return QString::number(variant.toInt());
    } else if (QLatin1String(variant.typeName()) == QLatin1String("QVector<KDecoration2::DecorationButtonType>")) {
        const auto &b = variant.value<QVector<KDecoration2::DecorationButtonType>>();
        QString buffer;
        for (auto it = b.begin(); it != b.end(); ++it) {
            if (it != b.begin()) {
                buffer.append(QStringLiteral(", "));
            }
            buffer.append(QString::number(int(*it)));
        }
        return buffer;
    }
    return variant.toString();
}

QString DecorationBridge::supportInformation() const
{
    QString b;
    if (m_noPlugin) {
        b.append(QStringLiteral("Decorations are disabled"));
    } else {
        b.append(QStringLiteral("Plugin: %1\n").arg(m_plugin));
        b.append(QStringLiteral("Theme: %1\n").arg(m_theme));
        b.append(QStringLiteral("Plugin recommends border size: %1\n").arg(m_recommendedBorderSize.isNull() ? "No" : m_recommendedBorderSize));
        const QMetaObject *metaOptions = m_settings->metaObject();
        for (int i = 0; i < metaOptions->propertyCount(); ++i) {
            const QMetaProperty property = metaOptions->property(i);
            if (QLatin1String(property.name()) == QLatin1String("objectName")) {
                continue;
            }
            b.append(QStringLiteral("%1: %2\n").arg(property.name(), settingsProperty(m_settings->property(property.name()))));
        }
    }
    return b;
}

} // Decoration
} // KWin
