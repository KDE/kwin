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

namespace KWin
{

class GenericScriptedConfigFactory : public KPluginFactory
{
    Q_OBJECT
public:
    GenericScriptedConfigFactory();

protected:
    virtual QObject *create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args, const QString &keyword);
};

class GenericScriptedConfig : public KCModule
{
    Q_OBJECT

public:
    GenericScriptedConfig(const KComponentData &data, const QString &keyword, QWidget *parent, const QVariantList &args);
    virtual ~GenericScriptedConfig();

public slots:
    virtual void save();

protected:
    const QString &packageName() const;
    void createUi();
    virtual QString typeName() const = 0;
    virtual KConfigGroup configGroup() = 0;
    virtual void reload();

private:
    QString m_packageName;
};

class ScriptedEffectConfig : public GenericScriptedConfig
{
    Q_OBJECT
public:
    ScriptedEffectConfig(const KComponentData &data, const QString &keyword, QWidget *parent, const QVariantList &args);
    virtual ~ScriptedEffectConfig();
protected:
    virtual QString typeName() const;
    virtual KConfigGroup configGroup();
    virtual void reload();
};

class ScriptingConfig : public GenericScriptedConfig
{
    Q_OBJECT
public:
    ScriptingConfig(const KComponentData &data, const QString &keyword, QWidget *parent, const QVariantList &args);
    virtual ~ScriptingConfig();

protected:
    virtual QString typeName() const;
    virtual KConfigGroup configGroup();
    virtual void reload();
};

inline
const QString &GenericScriptedConfig::packageName() const
{
    return m_packageName;
}

}

#endif // KWIN_GENERICSCRIPTEDCONFIG_H
