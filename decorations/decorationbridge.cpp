/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "decorationbridge.h"
#include "decoratedclient.h"
#include "decorationrenderer.h"
#include "settings.h"
// KWin core
#include "client.h"
#include "composite.h"
#include "scene.h"

// KDecoration
#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationSettings>

// Frameworks
#include <KPluginTrader>
#include <KPluginLoader>

// Qt
#include <QDebug>
#include <QPainter>

namespace KWin
{
namespace Decoration
{

static const QString s_pluginName = QStringLiteral("org.kde.kdecoration2");

DecorationBridge::DecorationBridge(QObject *parent)
    : QObject(parent)
    , KDecoration2::DecorationBridge()
    , m_factory(nullptr)
    , m_blur(false)
{
}

DecorationBridge::~DecorationBridge() = default;

DecorationBridge *DecorationBridge::self()
{
    return static_cast<KWin::Decoration::DecorationBridge*>(KDecoration2::DecorationBridge::self());
}

void DecorationBridge::init()
{
    KConfigGroup config = KSharedConfig::openConfig(KWIN_CONFIG)->group(s_pluginName);
    KDecoration2::DecorationSettings::self(this);

    const auto offers = KPluginTrader::self()->query(s_pluginName,
                                                     s_pluginName,
                                                     QStringLiteral("[X-KDE-PluginInfo-Name] == '%1'").arg(config.readEntry("library", "org.kde.breeze")));
    if (offers.isEmpty()) {
        qWarning() << "Could not locate decoration plugin";
        return;
    }
    qDebug() << "Trying to load decoration plugin: " << offers.first().libraryPath();
    KPluginLoader loader(offers.first().libraryPath());
    KPluginFactory *factory = loader.factory();
    if (!factory) {
        qWarning() << "Error loading plugin:" << loader.errorString();
    } else {
        m_factory = factory;
        loadMetaData(loader.metaData().value(QStringLiteral("MetaData")).toObject());
    }
}

void DecorationBridge::loadMetaData(const QJsonObject &object)
{
    // reset all settings
    m_blur = false;

    // load the settings
    const QJsonValue decoSettings = object.value(s_pluginName);
    if (decoSettings.isUndefined()) {
        // no settings
        return;
    }
    const QVariantMap decoSettingsMap = decoSettings.toObject().toVariantMap();
    auto blurIt = decoSettingsMap.find(QStringLiteral("blur"));
    if (blurIt != decoSettingsMap.end()) {
        m_blur = blurIt.value().toBool();
    }
}

KDecoration2::DecoratedClientPrivate *DecorationBridge::createClient(KDecoration2::DecoratedClient *client, KDecoration2::Decoration *decoration)
{
    return new DecoratedClientImpl(static_cast<Client*>(decoration->parent()), client, decoration);
}

KDecoration2::DecorationSettingsPrivate *DecorationBridge::settings(KDecoration2::DecorationSettings *parent)
{
    return new SettingsImpl(parent);
}

void DecorationBridge::update(KDecoration2::Decoration *decoration, const QRect &geometry)
{
    // TODO: remove check once all compositors implement it
    if (Renderer *renderer = static_cast<DecoratedClientImpl*>(decoration->client()->handle())->renderer()) {
        renderer->schedule(geometry);
    }
}

KDecoration2::Decoration *DecorationBridge::createDecoration(Client *client)
{
    if (!m_factory) {
        return nullptr;
    }
    return m_factory->create<KDecoration2::Decoration>(client);
}


} // Decoration
} // KWin
