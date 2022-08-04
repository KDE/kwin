/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <config-kwin.h>
#include <kwin_export.h>

#include <QObject>
#include <memory>

namespace KWin
{

#define PluginFactory_iid "org.kde.kwin.PluginFactoryInterface" KWIN_PLUGIN_VERSION_STRING

/**
 * The Plugin class is the baseclass for all binary compositor extensions.
 *
 * Note that a binary extension must be recompiled with every new KWin release.
 */
class KWIN_EXPORT Plugin : public QObject
{
    Q_OBJECT

public:
    explicit Plugin();
};

/**
 * The PluginFactory class creates binary compositor extensions.
 */
class KWIN_EXPORT PluginFactory : public QObject
{
    Q_OBJECT

public:
    explicit PluginFactory();

    virtual std::unique_ptr<Plugin> create() const = 0;
};

} // namespace KWin

Q_DECLARE_INTERFACE(KWin::PluginFactory, PluginFactory_iid)
