/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Bernhard Loos <nhuh.put@web.de>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include <kwinconfig.h>

#include "boxswitch/boxswitch_config.h"
#include "dashboard/dashboard_config.h"
#include "desktopgrid/desktopgrid_config.h"
#include "diminactive/diminactive_config.h"
#include "magiclamp/magiclamp_config.h"
#include "translucency/translucency_config.h"
#include "presentwindows/presentwindows_config.h"
#include "resize/resize_config.h"
#include "showfps/showfps_config.h"
#include "thumbnailaside/thumbnailaside_config.h"
#include "windowgeometry/windowgeometry_config.h"
#include "zoom/zoom_config.h"

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include "blur/blur_config.h"
#include "coverswitch/coverswitch_config.h"
#include "cube/cube_config.h"
#include "cube/cubeslide_config.h"
#include "flipswitch/flipswitch_config.h"
#include "glide/glide_config.h"
#include "invert/invert_config.h"
#include "lookingglass/lookingglass_config.h"
#if 0
// Magnifier currently broken due to removed PaintClipper
#include "magnifier/magnifier_config.h"
#endif
#include "mousemark/mousemark_config.h"
#include "trackmouse/trackmouse_config.h"
#include "wobblywindows/wobblywindows_config.h"
#endif

#include <kwineffects.h>

#include <KPluginLoader>

namespace KWin
{

KWIN_EFFECT_CONFIG_MULTIPLE(builtins,
                            KWIN_EFFECT_CONFIG_SINGLE(boxswitch, BoxSwitchEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(dashboard, DashboardEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(desktopgrid, DesktopGridEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(diminactive, DimInactiveEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(magiclamp, MagicLampEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(presentwindows, PresentWindowsEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(resize, ResizeEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(showfps, ShowFpsEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(translucency, TranslucencyEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(thumbnailaside, ThumbnailAsideEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(windowgeometry, WindowGeometryConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(zoom, ZoomEffectConfig)

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
                            KWIN_EFFECT_CONFIG_SINGLE(blur, BlurEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(coverswitch, CoverSwitchEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(cube, CubeEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(cubeslide, CubeSlideEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(flipswitch, FlipSwitchEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(glide, GlideEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(invert, InvertEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(lookingglass, LookingGlassEffectConfig)
#if 0
                            KWIN_EFFECT_CONFIG_SINGLE(magnifier, MagnifierEffectConfig)
#endif
                            KWIN_EFFECT_CONFIG_SINGLE(mousemark, MouseMarkEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(trackmouse, TrackMouseEffectConfig)
                            KWIN_EFFECT_CONFIG_SINGLE(wobblywindows, WobblyWindowsEffectConfig)
#endif
                           )

} // namespace
