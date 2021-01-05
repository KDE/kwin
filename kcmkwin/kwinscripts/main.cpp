/*
    SPDX-FileCopyrightText: 2011 Tamas Krutki <ktamasw@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KPluginFactory>

#include "module.h"
#include "kwinscriptsdata.h"

K_PLUGIN_FACTORY(KcmKWinScriptsFactory,
                 registerPlugin<Module>();
                 registerPlugin<KWinScriptsData>();
                )

#include "main.moc"
