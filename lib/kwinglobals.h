/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_LIB_KWINGLOBALS_H
#define KWIN_LIB_KWINGLOBALS_H

#include <QtGui/QX11Info>
#include <QtCore/QPoint>
#include <QtGui/QRegion>

#include <kdemacros.h>

#include <X11/Xlib.h>
#include <fixx11h.h>

namespace KWin
{

#define KWIN_EXPORT KDE_EXPORT


enum CompositingType
{
    NoCompositing = 0,
    OpenGLCompositing,
    XRenderCompositing
};

enum clientAreaOption
{
    PlacementArea,         // geometry where a window will be initially placed after being mapped
    MovementArea,          // ???  window movement snapping area?  ignore struts
    MaximizeArea,          // geometry to which a window will be maximized
    MaximizeFullArea,      // like MaximizeArea, but ignore struts - used e.g. for topmenu
    FullScreenArea,        // area for fullscreen windows
    // these below don't depend on xinerama settings
    WorkArea,              // whole workarea (all screens together)
    FullArea,              // whole area (all screens together), ignore struts
    ScreenArea             // one whole screen, ignore struts
};

enum ElectricBorder
{
    ElectricTop,
    ElectricTopRight,
    ElectricRight,
    ElectricBottomRight,
    ElectricBottom,
    ElectricBottomLeft,
    ElectricLeft,
    ElectricTopLeft,
    ELECTRIC_COUNT,
    ElectricNone
};

// DesktopMode and WindowsMode are based on the order in which the desktop
//  or window were viewed.
// DesktopListMode lists them in the order created.
enum TabBoxMode { TabBoxDesktopMode, TabBoxDesktopListMode, TabBoxWindowsMode };

inline
KWIN_EXPORT Display* display()
    {
    return QX11Info::display();
    }

inline
KWIN_EXPORT Window rootWindow()
    {
    return QX11Info::appRootWindow();
    }

inline
KWIN_EXPORT Window xTime()
    {
    return QX11Info::appTime();
    }

inline
KWIN_EXPORT int displayWidth()
    {
    return XDisplayWidth( display(), DefaultScreen( display()));
    }

inline
KWIN_EXPORT int displayHeight()
    {
    return XDisplayHeight( display(), DefaultScreen( display()));
    }

class KWIN_EXPORT Extensions
    {
    public:
        static void init();
        static bool shapeAvailable() { return shape_version > 0; }
        static bool shapeInputAvailable();
        static int shapeNotifyEvent();
        static bool hasShape( Window w );
        static bool randrAvailable() { return has_randr; }
        static int randrNotifyEvent();
        static bool damageAvailable() { return has_damage; }
        static int damageNotifyEvent();
        static bool compositeAvailable() { return composite_version > 0; }
        static bool compositeOverlayAvailable();
        static bool renderAvailable() { return render_version > 0; }
        static bool fixesAvailable() { return fixes_version > 0; }
        static bool fixesRegionAvailable();
        static bool glxAvailable() { return has_glx; }
        static bool syncAvailable() { return has_sync; }
        static int syncAlarmNotifyEvent();
    private:
        static int shape_version;
        static int shape_event_base;
        static bool has_randr;
        static int randr_event_base;
        static bool has_damage;
        static int damage_event_base;
        static int composite_version;
        static int render_version;
        static int fixes_version;
        static bool has_glx;
        static bool has_sync;
        static int sync_event_base;
    };

} // namespace

#endif
