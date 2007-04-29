/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_LIB_KWINGLOBALS_H
#define KWIN_LIB_KWINGLOBALS_H

#include <QX11Info>
#include <QPoint>

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

class KWIN_EXPORT WindowPaintData
    {
    public:
        WindowPaintData();
        /**
         * Window opacity, in range 0 = transparent to 1 = fully opaque
         */
        double opacity;
        double xScale;
        double yScale;
        int xTranslate;
        int yTranslate;
        /**
         * Saturation of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         *  unsaturated (greyscale). 0.5 would make the colors less intense,
         *  but not completely grey
         **/
        float saturation;
        /**
         * Brightness of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         * black. 0.5 would make it 50% darker than usual
         **/
        float brightness;
    };

class KWIN_EXPORT ScreenPaintData
    {
    public:
        ScreenPaintData();
        double xScale;
        double yScale;
        int xTranslate;
        int yTranslate;
    };

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

} // namespace

#endif
