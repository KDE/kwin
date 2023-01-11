/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "effectloader.h"
// config
#include <config-kwin.h>
// KWin
#include "plugin.h"
#include "scripting/scriptedeffect.h"
#include "utils/common.h"
#include <kwineffects.h>
// KDE
#include <KConfigGroup>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
// Qt
#include <QDebug>
#include <QFutureWatcher>
#include <QStaticPlugin>
#include <QStringList>
#include <QtConcurrentRun>

namespace KWin
{

AbstractEffectLoader::AbstractEffectLoader(QObject *parent)
    : QObject(parent)
{
}

AbstractEffectLoader::~AbstractEffectLoader()
{
}

void AbstractEffectLoader::setConfig(KSharedConfig::Ptr config)
{
    m_config = config;
}

LoadEffectFlags AbstractEffectLoader::readConfig(const QString &effectName, bool defaultValue) const
{
    Q_ASSERT(m_config);
    KConfigGroup plugins(m_config, QStringLiteral("Plugins"));

    const QString key = effectName + QStringLiteral("Enabled");

    // do we have a key for the effect?
    if (plugins.hasKey(key)) {
        // we have a key in the config, so read the enabled state
        const bool load = plugins.readEntry(key, defaultValue);
        return load ? LoadEffectFlags(LoadEffectFlag::Load) : LoadEffectFlags();
    }
    // we don't have a key, so we just use the enabled by default value
    if (defaultValue) {
        return LoadEffectFlag::Load | LoadEffectFlag::CheckDefaultFunction;
    }
    return LoadEffectFlags();
}

static const QString s_nameProperty = QStringLiteral("X-KDE-PluginInfo-Name");
static const QString s_jsConstraint = QStringLiteral("[X-Plasma-API] == 'javascript'");
static const QString s_serviceType = QStringLiteral("KWin/Effect");

ScriptedEffectLoader::ScriptedEffectLoader(QObject *parent)
    : AbstractEffectLoader(parent)
    , m_queue(new EffectLoadQueue<ScriptedEffectLoader, KPluginMetaData>(this))
{
}

ScriptedEffectLoader::~ScriptedEffectLoader()
{
}

bool ScriptedEffectLoader::hasEffect(const QString &name) const
{
    return findEffect(name).isValid();
}

bool ScriptedEffectLoader::isEffectSupported(const QString &name) const
{
    // scripted effects are in general supported
    if (!ScriptedEffect::supported()) {
        return false;
    }
    return hasEffect(name);
}

QStringList ScriptedEffectLoader::listOfKnownEffects() const
{
    const auto effects = findAllEffects();
    QStringList result;
    for (const auto &service : effects) {
        result << service.pluginId();
    }
    return result;
}

bool ScriptedEffectLoader::loadEffect(const QString &name)
{
    auto effect = findEffect(name);
    if (!effect.isValid()) {
        return false;
    }
    return loadEffect(effect, LoadEffectFlag::Load);
}

bool ScriptedEffectLoader::loadEffect(const KPluginMetaData &effect, LoadEffectFlags flags)
{
    const QString name = effect.pluginId();
    if (!flags.testFlag(LoadEffectFlag::Load)) {
        qCDebug(KWIN_CORE) << "Loading flags disable effect: " << name;
        return false;
    }
    if (m_loadedEffects.contains(name)) {
        qCDebug(KWIN_CORE) << name << "already loaded";
        return false;
    }

    if (!ScriptedEffect::supported()) {
        qCDebug(KWIN_CORE) << "Effect is not supported: " << name;
        return false;
    }

    ScriptedEffect *e = ScriptedEffect::create(effect);
    if (!e) {
        qCDebug(KWIN_CORE) << "Could not initialize scripted effect: " << name;
        return false;
    }
    connect(e, &ScriptedEffect::destroyed, this, [this, name]() {
        m_loadedEffects.removeAll(name);
    });

    qCDebug(KWIN_CORE) << "Successfully loaded scripted effect: " << name;
    Q_EMIT effectLoaded(e, name);
    m_loadedEffects << name;
    return true;
}

void ScriptedEffectLoader::queryAndLoadAll()
{
    if (m_queryConnection) {
        return;
    }
    // perform querying for the services in a thread
    QFutureWatcher<QList<KPluginMetaData>> *watcher = new QFutureWatcher<QList<KPluginMetaData>>(this);
    m_queryConnection = connect(
        watcher, &QFutureWatcher<QList<KPluginMetaData>>::finished, this, [this, watcher]() {
            const auto effects = watcher->result();
            for (const auto &effect : effects) {
                const LoadEffectFlags flags = readConfig(effect.pluginId(), effect.isEnabledByDefault());
                if (flags.testFlag(LoadEffectFlag::Load)) {
                    m_queue->enqueue(qMakePair(effect, flags));
                }
            }
            watcher->deleteLater();
            m_queryConnection = QMetaObject::Connection();
        },
        Qt::QueuedConnection);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    watcher->setFuture(QtConcurrent::run(this, &ScriptedEffectLoader::findAllEffects));
#else
    watcher->setFuture(QtConcurrent::run(&ScriptedEffectLoader::findAllEffects, this));
#endif
}

QList<KPluginMetaData> ScriptedEffectLoader::findAllEffects() const
{
    return KPackage::PackageLoader::self()->listPackages(s_serviceType, QStringLiteral("kwin/effects"));
}

KPluginMetaData ScriptedEffectLoader::findEffect(const QString &name) const
{
    const auto plugins = KPackage::PackageLoader::self()->findPackages(s_serviceType, QStringLiteral("kwin/effects"),
                                                                       [name](const KPluginMetaData &metadata) {
                                                                           return metadata.pluginId().compare(name, Qt::CaseInsensitive) == 0;
                                                                       });
    if (!plugins.isEmpty()) {
        return plugins.first();
    }
    return KPluginMetaData();
}

void ScriptedEffectLoader::clear()
{
    disconnect(m_queryConnection);
    m_queryConnection = QMetaObject::Connection();
    m_queue->clear();
}

PluginEffectLoader::PluginEffectLoader(QObject *parent)
    : AbstractEffectLoader(parent)
    , m_pluginSubDirectory(QStringLiteral("kwin/effects/plugins"))
{
}

PluginEffectLoader::~PluginEffectLoader()
{
}

bool PluginEffectLoader::hasEffect(const QString &name) const
{
    const auto info = findEffect(name);
    return info.isValid();
}

KPluginMetaData PluginEffectLoader::findEffect(const QString &name) const
{
    const auto plugins = KPluginMetaData::findPlugins(m_pluginSubDirectory,
                                                      [name](const KPluginMetaData &data) {
                                                          return data.pluginId().compare(name, Qt::CaseInsensitive) == 0;
                                                      });
    if (plugins.isEmpty()) {
        return KPluginMetaData();
    }
    return plugins.first();
}

bool PluginEffectLoader::isEffectSupported(const QString &name) const
{
    if (EffectPluginFactory *effectFactory = factory(findEffect(name))) {
        return effectFactory->isSupported();
    }
    return false;
}

EffectPluginFactory *PluginEffectLoader::factory(const KPluginMetaData &info) const
{
    if (!info.isValid()) {
        return nullptr;
    }
    KPluginFactory *factory;
    if (info.isStaticPlugin()) {
        // in case of static plugins we don't need to worry about the versions, because
        // they are shipped as part of the kwin executables
        factory = KPluginFactory::loadFactory(info).plugin;
    } else {
        QPluginLoader loader(info.fileName());
        if (loader.metaData().value("IID").toString() != EffectPluginFactory_iid) {
            qCDebug(KWIN_CORE) << info.pluginId() << " has not matching plugin version, expected " << PluginFactory_iid << "got "
                               << loader.metaData().value("IID");
            return nullptr;
        }
        factory = qobject_cast<KPluginFactory *>(loader.instance());
    }
    if (!factory) {
        qCDebug(KWIN_CORE) << "Did not get KPluginFactory for " << info.pluginId();
        return nullptr;
    }
    return dynamic_cast<EffectPluginFactory *>(factory);
}

QStringList PluginEffectLoader::listOfKnownEffects() const
{
    const auto plugins = findAllEffects();
    QStringList result;
    for (const auto &plugin : plugins) {
        result << plugin.pluginId();
    }
    qCDebug(KWIN_CORE) << result;
    return result;
}

bool PluginEffectLoader::loadEffect(const QString &name)
{
    const auto info = findEffect(name);
    if (!info.isValid()) {
        return false;
    }
    return loadEffect(info, LoadEffectFlag::Load);
}

bool PluginEffectLoader::loadEffect(const KPluginMetaData &info, LoadEffectFlags flags)
{
    if (!info.isValid()) {
        qCDebug(KWIN_CORE) << "Plugin info is not valid";
        return false;
    }
    const QString name = info.pluginId();
    if (!flags.testFlag(LoadEffectFlag::Load)) {
        qCDebug(KWIN_CORE) << "Loading flags disable effect: " << name;
        return false;
    }
    if (m_loadedEffects.contains(name)) {
        qCDebug(KWIN_CORE) << name << " already loaded";
        return false;
    }
    EffectPluginFactory *effectFactory = factory(info);
    if (!effectFactory) {
        qCDebug(KWIN_CORE) << "Couldn't get an EffectPluginFactory for: " << name;
        return false;
    }

    effects->makeOpenGLContextCurrent();
    if (!effectFactory->isSupported()) {
        qCDebug(KWIN_CORE) << "Effect is not supported: " << name;
        return false;
    }

    if (flags.testFlag(LoadEffectFlag::CheckDefaultFunction)) {
        if (!effectFactory->enabledByDefault()) {
            qCDebug(KWIN_CORE) << "Enabled by default function disables effect: " << name;
            return false;
        }
    }

    // ok, now we can try to create the Effect
    Effect *e = effectFactory->createEffect();
    if (!e) {
        qCDebug(KWIN_CORE) << "Failed to create effect: " << name;
        return false;
    }
    // insert in our loaded effects
    m_loadedEffects << name;
    connect(e, &Effect::destroyed, this, [this, name]() {
        m_loadedEffects.removeAll(name);
    });
    qCDebug(KWIN_CORE) << "Successfully loaded plugin effect: " << name;
    Q_EMIT effectLoaded(e, name);
    return true;
}

void PluginEffectLoader::queryAndLoadAll()
{
    const auto effects = findAllEffects();
    for (const auto &effect : effects) {
        const LoadEffectFlags flags = readConfig(effect.pluginId(), effect.isEnabledByDefault());
        if (flags.testFlag(LoadEffectFlag::Load)) {
            loadEffect(effect, flags);
        }
    }
}

QVector<KPluginMetaData> PluginEffectLoader::findAllEffects() const
{
    return KPluginMetaData::findPlugins(m_pluginSubDirectory);
}

void PluginEffectLoader::setPluginSubDirectory(const QString &directory)
{
    m_pluginSubDirectory = directory;
}

void PluginEffectLoader::clear()
{
}

EffectLoader::EffectLoader(QObject *parent)
    : AbstractEffectLoader(parent)
{
    m_loaders << new ScriptedEffectLoader(this)
              << new PluginEffectLoader(this);
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        connect(*it, &AbstractEffectLoader::effectLoaded, this, &AbstractEffectLoader::effectLoaded);
    }
}

EffectLoader::~EffectLoader()
{
}

bool EffectLoader::hasEffect(const QString &name) const
{
    return std::any_of(m_loaders.cbegin(), m_loaders.cend(), [&name](const auto &loader) {
        return loader->hasEffect(name);
    });
}

bool EffectLoader::isEffectSupported(const QString &name) const
{
    return std::any_of(m_loaders.cbegin(), m_loaders.cend(), [&name](const auto &loader) {
        return loader->isEffectSupported(name);
    });
}

QStringList EffectLoader::listOfKnownEffects() const
{
    QStringList result;
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        result << (*it)->listOfKnownEffects();
    }
    return result;
}

bool EffectLoader::loadEffect(const QString &name)
{
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        if ((*it)->loadEffect(name)) {
            return true;
        }
    }
    return false;
}

void EffectLoader::queryAndLoadAll()
{
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        (*it)->queryAndLoadAll();
    }
}

void EffectLoader::setConfig(KSharedConfig::Ptr config)
{
    AbstractEffectLoader::setConfig(config);
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        (*it)->setConfig(config);
    }
}

void EffectLoader::clear()
{
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        (*it)->clear();
    }
}

} // namespace KWin
