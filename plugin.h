/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <config-kwin.h>

#include "kwinglobals.h"

#include <QObject>

namespace KWin
{

#define KWIN_PLUGIN_API_VERSION QT_VERSION_CHECK(KWIN_VERSION_MAJOR, \
                                                 KWIN_VERSION_MINOR, \
                                                 KWIN_VERSION_PATCH)

#define PluginFactory_iid "org.kde.kwin.PluginFactoryInterface"

/**
 * The Plugin class is the baseclass for all binary compositor extensions.
 *
 * Note that a binary extension must be recompiled with every new KWin release.
 */
class KWIN_EXPORT Plugin : public QObject
{
    Q_OBJECT

public:
    explicit Plugin(QObject *parent = nullptr);
};

/**
 * The PluginFactory class creates binary compositor extensions.
 */
class KWIN_EXPORT PluginFactory : public QObject
{
    Q_OBJECT

public:
    explicit PluginFactory(QObject *parent = nullptr);

    virtual Plugin *create() const = 0;
};

} // namespace KWin

Q_DECLARE_INTERFACE(KWin::PluginFactory, PluginFactory_iid)
