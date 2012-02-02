/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#include <QtCore/QPair>
#include <QtCore/QSet>
#include <QtCore/QRect>
#include <QtGui/QRegion>

#include <QtCore/QVector>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QStack>

#include <KDE/KPluginFactory>
#include <KDE/KShortcutsEditor>

#include <assert.h>
#include <limits.h>
#include <netwm.h>

class KLibrary;
class KConfigGroup;
class KActionCollection;
class QFont;
class QKeyEvent;

namespace KWin
{

class EffectWindow;
class EffectWindowGroup;
class EffectFrame;
class EffectFramePrivate;
class Effect;
class WindowQuad;
class GLShader;
class XRenderPicture;
class RotationData;
class WindowQuadList;
class WindowPrePaintData;
class WindowPaintData;
class ScreenPrePaintData;
class ScreenPaintData;

typedef QPair< QString, Effect* > EffectPair;
typedef QPair< Effect*, Window > InputWindowPair;
typedef QList< KWin::EffectWindow* > EffectWindowList;


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
#define KWIN_EFFECT_API_VERSION_MINOR 182
#define KWIN_EFFECT_API_VERSION KWIN_EFFECT_API_MAKE_VERSION( \
        KWIN_EFFECT_API_VERSION_MAJOR, KWIN_EFFECT_API_VERSION_MINOR )

enum WindowQuadType {
    WindowQuadError, // for the stupid default ctor
    WindowQuadContents,
    WindowQuadDecoration,
    // Shadow Quad types
    WindowQuadShadowTop,
    WindowQuadShadowTopRight,
    WindowQuadShadowRight,
    WindowQuadShadowBottomRight,
    WindowQuadShadowBottom,
    WindowQuadShadowBottomLeft,
    WindowQuadShadowLeft,
    WindowQuadShadowTopLeft,
    EFFECT_QUAD_TYPE_START = 100 ///< @internal
};

/**
 * EffectWindow::setData() and EffectWindow::data() global roles.
 * All values between 0 and 999 are reserved for global roles.
 */
enum DataRole {
    // Grab roles are used to force all other animations to ignore the window.
    // The value of the data is set to the Effect's `this` value.
    WindowAddedGrabRole = 1,
    WindowClosedGrabRole,
    WindowMinimizedGrabRole,
    WindowUnminimizedGrabRole,
    WindowForceBlurRole, ///< For fullscreen effects to enforce blurring of windows,
    WindowBlurBehindRole, ///< For single windows to blur behind
    LanczosCacheRole
};

/**
 * Style types used by @ref EffectFrame.
 * @since 4.6
 */
enum EffectFrameStyle {
    EffectFrameNone, ///< Displays no frame around the contents.
    EffectFrameUnstyled, ///< Displays a basic box around the contents.
    EffectFrameStyled ///< Displays a Plasma-styled frame around the contents.
};

/**
 * Infinite region (i.e. a special region type saying that everything needs to be painted).
 */
KWIN_EXPORT inline
QRect infiniteRegion()
{
    // INT_MIN / 2 because width/height is used (INT_MIN+INT_MAX==-1)
    return QRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
}

/**
 * @short Base class for all KWin effects
 *
 * This is the base class for all effects. By reimplementing virtual methods
 *  of this class, you can customize how the windows are painted.
 *
 * The virtual methods are used for painting and need to be implemented for
 * custom painting.
 *
 * In order to react to state changes (e.g. a window gets closed) the effect
 * should provide slots for the signals emitted by the EffectsHandler.
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
class KWIN_EXPORT Effect : public QObject
{
    Q_OBJECT
public:
    /** Flags controlling how painting is done. */
    // TODO: is that ok here?
    enum {
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
        // PAINT_DECORATION_ONLY = 1 << 7 has been deprecated
        /**
         * Window will be painted with a lanczos filter.
         **/
        PAINT_WINDOW_LANCZOS = 1 << 8
        // PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS = 1 << 9 has been removed
    };

    enum Feature {
        Nothing = 0, Resize, GeometryTip,
        Outline
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
    enum ReconfigureFlag {
        ReconfigureAll = 1 << 0 /// Everything needs to be reconfigured.
    };
    Q_DECLARE_FLAGS(ReconfigureFlags, ReconfigureFlag)

    /**
     * Called when configuration changes (either the effect's or KWin's global).
     */
    virtual void reconfigure(ReconfigureFlags flags);

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
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    /**
     * In this method you can:
     * @li paint something on top of the windows (by painting after calling
     *      effects->paintScreen())
     * @li paint multiple desktops and/or multiple copies of the same desktop
     *      by calling effects->paintScreen() multiple times
     **/
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
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
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    /**
     * This is the main method for painting windows.
     * In this method you can:
     * @li do various transformations
     * @li change opacity of the window
     * @li change brightness and/or saturation, if it's supported
     **/
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    /**
     * Called for every window after all painting has been finished.
     * In this method you can:
     * @li schedule next repaint for individual window(s) in case of animations
     * You shouldn't paint anything here.
     **/
    virtual void postPaintWindow(EffectWindow* w);

    /**
     * This method is called directly before painting an @ref EffectFrame.
     * You can implement this method if you need to bind a shader or perform
     * other operations before the frame is rendered.
     * @param frame The EffectFrame which will be rendered
     * @param region Region to restrict painting to
     * @param opacity Opacity of text/icon
     * @param frameOpacity Opacity of background
     * @since 4.6
     **/
    virtual void paintEffectFrame(EffectFrame* frame, QRegion region, double opacity, double frameOpacity);

    /**
     * Called on Transparent resizes.
     * return true if your effect substitutes the XOR rubberband
    */
    virtual bool provides(Feature);

    /**
     * Can be called to draw multiple copies (e.g. thumbnails) of a window.
     * You can change window's opacity/brightness/etc here, but you can't
     *  do any transformations
     **/
    virtual void drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);

    /**
     * Define new window quads so that they can be transformed by other effects.
     * It's up to the effect to keep track of them.
     **/
    virtual void buildQuads(EffectWindow* w, WindowQuadList& quadList);

    virtual void windowInputMouseEvent(Window w, QEvent* e);
    virtual void grabbedKeyboardEvent(QKeyEvent* e);

    virtual bool borderActivated(ElectricBorder border);

    /**
     * Overwrite this method to indicate whether your effect will be doing something in
     * the next frame to be rendered. If the method returns @c false the effect will be
     * excluded from the chained methods in the next rendered frame.
     *
     * This method is called always directly before the paint loop begins. So it is totally
     * fine to e.g. react on a window event, issue a repaint to trigger an animation and
     * change a flag to indicate that this method returns @c true.
     *
     * As the method is called each frame, you should not perform complex calculations.
     * Best use just a boolean flag.
     *
     * The default implementation of this method returns @c true.
     * @since 4.8
     **/
    virtual bool isActive() const;

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
    static double animationTime(const KConfigGroup& cfg, const QString& key, int defaultTime);
    /**
     * @overload Use this variant if the animation time is hardcoded and not configurable
     * in the effect itself.
     */
    static double animationTime(int defaultTime);
    /**
     * Linearly interpolates between @p x and @p y.
     *
     * Returns @p x when @p a = 0; returns @p y when @p a = 1.
     **/
    static double interpolate(double x, double y, double a) {
        return x * (1 - a) + y * a;
    }
    /** Helper to set WindowPaintData and QRegion to necessary transformations so that
     * a following drawWindow() would put the window at the requested geometry (useful for thumbnails)
     **/
    static void setPositionTransformations(WindowPaintData& data, QRect& region, EffectWindow* w,
                                           const QRect& r, Qt::AspectRatioMode aspect);
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
 * Defines the function used to check whether an effect should be enabled by default
 *
 * This function provides a way for an effect to override the default at runtime,
 * e.g. based on the capabilities of the hardware.
 *
 * This function is optional; the effect doesn't have to provide it.
 *
 * Note that this function is only called if the supported() function returns true,
 * and if X-KDE-PluginInfo-EnabledByDefault is set to true in the .desktop file.
 *
 * Example:  KWIN_EFFECT_ENABLEDBYDEFAULT(flames, MyFlameEffect::enabledByDefault())
 **/
#define KWIN_EFFECT_ENABLEDBYDEFAULT(name, function) \
    extern "C" { \
        KWIN_EXPORT bool effect_enabledbydefault_kwin4_effect_##name() { return function; } \
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
class KWIN_EXPORT EffectsHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentDesktop READ currentDesktop WRITE setCurrentDesktop NOTIFY desktopChanged)
    Q_PROPERTY(QString currentActivity READ currentActivity)
    Q_PROPERTY(KWin::EffectWindow *activeWindow READ activeWindow WRITE activateWindow NOTIFY windowActivated)
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize)
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth)
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight)
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)
    /**
     * The number of desktops currently used. Minimum number of desktops is 1, maximum 20.
     **/
    Q_PROPERTY(int desktops READ numberOfDesktops WRITE setNumberOfDesktops NOTIFY numberDesktopsChanged)
    Q_PROPERTY(bool optionRollOverDesktops READ optionRollOverDesktops)
    Q_PROPERTY(int activeScreen READ activeScreen)
    Q_PROPERTY(int numScreens READ numScreens)
    /**
     * Factor by which animation speed in the effect should be modified (multiplied).
     * If configurable in the effect itself, the option should have also 'default'
     * animation speed. The actual value should be determined using animationTime().
     * Note: The factor can be also 0, so make sure your code can cope with 0ms time
     * if used manually.
     */
    Q_PROPERTY(qreal animationTimeFactor READ animationTimeFactor)
    Q_PROPERTY(QList< KWin::EffectWindow* > stackingOrder READ stackingOrder)
    /**
     * Whether window decorations use the alpha channel.
     **/
    Q_PROPERTY(bool decorationsHaveAlpha READ decorationsHaveAlpha)
    /**
     * Whether the window decorations support blurring behind the decoration.
     **/
    Q_PROPERTY(bool decorationSupportsBlurBehind READ decorationSupportsBlurBehind)
    Q_PROPERTY(CompositingType compositingType READ compositingType CONSTANT)
    Q_PROPERTY(QPoint cursorPos READ cursorPos)
    friend class Effect;
public:
    EffectsHandler(CompositingType type);
    virtual ~EffectsHandler();
    // for use by effects
    virtual void prePaintScreen(ScreenPrePaintData& data, int time) = 0;
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data) = 0;
    virtual void postPaintScreen() = 0;
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) = 0;
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) = 0;
    virtual void postPaintWindow(EffectWindow* w) = 0;
    virtual void paintEffectFrame(EffectFrame* frame, QRegion region, double opacity, double frameOpacity) = 0;
    virtual void drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) = 0;
    virtual void buildQuads(EffectWindow* w, WindowQuadList& quadList) = 0;
    virtual QVariant kwinOption(KWinOption kwopt) = 0;
    // Functions for handling input - e.g. when an Expose-like effect is shown, an input window
    // covering the whole screen is created and all mouse events will be intercepted by it.
    // The effect's windowInputMouseEvent() will get called with such events.
    virtual Window createInputWindow(Effect* e, int x, int y, int w, int h, const QCursor& cursor) = 0;
    Window createInputWindow(Effect* e, const QRect& r, const QCursor& cursor);
    virtual Window createFullScreenInputWindow(Effect* e, const QCursor& cursor);
    virtual void destroyInputWindow(Window w) = 0;
    virtual QPoint cursorPos() const = 0;
    virtual bool grabKeyboard(Effect* effect) = 0;
    virtual void ungrabKeyboard() = 0;

    /**
     * Retrieve the proxy class for an effect if it has one. Will return NULL if
     * the effect isn't loaded or doesn't have a proxy class.
     */
    virtual void* getProxy(QString name) = 0;

    // Mouse polling
    virtual void startMousePolling() = 0;
    virtual void stopMousePolling() = 0;

    virtual void checkElectricBorder(const QPoint &pos, Time time) = 0;
    virtual void reserveElectricBorder(ElectricBorder border) = 0;
    virtual void unreserveElectricBorder(ElectricBorder border) = 0;
    virtual void reserveElectricBorderSwitching(bool reserve) = 0;

    // functions that allow controlling windows/desktop
    virtual void activateWindow(KWin::EffectWindow* c) = 0;
    virtual KWin::EffectWindow* activeWindow() const = 0 ;
    Q_SCRIPTABLE virtual void moveWindow(KWin::EffectWindow* w, const QPoint& pos, bool snap = false, double snapAdjust = 1.0) = 0;
    Q_SCRIPTABLE virtual void windowToDesktop(KWin::EffectWindow* w, int desktop) = 0;
    Q_SCRIPTABLE virtual void windowToScreen(KWin::EffectWindow* w, int screen) = 0;
    virtual void setShowingDesktop(bool showing) = 0;


    // Activities
    /**
     * @returns The ID of the current activity.
     */
    virtual QString currentActivity() const = 0;
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
    virtual void setCurrentDesktop(int desktop) = 0;
    /**
    * Sets the total number of desktops to @a desktops.
    */
    virtual void setNumberOfDesktops(int desktops) = 0;
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
    virtual int desktopAtCoords(QPoint coords) const = 0;
    /**
     * @returns The coords of desktop @a id in grid units.
     */
    virtual QPoint desktopGridCoords(int id) const = 0;
    /**
     * @returns The coords of the top-left corner of desktop @a id in pixels.
     */
    virtual QPoint desktopCoords(int id) const = 0;
    /**
     * @returns The ID of the desktop above desktop @a id. Wraps around to the bottom of
     * the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE virtual int desktopAbove(int desktop = 0, bool wrap = true) const = 0;
    /**
     * @returns The ID of the desktop to the right of desktop @a id. Wraps around to the
     * left of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE virtual int desktopToRight(int desktop = 0, bool wrap = true) const = 0;
    /**
     * @returns The ID of the desktop below desktop @a id. Wraps around to the top of the
     * layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE virtual int desktopBelow(int desktop = 0, bool wrap = true) const = 0;
    /**
     * @returns The ID of the desktop to the left of desktop @a id. Wraps around to the
     * right of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE virtual int desktopToLeft(int desktop = 0, bool wrap = true) const = 0;
    Q_SCRIPTABLE virtual QString desktopName(int desktop) const = 0;
    virtual bool optionRollOverDesktops() const = 0;

    virtual int activeScreen() const = 0; // Xinerama
    virtual int numScreens() const = 0; // Xinerama
    Q_SCRIPTABLE virtual int screenNumber(const QPoint& pos) const = 0;   // Xinerama
    virtual QRect clientArea(clientAreaOption, int screen, int desktop) const = 0;
    virtual QRect clientArea(clientAreaOption, const EffectWindow* c) const = 0;
    virtual QRect clientArea(clientAreaOption, const QPoint& p, int desktop) const = 0;
    /**
     * Factor by which animation speed in the effect should be modified (multiplied).
     * If configurable in the effect itself, the option should have also 'default'
     * animation speed. The actual value should be determined using animationTime().
     * Note: The factor can be also 0, so make sure your code can cope with 0ms time
     * if used manually.
     */
    virtual double animationTimeFactor() const = 0;
    virtual WindowQuadType newWindowQuadType() = 0;

    Q_SCRIPTABLE virtual KWin::EffectWindow* findWindow(WId id) const = 0;
    virtual EffectWindowList stackingOrder() const = 0;
    // window will be temporarily painted as if being at the top of the stack
    virtual void setElevatedWindow(EffectWindow* w, bool set) = 0;

    virtual void setTabBoxWindow(EffectWindow*) = 0;
    virtual void setTabBoxDesktop(int) = 0;
    virtual EffectWindowList currentTabBoxWindowList() const = 0;
    virtual void refTabBox() = 0;
    virtual void unrefTabBox() = 0;
    virtual void closeTabBox() = 0;
    virtual QList< int > currentTabBoxDesktopList() const = 0;
    virtual int currentTabBoxDesktop() const = 0;
    virtual EffectWindow* currentTabBoxWindow() const = 0;

    virtual void setActiveFullScreenEffect(Effect* e) = 0;
    virtual Effect* activeFullScreenEffect() const = 0;

    /**
     * Schedules the entire workspace to be repainted next time.
     * If you call it during painting (including prepaint) then it does not
     *  affect the current painting.
     **/
    Q_SCRIPTABLE virtual void addRepaintFull() = 0;
    Q_SCRIPTABLE virtual void addRepaint(const QRect& r) = 0;
    Q_SCRIPTABLE virtual void addRepaint(const QRegion& r) = 0;
    Q_SCRIPTABLE virtual void addRepaint(int x, int y, int w, int h) = 0;

    CompositingType compositingType() const;
    virtual unsigned long xrenderBufferPicture() = 0;
    virtual void reconfigure() = 0;

    /**
     Makes KWin core watch PropertyNotify events for the given atom,
     or stops watching if reg is false (must be called the same number
     of times as registering). Events are sent using Effect::propertyNotify().
     Note that even events that haven't been registered for can be received.
    */
    virtual void registerPropertyType(long atom, bool reg) = 0;
    virtual QByteArray readRootProperty(long atom, long type, int format) const = 0;
    virtual void deleteRootProperty(long atom) const = 0;

    /**
     * Returns @a true if the active window decoration has shadow API hooks.
     */
    virtual bool hasDecorationShadows() const = 0;

    /**
     * Returns @a true if the window decorations use the alpha channel, and @a false otherwise.
     * @since 4.5
     */
    virtual bool decorationsHaveAlpha() const = 0;

    /**
     * Returns @a true if the window decorations support blurring behind the decoration, and @a false otherwise
     * @since 4.6
     */
    virtual bool decorationSupportsBlurBehind() const = 0;

    /**
     * Creates a new frame object. If the frame does not have a static size
     * then it will be located at @a position with @a alignment. A
     * non-static frame will automatically adjust its size to fit the contents.
     * @returns A new @ref EffectFrame. It is the responsibility of the caller to delete the
     * EffectFrame.
     * @since 4.6
     */
    virtual EffectFrame* effectFrame(EffectFrameStyle style, bool staticSize = true,
                                     const QPoint& position = QPoint(-1, -1), Qt::Alignment alignment = Qt::AlignCenter) const = 0;

    /**
     * Allows an effect to trigger a reload of itself.
     * This can be used by an effect which needs to be reloaded when screen geometry changes.
     * It is possible that the effect cannot be loaded again as it's supported method does no longer
     * hold.
     * @param effect The effect to reload
     * @since 4.8
     **/
    virtual void reloadEffect(Effect *effect) = 0;

    /**
     * Sends message over DCOP to reload given effect.
     * @param effectname effect's name without "kwin4_effect_" prefix.
     * Can be called from effect's config module to apply config changes.
     **/
    static void sendReloadMessage(const QString& effectname);
    /**
     * @return @ref KConfigGroup which holds given effect's config options
     **/
    static KConfigGroup effectConfig(const QString& effectname);

Q_SIGNALS:
    /**
     * Signal emitted when the current desktop changed.
     * @param oldDesktop The previously current desktop
     * @param newDesktop The new current desktop
     * @since 4.7
     **/
    void desktopChanged(int oldDesktop, int newDesktop);
    /**
    * Signal emitted when the number of currently existing desktops is changed.
    * @param old The previous number of desktops in used.
    * @see EffectsHandler::numberOfDesktops.
    * @since 4.7
    */
    void numberDesktopsChanged(int old);
    /**
     * Signal emitted when a new window has been added to the Workspace.
     * @param w The added window
     * @since 4.7
     **/
    void windowAdded(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window is being removed from the Workspace.
     * An effect which wants to animate the window closing should connect
     * to this signal and reference the window by using
     * @link EffectWindow::refWindow
     * @param w The window which is being closed
     * @since 4.7
     **/
    void windowClosed(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window get's activated.
     * @param w The new active window, or @c NULL if there is no active window.
     * @since 4.7
     **/
    void windowActivated(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window is deleted.
     * This means that a closed window is not referenced any more.
     * An effect bookkeeping the closed windows should connect to this
     * signal to clean up the internal references.
     * @param w The window which is going to be deleted.
     * @see EffectWindow::refWindow
     * @see EffectWindow::unrefWindow
     * @see windowClosed
     * @since 4.7
     **/
    void windowDeleted(KWin::EffectWindow *w);
    /**
     * Signal emitted when a user begins a window move or resize operation.
     * To figure out whether the user resizes or moves the window use
     * @link EffectWindow::isUserMove or @link EffectWindow::isUserResize.
     * Whenever the geometry is updated the signal @link windowStepUserMovedResized
     * is emitted with the current geometry.
     * The move/resize operation ends with the signal @link windowFinishUserMovedResized.
     * Only one window can be moved/resized by the user at the same time!
     * @param w The window which is being moved/resized
     * @see windowStepUserMovedResized
     * @see windowFinishUserMovedResized
     * @see EffectWindow::isUserMove
     * @see EffectWindow::isUserResize
     * @since 4.7
     **/
    void windowStartUserMovedResized(KWin::EffectWindow *w);
    /**
     * Signal emitted during a move/resize operation when the user changed the geometry.
     * Please note: KWin supports two operation modes. In one mode all changes are applied
     * instantly. This means the window's geometry matches the passed in @p geometry. In the
     * other mode the geometry is changed after the user ended the move/resize mode.
     * The @p geometry differs from the window's geometry. Also the window's pixmap still has
     * the same size as before. Depending what the effect wants to do it would be recommended
     * to scale/translate the window.
     * @param w The window which is being moved/resized
     * @param geometry The geometry of the window in the current move/resize step.
     * @see windowStartUserMovedResized
     * @see windowFinishUserMovedResized
     * @see EffectWindow::isUserMove
     * @see EffectWindow::isUserResize
     * @since 4.7
     **/
    void windowStepUserMovedResized(KWin::EffectWindow *w, const QRect &geometry);
    /**
     * Signal emitted when the user finishes move/resize of window @p w.
     * @param w The window which has been moved/resized
     * @see windowStartUserMovedResized
     * @see windowFinishUserMovedResized
     * @since 4.7
     **/
    void windowFinishUserMovedResized(KWin::EffectWindow *w);
    /**
     * Signal emitted when the maximized state of the window @p w changed.
     * A window can be in one of four states:
     * @li restored: both @p horizontal and @p vertical are @c false
     * @li horizontally maximized: @p horizontal is @c true and @p vertical is @c false
     * @li vertically maximized: @p horizontal is @c false and @p vertical is @c true
     * @li completely maximized: both @p horizontal and @p vertical are @C true
     * @param w The window whose maximized state changed
     * @param horizontal If @c true maximized horizontally
     * @param vertical If @c true maximized vertically
     * @since 4.7
     **/
    void windowMaximizedStateChanged(KWin::EffectWindow *w, bool horizontal, bool vertical);
    /**
     * Signal emitted when the geometry or shape of a window changed.
     * This is caused if the window changes geometry without user interaction.
     * E.g. the decoration is changed. This is in opposite to windowUserMovedResized
     * which is caused by direct user interaction.
     * @param w The window whose geometry changed
     * @param old The previous geometry
     * @see windowUserMovedResized
     * @since 4.7
     **/
    void windowGeometryShapeChanged(KWin::EffectWindow *w, const QRect &old);
    /**
     * Signal emitted when the windows opacity is changed.
     * @param w The window whose opacity level is changed.
     * @param oldOpacity The previous opacity level
     * @param newOpacity The new opacity level
     * @since 4.7
     **/
    void windowOpacityChanged(KWin::EffectWindow *w, qreal oldOpacity, qreal newOpacity);
    /**
     * Signal emitted when a window got minimized.
     * @param w The window which was minimized
     * @since 4.7
     **/
    void windowMinimized(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window got unminimized.
     * @param w The window which was unminimized
     * @since 4.7
     **/
    void windowUnminimized(KWin::EffectWindow *w);
    /**
     * Signal emitted when an area of a window is scheduled for repainting.
     * Use this signal in an effect if another area needs to be synced as well.
     * @param w The window which is scheduled for repainting
     * @param r The damaged rect
     * @since 4.7
     **/
    void windowDamaged(KWin::EffectWindow *w, const QRect &r);
    /**
     * Signal emitted when a tabbox is added.
     * An effect who wants to replace the tabbox with itself should use @link refTabBox.
     * @param mode The TabBoxMode.
     * @see refTabBox
     * @see tabBoxClosed
     * @see tabBoxUpdated
     * @see tabBoxKeyEvent
     * @since 4.7
     **/
    void tabBoxAdded(int mode);
    /**
     * Signal emitted when the TabBox was closed by KWin core.
     * An effect which referenced the TabBox should use @link unrefTabBox to unref again.
     * @see unrefTabBox
     * @see tabBoxAdded
     * @since 4.7
     **/
    void tabBoxClosed();
    /**
     * Signal emitted when the selected TabBox window changed or the TabBox List changed.
     * An effect should only response to this signal if it referenced the TabBox with @link refTabBox.
     * @see refTabBox
     * @see currentTabBoxWindowList
     * @see currentTabBoxDesktopList
     * @see currentTabBoxWindow
     * @see currentTabBoxDesktop
     * @since 4.7
     **/
    void tabBoxUpdated();
    /**
     * Signal emitted when a key event, which is not handled by TabBox directly is, happens while
     * TabBox is active. An effect might use the key event to e.g. change the selected window.
     * An effect should only response to this signal if it referenced the TabBox with @link refTabBox.
     * @param event The key event not handled by TabBox directly
     * @see refTabBox
     * @since 4.7
     **/
    void tabBoxKeyEvent(QKeyEvent* event);
    void currentTabAboutToChange(KWin::EffectWindow* from, KWin::EffectWindow* to);
    void tabAdded(KWin::EffectWindow* from, KWin::EffectWindow* to);   // from merged with to
    void tabRemoved(KWin::EffectWindow* c, KWin::EffectWindow* group);   // c removed from group
    /**
     * Signal emitted when mouse changed.
     * If an effect needs to get updated mouse positions, it needs to first call @link startMousePolling.
     * For a fullscreen effect it is better to use an input window and react on @link windowInputMouseEvent.
     * @param pos The new mouse position
     * @param oldpos The previously mouse position
     * @param buttons The pressed mouse buttons
     * @param oldbuttons The previously pressed mouse buttons
     * @param modifiers Pressed keyboard modifiers
     * @param oldmodifiers Previously pressed keyboard modifiers.
     * @see startMousePolling
     * @since 4.7
     **/
    void mouseChanged(const QPoint& pos, const QPoint& oldpos,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    /**
     * Receives events registered for using @link registerPropertyType.
     * Use readProperty() to get the property data.
     * Note that the property may be already set on the window, so doing the same
     * processing from windowAdded() (e.g. simply calling propertyNotify() from it)
     * is usually needed.
     * @param w The window whose property changed, is @c null if it is a root window property
     * @param atom The property
     * @since 4.7
     */
    void propertyNotify(KWin::EffectWindow* w, long atom);
    /**
     * Requests to show an outline. An effect providing to show an outline should
     * connect to the signal and render an outline.
     * The outline should be shown till the signal is emitted again with a new
     * geometry or the @link hideOutline signal is emitted.
     * @param outline The geometry of the outline to render.
     * @see hideOutline
     * @since 4.7
     **/
    void showOutline(const QRect& outline);
    /**
     * Signal emitted when the outline should no longer be shown.
     * @see showOutline
     * @since 4.7
     **/
    void hideOutline();

    /**
     * Signal emitted after the screen geometry changed (e.g. add of a monitor).
     * Effects using displayWidth()/displayHeight() to cache information should
     * react on this signal and update the caches.
     * @param size The new screen size
     * @since 4.8
     **/
    void screenGeometryChanged(const QSize &size);

protected:
    QVector< EffectPair > loaded_effects;
    QHash< QString, KLibrary* > effect_libraries;
    QList< InputWindowPair > input_windows;
    //QHash< QString, EffectFactory* > effect_factories;
    CompositingType compositing_type;
};


/**
 * @short Representation of a window used by/for Effect classes.
 *
 * The purpose is to hide internal data and also to serve as a single
 *  representation for the case when Client/Unmanaged becomes Deleted.
 **/
class KWIN_EXPORT EffectWindow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool alpha READ hasAlpha CONSTANT)
    Q_PROPERTY(QRect geometry READ geometry)
    Q_PROPERTY(int height READ height)
    Q_PROPERTY(qreal opacity READ opacity)
    Q_PROPERTY(QPoint pos READ pos)
    Q_PROPERTY(int screen READ screen)
    Q_PROPERTY(QSize size READ size)
    Q_PROPERTY(int width READ width)
    Q_PROPERTY(int x READ x)
    Q_PROPERTY(int y READ y)
    Q_PROPERTY(int desktop READ desktop)
    Q_PROPERTY(bool onAllDesktops READ isOnAllDesktops)
    Q_PROPERTY(bool onCurrentDesktop READ isOnCurrentDesktop)
    Q_PROPERTY(QRect rect READ rect)
    Q_PROPERTY(QString windowClass READ windowClass)
    Q_PROPERTY(QString windowRole READ windowRole)
    /**
     * Returns whether the window is a desktop background window (the one with wallpaper).
     * See _NET_WM_WINDOW_TYPE_DESKTOP at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool desktopWindow READ isDesktop)
    /**
     * Returns whether the window is a dock (i.e. a panel).
     * See _NET_WM_WINDOW_TYPE_DOCK at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dock READ isDock)
    /**
     * Returns whether the window is a standalone (detached) toolbar window.
     * See _NET_WM_WINDOW_TYPE_TOOLBAR at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool toolbar READ isToolbar)
    /**
     * Returns whether the window is a torn-off menu.
     * See _NET_WM_WINDOW_TYPE_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool menu READ isMenu)
    /**
     * Returns whether the window is a "normal" window, i.e. an application or any other window
     * for which none of the specialized window types fit.
     * See _NET_WM_WINDOW_TYPE_NORMAL at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool normalWindow READ isNormalWindow)
    /**
     * Returns whether the window is a dialog window.
     * See _NET_WM_WINDOW_TYPE_DIALOG at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dialog READ isDialog)
    /**
     * Returns whether the window is a splashscreen. Note that many (especially older) applications
     * do not support marking their splash windows with this type.
     * See _NET_WM_WINDOW_TYPE_SPLASH at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool splash READ isSplash)
    /**
     * Returns whether the window is a utility window, such as a tool window.
     * See _NET_WM_WINDOW_TYPE_UTILITY at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool utility READ isUtility)
    /**
     * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
     * from the applications menubar).
     * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dropdownMenu READ isDropdownMenu)
    /**
     * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
     * See _NET_WM_WINDOW_TYPE_POPUP_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool popupMenu READ isPopupMenu)
    /**
     * Returns whether the window is a tooltip.
     * See _NET_WM_WINDOW_TYPE_TOOLTIP at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool tooltip READ isTooltip)
    /**
     * Returns whether the window is a window with a notification.
     * See _NET_WM_WINDOW_TYPE_NOTIFICATION at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool notification READ isNotification)
    /**
     * Returns whether the window is a combobox popup.
     * See _NET_WM_WINDOW_TYPE_COMBO at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool comboBox READ isComboBox)
    /**
     * Returns whether the window is a Drag&Drop icon.
     * See _NET_WM_WINDOW_TYPE_DND at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dndIcon READ isDNDIcon)
    /**
     * Returns the NETWM window type
     * See http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(int windowType READ windowType)
    /**
     * Whether this EffectWindow is managed by KWin (it has control over its placement and other
     * aspects, as opposed to override-redirect windows that are entirely handled by the application).
     **/
    Q_PROPERTY(bool managed READ isManaged)
    /**
     * Whether this EffectWindow represents an already deleted window and only kept for the compositor for animations.
     **/
    Q_PROPERTY(bool deleted READ isDeleted)
    /**
     * Whether the window has an own shape
     **/
    Q_PROPERTY(bool shaped READ hasOwnShape)
    /**
     * The Window's shape
     **/
    Q_PROPERTY(QRegion shape READ shape)
    /**
     * The Caption of the window. Read from WM_NAME property together with a suffix for hostname and shortcut.
     **/
    Q_PROPERTY(QString caption READ caption)
    /**
     * Whether the window is set to be kept above other windows.
     **/
    Q_PROPERTY(bool keepAbove READ keepAbove)
    /**
     * Whether the window is minimized.
     **/
    Q_PROPERTY(bool minimized READ isMinimized WRITE setMinimized)
    /**
     * Whether the window represents a modal window.
     **/
    Q_PROPERTY(bool modal READ isModal)
    /**
     * Whether the window is moveable. Even if it is not moveable, it might be possible to move
     * it to another screen.
     * @see moveableAcrossScreens
     **/
    Q_PROPERTY(bool moveable READ isMovable)
    /**
     * Whether the window can be moved to another screen.
     * @see moveable
     **/
    Q_PROPERTY(bool moveableAcrossScreens READ isMovableAcrossScreens)
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     */
    Q_PROPERTY(QSize basicUnit READ basicUnit)
    /**
     * Whether the window is currently being moved by the user.
     **/
    Q_PROPERTY(bool move READ isUserMove)
    /**
     * Whether the window is currently being resized by the user.
     **/
    Q_PROPERTY(bool resize READ isUserResize)
    /**
     * The optional geometry representing the minimized Client in e.g a taskbar.
     * See _NET_WM_ICON_GEOMETRY at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     **/
    Q_PROPERTY(QRect iconGeometry READ iconGeometry)
    /**
     * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
     * i.e. window types that usually don't have a window frame and the user does not use window
     * management (moving, raising,...) on them.
     **/
    Q_PROPERTY(bool specialWindow READ isSpecialWindow)
    Q_PROPERTY(QPixmap icon READ icon)
    /**
     * Whether the window should be excluded from window switching effects.
     **/
    Q_PROPERTY(bool skipSwitcher READ isSkipSwitcher)
    /**
     * Geometry of the actual window contents inside the whole (including decorations) window.
     */
    Q_PROPERTY(QRect contentsRect READ contentsRect)
    /**
     * Geometry of the transparent rect in the decoration.
     * May be different from contentsRect if the decoration is extended into the client area.
     */
    Q_PROPERTY(QRect decorationInnerRect READ decorationInnerRect)
    Q_PROPERTY(bool hasDecoration READ hasDecoration)
    Q_PROPERTY(QStringList activities READ activities)
    Q_PROPERTY(bool onCurrentActivity READ isOnCurrentActivity)
    Q_PROPERTY(bool onAllActivities READ isOnAllActivities)
public:
    /**  Flags explaining why painting should be disabled  */
    enum {
        /**  Window will not be painted  */
        PAINT_DISABLED                 = 1 << 0,
        /**  Window will not be painted because it is deleted  */
        PAINT_DISABLED_BY_DELETE       = 1 << 1,
        /**  Window will not be painted because of which desktop it's on  */
        PAINT_DISABLED_BY_DESKTOP      = 1 << 2,
        /**  Window will not be painted because it is minimized  */
        PAINT_DISABLED_BY_MINIMIZE     = 1 << 3,
        /**  Window will not be painted because it is not the active window in a client group  */
        PAINT_DISABLED_BY_TAB_GROUP = 1 << 4,
        /**  Window will not be painted because it's not on the current activity  */
        PAINT_DISABLED_BY_ACTIVITY     = 1 << 5
    };

    EffectWindow(QObject *parent = NULL);
    virtual ~EffectWindow();

    virtual void enablePainting(int reason) = 0;
    virtual void disablePainting(int reason) = 0;
    virtual bool isPaintingEnabled() = 0;
    Q_SCRIPTABLE void addRepaint(const QRect& r);
    Q_SCRIPTABLE void addRepaint(int x, int y, int w, int h);
    Q_SCRIPTABLE void addRepaintFull();
    Q_SCRIPTABLE void addLayerRepaint(const QRect& r);
    Q_SCRIPTABLE void addLayerRepaint(int x, int y, int w, int h);

    virtual void refWindow() = 0;
    virtual void unrefWindow() = 0;
    bool isDeleted() const;

    bool isMinimized() const;
    double opacity() const;
    bool hasAlpha() const;

    bool isOnCurrentActivity() const;
    Q_SCRIPTABLE bool isOnActivity(QString id) const;
    bool isOnAllActivities() const;
    QStringList activities() const;

    bool isOnDesktop(int d) const;
    bool isOnCurrentDesktop() const;
    bool isOnAllDesktops() const;
    int desktop() const; // prefer isOnXXX()

    int x() const;
    int y() const;
    int width() const;
    int height() const;
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     */
    QSize basicUnit() const;
    QRect geometry() const;
    virtual QRegion shape() const = 0;
    int screen() const;
    /** @internal Do not use */
    bool hasOwnShape() const; // only for shadow effect, for now
    QPoint pos() const;
    QSize size() const;
    QRect rect() const;
    bool isMovable() const;
    bool isMovableAcrossScreens() const;
    bool isUserMove() const;
    bool isUserResize() const;
    QRect iconGeometry() const;
    /**
     * Geometry of the actual window contents inside the whole (including decorations) window.
     */
    QRect contentsRect() const;
    /**
     * Geometry of the transparent rect in the decoration.
     * May be different from contentsRect() if the decoration is extended into the client area.
     * @since 4.5
     */
    virtual QRect decorationInnerRect() const = 0;
    bool hasDecoration() const;
    virtual QByteArray readProperty(long atom, long type, int format) const = 0;
    virtual void deleteProperty(long atom) const = 0;

    QString caption() const;
    QPixmap icon() const;
    QString windowClass() const;
    QString windowRole() const;
    virtual const EffectWindowGroup* group() const = 0;

    /**
     * Returns whether the window is a desktop background window (the one with wallpaper).
     * See _NET_WM_WINDOW_TYPE_DESKTOP at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDesktop() const;
    /**
     * Returns whether the window is a dock (i.e. a panel).
     * See _NET_WM_WINDOW_TYPE_DOCK at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDock() const;
    /**
     * Returns whether the window is a standalone (detached) toolbar window.
     * See _NET_WM_WINDOW_TYPE_TOOLBAR at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isToolbar() const;
    /**
     * Returns whether the window is a torn-off menu.
     * See _NET_WM_WINDOW_TYPE_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isMenu() const;
    /**
     * Returns whether the window is a "normal" window, i.e. an application or any other window
     * for which none of the specialized window types fit.
     * See _NET_WM_WINDOW_TYPE_NORMAL at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isNormalWindow() const; // normal as in 'NET::Normal or NET::Unknown non-transient'
    /**
     * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
     * i.e. window types that usually don't have a window frame and the user does not use window
     * management (moving, raising,...) on them.
     */
    bool isSpecialWindow() const;
    /**
     * Returns whether the window is a dialog window.
     * See _NET_WM_WINDOW_TYPE_DIALOG at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDialog() const;
    /**
     * Returns whether the window is a splashscreen. Note that many (especially older) applications
     * do not support marking their splash windows with this type.
     * See _NET_WM_WINDOW_TYPE_SPLASH at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isSplash() const;
    /**
     * Returns whether the window is a utility window, such as a tool window.
     * See _NET_WM_WINDOW_TYPE_UTILITY at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isUtility() const;
    /**
     * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
     * from the applications menubar).
     * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDropdownMenu() const;
    /**
     * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
     * See _NET_WM_WINDOW_TYPE_POPUP_MENU at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isPopupMenu() const; // a context popup, not dropdown, not torn-off
    /**
     * Returns whether the window is a tooltip.
     * See _NET_WM_WINDOW_TYPE_TOOLTIP at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isTooltip() const;
    /**
     * Returns whether the window is a window with a notification.
     * See _NET_WM_WINDOW_TYPE_NOTIFICATION at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isNotification() const;
    /**
     * Returns whether the window is a combobox popup.
     * See _NET_WM_WINDOW_TYPE_COMBO at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isComboBox() const;
    /**
     * Returns whether the window is a Drag&Drop icon.
     * See _NET_WM_WINDOW_TYPE_DND at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDNDIcon() const;
    /**
     * Returns the NETWM window type
     * See http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    NET::WindowType windowType() const;
    /**
     * Returns whether the window is managed by KWin (it has control over its placement and other
     * aspects, as opposed to override-redirect windows that are entirely handled by the application).
     */
    bool isManaged() const; // whether it's managed or override-redirect
    /**
     * Returns whether or not the window can accept keyboard focus.
     */
    bool acceptsFocus() const;
    /**
     * Returns whether or not the window is kept above all other windows.
     */
    bool keepAbove() const;

    bool isModal() const;
    Q_SCRIPTABLE virtual KWin::EffectWindow* findModal() = 0;
    Q_SCRIPTABLE virtual EffectWindowList mainWindows() const = 0;

    /**
    * Returns whether the window should be excluded from window switching effects.
    * @since 4.5
    */
    bool isSkipSwitcher() const;

    /**
     * Returns the unmodified window quad list. Can also be used to force rebuilding.
     */
    virtual WindowQuadList buildQuads(bool force = false) const = 0;

    void setMinimized(bool minimize);
    void minimize();
    void unminimize();
    Q_SCRIPTABLE void closeWindow() const;

    bool isCurrentTab() const;

    /**
     * Can be used to by effects to store arbitrary data in the EffectWindow.
     */
    Q_SCRIPTABLE virtual void setData(int role, const QVariant &data) = 0;
    Q_SCRIPTABLE virtual QVariant data(int role) const = 0;
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
    GlobalShortcutsEditor(QWidget *parent);
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
    void move(double x, double y);
    void setX(double x);
    void setY(double y);
    double originalX() const;
    double originalY() const;
    double textureX() const;
    double textureY() const;
    WindowVertex();
    WindowVertex(double x, double y, double tx, double ty);
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
    explicit WindowQuad(WindowQuadType type, int id = -1);
    WindowQuad makeSubQuad(double x1, double y1, double x2, double y2) const;
    WindowVertex& operator[](int index);
    const WindowVertex& operator[](int index) const;
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
    WindowQuadList splitAtX(double x) const;
    WindowQuadList splitAtY(double y) const;
    WindowQuadList makeGrid(int maxquadsize) const;
    WindowQuadList makeRegularGrid(int xSubdivisions, int ySubdivisions) const;
    WindowQuadList select(WindowQuadType type) const;
    WindowQuadList filterOut(WindowQuadType type) const;
    bool smoothNeeded() const;
    void makeArrays(float** vertices, float** texcoords, const QSizeF &size, bool yInverted) const;
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
    WindowPaintData(EffectWindow* w);
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
    enum RotationAxis {
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
    PaintClipper(const QRegion& allowed_area);
    /**
     * Calls pop().
     */
    ~PaintClipper();
    /**
     * Allows painting only in the given area. When areas have been already
     * specified, painting is allowed only in the intersection of all areas.
     */
    static void push(const QRegion& allowed_area);
    /**
     * Removes the given area. It must match the top item in the stack.
     */
    static void pop(const QRegion& allowed_area);
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
     * for ( PaintClipper::Iterator iterator;
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
    explicit Motion(T initial, double strength, double smoothness);
    /**
     * Creates an exact copy of another motion object, including
     * position, target and velocity.
     */
    Motion(const Motion<T> &other);
    ~Motion();

    inline T value() const {
        return m_value;
    }
    inline void setValue(const T value) {
        m_value = value;
    }
    inline T target() const {
        return m_target;
    }
    inline void setTarget(const T target) {
        m_start = m_value;
        m_target = target;
    }
    inline T velocity() const {
        return m_velocity;
    }
    inline void setVelocity(const T velocity) {
        m_velocity = velocity;
    }

    inline double strength() const {
        return m_strength;
    }
    inline void setStrength(const double strength) {
        m_strength = strength;
    }
    inline double smoothness() const {
        return m_smoothness;
    }
    inline void setSmoothness(const double smoothness) {
        m_smoothness = smoothness;
    }
    inline T startValue() {
        return m_start;
    }

    /**
     * The distance between the current position and the target.
     */
    inline T distance() const {
        return m_target - m_value;
    }

    /**
     * Calculates the new position if not at the target. Called
     * once per frame only.
     */
    void calculate(const int msec);
    /**
     * Place the object on top of the target immediately,
     * bypassing all movement calculation.
     */
    void finish();

private:
    T m_value;
    T m_start;
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
    explicit Motion1D(double initial = 0.0, double strength = 0.08, double smoothness = 4.0);
    Motion1D(const Motion1D &other);
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
    explicit Motion2D(QPointF initial = QPointF(), double strength = 0.08, double smoothness = 4.0);
    Motion2D(const Motion2D &other);
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
    explicit WindowMotionManager(bool useGlobalAnimationModifier = true);
    ~WindowMotionManager();

    /**
     * Register a window for managing.
     */
    void manage(EffectWindow *w);
    /**
     * Register a list of windows for managing.
     */
    inline void manage(EffectWindowList list) {
        for (int i = 0; i < list.size(); i++)
            manage(list.at(i));
    }
    /**
     * Deregister a window. All transformations applied to the
     * window will be permanently removed and cannot be recovered.
     */
    void unmanage(EffectWindow *w);
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
    void calculate(int time);
    /**
     * Modify a registered window's paint data to make it appear
     * at its real location on the screen. Usually called in
     * paintWindow(). Remember to flag the window as having been
     * transformed in prePaintWindow() by calling
     * WindowPrePaintData::setTransformed()
     */
    void apply(EffectWindow *w, WindowPaintData &data);
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
    void reset(EffectWindow *w);

    /**
     * Ask the manager to move the window to the target position
     * with the specified scale. If `yScale` is not provided or
     * set to 0.0, `scale` will be used as the scale in the
     * vertical direction as well as in the horizontal direction.
     */
    void moveWindow(EffectWindow *w, QPoint target, double scale = 1.0, double yScale = 0.0);
    /**
     * This is an overloaded method, provided for convenience.
     *
     * Ask the manager to move the window to the target rectangle.
     * Automatically determines scale.
     */
    inline void moveWindow(EffectWindow *w, QRect target) {
        // TODO: Scale might be slightly different in the comparison due to rounding
        moveWindow(w, target.topLeft(),
                   target.width() / double(w->width()), target.height() / double(w->height()));
    }

    /**
     * Retrieve the current tranformed geometry of a registered
     * window.
     */
    QRectF transformedGeometry(EffectWindow *w) const;
    /**
     * Sets the current transformed geometry of a registered window to the given geometry.
     * @see transformedGeometry
     * @since 4.5
     */
    void setTransformedGeometry(EffectWindow *w, const QRectF &geometry);
    /**
     * Retrieve the current target geometry of a registered
     * window.
     */
    QRectF targetGeometry(EffectWindow *w) const;
    /**
     * Return the window that has its transformed geometry under
     * the specified point. It is recommended to use the stacking
     * order as it's what the user sees, but it is slightly
     * slower to process.
     */
    EffectWindow* windowAtPoint(QPoint point, bool useStackingOrder = true) const;

    /**
     * Return a list of all currently registered windows.
     */
    inline EffectWindowList managedWindows() const {
        return m_managedWindows.keys();
    }
    /**
     * Returns whether or not a specified window is being managed
     * by this manager object.
     */
    inline bool isManaging(EffectWindow *w) const {
        return m_managedWindows.contains(w);
    }
    /**
     * Returns whether or not this manager object is actually
     * managing any windows or not.
     */
    inline bool managingWindows() const {
        return !m_managedWindows.empty();
    }
    /**
     * Returns whether all windows have reached their targets yet
     * or not. Can be used to see if an effect should be
     * processed and displayed or not.
     */
    inline bool areWindowsMoving() const {
        return !m_movingWindowsSet.isEmpty();
    }
    /**
     * Returns whether a window has reached its targets yet
     * or not.
     */
    inline bool isWindowMoving(EffectWindow *w) const {
        return m_movingWindowsSet.contains(w);
    }

private:
    bool m_useGlobalAnimationModifier;
    struct WindowMotion {
        // TODO: Rotation, etc?
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
class KWIN_EXPORT EffectFrame
{
public:
    EffectFrame();
    virtual ~EffectFrame();

    /**
     * Delete any existing textures to free up graphics memory. They will
     * be automatically recreated the next time they are required.
     */
    virtual void free() = 0;

    /**
     * Render the frame.
     */
    virtual void render(QRegion region = infiniteRegion(), double opacity = 1.0, double frameOpacity = 1.0) = 0;

    virtual void setPosition(const QPoint& point) = 0;
    /**
     * Set the text alignment for static frames and the position alignment
     * for non-static.
     */
    virtual void setAlignment(Qt::Alignment alignment) = 0;
    virtual Qt::Alignment alignment() const = 0;
    virtual void setGeometry(const QRect& geometry, bool force = false) = 0;
    virtual const QRect& geometry() const = 0;

    virtual void setText(const QString& text) = 0;
    virtual const QString& text() const = 0;
    virtual void setFont(const QFont& font) = 0;
    virtual const QFont& font() const = 0;
    /**
     * Set the icon that will appear on the left-hand size of the frame.
     */
    virtual void setIcon(const QPixmap& icon) = 0;
    virtual const QPixmap& icon() const = 0;
    virtual void setIconSize(const QSize& size) = 0;
    virtual const QSize& iconSize() const = 0;

    /**
     * Sets the geometry of a selection.
     * To remove the selection set a null rect.
     * @param selection The geometry of the selection in screen coordinates.
     **/
    virtual void setSelection(const QRect& selection) = 0;

    /**
     * @param shader The GLShader for rendering.
     **/
    virtual void setShader(GLShader* shader) = 0;
    /**
     * @returns The GLShader used for rendering or null if none.
     **/
    virtual GLShader* shader() const = 0;

    /**
     * @returns The style of this EffectFrame.
     **/
    virtual EffectFrameStyle style() const = 0;

    /**
     * If @p enable is @c true cross fading between icons and text is enabled
     * By default disabled. Use setCrossFadeProgress to cross fade.
     * Cross Fading is currently only available if OpenGL is used.
     * @param enable @c true enables cross fading, @c false disables it again
     * @see isCrossFade
     * @see setCrossFadeProgress
     * @since 4.6
     **/
    void enableCrossFade(bool enable);
    /**
     * @returns @c true if cross fading is enabled, @c false otherwise
     * @see enableCrossFade
     * @since 4.6
     **/
    bool isCrossFade() const;
    /**
     * Sets the current progress for cross fading the last used icon/text
     * with current icon/text to @p progress.
     * A value of 0.0 means completely old icon/text, a value of 1.0 means
     * completely current icon/text.
     * Default value is 1.0. You have to enable cross fade before using it.
     * Cross Fading is currently only available if OpenGL is used.
     * @see enableCrossFade
     * @see isCrossFade
     * @see crossFadeProgress
     * @since 4.6
     **/
    void setCrossFadeProgress(qreal progress);
    /**
     * @returns The current progress for cross fading
     * @see setCrossFadeProgress
     * @see enableCrossFade
     * @see isCrossFade
     * @since 4.6
     **/
    qreal crossFadeProgress() const;

private:
    EffectFramePrivate* const d;
};

/**
 * Pointer to the global EffectsHandler object.
 **/
extern KWIN_EXPORT EffectsHandler* effects;

/***************************************************************
 WindowVertex
***************************************************************/

inline
WindowVertex::WindowVertex()
    : px(0), py(0), tx(0), ty(0)
{
}

inline
WindowVertex::WindowVertex(double _x, double _y, double _tx, double _ty)
    : px(_x), py(_y), ox(_x), oy(_y), tx(_tx), ty(_ty)
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
void WindowVertex::move(double x, double y)
{
    px = x;
    py = y;
}

inline
void WindowVertex::setX(double x)
{
    px = x;
}

inline
void WindowVertex::setY(double y)
{
    py = y;
}

/***************************************************************
 WindowQuad
***************************************************************/

inline
WindowQuad::WindowQuad(WindowQuadType t, int id)
    : quadType(t)
    , quadID(id)
{
}

inline
WindowVertex& WindowQuad::operator[](int index)
{
    assert(index >= 0 && index < 4);
    return verts[ index ];
}

inline
const WindowVertex& WindowQuad::operator[](int index) const
{
    assert(index >= 0 && index < 4);
    return verts[ index ];
}

inline
WindowQuadType WindowQuad::type() const
{
    assert(quadType != WindowQuadError);
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
    assert(quadType != WindowQuadError);
    return quadType == WindowQuadDecoration;
}

inline
bool WindowQuad::effect() const
{
    assert(quadType != WindowQuadError);
    return quadType >= EFFECT_QUAD_TYPE_START;
}

inline
bool WindowQuad::isTransformed() const
{
    return !(verts[ 0 ].px == verts[ 0 ].ox && verts[ 0 ].py == verts[ 0 ].oy
             && verts[ 1 ].px == verts[ 1 ].ox && verts[ 1 ].py == verts[ 1 ].oy
             && verts[ 2 ].px == verts[ 2 ].ox && verts[ 2 ].py == verts[ 2 ].oy
             && verts[ 3 ].px == verts[ 3 ].ox && verts[ 3 ].py == verts[ 3 ].oy);
}

inline
double WindowQuad::left() const
{
    return qMin(verts[ 0 ].px, qMin(verts[ 1 ].px, qMin(verts[ 2 ].px, verts[ 3 ].px)));
}

inline
double WindowQuad::right() const
{
    return qMax(verts[ 0 ].px, qMax(verts[ 1 ].px, qMax(verts[ 2 ].px, verts[ 3 ].px)));
}

inline
double WindowQuad::top() const
{
    return qMin(verts[ 0 ].py, qMin(verts[ 1 ].py, qMin(verts[ 2 ].py, verts[ 3 ].py)));
}

inline
double WindowQuad::bottom() const
{
    return qMax(verts[ 0 ].py, qMax(verts[ 1 ].py, qMax(verts[ 2 ].py, verts[ 3 ].py)));
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
Motion<T>::Motion(T initial, double strength, double smoothness)
    :   m_value(initial)
    ,   m_start(initial)
    ,   m_target(initial)
    ,   m_velocity()
    ,   m_strength(strength)
    ,   m_smoothness(smoothness)
{
}

template <typename T>
Motion<T>::Motion(const Motion &other)
    :   m_value(other.value())
    ,   m_start(other.target())
    ,   m_target(other.target())
    ,   m_velocity(other.velocity())
    ,   m_strength(other.strength())
    ,   m_smoothness(other.smoothness())
{
}

template <typename T>
Motion<T>::~Motion()
{
}

template <typename T>
void Motion<T>::calculate(const int msec)
{
    if (m_value == m_target && m_velocity == T())   // At target and not moving
        return;

    // Poor man's time independent calculation
    int steps = qMax(1, msec / 5);
    for (int i = 0; i < steps; i++) {
        T diff = m_target - m_value;
        T strength = diff * m_strength;
        m_velocity = (m_smoothness * m_velocity + strength) / (m_smoothness + 1.0);
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
Q_DECLARE_METATYPE(KWin::EffectWindow*)
Q_DECLARE_METATYPE(QList<KWin::EffectWindow*>)

/** @} */

#endif // KWINEFFECTS_H
