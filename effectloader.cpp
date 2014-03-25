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
// own
#include "effectloader.h"
// KWin
#include <config-kwin.h>
#include <kwineffects.h>
#include "effects/effect_builtins.h"
#include "scripting/scriptedeffect.h"
// KDE
#include <KConfigGroup>
#include <KPluginTrader>
#include <KServiceTypeTrader>
// Qt
#include <QtConcurrentRun>
#include <QDebug>
#include <QFutureWatcher>
#include <QMap>
#include <QStringList>

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

BuiltInEffectLoader::BuiltInEffectLoader(QObject *parent)
    : AbstractEffectLoader(parent)
    , m_queue(new EffectLoadQueue<BuiltInEffectLoader, BuiltInEffect>(this))
{
}

BuiltInEffectLoader::~BuiltInEffectLoader()
{
}

bool BuiltInEffectLoader::hasEffect(const QString &name) const
{
    return BuiltInEffects::available(internalName(name));
}

bool BuiltInEffectLoader::isEffectSupported(const QString &name) const
{
    return BuiltInEffects::supported(internalName(name));
}

QStringList BuiltInEffectLoader::listOfKnownEffects() const
{
    const QList<QByteArray> availableEffects = BuiltInEffects::availableEffectNames();
    QStringList result;
    for (const QByteArray name : availableEffects) {
        result << QString::fromUtf8(name);
    }
    return result;
}

bool BuiltInEffectLoader::loadEffect(const QString &name)
{
    return loadEffect(name, BuiltInEffects::builtInForName(internalName(name)), LoadEffectFlag::Load);
}

void BuiltInEffectLoader::queryAndLoadAll()
{
    const QList<BuiltInEffect> effects = BuiltInEffects::availableEffects();
    for (BuiltInEffect effect : effects) {
        // check whether it is already loaded
        if (m_loadedEffects.contains(effect)) {
            continue;
        }
        // as long as the KCM uses kwin4_effect_ we need to add it, TODO remove
        const QString key = QStringLiteral("kwin4_effect_") +
                            QString::fromUtf8(BuiltInEffects::nameForEffect(effect));
        const LoadEffectFlags flags = readConfig(key, BuiltInEffects::enabledByDefault(effect));
        if (flags.testFlag(LoadEffectFlag::Load)) {
            m_queue->enqueue(qMakePair(effect, flags));
        }
    }
}

bool BuiltInEffectLoader::loadEffect(BuiltInEffect effect, LoadEffectFlags flags)
{
    return loadEffect(QString::fromUtf8(BuiltInEffects::nameForEffect(effect)), effect, flags);
}

bool BuiltInEffectLoader::loadEffect(const QString &name, BuiltInEffect effect, LoadEffectFlags flags)
{
    if (effect == BuiltInEffect::Invalid) {
        return false;
    }
    if (!flags.testFlag(LoadEffectFlag::Load)) {
        qDebug() << "Loading flags disable effect: " << name;
        return false;
    }
    // check that it is not already loaded
    if (m_loadedEffects.contains(effect)) {
        return false;
    }

    // supported might need a context
#ifndef KWIN_UNIT_TEST
    effects->makeOpenGLContextCurrent();
#endif
    if (!BuiltInEffects::supported(effect)) {
        qDebug() << "Effect is not supported: " << name;
        return false;
    }

    if (flags.testFlag(LoadEffectFlag::CheckDefaultFunction)) {
        if (!BuiltInEffects::checkEnabledByDefault(effect)) {
            qDebug() << "Enabled by default function disables effect: " << name;
            return false;
        }
    }

    // ok, now we can try to create the Effect
    Effect *e = BuiltInEffects::create(effect);
    if (!e) {
        qDebug() << "Failed to create effect: " << name;
        return false;
    }
    // insert in our loaded effects
    m_loadedEffects.insert(effect, e);
    connect(e, &Effect::destroyed, this,
        [this, effect]() {
            m_loadedEffects.remove(effect);
        }
    );
    qDebug() << "Successfully loaded built-in effect: " << name;
    emit effectLoaded(e, name);
    return true;
}

QByteArray BuiltInEffectLoader::internalName(const QString& name) const
{
    QString internalName = name.toLower();
    // as long as the KCM uses kwin4_effect_ we need to add it, TODO remove
    if (internalName.startsWith(QStringLiteral("kwin4_effect_"))) {
        internalName = internalName.mid(13);
    }
    return internalName.toUtf8();
}

static const QString s_nameProperty = QStringLiteral("X-KDE-PluginInfo-Name");
static const QString s_jsConstraint = QStringLiteral("[X-Plasma-API] == 'javascript'");
static const QString s_serviceType = QStringLiteral("KWin/Effect");

ScriptedEffectLoader::ScriptedEffectLoader(QObject *parent)
    : AbstractEffectLoader(parent)
    , m_queue(new EffectLoadQueue<ScriptedEffectLoader, KService::Ptr>(this))
{
}

ScriptedEffectLoader::~ScriptedEffectLoader()
{
}

bool ScriptedEffectLoader::hasEffect(const QString &name) const
{
    return findEffect(name);
}

bool ScriptedEffectLoader::isEffectSupported(const QString &name) const
{
    // scripted effects are in general supported
    return hasEffect(name);
}

QStringList ScriptedEffectLoader::listOfKnownEffects() const
{
    const KService::List effects = findAllEffects();
    QStringList result;
    for (KService::Ptr service : effects) {
        result << service->property(s_nameProperty).toString();
    }
    return result;
}

bool ScriptedEffectLoader::loadEffect(const QString &name)
{
    KService::Ptr effect = findEffect(name);
    if (!effect) {
        return false;
    }
    return loadEffect(effect, LoadEffectFlag::Load);
}

bool ScriptedEffectLoader::loadEffect(KService::Ptr effect, LoadEffectFlags flags)
{
    const QString name = effect->property(s_nameProperty).toString();
    if (!flags.testFlag(LoadEffectFlag::Load)) {
        qDebug() << "Loading flags disable effect: " << name;
        return false;
    }
    if (m_loadedEffects.contains(name)) {
        qDebug() << name << "already loaded";
        return false;
    }

    ScriptedEffect *e = ScriptedEffect::create(effect);
    if (!e) {
        qDebug() << "Could not initialize scripted effect: " << name;
        return false;
    }
    connect(e, &ScriptedEffect::destroyed, this,
        [this, name]() {
            m_loadedEffects.removeAll(name);
        }
    );

    qDebug() << "Successfully loaded scripted effect: " << name;
    emit effectLoaded(e, name);
    m_loadedEffects << name;
    return true;
}

void ScriptedEffectLoader::queryAndLoadAll()
{
    // perform querying for the services in a thread
    QFutureWatcher<KService::List> *watcher = new QFutureWatcher<KService::List>(this);
    connect(watcher, &QFutureWatcher<KService::List>::finished, this,
        [this, watcher]() {
            const KService::List effects = watcher->result();
            for (KService::Ptr effect : effects) {
                const LoadEffectFlags flags = readConfig(effect->property(s_nameProperty).toString(),
                                                        effect->property(QStringLiteral("X-KDE-PluginInfo-EnabledByDefault")).toBool());
                if (flags.testFlag(LoadEffectFlag::Load)) {
                    m_queue->enqueue(qMakePair(effect, flags));
                }
            }
            watcher->deleteLater();
        },
        Qt::QueuedConnection);
    watcher->setFuture(QtConcurrent::run(this, &ScriptedEffectLoader::findAllEffects));
}

KService::List ScriptedEffectLoader::findAllEffects() const
{
    return KServiceTypeTrader::self()->query(s_serviceType, s_jsConstraint);
}

KService::Ptr ScriptedEffectLoader::findEffect(const QString &name) const
{
    const QString constraint = QStringLiteral("%1 and [%2] == '%3'").arg(s_jsConstraint).arg(s_nameProperty).arg(name.toLower());
    const KService::List services = KServiceTypeTrader::self()->query(s_serviceType,
                                                                      constraint);
    if (!services.isEmpty()) {
        return services.first();
    }
    return KService::Ptr();
}


PluginEffectLoader::PluginEffectLoader(QObject *parent)
    : AbstractEffectLoader(parent)
    , m_queue(new EffectLoadQueue< PluginEffectLoader, KPluginInfo>(this))
    , m_pluginSubDirectory(QStringLiteral("kwin/effects/plugins/"))
{
}

PluginEffectLoader::~PluginEffectLoader()
{
}

bool PluginEffectLoader::hasEffect(const QString &name) const
{
    KPluginInfo info = findEffect(name);
    return info.isValid();
}

KPluginInfo PluginEffectLoader::findEffect(const QString &name) const
{
    const QString constraint = QStringLiteral("[%1] == '%2'").arg(s_nameProperty).arg(name.toLower());
    KPluginInfo::List plugins = KPluginTrader::self()->query(m_pluginSubDirectory, s_serviceType, constraint);
    if (plugins.isEmpty()) {
        return KPluginInfo();
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

EffectPluginFactory *PluginEffectLoader::factory(const KPluginInfo &info) const
{
    if (!info.isValid()) {
        return nullptr;
    }
    KPluginLoader loader(info.libraryPath());
    if (loader.pluginVersion() != KWIN_EFFECT_API_VERSION) {
        qDebug() << info.pluginName() << " has not matching plugin version, expected " << KWIN_EFFECT_API_VERSION << "got " << loader.pluginVersion();
        return nullptr;
    }
    KPluginFactory *factory = loader.factory();
    if (!factory) {
        qDebug() << "Did not get KPluginFactory for " << info.pluginName();
        return nullptr;
    }
    return dynamic_cast< EffectPluginFactory* >(factory);
}

QStringList PluginEffectLoader::listOfKnownEffects() const
{
    const KPluginInfo::List plugins = findAllEffects();
    QStringList result;
    for (const KPluginInfo &plugin : plugins) {
        result << plugin.pluginName();
    }
    return result;
}

bool PluginEffectLoader::loadEffect(const QString &name)
{
    KPluginInfo info = findEffect(name);
    if (!info.isValid()) {
        return false;
    }
    return loadEffect(info, LoadEffectFlag::Load);
}

bool PluginEffectLoader::loadEffect(const KPluginInfo &info, LoadEffectFlags flags)
{
    if (!info.isValid()) {
        qDebug() << "Plugin info is not valid";
        return false;
    }
    const QString name = info.pluginName();
    if (!flags.testFlag(LoadEffectFlag::Load)) {
        qDebug() << "Loading flags disable effect: " << name;
        return false;
    }
    if (m_loadedEffects.contains(name)) {
        qDebug() << name << " already loaded";
        return false;
    }
    EffectPluginFactory *effectFactory = factory(info);
    if (!effectFactory) {
        qDebug() << "Couldn't get an EffectPluginFactory for: " << name;
        return false;
    }

#ifndef KWIN_UNIT_TEST
    effects->makeOpenGLContextCurrent();
#endif
    if (!effectFactory->isSupported()) {
        qDebug() << "Effect is not supported: " << name;
        return false;
    }

    if (flags.testFlag(LoadEffectFlag::CheckDefaultFunction)) {
        if (!effectFactory->enabledByDefault()) {
            qDebug() << "Enabled by default function disables effect: " << name;
            return false;
        }
    }

    // ok, now we can try to create the Effect
    Effect *e = effectFactory->createEffect();
    if (!e) {
        qDebug() << "Failed to create effect: " << name;
        return false;
    }
    // insert in our loaded effects
    m_loadedEffects << name;
    connect(e, &Effect::destroyed, this,
        [this, name]() {
            m_loadedEffects.removeAll(name);
        }
    );
    qDebug() << "Successfully loaded plugin effect: " << name;
    emit effectLoaded(e, name);
    return true;
}

void PluginEffectLoader::queryAndLoadAll()
{
    // perform querying for the services in a thread
    QFutureWatcher<KPluginInfo::List> *watcher = new QFutureWatcher<KPluginInfo::List>(this);
    connect(watcher, &QFutureWatcher<KPluginInfo::List>::finished, this,
        [this, watcher]() {
            const KPluginInfo::List effects = watcher->result();
            for (const KPluginInfo &effect : effects) {
                const LoadEffectFlags flags = readConfig(effect.pluginName(), effect.isPluginEnabledByDefault());
                if (flags.testFlag(LoadEffectFlag::Load)) {
                    m_queue->enqueue(qMakePair(effect, flags));
                }
            }
            watcher->deleteLater();
        },
        Qt::QueuedConnection);
    watcher->setFuture(QtConcurrent::run(this, &PluginEffectLoader::findAllEffects));
}

KPluginInfo::List PluginEffectLoader::findAllEffects() const
{
    return KPluginTrader::self()->query(m_pluginSubDirectory, s_serviceType);
}

void PluginEffectLoader::setPluginSubDirectory(const QString &directory)
{
    m_pluginSubDirectory = directory;
}

EffectLoader::EffectLoader(QObject *parent)
    : AbstractEffectLoader(parent)
{
    m_loaders << new BuiltInEffectLoader(this)
              << new ScriptedEffectLoader(this)
              << new PluginEffectLoader(this);
    for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) {
        connect(*it, &AbstractEffectLoader::effectLoaded, this, &AbstractEffectLoader::effectLoaded);
    }
}

EffectLoader::~EffectLoader()
{
}

#define BOOL_MERGE( method ) \
    bool EffectLoader::method(const QString &name) const \
    { \
        for (auto it = m_loaders.constBegin(); it != m_loaders.constEnd(); ++it) { \
            if ((*it)->method(name)) { \
                return true; \
            } \
        } \
        return false; \
    }

BOOL_MERGE(hasEffect)
BOOL_MERGE(isEffectSupported)

#undef BOOL_MERGE

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

} // namespace KWin
