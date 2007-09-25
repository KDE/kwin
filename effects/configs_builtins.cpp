/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Bernhard Loos <nhuh.put@web.de>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "shadow_config.h"
#include "presentwindows_config.h"
#include "desktopgrid_config.h"

#include <kwineffects.h>

#include <KPluginLoader>

KWIN_EFFECT_CONFIG_FACTORY
K_PLUGIN_FACTORY_DEFINITION(EffectFactory, registerPlugin<KWin::ShadowEffectConfig>("shadow");
                                registerPlugin<KWin::PresentWindowsEffectConfig>("presentwindows");
                                registerPlugin<KWin::DesktopGridEffectConfig>("desktopgrid");
                           )
K_EXPORT_PLUGIN(EffectFactory("kwin"))

