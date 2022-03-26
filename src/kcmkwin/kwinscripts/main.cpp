/*
    SPDX-FileCopyrightText: 2011 Tamas Krutki <ktamasw@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KPluginFactory>

#include "kwinscriptsdata.h"
#include "module.h"

K_PLUGIN_FACTORY(KcmKWinScriptsFactory,
                 registerPlugin<Module>();
                 registerPlugin<KWinScriptsData>();)

#include "main.moc"
