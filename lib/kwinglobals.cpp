/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "kwinglobals.h"

#include <config-workspace.h>
#include <config-X11.h>

#include <kdebug.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>
#endif
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#endif
#ifdef HAVE_OPENGL
#include <GL/glx.h>
#endif
#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

namespace KWin
{

int Extensions::shape_version = 0;
int Extensions::shape_event_base = 0;
bool Extensions::has_randr = false;
int Extensions::randr_event_base = 0;
bool Extensions::has_damage = false;
int Extensions::damage_event_base = 0;
int Extensions::composite_version = 0;
int Extensions::fixes_version = 0;
int Extensions::render_version = 0;
bool Extensions::has_glx = false;
bool Extensions::has_sync = false;
int Extensions::sync_event_base = 0;

void Extensions::init()
    {
    int dummy;
    shape_version = 0;
    if( XShapeQueryExtension( display(), &shape_event_base, &dummy ))
        {
        int major, minor;
        if( XShapeQueryVersion( display(), &major, &minor ))
            shape_version = major * 0x10 + minor;
        }
#ifdef HAVE_XRANDR
    has_randr = XRRQueryExtension( display(), &randr_event_base, &dummy );
    if( has_randr )
        {
        int major, minor;
        XRRQueryVersion( display(), &major, &minor );
        has_randr = ( major > 1 || ( major == 1 && minor >= 1 ) );
        }
#else
    has_randr = false;
#endif
#ifdef HAVE_XDAMAGE
    has_damage = XDamageQueryExtension( display(), &damage_event_base, &dummy );
#else
    has_damage = false;
#endif
    composite_version = 0;
#ifdef HAVE_XCOMPOSITE
    if( XCompositeQueryExtension( display(), &dummy, &dummy ))
        {
        int major = 0, minor = 0;
        XCompositeQueryVersion( display(), &major, &minor );
        composite_version = major * 0x10 + minor;
        }
#endif
    fixes_version = 0;
#ifdef HAVE_XFIXES
    if( XFixesQueryExtension( display(), &dummy, &dummy ))
        {
        int major = 0, minor = 0;
        XFixesQueryVersion( display(), &major, &minor );
        fixes_version = major * 0x10 + minor;
        }
#endif
    render_version = 0;
#ifdef HAVE_XRENDER
    if( XRenderQueryExtension( display(), &dummy, &dummy ))
        {
        int major = 0, minor = 0;
        XRenderQueryVersion( display(), &major, &minor );
        render_version = major * 0x10 + minor;
        }
#endif
    has_glx = false;
#ifdef HAVE_OPENGL
    has_glx = glXQueryExtension( display(), &dummy, &dummy );
#endif
#ifdef HAVE_XSYNC
    if( XSyncQueryExtension( display(), &sync_event_base, &dummy ))
        {
        int major = 0, minor = 0;
        if( XSyncInitialize( display(), &major, &minor ))
            has_sync = true;
        }
#endif
    kDebug( 1212 ) << "Extensions: shape: 0x" << QString::number( shape_version, 16 )
        << " composite: 0x" << QString::number( composite_version, 16 )
        << " render: 0x" << QString::number( render_version, 16 )
        << " fixes: 0x" << QString::number( fixes_version, 16 ) << endl;
    }

int Extensions::shapeNotifyEvent()
    {
    return shape_event_base + ShapeNotify;
    }

// does the window w need a shape combine mask around it?
bool Extensions::hasShape( Window w )
    {
    int xws, yws, xbs, ybs;
    unsigned int wws, hws, wbs, hbs;
    int boundingShaped = 0, clipShaped = 0;
    if( !shapeAvailable())
        return false;
    XShapeQueryExtents(display(), w,
                       &boundingShaped, &xws, &yws, &wws, &hws,
                       &clipShaped, &xbs, &ybs, &wbs, &hbs);
    return boundingShaped != 0;
    }

bool Extensions::shapeInputAvailable()
    {
    return shape_version >= 0x11; // 1.1
    }

int Extensions::randrNotifyEvent()
    {
#ifdef HAVE_XRANDR
    return randr_event_base + RRScreenChangeNotify;
#else
    return 0;
#endif
    }

int Extensions::damageNotifyEvent()
    {
#ifdef HAVE_XDAMAGE
    return damage_event_base + XDamageNotify;
#else
    return 0;
#endif
    }

bool Extensions::compositeOverlayAvailable()
    {
    return composite_version >= 0x03; // 0.3
    }

bool Extensions::fixesRegionAvailable()
    {
    return fixes_version >= 0x30; // 3
    }

int Extensions::syncAlarmNotifyEvent()
    {
#ifdef HAVE_XSYNC
    return sync_event_base + XSyncAlarmNotify;
#else
    return 0;
#endif
    }

} // namespace

