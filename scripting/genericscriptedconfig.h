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

#ifndef KWIN_GENERICSCRIPTEDCONFIG_H
#define KWIN_GENERICSCRIPTEDCONFIG_H

#include <KCModule>
#include <KPluginFactory>
#include <KConfigGroup>

class KLocalizedTranslator;

namespace KWin
{

class GenericScriptedConfigFactory : public KPluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.KPluginFactory"  FILE "genericscriptedconfig.json" )
    Q_INTERFACES(KPluginFactory)

protected:
    QObject *create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args, const QString &keyword) override;
};

class GenericScriptedConfig : public KCModule
{
    Q_OBJECT

public:
    GenericScriptedConfig(const QString &componentName, const QString &keyword, QWidget *parent, const QVariantList &args);
    virtual ~GenericScriptedConfig();

public Q_SLOTS:
    void save() Q_DECL_OVERRIDE;

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
    ScriptedEffectConfig(const QString &componentName, const QString &keyword, QWidget *parent, const QVariantList &args);
    virtual ~ScriptedEffectConfig();
protected:
    QString typeName() const Q_DECL_OVERRIDE;
    KConfigGroup configGroup() Q_DECL_OVERRIDE;
    void reload() Q_DECL_OVERRIDE;
};

class ScriptingConfig : public GenericScriptedConfig
{
    Q_OBJECT
public:
    ScriptingConfig(const QString &componentName, const QString &keyword, QWidget *parent, const QVariantList &args);
    virtual ~ScriptingConfig();

protected:
    QString typeName() const Q_DECL_OVERRIDE;
    KConfigGroup configGroup() Q_DECL_OVERRIDE;
    void reload() Q_DECL_OVERRIDE;
};

inline
const QString &GenericScriptedConfig::packageName() const
{
    return m_packageName;
}

}

#endif // KWIN_GENERICSCRIPTEDCONFIG_H
