/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>
#include <KConfigGroup>
#include <KPluginFactory>

class KLocalizedTranslator;

namespace KWin
{

class GenericScriptedConfigFactory : public KPluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.KPluginFactory" FILE "genericscriptedconfig.json")
    Q_INTERFACES(KPluginFactory)

protected:
    QObject *create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args, const QString &keyword) override;
};

class GenericScriptedConfig : public KCModule
{
    Q_OBJECT

public:
    GenericScriptedConfig(const QString &keyword, QWidget *parent, const QVariantList &args);
    ~GenericScriptedConfig() override;

public Q_SLOTS:
    void save() override;

protected:
    const QString &packageName() const;
    void createUi();
    virtual QString typeName() const = 0;
    virtual KConfigGroup configGroup() = 0;
    virtual void reload();

private:
    QString m_packageName;
    KLocalizedTranslator *m_translator;
};

class ScriptedEffectConfig : public GenericScriptedConfig
{
    Q_OBJECT
public:
    ScriptedEffectConfig(const QString &keyword, QWidget *parent, const QVariantList &args);
    ~ScriptedEffectConfig() override;

protected:
    QString typeName() const override;
    KConfigGroup configGroup() override;
    void reload() override;
};

class ScriptingConfig : public GenericScriptedConfig
{
    Q_OBJECT
public:
    ScriptingConfig(const QString &keyword, QWidget *parent, const QVariantList &args);
    ~ScriptingConfig() override;

protected:
    QString typeName() const override;
    KConfigGroup configGroup() override;
    void reload() override;
};

inline const QString &GenericScriptedConfig::packageName() const
{
    return m_packageName;
}

}
