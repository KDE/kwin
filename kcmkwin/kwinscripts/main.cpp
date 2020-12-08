/*
    SPDX-FileCopyrightText: 2011 Tamas Krutki <ktamasw@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KPluginFactory>

#include "module.h"

K_PLUGIN_FACTORY(KcmKWinScriptsFactory,
                 registerPlugin<Module>("kwin-scripts");)

#include "main.moc"
