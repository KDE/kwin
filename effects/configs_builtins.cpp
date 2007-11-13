/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Bernhard Loos <nhuh.put@web.de>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include <config-X11.h>

#include "shadow_config.h"
#include "presentwindows_config.h"
#include "desktopgrid_config.h"
#include "maketransparent_config.h"
#include "diminactive_config.h"
#include "thumbnailaside_config.h"
#include "zoom_config.h"

#ifdef HAVE_OPENGL
#include "invert_config.h"
#include "lookingglass_config.h"
#include "mousemark_config.h"
#include "magnifier_config.h"
#include "sharpen_config.h"
#include "trackmouse_config.h"
#endif

#include <kwineffects.h>

#include <KPluginLoader>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif
K_PLUGIN_FACTORY_DEFINITION(EffectFactory,
    registerPlugin<KWin::DesktopGridEffectConfig>("desktopgrid");
    registerPlugin<KWin::DimInactiveEffectConfig>("diminactive");
    registerPlugin<KWin::MakeTransparentEffectConfig>("maketransparent");
    registerPlugin<KWin::PresentWindowsEffectConfig>("presentwindows");
    registerPlugin<KWin::ShadowEffectConfig>("shadow");
    registerPlugin<KWin::ThumbnailAsideEffectConfig>("thumbnailaside");
    registerPlugin<KWin::ZoomEffectConfig>("zoom");
#ifdef HAVE_OPENGL
    registerPlugin<KWin::InvertEffectConfig>("invert");
    registerPlugin<KWin::LookingGlassEffectConfig>("lookingglass");
    registerPlugin<KWin::MouseMarkEffectConfig>("mousemark");
    registerPlugin<KWin::MagnifierEffectConfig>("magnifier");
    registerPlugin<KWin::SharpenEffectConfig>("sharpen");
    registerPlugin<KWin::TrackMouseEffectConfig>("trackmouse");
#endif
    )
K_EXPORT_PLUGIN(EffectFactory("kwin"))
