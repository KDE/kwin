/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scripting/scriptedquicksceneeffect.h"
#include "main.h"

#include <KConfigGroup>
#include <KConfigLoader>

#include <QFile>

namespace KWin
{

ScriptedQuickSceneEffect::ScriptedQuickSceneEffect()
{
    m_visibleTimer.setSingleShot(true);
    connect(&m_visibleTimer, &QTimer::timeout, this, [this]() {
        setRunning(false);
    });
}

ScriptedQuickSceneEffect::~ScriptedQuickSceneEffect()
{
}

void ScriptedQuickSceneEffect::reconfigure(ReconfigureFlags flags)
{
    m_configLoader->load();
    Q_EMIT m_configLoader->configChanged();
}

int ScriptedQuickSceneEffect::requestedEffectChainPosition() const
{
    return m_requestedEffectChainPosition;
}

void ScriptedQuickSceneEffect::setMetaData(const KPluginMetaData &metaData)
{
    m_requestedEffectChainPosition = metaData.value(QStringLiteral("X-KDE-Ordering"), 50);

    KConfigGroup cg = kwinApp()->config()->group(QStringLiteral("Effect-%1").arg(metaData.pluginId()));
    QString configFilePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + QLatin1String("/effects/") + metaData.pluginId() + QLatin1String("/contents/config/main.xml"));
    if (configFilePath.isNull()) {
        configFilePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kwin/effects/") + metaData.pluginId() + QLatin1String("/contents/config/main.xml"));
    }
    if (configFilePath.isNull()) {
        m_configLoader = new KConfigLoader(cg, nullptr, this);
    } else {
        QFile xmlFile(configFilePath);
        m_configLoader = new KConfigLoader(cg, &xmlFile, this);
        m_configLoader->load();
    }

    m_configuration = new KConfigPropertyMap(m_configLoader, this);
    connect(m_configLoader, &KConfigLoader::configChanged, this, &ScriptedQuickSceneEffect::configurationChanged);
}

bool ScriptedQuickSceneEffect::isVisible() const
{
    return m_isVisible;
}

void ScriptedQuickSceneEffect::setVisible(bool visible)
{
    if (m_isVisible == visible) {
        return;
    }
    m_isVisible = visible;

    if (m_isVisible) {
        m_visibleTimer.stop();
        setRunning(true);
    } else {
        // Delay setRunning(false) to avoid destroying views while still executing JS code.
        m_visibleTimer.start();
    }

    Q_EMIT visibleChanged();
}

KConfigPropertyMap *ScriptedQuickSceneEffect::configuration() const
{
    return m_configuration;
}

QQmlListProperty<QObject> ScriptedQuickSceneEffect::data()
{
    return QQmlListProperty<QObject>(this, nullptr,
                                     data_append,
                                     data_count,
                                     data_at,
                                     data_clear);
}

void ScriptedQuickSceneEffect::data_append(QQmlListProperty<QObject> *objects, QObject *object)
{
    if (!object) {
        return;
    }

    ScriptedQuickSceneEffect *effect = static_cast<ScriptedQuickSceneEffect *>(objects->object);
    if (!effect->m_children.contains(object)) {
        object->setParent(effect);
        effect->m_children.append(object);
    }
}

qsizetype ScriptedQuickSceneEffect::data_count(QQmlListProperty<QObject> *objects)
{
    ScriptedQuickSceneEffect *effect = static_cast<ScriptedQuickSceneEffect *>(objects->object);
    return effect->m_children.count();
}

QObject *ScriptedQuickSceneEffect::data_at(QQmlListProperty<QObject> *objects, qsizetype index)
{
    ScriptedQuickSceneEffect *effect = static_cast<ScriptedQuickSceneEffect *>(objects->object);
    return effect->m_children.value(index);
}

void ScriptedQuickSceneEffect::data_clear(QQmlListProperty<QObject> *objects)
{
    ScriptedQuickSceneEffect *effect = static_cast<ScriptedQuickSceneEffect *>(objects->object);
    while (!effect->m_children.isEmpty()) {
        QObject *child = effect->m_children.takeLast();
        child->setParent(nullptr);
    }
}

} // namespace KWin

#include "moc_scriptedquicksceneeffect.cpp"
