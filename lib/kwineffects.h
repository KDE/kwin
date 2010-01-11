/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
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

#ifndef KWINEFFECTS_H
#define KWINEFFECTS_H

#include <kwinconfig.h>
#include <kwinglobals.h>
#include "kdecoration.h"

#include <QtCore/QPair>
#include <QtCore/QRect>
#include <QtGui/QRegion>
#include <QtGui/QFont>

#include <QtCore/QVector>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QStack>
#include <QtCore/QTimeLine>

#include <KDE/KPluginFactory>
#include <KDE/KShortcutsEditor>

#include <KDE/Plasma/FrameSvg>
#include <KDE/Plasma/Theme>

#include <assert.h>
#include <limits.h>

class KLibrary;
class KConfigGroup;
class KActionCollection;
class QKeyEvent;

namespace KWin
{


class EffectWindow;
class EffectWindowGroup;
class Effect;
class WindowQuad;
class GLRenderTarget;
class GLShader;
class GLTexture;
class XRenderPicture;
class RotationData;
class WindowQuadList;
class WindowPrePaintData;
class WindowPaintData;
class ScreenPrePaintData;
class ScreenPaintData;

typedef QPair< QString, Effect* > EffectPair;
typedef QPair< Effect*, Window > InputWindowPair;
typedef QList< EffectWindow* > EffectWindowList;


/** @defgroup kwineffects KWin effects library
 * KWin effects library contains necessary classes for creating new KWin
 * compositing effects.
 *
 * @section creating Creating new effects
 * This example will demonstrate the basics of creating an effect. We'll use
 *  CoolEffect as the class name, cooleffect as internal name and
 *  "Cool Effect" as user-visible name of the effect.
 *
 * This example doesn't demonstrate how to write the effect's code. For that,
 *  see the documentation of the Effect class.
 *
 * @subsection creating-class CoolEffect class
 * First you need to create CoolEffect class which has to be a subclass of
 *  @ref KWin::Effect. In that class you can reimplement various virtual
 *  methods to control how and where the windows are drawn.
 *
 * @subsection creating-macro KWIN_EFFECT macro
 * To make KWin aware of your new effect, you first need to use the
 *  @ref KWIN_EFFECT macro to connect your effect's class to it's internal
 *  name. The internal name is used by KWin to identify your effect. It can be
 *  freely chosen (although it must be a single word), must be unique and won't
 * be shown to the user. For our example, you would use the macro like this:
 * @code
 * KWIN_EFFECT(cooleffect, CoolEffect)
 * @endcode
 *
 * @subsection creating-buildsystem Buildsystem
 * To build the effect, you can use the KWIN_ADD_EFFECT() cmake macro which
 *  can be found in effects/CMakeLists.txt file in KWin's source. First
 *  argument of the macro is the name of the library that will contain
 *  your effect. Although not strictly required, it is usually a good idea to
 *  use the same name as your effect's internal name there. Following arguments
 *  to the macro are the files containing your effect's source. If our effect's
 *  source is in cooleffect.cpp, we'd use following:
 * @code
 *  KWIN_ADD_EFFECT(cooleffect cooleffect.cpp)
 * @endcode
 *
 * This macro takes care of compiling your effect. You'll also need to install
 *  your effect's .desktop file, so the example CMakeLists.txt file would be
 *  as follows:
 * @code
 *  KWIN_ADD_EFFECT(cooleffect cooleffect.cpp)
 *  install( FILES cooleffect.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
 * @endcode
 *
 * @subsection creating-desktop Effect's .desktop file
 * You will also need to create .desktop file to set name, description, icon
 *  and other properties of your effect. Important fields of the .desktop file
 *  are:
 *  @li Name User-visible name of your effect
 *  @li Icon Name of the icon of the effect
 *  @li Comment Short description of the effect
 *  @li Type must be "Service"
 *  @li X-KDE-ServiceTypes must be "KWin/Effect"
 *  @li X-KDE-PluginInfo-Name effect's internal name as passed to the KWIN_EFFECT macro plus "kwin4_effect_" prefix
 *  @li X-KDE-PluginInfo-Category effect's category. Should be one of Appearance, Accessibility, Window Management, Demos, Tests, Misc
 *  @li X-KDE-PluginInfo-EnabledByDefault whether the effect should be enabled by default (use sparingly). Default is false
 *  @li X-KDE-Library name of the library containing the effect. This is the first argument passed to the KWIN_ADD_EFFECT macro in cmake file plus "kwin4_effect_" prefix.
 *
 * Example cooleffect.desktop file follows:
 * @code
[Desktop Entry]
Name=Cool Effect
Comment=The coolest effect you've ever seen
Icon=preferences-system-windows-effect-cooleffect

Type=Service
X-KDE-ServiceTypes=KWin/Effect
X-KDE-PluginInfo-Author=My Name
X-KDE-PluginInfo-Email=my@email.here
X-KDE-PluginInfo-Name=kwin4_effect_cooleffect
X-KDE-PluginInfo-Category=Misc
X-KDE-Library=kwin4_effect_cooleffect
 * @endcode
 *
 *
 * @section accessing Accessing windows and workspace
 * Effects can gain access to the properties of windows and workspace via
 *  EffectWindow and EffectsHandler classes.
 *
 * There is one global EffectsHandler object which you can access using the
 *  @ref effects pointer.
 * For each window, there is an EffectWindow object which can be used to read
 *  window properties such as position and also to change them.
 *
 * For more information about this, see the documentation of the corresponding
 *  classes.
 *
 * @{
 **/

#define KWIN_EFFECT_API_MAKE_VERSION( major, minor ) (( major ) << 8 | ( minor ))
#define KWIN_EFFECT_API_VERSION_MAJOR 0
#define KWIN_EFFECT_API_VERSION_MINOR 112
#define KWIN_EFFECT_API_VERSION KWIN_EFFECT_API_MAKE_VERSION( \
    KWIN_EFFECT_API_VERSION_MAJOR, KWIN_EFFECT_API_VERSION_MINOR )

enum WindowQuadType
    {
    WindowQuadError, // for the stupid default ctor
    WindowQuadContents,
    WindowQuadDecoration,
    EFFECT_QUAD_TYPE_START = 100 ///< @internal
    };

/**
 * EffectWindow::setData() and EffectWindow::data() global roles.
 * All values between 0 and 999 are reserved for global roles.
 */
enum DataRole
    {
    // Grab roles are used to force all other animations to ignore the window.
    // The value of the data is set to the Effect's `this` value.
    WindowAddedGrabRole = 1,
    WindowClosedGrabRole,
    WindowMinimizedGrabRole,
    WindowUnminimizedGrabRole
    };

/**
 * Infinite region (i.e. a special region type saying that everything needs to be painted).
 */
KWIN_EXPORT inline
QRect infiniteRegion()
    { // INT_MIN / 2 because width/height is used (INT_MIN+INT_MAX==-1)
    return QRect( INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX );
    }

/**
 * @short Base class for all KWin effects
 *
 * This is the base class for all effects. By reimplementing virtual methods
 *  of this class, you can customize how the windows are painted.
 *
 * The virtual methods of this class can broadly be divided into two
 *  categories: the methods used for painting and those you can use to be
 *  notified and react to certain events, e.g. that a window was closed.
 *
 * @section Chaining
 * Most methods of this class are called in chain style. This means that when
 *  effects A and B area active then first e.g. A::paintWindow() is called and
 *  then from within that method B::paintWindow() is called (although
 *  indirectly). To achieve this, you need to make sure to call corresponding
 *  method in EffectsHandler class from each such method (using @ref effects
 *  pointer):
 * @code
 *  void MyEffect::postPaintScreen()
 *  {
 *      // Do your own processing here
 *      ...
 *      // Call corresponding EffectsHandler method
 *      effects->postPaintScreen();
 *  }
 * @endcode
 *
 * @section Effectsptr Effects pointer
 * @ref effects pointer points to the global EffectsHandler object that you can
 *  use to interact with the windows.
 *
 * @section painting Painting stages
 * Painting of windows is done in three stages:
 * @li First, the prepaint pass.<br>
 *  Here you can specify how the windows will be painted, e.g. that they will
 *  be translucent and transformed.
 * @li Second, the paint pass.<br>
 *  Here the actual painting takes place. You can change attributes such as
 *  opacity of windows as well as apply transformations to them. You can also
 *  paint something onto the screen yourself.
 * @li Finally, the postpaint pass.<br>
 *  Here you can mark windows, part of windows or even the entire screen for
 *  repainting to create animations.
 *
 * For each stage there are *Screen() and *Window() methods. The window method
 *  is called for every window which the screen method is usually called just
 *  once.
 **/
class KWIN_EXPORT Effect
    {
    public:
        /** Flags controlling how painting is done. */
        // TODO: is that ok here?
        enum
        {
            /**
             * Window (or at least part of it) will be painted opaque.
             **/
            PAINT_WINDOW_OPAQUE         = 1 << 0,
            /**
             * Window (or at least part of it) will be painted translucent.
             **/
            PAINT_WINDOW_TRANSLUCENT    = 1 << 1,
            /**
             * Window will be painted with transformed geometry.
             **/
            PAINT_WINDOW_TRANSFORMED    = 1 << 2,
            /**
             * Paint only a region of the screen (can be optimized, cannot
             * be used together with TRANSFORMED flags).
             **/
            PAINT_SCREEN_REGION         = 1 << 3,
            /**
             * The whole screen will be painted with transformed geometry.
             * Forces the entire screen to be painted.
             **/
            PAINT_SCREEN_TRANSFORMED    = 1 << 4,
            /**
             * At least one window will be painted with transformed geometry.
             * Forces the entire screen to be painted.
             **/
            PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
            /**
             * Clear whole background as the very first step, without optimizing it
             **/
            PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6,
            /**
            * Temporary solution since (_OPAQUE | _TRANSLUCENT) is not working currently.
            **/
            PAINT_DECORATION_ONLY = 1 << 7
        };

        /**
         * Constructs new Effect object.
         **/
        Effect();
        /**
         * Destructs the Effect object.
         **/
        virtual ~Effect();

        /**
         * Flags describing which parts of configuration have changed.
         */
        enum ReconfigureFlag
            {
            ReconfigureAll = 1 << 0 /// Everything needs to be reconfigured.
            };
        Q_DECLARE_FLAGS( ReconfigureFlags, ReconfigureFlag )

        /**
         * Called when configuration changes (either the effect's or KWin's global).
         */
        virtual void reconfigure( ReconfigureFlags flags );

        /**
         * Called when another effect requests the proxy for this effect.
         */
        virtual void* proxy();

        /**
         * Called before starting to paint the screen.
         * In this method you can:
         * @li set whether the windows or the entire screen will be transformed
         * @li change the region of the screen that will be painted
         * @li do various housekeeping tasks such as initing your effect's variables
                for the upcoming paint pass or updating animation's progress
        **/
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        /**
         * In this method you can:
         * @li paint something on top of the windows (by painting after calling
         *      effects->paintScreen())
         * @li paint multiple desktops and/or multiple copies of the same desktop
         *      by calling effects->paintScreen() multiple times
         **/
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        /**
         * Called after all the painting has been finished.
         * In this method you can:
         * @li schedule next repaint in case of animations
         * You shouldn't paint anything here.
         **/
        virtual void postPaintScreen();

        /**
         * Called for every window before the actual paint pass
         * In this method you can:
         * @li enable or disable painting of the window (e.g. enable paiting of minimized window)
         * @li set window to be painted with translucency
         * @li set window to be transformed
         * @li request the window to be divided into multiple parts
         **/
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        /**
         * This is the main method for painting windows.
         * In this method you can:
         * @li do various transformations
         * @li change opacity of the window
         * @li change brightness and/or saturation, if it's supported
         **/
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        /**
         * Called for every window after all painting has been finished.
         * In this method you can:
         * @li schedule next repaint for individual window(s) in case of animations
         * You shouldn't paint anything here.
         **/
        virtual void postPaintWindow( EffectWindow* w );

        /**
         * Can be called to draw multiple copies (e.g. thumbnails) of a window.
         * You can change window's opacity/brightness/etc here, but you can't
         *  do any transformations
         **/
        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        /**
         * Define new window quads so that they can be transformed by other effects.
         * It's up to the effect to keep track of them.
         **/
        virtual void buildQuads( EffectWindow* w, WindowQuadList& quadList );

        /**
         * This function is used e.g. by the shadow effect which adds area around windows
         * that needs to be painted as well - e.g. when a window is hidden and the workspace needs
         * to be repainted at that area, shadow's transformWindowDamage() adds the shadow area
         * to it, so that it is repainted as well.
         **/
        virtual QRect transformWindowDamage( EffectWindow* w, const QRect& r );

        /** called when moved/resized or once after it's finished */
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        /** called when the geometry changed during moving/resizing. */
        virtual void windowMoveResizeGeometryUpdate( EffectWindow* c, const QRect& geometry );
        virtual void windowOpacityChanged( EffectWindow* c, double old_opacity );
        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );
        virtual void windowActivated( EffectWindow* c );
        virtual void windowMinimized( EffectWindow* c );
        virtual void windowUnminimized( EffectWindow* c );
        virtual void clientGroupItemSwitched( EffectWindow* from, EffectWindow* to );
        virtual void clientGroupItemAdded( EffectWindow* from, EffectWindow* to ); // from merged with to
        virtual void clientGroupItemRemoved( EffectWindow* c, EffectWindow* group ); // c removed from group
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void desktopChanged( int old );
        virtual void windowDamaged( EffectWindow* w, const QRect& r );
        virtual void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );
        virtual void mouseChanged( const QPoint& pos, const QPoint& oldpos,
            Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
            Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );
        /**
         Receives events registered for using EffectsHandler::registerPropertyType().
         Use readProperty() to get the property data.
         Note that the property may be already set on the window, so doing the same
         processing from windowAdded() (e.g. simply calling propertyNotify() from it)
         is usually needed.
         */
        virtual void propertyNotify( EffectWindow* w, long atom );

        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();
        virtual void tabBoxKeyEvent( QKeyEvent* event );
        virtual bool borderActivated( ElectricBorder border );

        static int displayWidth();
        static int displayHeight();
        static QPoint cursorPos();

        /**
         * Read animation time from the configuration and possibly adjust using animationTimeFactor().
         * The configuration value in the effect should also have special value 'default' (set using
         * QSpinBox::setSpecialValueText()) with the value 0. This special value is adjusted
         * using the global animation speed, otherwise the exact time configured is returned.
         * @param cfg configuration group to read value from
         * @param key configuration key to read value from
         * @param defaultTime default animation time in milliseconds
         */
        // return type is intentionally double so that one can divide using it without losing data
        static double animationTime( const KConfigGroup& cfg, const QString& key, int defaultTime );
        /**
         * @overload Use this variant if the animation time is hardcoded and not configurable
         * in the effect itself.
         */
        static double animationTime( int defaultTime );
        /**
         * Linearly interpolates between @p x and @p y.
         *
         * Returns @p x when @p a = 0; returns @p y when @p a = 1.
         **/
        static double interpolate(double x, double y, double a)
            {
            return x * (1 - a) + y * a;
            }
        /** Helper to set WindowPaintData and QRegion to necessary transformations so that
         * a following drawWindow() would put the window at the requested geometry (useful for thumbnails)
         **/
        static void setPositionTransformations( WindowPaintData& data, QRect& region, EffectWindow* w,
            const QRect& r, Qt::AspectRatioMode aspect );
    };


/**
 * Defines the class to be used for effect with given name.
 * The name must be same as effect's X-KDE-PluginInfo-Name values in .desktop
 *  file, but without the "kwin4_effect_" prefix.
 * E.g.  KWIN_EFFECT( flames, MyFlameEffect )
 * In this case object of MyFlameEffect class would be created when effect
 *  "flames" (which has X-KDE-PluginInfo-Name=kwin4_effect_flames in .desktop
 *  file) is loaded.
 **/
#define KWIN_EFFECT( name, classname ) \
    extern "C" { \
        KWIN_EXPORT Effect* effect_create_kwin4_effect_##name() { return new classname; } \
        KWIN_EXPORT int effect_version_kwin4_effect_##name() { return KWIN_EFFECT_API_VERSION; } \
    }
/**
 * Defines the function used to check whether an effect is supported
 * E.g.  KWIN_EFFECT_SUPPORTED( flames, MyFlameEffect::supported() )
 **/
#define KWIN_EFFECT_SUPPORTED( name, function ) \
    extern "C" { \
        KWIN_EXPORT bool effect_supported_kwin4_effect_##name() { return function; } \
    }
/**
 * Defines the function used to retrieve an effect's config widget
 * E.g.  KWIN_EFFECT_CONFIG( flames, MyFlameEffectConfig )
 **/
#define KWIN_EFFECT_CONFIG( name, classname ) \
    K_PLUGIN_FACTORY(EffectFactory, registerPlugin<classname>(#name);) \
    K_EXPORT_PLUGIN(EffectFactory("kcm_kwin4_effect_" #name))
/**
 * Defines the function used to retrieve multiple effects' config widget
 * E.g.  KWIN_EFFECT_CONFIG_MULTIPLE( flames,
 *           KWIN_EFFECT_CONFIG_SINGLE( flames, MyFlameEffectConfig )
 *           KWIN_EFFECT_CONFIG_SINGLE( fire, MyFireEffectConfig )
 *           )
 **/
#define KWIN_EFFECT_CONFIG_MULTIPLE( name, singles ) \
    K_PLUGIN_FACTORY(EffectFactory, singles) \
    K_EXPORT_PLUGIN(EffectFactory("kcm_kwin4_effect_" #name))
/**
 * @see KWIN_EFFECT_CONFIG_MULTIPLE
 */
#define KWIN_EFFECT_CONFIG_SINGLE( name, classname ) \
    registerPlugin<classname>(#name);
/**
 * The declaration of the factory to export the effect
 */
#define KWIN_EFFECT_CONFIG_FACTORY K_PLUGIN_FACTORY_DECLARATION(EffectFactory)


/**
 * @short Manager class that handles all the effects.
 *
 * This class creates Effect objects and calls it's appropriate methods.
 *
 * Effect objects can call methods of this class to interact with the
 *  workspace, e.g. to activate or move a specific window, change current
 *  desktop or create a special input window to receive mouse and keyboard
 *  events.
 **/
class KWIN_EXPORT EffectsHandler
    {
    friend class Effect;
    public:
        EffectsHandler(CompositingType type);
        virtual ~EffectsHandler();
        // for use by effects
        virtual void prePaintScreen( ScreenPrePaintData& data, int time ) = 0;
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data ) = 0;
        virtual void postPaintScreen() = 0;
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time ) = 0;
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data ) = 0;
        virtual void postPaintWindow( EffectWindow* w ) = 0;
        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data ) = 0;
        virtual void buildQuads( EffectWindow* w, WindowQuadList& quadList ) = 0;
        virtual QRect transformWindowDamage( EffectWindow* w, const QRect& r );
        // Functions for handling input - e.g. when an Expose-like effect is shown, an input window
        // covering the whole screen is created and all mouse events will be intercepted by it.
        // The effect's windowInputMouseEvent() will get called with such events.
        virtual Window createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor ) = 0;
        Window createInputWindow( Effect* e, const QRect& r, const QCursor& cursor );
        virtual Window createFullScreenInputWindow( Effect* e, const QCursor& cursor );
        virtual void destroyInputWindow( Window w ) = 0;
        virtual QPoint cursorPos() const = 0;
        virtual bool grabKeyboard( Effect* effect ) = 0;
        virtual void ungrabKeyboard() = 0;

        /**
         * Retrieve the proxy class for an effect if it has one. Will return NULL if
         * the effect isn't loaded or doesn't have a proxy class.
         */
        virtual void* getProxy( QString name ) = 0;

        // Mouse polling
        virtual void startMousePolling() = 0;
        virtual void stopMousePolling() = 0;

        virtual void checkElectricBorder(const QPoint &pos, Time time) = 0;
        virtual void reserveElectricBorder( ElectricBorder border ) = 0;
        virtual void unreserveElectricBorder( ElectricBorder border ) = 0;
        virtual void reserveElectricBorderSwitching( bool reserve ) = 0;

        // functions that allow controlling windows/desktop
        virtual void activateWindow( EffectWindow* c ) = 0;
        virtual EffectWindow* activeWindow() const = 0 ;
        virtual void moveWindow( EffectWindow* w, const QPoint& pos, bool snap = false, double snapAdjust = 1.0 ) = 0;
        virtual void windowToDesktop( EffectWindow* w, int desktop ) = 0;
        virtual void windowToScreen( EffectWindow* w, int screen ) = 0;
        virtual void setShowingDesktop( bool showing ) = 0;

        // Desktops
        /**
         * @returns The ID of the current desktop.
         */
        virtual int currentDesktop() const = 0;
        /**
         * @returns Total number of desktops currently in existence.
         */
        virtual int numberOfDesktops() const = 0;
        /**
         * Set the current desktop to @a desktop.
         */
        virtual void setCurrentDesktop( int desktop ) = 0;
        /**
         * @returns The size of desktop layout in grid units.
         */
        virtual QSize desktopGridSize() const = 0;
        /**
         * @returns The width of desktop layout in grid units.
         */
        virtual int desktopGridWidth() const = 0;
        /**
         * @returns The height of desktop layout in grid units.
         */
        virtual int desktopGridHeight() const = 0;
        /**
         * @returns The width of desktop layout in pixels.
         */
        virtual int workspaceWidth() const = 0;
        /**
         * @returns The height of desktop layout in pixels.
         */
        virtual int workspaceHeight() const = 0;
        /**
         * @returns The ID of the desktop at the point @a coords or 0 if no desktop exists at that
         * point. @a coords is to be in grid units.
         */
        virtual int desktopAtCoords( QPoint coords ) const = 0;
        /**
         * @returns The coords of desktop @a id in grid units.
         */
        virtual QPoint desktopGridCoords( int id ) const = 0;
        /**
         * @returns The coords of the top-left corner of desktop @a id in pixels.
         */
        virtual QPoint desktopCoords( int id ) const = 0;
        /**
         * @returns The ID of the desktop above desktop @a id. Wraps around to the bottom of
         * the layout if @a wrap is set. If @a id is not set use the current one.
         */
        virtual int desktopAbove( int desktop = 0, bool wrap = true ) const = 0;
        /**
         * @returns The ID of the desktop to the right of desktop @a id. Wraps around to the
         * left of the layout if @a wrap is set. If @a id is not set use the current one.
         */
        virtual int desktopToRight( int desktop = 0, bool wrap = true ) const = 0;
        /**
         * @returns The ID of the desktop below desktop @a id. Wraps around to the top of the
         * layout if @a wrap is set. If @a id is not set use the current one.
         */
        virtual int desktopBelow( int desktop = 0, bool wrap = true ) const = 0;
        /**
         * @returns The ID of the desktop to the left of desktop @a id. Wraps around to the
         * right of the layout if @a wrap is set. If @a id is not set use the current one.
         */
        virtual int desktopToLeft( int desktop = 0, bool wrap = true ) const = 0;
        /**
         * @returns Whether or not the desktop layout is allowed to be modified by the user.
         */
        virtual bool isDesktopLayoutDynamic() const = 0;
        /**
         * Create new desktop at the point @a coords
         * @returns The ID of the created desktop
         */
        virtual int addDesktop( QPoint coords ) = 0;
        /**
         * Deletes the desktop with the ID @a id. All desktops with an ID greater than the one that
         * was deleted will have their IDs' decremented.
         */
        virtual void deleteDesktop( int id ) = 0;
        virtual QString desktopName( int desktop ) const = 0;
        virtual bool optionRollOverDesktops() const = 0;

        virtual int activeScreen() const = 0; // Xinerama
        virtual int numScreens() const = 0; // Xinerama
        virtual int screenNumber( const QPoint& pos ) const = 0; // Xinerama
        virtual QRect clientArea( clientAreaOption, int screen, int desktop ) const = 0;
        virtual QRect clientArea( clientAreaOption, const EffectWindow* c ) const = 0;
        virtual QRect clientArea( clientAreaOption, const QPoint& p, int desktop ) const = 0;
        /**
         * Factor by which animation speed in the effect should be modified (multiplied).
         * If configurable in the effect itself, the option should have also 'default'
         * animation speed. The actual value should be determined using animationTime().
         * Note: The factor can be also 0, so make sure your code can cope with 0ms time
         * if used manually.
         */
        virtual double animationTimeFactor() const = 0;
        virtual WindowQuadType newWindowQuadType() = 0;

        virtual EffectWindow* findWindow( WId id ) const = 0;
        virtual EffectWindowList stackingOrder() const = 0;
        // window will be temporarily painted as if being at the top of the stack
        virtual void setElevatedWindow( EffectWindow* w, bool set ) = 0;

        virtual void setTabBoxWindow(EffectWindow*) = 0;
        virtual void setTabBoxDesktop(int) = 0;
        virtual EffectWindowList currentTabBoxWindowList() const = 0;
        virtual void refTabBox() = 0;
        virtual void unrefTabBox() = 0;
        virtual void closeTabBox() = 0;
        virtual QList< int > currentTabBoxDesktopList() const = 0;
        virtual int currentTabBoxDesktop() const = 0;
        virtual EffectWindow* currentTabBoxWindow() const = 0;

        virtual void setActiveFullScreenEffect( Effect* e ) = 0;
        virtual Effect* activeFullScreenEffect() const = 0;

        virtual void pushRenderTarget(GLRenderTarget* target) = 0;
        virtual GLRenderTarget* popRenderTarget() = 0;

        /**
         * Schedules the entire workspace to be repainted next time.
         * If you call it during painting (including prepaint) then it does not
         *  affect the current painting.
         **/
        virtual void addRepaintFull() = 0;
        virtual void addRepaint( const QRect& r ) = 0;
        virtual void addRepaint( const QRegion& r ) = 0;
        virtual void addRepaint( int x, int y, int w, int h ) = 0;

        CompositingType compositingType() const;
        virtual unsigned long xrenderBufferPicture() = 0;
        bool saturationSupported() const;
        virtual void reconfigure() = 0;

        /**
         Makes KWin core watch PropertyNotify events for the given atom,
         or stops watching if reg is false (must be called the same number
         of times as registering). Events are sent using Effect::propertyNotify().
         Note that even events that haven't been registered for can be received.
        */
        virtual void registerPropertyType( long atom, bool reg ) = 0;

        /**
         * Returns @a true if the active window decoration has shadow API hooks.
         */
        virtual bool hasDecorationShadows() const = 0;

        /**
         * @deprecated
         * @see EffectFrame
         * Paints given text onto screen, possibly in elided form
         * @param text
         * @param center center point of the painted text
         * @param maxwidth if text is longer than this, is will be elided
         * @param color color of the text, may contain alpha
         * @param font font for the text
         **/
        bool paintText( const QString& text, const QPoint& center, int maxwidth,
                        const QColor& color, const QFont& font = QFont() );
        /**
         * @deprecated
         * @see EffectFrame
         */
        bool paintText( const QString& text, const QRect& rect, const QColor& color,
                        const QFont& font = QFont(), const Qt::Alignment& alignment = Qt::AlignCenter );
        /**
         * @deprecated
         * @see EffectFrame
         */
        bool paintTextWithBackground( const QString& text, const QPoint& center, int maxwidth,
                                      const QColor& color, const QColor& bgcolor,
                                      const QFont& font = QFont() );
        /**
         * @deprecated
         * @see EffectFrame
         */
        bool paintTextWithBackground( const QString& text, const QRect& rect, const QColor& color,
                                      const QColor& bgcolor, const QFont& font = QFont(),
                                      const Qt::Alignment& alignment = Qt::AlignCenter );


        /**
         * Sends message over DCOP to reload given effect.
         * @param effectname effect's name without "kwin4_effect_" prefix.
         * Can be called from effect's config module to apply config changes.
         **/
        static void sendReloadMessage( const QString& effectname );
        /**
         * @return @ref KConfigGroup which holds given effect's config options
         **/
        static KConfigGroup effectConfig( const QString& effectname );


    protected:
        QVector< EffectPair > loaded_effects;
        QHash< QString, KLibrary* > effect_libraries;
        QList< InputWindowPair > input_windows;
        //QHash< QString, EffectFactory* > effect_factories;
        int current_paint_screen;
        int current_paint_window;
        int current_draw_window;
        int current_build_quads;
        int current_transform;
        CompositingType compositing_type;
    };


/**
 * @short Representation of a window used by/for Effect classes.
 *
 * The purpose is to hide internal data and also to serve as a single
 *  representation for the case when Client/Unmanaged becomes Deleted.
 **/
class KWIN_EXPORT EffectWindow
    {
    public:
        /**  Flags explaining why painting should be disabled  */
        enum
        {
            /**  Window will not be painted  */
            PAINT_DISABLED                 = 1 << 0,
            /**  Window will not be painted because it is deleted  */
            PAINT_DISABLED_BY_DELETE       = 1 << 1,
            /**  Window will not be painted because of which desktop it's on  */
            PAINT_DISABLED_BY_DESKTOP      = 1 << 2,
            /**  Window will not be painted because it is minimized  */
            PAINT_DISABLED_BY_MINIMIZE     = 1 << 3,
            /**  Window will not be painted because it is not the active window in a client group  */
            PAINT_DISABLED_BY_CLIENT_GROUP = 1 << 4
        };

        EffectWindow();
        virtual ~EffectWindow();

        virtual void enablePainting( int reason ) = 0;
        virtual void disablePainting( int reason ) = 0;
        virtual bool isPaintingEnabled() = 0;
        virtual void addRepaint( const QRect& r ) = 0;
        virtual void addRepaint( int x, int y, int w, int h ) = 0;
        virtual void addRepaintFull() = 0;

        virtual void refWindow() = 0;
        virtual void unrefWindow() = 0;
        virtual bool isDeleted() const = 0;

        virtual bool isMinimized() const = 0;
        virtual double opacity() const = 0;
        virtual bool hasAlpha() const = 0;

        virtual bool isOnDesktop( int d ) const;
        virtual bool isOnCurrentDesktop() const;
        virtual bool isOnAllDesktops() const = 0;
        virtual int desktop() const = 0; // prefer isOnXXX()

        virtual int x() const = 0;
        virtual int y() const = 0;
        virtual int width() const = 0;
        virtual int height() const = 0;
        virtual QRect geometry() const = 0;
        virtual QRegion shape() const = 0;
        virtual int screen() const = 0;
        /** @internal Do not use */
        virtual bool hasOwnShape() const = 0; // only for shadow effect, for now
        virtual QPoint pos() const = 0;
        virtual QSize size() const = 0;
        virtual QRect rect() const = 0;
        virtual bool isMovable() const = 0;
        virtual bool isMovableAcrossScreens() const = 0;
        virtual bool isUserMove() const = 0;
        virtual bool isUserResize() const = 0;
        virtual QRect iconGeometry() const = 0;
        /**
         * Geometry of the actual window contents inside the whole (including decorations) window.
         */
        virtual QRect contentsRect() const = 0;
        bool hasDecoration() const;
        virtual QByteArray readProperty( long atom, long type, int format ) const = 0;
        virtual void deleteProperty( long atom ) const = 0;

        virtual QString caption() const = 0;
        virtual QPixmap icon() const = 0;
        virtual QString windowClass() const = 0;
        virtual QString windowRole() const = 0;
        virtual const EffectWindowGroup* group() const = 0;

        /**
         * Returns whether the window is a desktop background window (the one with wallpaper).
         * See _NET_WM_WINDOW_TYPE_DESKTOP at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isDesktop() const = 0;
        /**
         * Returns whether the window is a dock (i.e. a panel).
         * See _NET_WM_WINDOW_TYPE_DOCK at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isDock() const = 0;
        /**
         * Returns whether the window is a standalone (detached) toolbar window.
         * See _NET_WM_WINDOW_TYPE_TOOLBAR at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isToolbar() const = 0;
        /**
         * Returns whether the window is standalone menubar (AKA macmenu).
         * This window type is a KDE extension.
         */
        virtual bool isTopMenu() const = 0;
        /**
         * Returns whether the window is a torn-off menu.
         * See _NET_WM_WINDOW_TYPE_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isMenu() const = 0;
        /**
         * Returns whether the window is a "normal" window, i.e. an application or any other window
         * for which none of the specialized window types fit.
         * See _NET_WM_WINDOW_TYPE_NORMAL at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isNormalWindow() const = 0; // normal as in 'NET::Normal or NET::Unknown non-transient'
        /**
         * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
         * i.e. window types that usually don't have a window frame and the user does not use window
         * management (moving, raising,...) on them.
         */
        virtual bool isSpecialWindow() const = 0;
        /**
         * Returns whether the window is a dialog window.
         * See _NET_WM_WINDOW_TYPE_DIALOG at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isDialog() const = 0;
        /**
         * Returns whether the window is a splashscreen. Note that many (especially older) applications
         * do not support marking their splash windows with this type.
         * See _NET_WM_WINDOW_TYPE_SPLASH at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isSplash() const = 0;
        /**
         * Returns whether the window is a utility window, such as a tool window.
         * See _NET_WM_WINDOW_TYPE_UTILITY at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isUtility() const = 0;
        /**
         * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
         * from the applications menubar).
         * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isDropdownMenu() const = 0;
        /**
         * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
         * See _NET_WM_WINDOW_TYPE_POPUP_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isPopupMenu() const = 0; // a context popup, not dropdown, not torn-off
        /**
         * Returns whether the window is a tooltip.
         * See _NET_WM_WINDOW_TYPE_TOOLTIP at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isTooltip() const = 0;
        /**
         * Returns whether the window is a window with a notification.
         * See _NET_WM_WINDOW_TYPE_NOTIFICATION at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isNotification() const = 0;
        /**
         * Returns whether the window is a combobox popup.
         * See _NET_WM_WINDOW_TYPE_COMBO at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isComboBox() const = 0;
        /**
         * Returns whether the window is a Drag&Drop icon.
         * See _NET_WM_WINDOW_TYPE_DND at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
         */
        virtual bool isDNDIcon() const = 0;
        /**
         * Returns whether the window is managed by KWin (it has control over its placement and other
         * aspects, as opposed to override-redirect windows that are entirely handled by the application).
         */
        virtual bool isManaged() const = 0; // whether it's managed or override-redirect
        /**
         * Returns whether or not the window can accept keyboard focus.
         */
        virtual bool acceptsFocus() const = 0;
        /**
         * Returns whether or not the window is kept above all other windows.
         */
        virtual bool keepAbove() const = 0;

        virtual bool isModal() const = 0;
        virtual EffectWindow* findModal() = 0;
        virtual EffectWindowList mainWindows() const = 0;

        /**
         * Returns the unmodified window quad list. Can also be used to force rebuilding.
         */
        virtual WindowQuadList buildQuads( bool force = false ) const = 0;

        virtual void minimize() const = 0;
        virtual void unminimize() const = 0;
        virtual void closeWindow() const = 0;

        virtual bool visibleInClientGroup() const = 0;

        /**
         * Can be used to by effects to store arbitrary data in the EffectWindow. 
         */
        virtual void setData( int role, const QVariant &data ) = 0;
        virtual QVariant data( int role ) const = 0;
    };

class KWIN_EXPORT EffectWindowGroup
    {
    public:
        virtual ~EffectWindowGroup();
        virtual EffectWindowList members() const = 0;
    };

class KWIN_EXPORT GlobalShortcutsEditor : public KShortcutsEditor
    {
    public:
        GlobalShortcutsEditor( QWidget *parent );
    };

/**
 * @short Vertex class
 *
 * A vertex is one position in a window. WindowQuad consists of four WindowVertex objects
 * and represents one part of a window.
 **/
class KWIN_EXPORT WindowVertex
    {
    public:
        double x() const;
        double y() const;
        void move( double x, double y );
        void setX( double x );
        void setY( double y );
        double originalX() const;
        double originalY() const;
        double textureX() const;
        double textureY() const;
        WindowVertex();
        WindowVertex( double x, double y, double tx, double ty );
    private:
        friend class WindowQuad;
        friend class WindowQuadList;
        double px, py; // position
        double ox, oy; // origional position
        double tx, ty; // texture coords
    };

/**
 * @short Class representing one area of a window.
 *
 * WindowQuads consists of four WindowVertex objects and represents one part of a window.
 */
// NOTE: This class expects the (original) vertices to be in the clockwise order starting from topleft.
class KWIN_EXPORT WindowQuad
    {
    public:
        explicit WindowQuad( WindowQuadType type, int id = -1 );
        WindowQuad makeSubQuad( double x1, double y1, double x2, double y2 ) const;
        WindowVertex& operator[]( int index );
        const WindowVertex& operator[]( int index ) const;
        WindowQuadType type() const;
        int id() const;
        bool decoration() const;
        bool effect() const;
        double left() const;
        double right() const;
        double top() const;
        double bottom() const;
        double originalLeft() const;
        double originalRight() const;
        double originalTop() const;
        double originalBottom() const;
        bool smoothNeeded() const;
        bool isTransformed() const;
    private:
        friend class WindowQuadList;
        WindowVertex verts[ 4 ];
        WindowQuadType quadType; // 0 - contents, 1 - decoration
        int quadID;
    };

class KWIN_EXPORT WindowQuadList
    : public QList< WindowQuad >
    {
    public:
        WindowQuadList splitAtX( double x ) const;
        WindowQuadList splitAtY( double y ) const;
        WindowQuadList makeGrid( int maxquadsize ) const;
        WindowQuadList makeRegularGrid( int xSubdivisions, int ySubdivisions ) const;
        WindowQuadList select( WindowQuadType type ) const;
        WindowQuadList filterOut( WindowQuadType type ) const;
        bool smoothNeeded() const;
        void makeArrays( float** vertices, float** texcoords ) const;
        bool isTransformed() const;
    };

class KWIN_EXPORT WindowPrePaintData
    {
    public:
        int mask;
        /**
         * Region that will be painted, in screen coordinates.
         **/
        QRegion paint;
        /**
         * The clip region will be substracted from paint region of following windows.
         * I.e. window will definitely cover it's clip region
         **/
        QRegion clip;
        WindowQuadList quads;
        /**
         * Simple helper that sets data to say the window will be painted as non-opaque.
         * Takes also care of changing the regions.
         */
        void setTranslucent();
        /**
         * Helper to mark that this window will be transformed
         **/
        void setTransformed();
    };

class KWIN_EXPORT WindowPaintData
    {
    public:
        WindowPaintData( EffectWindow* w );
        /**
         * Window opacity, in range 0 = transparent to 1 = fully opaque
         * Opacity for contents is opacity*contents_opacity, the same
         * way for decoration.
         */
        double opacity;
        double contents_opacity;
        double decoration_opacity;
        double xScale;
        double yScale;
        double zScale;
        int xTranslate;
        int yTranslate;
        double zTranslate;
        /**
         * Saturation of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         *  unsaturated (greyscale). 0.5 would make the colors less intense,
         *  but not completely grey
         * Use EffectsHandler::saturationSupported() to find out whether saturation
         * is supported by the system, otherwise this value has no effect.
         **/
        double saturation;
        /**
         * Brightness of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         * black. 0.5 would make it 50% darker than usual
         **/
        double brightness;
        WindowQuadList quads;
        /**
         * Shader to be used for rendering, if any.
         */
        GLShader* shader;
        RotationData* rotation;
    };

class KWIN_EXPORT ScreenPaintData
    {
    public:
        ScreenPaintData();
        double xScale;
        double yScale;
        double zScale;
        int xTranslate;
        int yTranslate;
        double zTranslate;
        RotationData* rotation;
    };

class KWIN_EXPORT ScreenPrePaintData
    {
    public:
        int mask;
        QRegion paint;
    };

class KWIN_EXPORT RotationData
    {
    public:
        RotationData();
        enum RotationAxis
            {
            XAxis,
            YAxis,
            ZAxis
            };
        RotationAxis axis;
        float angle;
        float xRotationPoint;
        float yRotationPoint;
        float zRotationPoint;
    };

/**
 * @short Helper class for restricting painting area only to allowed area.
 *
 * This helper class helps specifying areas that should be painted, clipping
 * out the rest. The simplest usage is creating an object on the stack
 * and giving it the area that is allowed to be painted to. When the object
 * is destroyed, the restriction will be removed.
 * Note that all painting code must use paintArea() to actually perform the clipping.
 */
class KWIN_EXPORT PaintClipper
    {
    public:
        /**
         * Calls push().
         */
        PaintClipper( const QRegion& allowed_area );
        /**
         * Calls pop().
         */
        ~PaintClipper();
        /**
         * Allows painting only in the given area. When areas have been already
         * specified, painting is allowed only in the intersection of all areas.
         */
        static void push( const QRegion& allowed_area );
        /**
         * Removes the given area. It must match the top item in the stack.
         */
        static void pop( const QRegion& allowed_area );
        /**
         * Returns true if any clipping should be performed.
         */
        static bool clip();
        /**
         * If clip() returns true, this function gives the resulting area in which
         * painting is allowed. It is usually simpler to use the helper Iterator class.
         */
        static QRegion paintArea();
        /**
         * Helper class to perform the clipped painting. The usage is:
         * @code
         * for( PaintClipper::Iterator iterator;
         *      !iterator.isDone();
         *      iterator.next())
         *     { // do the painting, possibly use iterator.boundingRect()
         *     }
         * @endcode
         */
        class KWIN_EXPORT Iterator
            {
            public:
                Iterator();
                ~Iterator();
                bool isDone();
                void next();
                QRect boundingRect() const;
            private:
                struct Data;
                Data* data;
            };
    private:
        QRegion area;
        static QStack< QRegion >* areas;
    };


/**
 * @short Wrapper class for using timelines in KWin effects.
 *
 * This class provides an easy and specialized interface for
 * effects that want a non-linear timeline. Currently, most
 * it does is wrapping QTimeLine. In the future, this class
 * could help using physics animations in KWin.
 *
 * KWin effects will usually use this feature by keeping one
 * TimeLine per animation, for example per effect (when only
 * one animation is done by the effect) or per windows (in
 * the case that multiple animations can take place at the
 * same time (such as minizing multiple windows with a short
 * time offset. Increasing the internal time state of the
 * TimeLine is done either by changing the 'current' time in
 * the TimeLine, which is an int value between 0 and the
 * duration set (defaulting to 250msec). The current time
 * can also be changed by setting the 'progress' of the
 * TimeLine, a double between 0.0 and 1.0. setProgress(),
 * addProgress(), addTime(), removeTime() can all be used to
 * manipulate the 'current' time (and at the same time
 * progress) of the TimeLine.
 *
 * The internal state of the TimeLine is determined by the
 * duration of the TimeLine, int in milliseconds, defaulting.
 * the 'current' time and the current 'progress' are
 * interchangeable, both determine the x-axis of a graph
 * of a TimeLine. The value() returned represents the y-axis
 * in this graph.
 *
 * m_TimeLine.setProgress(0.5) would change the 'current'
 * time of a default TimeLine to 125 msec. m_TimeLine.value()
 * would then return the progress value (a double between 0.0
 * and 1.0), which can be lower than 0.5 in case the animation
 * should start slowly, such as for the EaseInCurve.
 * In KWin effect, the prePaintWindow() or prePaintScreen()
 * methods have int time as one of their arguments. This int
 * can be used to increase the 'current' time in the TimeLine.
 * The double the is subsequently returned by value() (usually
 * in paintWindow() or paintScreen() methods can then be used
 * to manipulate windows, or their positions.
 */
class KWIN_EXPORT TimeLine
    {

    //Q_ENUMS( CurveShape ) // Requires Q_OBJECT

    public:
        /**
         * The CurveShape describes the relationship between time
         * and values. We can pass some of them through to QTimeLine
         * but also invent our own ones.
         */
        enum CurveShape
        {
            EaseInCurve = 0,
            EaseOutCurve,
            EaseInOutCurve,
            LinearCurve,
            SineCurve
        };

        /**
         * Creates a TimeLine and computes the progress data. Usually, for larger
         * animations you want to choose values more towards 300 milliseconds.
         * For small animations, values around 150 milliseconds are sensible.
         * Note that duration 0 is not valid.
         */
        explicit TimeLine(const int duration = 0);

        /**
         * Creates a copy of the TimeLine so we can have the state copied
         * as well.
         */
        TimeLine(const TimeLine &other);
        /**
         * Cleans up.
         */
        ~TimeLine();
        /**
         * Returns the duration of the timeline in msec.
         */
        int duration() const;
        /**
         * Set the duration of the TimeLine.
         */
        void setDuration(const int msec);
        /**
         * Returns the Value at the time set, this method will
         * usually be used to get the progress in the paintWindow()
         * and related methods. The value represents the y-axis' value
         * corresponding to the current progress (or time) set by
         * setProgress(), addProgress(), addTime(), removeTime()
         */
        double value() const;
        /**
         * Returns the Value at the time provided, this method will
         * usually be used to get the progress in the paintWindow()
         * and related methods, the y value of the current state x.
         */
        double valueForTime(const int msec) const;
        /**
         * Returns the current time of the TimeLine, between 0 and duration()
         * The value returned is equivalent to the x-axis on a curve.
         */
        int time() const;
        /**
         * Returns the progress of the TimeLine, between 0.0 and 1.0.
         * The value returned is equivalent to the y-axis on a curve.
         */
        double progress() const;
        /**
         * Increases the internal progress accounting of the timeline.
         */
        void addProgress(const double progress);
        /**
         * Increases the internal counter, this is usually done in
         * prePaintWindow().
         */
        void addTime(const int msec);
        /**
         * Decreases the internal counter, this is usually done in
         * prePaintWindow(). This function comes handy for reverse
         * animations.
         */
        void removeTime(const int msec);
        /**
         * Set the time to progress * duration. This will change the
         * internal time in the TimeLine. It's usually used in
         * prePaintWindow() or prePaintScreen() so the value()
         * taken in paint* is increased.
         */
        void setProgress(const double progress);
        /**
         * Set the CurveShape. The CurveShape describes the relation
         * between the value and the time. progress is between 0 and 1
         * It's used as input for the timeline, the x axis of the curve.
         */
        void setCurveShape(CurveShape curveShape);
        /**
         * Set the CurveShape. The CurveShape describes the relation
         * between the value and the time.
         */
        //void setCurveShape(CurveShape curveShape);

    private:
        QTimeLine* m_TimeLine;
        int m_Time;
        double m_Progress;
        int m_Duration;
        CurveShape m_CurveShape;
        //Q_DISABLE_COPY(TimeLine)
    };

/**
 * @internal
 */
template <typename T>
class KWIN_EXPORT Motion
    {
    public:
        /**
         * Creates a new motion object. "Strength" is the amount of
         * acceleration that is applied to the object when the target
         * changes and "smoothness" relates to how fast the object
         * can change its direction and speed.
         */
        explicit Motion( T initial, double strength, double smoothness );
        /**
         * Creates an exact copy of another motion object, including
         * position, target and velocity.
         */
        Motion( const Motion<T> &other );
        ~Motion();

        inline T value() const { return m_value; }
        inline void setValue( const T value ) { m_value = value; }
        inline T target() const { return m_target; }
        inline void setTarget( const T target ) { m_target = target; }
        inline T velocity() const { return m_velocity; }
        inline void setVelocity( const T velocity ) { m_velocity = velocity; }

        inline double strength() const { return m_strength; }
        inline void setStrength( const double strength ) { m_strength = strength; }
        inline double smoothness() const { return m_smoothness; }
        inline void setSmoothness( const double smoothness ) { m_smoothness = smoothness; }

        /**
         * The distance between the current position and the target.
         */
        inline T distance() const { return m_target - m_value; }

        /**
         * Calculates the new position if not at the target. Called
         * once per frame only.
         */
        void calculate( const int msec );
        /**
         * Place the object on top of the target immediately,
         * bypassing all movement calculation.
         */
        void finish();

    private:
        T m_value;

        T m_target;
        T m_velocity;
        double m_strength;
        double m_smoothness;
    };

/**
 * @short A single 1D motion dynamics object.
 *
 * This class represents a single object that can be moved around a
 * 1D space. Although it can be used directly by itself it is
 * recommended to use a motion manager instead.
 */
class KWIN_EXPORT Motion1D : public Motion<double>
    {
    public:
        explicit Motion1D( double initial = 0.0, double strength = 0.08, double smoothness = 4.0 );
        Motion1D( const Motion1D &other );
        ~Motion1D();
    };

/**
 * @short A single 2D motion dynamics object.
 *
 * This class represents a single object that can be moved around a
 * 2D space. Although it can be used directly by itself it is
 * recommended to use a motion manager instead.
 */
class KWIN_EXPORT Motion2D : public Motion<QPointF>
    {
    public:
        explicit Motion2D( QPointF initial = QPointF(), double strength = 0.08, double smoothness = 4.0 );
        Motion2D( const Motion2D &other );
        ~Motion2D();
    };

/**
 * @short Helper class for motion dynamics in KWin effects.
 *
 * This motion manager class is intended to help KWin effect authors
 * move windows across the screen smoothly and naturally. Once
 * windows are registered by the manager the effect can issue move
 * commands with the moveWindow() methods. The position of any
 * managed window can be determined in realtime by the
 * transformedGeometry() method. As the manager knows if any windows
 * are moving at any given time it can also be used as a notifier as
 * to see whether the effect is active or not.
 */
class KWIN_EXPORT WindowMotionManager
    {
    public:
        /**
         * Creates a new window manager object.
         */
        explicit WindowMotionManager( bool useGlobalAnimationModifier = true );
        ~WindowMotionManager();

        /**
         * Register a window for managing.
         */
        void manage( EffectWindow *w );
        /**
         * Register a list of windows for managing.
         */
        inline void manage( EffectWindowList list )
            {
            for( int i = 0; i < list.size(); i++ )
                manage( list.at( i ));
            }
        /**
         * Deregister a window. All transformations applied to the
         * window will be permanently removed and cannot be recovered.
         */
        void unmanage( EffectWindow *w );
        /**
         * Deregister all windows, returning the manager to its
         * originally initiated state.
         */
        void unmanageAll();
        /**
         * Determine the new positions for windows that have not
         * reached their target. Called once per frame, usually in
         * prePaintScreen(). Remember to set the
         * Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS flag.
         */
        void calculate( int time );
        /**
         * Modify a registered window's paint data to make it appear
         * at its real location on the screen. Usually called in
         * paintWindow(). Remember to flag the window as having been
         * transformed in prePaintWindow() by calling
         * WindowPrePaintData::setTransformed()
         */
        void apply( EffectWindow *w, WindowPaintData &data );
        /**
         * Set all motion targets and values back to where the
         * windows were before transformations. The same as
         * unmanaging then remanaging all windows.
         */
        void reset();
        /**
         * Resets the motion target and current value of a single
         * window.
         */
        void reset( EffectWindow *w );

        /**
         * Ask the manager to move the window to the target position
         * with the specified scale. If `yScale` is not provided or
         * set to 0.0, `scale` will be used as the scale in the
         * vertical direction as well as in the horizontal direction.
         */
        void moveWindow( EffectWindow *w, QPoint target, double scale = 1.0, double yScale = 0.0 );
        /**
         * This is an overloaded method, provided for convenience.
         * 
         * Ask the manager to move the window to the target rectangle.
         * Automatically determines scale.
         */
        inline void moveWindow( EffectWindow *w, QRect target )
            {
            // TODO: Scale might be slightly different in the comparison due to rounding
            moveWindow( w, target.topLeft(),
                target.width() / double( w->width() ), target.height() / double( w->height() ));
            }

        /**
         * Retrieve the current tranformed geometry of a registered
         * window.
         */
        QRectF transformedGeometry( EffectWindow *w ) const;
        /**
         * Retrieve the current target geometry of a registered
         * window.
         */
        QRectF targetGeometry( EffectWindow *w ) const;
        /**
         * Return the window that has its transformed geometry under
         * the specified point. It is recommended to use the stacking
         * order as it's what the user sees, but it is slightly
         * slower to process.
         */
        EffectWindow* windowAtPoint( QPoint point, bool useStackingOrder = true ) const;

        /**
         * Return a list of all currently registered windows.
         */
        inline EffectWindowList managedWindows() const { return m_managedWindows.keys(); }
        /**
         * Returns whether or not a specified window is being managed
         * by this manager object.
         */
        inline bool isManaging( EffectWindow *w ) { return m_managedWindows.contains( w ); }
        /**
         * Returns whether or not this manager object is actually
         * managing any windows or not.
         */
        inline bool managingWindows() { return !m_managedWindows.empty(); }
        /**
         * Returns whether all windows have reached their targets yet
         * or not. Can be used to see if an effect should be
         * processed and displayed or not.
         */
        inline bool areWindowsMoving() { return !m_movingWindowsSet.isEmpty(); }

    private:
        bool m_useGlobalAnimationModifier;
        struct WindowMotion
            { // TODO: Rotation, etc?
            Motion2D translation; // Absolute position
            Motion2D scale; // xScale and yScale
            };
        QHash<EffectWindow*, WindowMotion> m_managedWindows;
        QSet<EffectWindow*> m_movingWindowsSet;
    };

/**
 * @short Helper class for displaying text and icons in frames.
 *
 * Paints text and/or and icon with an optional frame around them. The
 * available frames includes one that follows the default Plasma theme and
 * another that doesn't.
 * It is recommended to use this class whenever displaying text.
 */
class KWIN_EXPORT EffectFrame : public QObject
    {
    Q_OBJECT

    public:
        enum Style
            {
            None, ///< Displays no frame around the contents.
            Unstyled, ///< Displays a basic box around the contents.
            Styled ///< Displays a Plasma-styled frame around the contents.
            };

        /**
         * Creates a new frame object. If the frame does not have a static size
         * then it will be located at @a position with @a alignment. A
         * non-static frame will automatically adjust its size to fit the
         * contents.
         */
        explicit EffectFrame( Style style, bool staticSize = true, QPoint position = QPoint( -1, -1 ),
            Qt::Alignment alignment = Qt::AlignCenter );
        ~EffectFrame();

        /**
         * Delete any existing textures to free up graphics memory. They will
         * be automatically recreated the next time they are required.
         */
        void free();

        /**
         * Render the frame.
         */
        void render( QRegion region = infiniteRegion(), double opacity = 1.0, double frameOpacity = 1.0 );

        void setPosition( const QPoint& point );
        /**
         * Set the text alignment for static frames and the position alignment
         * for non-static.
         */
        inline void setAlignment( Qt::Alignment alignment )
            { m_alignment = alignment; }; // Doesn't change geometry
        void setGeometry( const QRect& geometry, bool force = false );
        inline QRect geometry() const // Inner/contents geometry
            { return m_geometry; };

        void setText( const QString& text );
        inline QString text() const
            { return m_text; };
        void setFont( const QFont& font );
        inline QFont font() const
            { return m_font; };
        /**
         * Set the icon that will appear on the left-hand size of the frame.
         */
        void setIcon( const QPixmap& icon );
        inline QPixmap icon() const
            { return m_icon; };
        void setIconSize( const QSize& size );
        inline QSize iconSize() const
            { return m_iconSize; };

        /**
         * The foreground text color as specified by the default Plasma theme.
         */
        static QColor styledTextColor();

        /**
         * Clean up all static texture data. Called when compositing is being disabled.
         * @internal
         */
        static void cleanup();

    private Q_SLOTS:
        void plasmaThemeChanged();

    private:
        Q_DISABLE_COPY( EffectFrame ) // As we need to use Qt slots we cannot copy this class

        void autoResize(); // Auto-resize if not a static size
        void updateTexture(); // Update OpenGL styled frame texture
        void updateTextTexture(); // Update OpenGL text texture
        void updatePicture(); // Update XRender styled frame picture
        void updateTextPicture(); // Update XRender text picture

        Style m_style;
        Plasma::FrameSvg m_frame;
        GLTexture* m_texture;
        GLTexture* m_textTexture;
        XRenderPicture* m_picture;
        XRenderPicture* m_textPicture;

        // Position
        bool m_static;
        QPoint m_point;
        Qt::Alignment m_alignment;
        QRect m_geometry;

        // Contents
        QString m_text;
        QFont m_font;
        QPixmap m_icon;
        QSize m_iconSize;

        static GLTexture* m_unstyledTexture;
        static void updateUnstyledTexture(); // Update OpenGL unstyled frame texture
    };

/**
 * Pointer to the global EffectsHandler object.
 **/
extern KWIN_EXPORT EffectsHandler* effects;

/***************************************************************
 EffectsHandler
***************************************************************/

inline
bool EffectsHandler::paintText( const QString& text, const QPoint& center, int maxwidth,
        const QColor& color, const QFont& font )
{
    return paintText( text, QRect( center.x() - maxwidth / 2, center.y() - 5000, maxwidth, 10000 ),
        color, font, Qt::AlignCenter );
}

inline
bool EffectsHandler::paintTextWithBackground( const QString& text, const QPoint& center, int maxwidth,
        const QColor& color, const QColor& bgcolor, const QFont& font )
{
    return paintTextWithBackground( text,
        QRect( center.x() - maxwidth / 2, center.y() - 5000, maxwidth, 10000 ),
        color, bgcolor, font, Qt::AlignCenter );
}

/***************************************************************
 WindowVertex
***************************************************************/

inline
WindowVertex::WindowVertex()
    : px( 0 ), py( 0 ), tx( 0 ), ty( 0 )
    {
    }

inline
WindowVertex::WindowVertex( double _x, double _y, double _tx, double _ty )
    : px( _x ), py( _y ), ox( _x ), oy( _y ), tx( _tx ), ty( _ty )
    {
    }

inline
double WindowVertex::x() const
    {
    return px;
    }

inline
double WindowVertex::y() const
    {
    return py;
    }

inline
double WindowVertex::originalX() const
    {
    return ox;
    }

inline
double WindowVertex::originalY() const
    {
    return oy;
    }

inline
double WindowVertex::textureX() const
    {
    return tx;
    }

inline
double WindowVertex::textureY() const
    {
    return ty;
    }

inline
void WindowVertex::move( double x, double y )
    {
    px = x;
    py = y;
    }

inline
void WindowVertex::setX( double x )
    {
    px = x;
    }

inline
void WindowVertex::setY( double y )
    {
    py = y;
    }

/***************************************************************
 WindowQuad
***************************************************************/

inline
WindowQuad::WindowQuad( WindowQuadType t, int id )
    : quadType( t )
    , quadID( id )
    {
    }

inline
WindowVertex& WindowQuad::operator[]( int index )
    {
    assert( index >= 0 && index < 4 );
    return verts[ index ];
    }

inline
const WindowVertex& WindowQuad::operator[]( int index ) const
    {
    assert( index >= 0 && index < 4 );
    return verts[ index ];
    }

inline
WindowQuadType WindowQuad::type() const
    {
    assert( quadType != WindowQuadError );
    return quadType;
    }

inline
int WindowQuad::id() const
    {
    return quadID;
    }

inline
bool WindowQuad::decoration() const
    {
    assert( quadType != WindowQuadError );
    return quadType == WindowQuadDecoration;
    }

inline
bool WindowQuad::effect() const
    {
    assert( quadType != WindowQuadError );
    return quadType >= EFFECT_QUAD_TYPE_START;
    }

inline
bool WindowQuad::isTransformed() const
    {
    return !( verts[ 0 ].px == verts[ 0 ].ox && verts[ 0 ].py == verts[ 0 ].oy
        && verts[ 1 ].px == verts[ 1 ].ox && verts[ 1 ].py == verts[ 1 ].oy
        && verts[ 2 ].px == verts[ 2 ].ox && verts[ 2 ].py == verts[ 2 ].oy
        && verts[ 3 ].px == verts[ 3 ].ox && verts[ 3 ].py == verts[ 3 ].oy );
    }

inline
double WindowQuad::left() const
    {
    return qMin( verts[ 0 ].px, qMin( verts[ 1 ].px, qMin( verts[ 2 ].px, verts[ 3 ].px )));
    }

inline
double WindowQuad::right() const
    {
    return qMax( verts[ 0 ].px, qMax( verts[ 1 ].px, qMax( verts[ 2 ].px, verts[ 3 ].px )));
    }

inline
double WindowQuad::top() const
    {
    return qMin( verts[ 0 ].py, qMin( verts[ 1 ].py, qMin( verts[ 2 ].py, verts[ 3 ].py )));
    }

inline
double WindowQuad::bottom() const
    {
    return qMax( verts[ 0 ].py, qMax( verts[ 1 ].py, qMax( verts[ 2 ].py, verts[ 3 ].py )));
    }

inline
double WindowQuad::originalLeft() const
    {
    return verts[ 0 ].ox;
    }

inline
double WindowQuad::originalRight() const
    {
    return verts[ 2 ].ox;
    }

inline
double WindowQuad::originalTop() const
    {
    return verts[ 0 ].oy;
    }

inline
double WindowQuad::originalBottom() const
    {
    return verts[ 2 ].oy;
    }

/***************************************************************
 Motion
***************************************************************/

template <typename T>
Motion<T>::Motion( T initial, double strength, double smoothness )
    :   m_value( initial )
    ,   m_target( initial )
    ,   m_velocity()
    ,   m_strength( strength )
    ,   m_smoothness( smoothness )
    {
    }

template <typename T>
Motion<T>::Motion( const Motion &other )
    :   m_value( other.value() )
    ,   m_target( other.target() )
    ,   m_velocity( other.velocity() )
    ,   m_strength( other.strength() )
    ,   m_smoothness( other.smoothness() )
    {
    }

template <typename T>
Motion<T>::~Motion()
    {
    }

template <typename T>
void Motion<T>::calculate( const int msec )
    {
    if( m_value == m_target && m_velocity == T() ) // At target and not moving
        return;

    // Poor man's time independent calculation
    int steps = qMax( 1, msec / 5 );
    for( int i = 0; i < steps; i++ )
        {
        T diff = m_target - m_value;
        T strength = diff * m_strength;
        m_velocity = ( m_smoothness * m_velocity + strength ) / ( m_smoothness + 1.0 );
        m_value += m_velocity;
        }
    }

template <typename T>
void Motion<T>::finish()
    {
    m_value = m_target;
    m_velocity = T();
    }

} // namespace

/** @} */

#endif // KWINEFFECTS_H
