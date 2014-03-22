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
#include <kwineffects.h>
#include "effects/effect_builtins.h"
// KDE
#include <KConfigGroup>
// Qt
#include <QDebug>
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

} // namespace KWin
