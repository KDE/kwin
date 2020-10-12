/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWINEFFECTS_H
#define KWINEFFECTS_H

#include <kwinconfig.h>
#include <kwineffects_export.h>
#include <kwinglobals.h>

#include <QEasingCurve>
#include <QIcon>
#include <QPair>
#include <QSet>
#include <QRect>
#include <QRegion>
#include <QVector2D>
#include <QVector3D>

#include <QVector>
#include <QLoggingCategory>
#include <QList>
#include <QHash>
#include <QStack>
#include <QScopedPointer>

#include <KPluginFactory>
#include <KSharedConfig>

#include <netwm.h>

#include <climits>
#include <functional>

class KConfigGroup;
class QFont;
class QGraphicsScale;
class QKeyEvent;
class QMatrix4x4;
class QAction;

/**
 * Logging category to be used inside the KWin effects.
 * Do not use in this library.
 */
Q_DECLARE_LOGGING_CATEGORY(KWINEFFECTS)

namespace KWaylandServer {
    class SurfaceInterface;
    class Display;
}

namespace KWin
{

class PaintDataPrivate;
class WindowPaintDataPrivate;

class EffectWindow;
class EffectWindowGroup;
class EffectFrame;
class EffectFramePrivate;
class EffectQuickView;
class Effect;
class WindowQuad;
class GLShader;
class XRenderPicture;
class WindowQuadList;
class WindowPrePaintData;
class WindowPaintData;
class ScreenPrePaintData;
class ScreenPaintData;

typedef QPair< QString, Effect* > EffectPair;
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
 * @subsection creating-macro KWIN_EFFECT_FACTORY macro
 * This library provides a specialized KPluginFactory subclass and macros to
 * create a sub class. This subclass of KPluginFactory has to be used, otherwise
 * KWin won't load the plugin. Use the @ref KWIN_EFFECT_FACTORY macro to create the
 * plugin factory.
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
 */

#define KWIN_EFFECT_API_MAKE_VERSION( major, minor ) (( major ) << 8 | ( minor ))
#define KWIN_EFFECT_API_VERSION_MAJOR 0
#define KWIN_EFFECT_API_VERSION_MINOR 231
#define KWIN_EFFECT_API_VERSION KWIN_EFFECT_API_MAKE_VERSION( \
        KWIN_EFFECT_API_VERSION_MAJOR, KWIN_EFFECT_API_VERSION_MINOR )

enum WindowQuadType {
    WindowQuadError, // for the stupid default ctor
    WindowQuadContents,
    WindowQuadDecoration,
    // Shadow Quad types
    WindowQuadShadow,             // OpenGL only. The other shadow types are only used by Xrender
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
    WindowForceBackgroundContrastRole, ///< For fullscreen effects to enforce the background contrast,
    WindowBackgroundContrastRole, ///< For single windows to enable Background contrast
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
KWINEFFECTS_EXPORT inline
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
 *
 * @section OpenGL
 * Effects can use OpenGL if EffectsHandler::isOpenGLCompositing() returns @c true.
 * The OpenGL context may not always be current when code inside the effect is
 * executed. The framework ensures that the OpenGL context is current when the Effect
 * gets created, destroyed or reconfigured and during the painting stages. All virtual
 * methods which have the OpenGL context current are documented.
 *
 * If OpenGL code is going to be executed outside the painting stages, e.g. in reaction
 * to a global shortcut, it is the task of the Effect to make the OpenGL context current:
 * @code
 * effects->makeOpenGLContextCurrent();
 * @endcode
 *
 * There is in general no need to call the matching doneCurrent method.
 */
class KWINEFFECTS_EXPORT Effect : public QObject
{
    Q_OBJECT
public:
    /** Flags controlling how painting is done. */
    // TODO: is that ok here?
    enum {
        /**
         * Window (or at least part of it) will be painted opaque.
         */
        PAINT_WINDOW_OPAQUE         = 1 << 0,
        /**
         * Window (or at least part of it) will be painted translucent.
         */
        PAINT_WINDOW_TRANSLUCENT    = 1 << 1,
        /**
         * Window will be painted with transformed geometry.
         */
        PAINT_WINDOW_TRANSFORMED    = 1 << 2,
        /**
         * Paint only a region of the screen (can be optimized, cannot
         * be used together with TRANSFORMED flags).
         */
        PAINT_SCREEN_REGION         = 1 << 3,
        /**
         * The whole screen will be painted with transformed geometry.
         * Forces the entire screen to be painted.
         */
        PAINT_SCREEN_TRANSFORMED    = 1 << 4,
        /**
         * At least one window will be painted with transformed geometry.
         * Forces the entire screen to be painted.
         */
        PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
        /**
         * Clear whole background as the very first step, without optimizing it
         */
        PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6,
        // PAINT_DECORATION_ONLY = 1 << 7 has been deprecated
        /**
         * Window will be painted with a lanczos filter.
         */
        PAINT_WINDOW_LANCZOS = 1 << 8
        // PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS = 1 << 9 has been removed
    };

    enum Feature {
        Nothing = 0,
        Resize,
        GeometryTip,
        Outline, /**< @deprecated */
        ScreenInversion,
        Blur,
        Contrast,
        HighlightWindows
    };

    /**
     * Constructs new Effect object.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when the Effect is constructed.
     */
    Effect();
    /**
     * Destructs the Effect object.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when the Effect is destroyed.
     */
    ~Effect() override;

    /**
     * Flags describing which parts of configuration have changed.
     */
    enum ReconfigureFlag {
        ReconfigureAll = 1 << 0 /// Everything needs to be reconfigured.
    };
    Q_DECLARE_FLAGS(ReconfigureFlags, ReconfigureFlag)

    /**
     * Called when configuration changes (either the effect's or KWin's global).
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when the Effect is reconfigured. If this method is called from within the Effect it is
     * required to ensure that the context is current if the implementation does OpenGL calls.
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
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
    */
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    /**
     * In this method you can:
     * @li paint something on top of the windows (by painting after calling
     *      effects->paintScreen())
     * @li paint multiple desktops and/or multiple copies of the same desktop
     *      by calling effects->paintScreen() multiple times
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void paintScreen(int mask, const QRegion &region, ScreenPaintData& data);
    /**
     * Called after all the painting has been finished.
     * In this method you can:
     * @li schedule next repaint in case of animations
     * You shouldn't paint anything here.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void postPaintScreen();

    /**
     * Called for every window before the actual paint pass
     * In this method you can:
     * @li enable or disable painting of the window (e.g. enable paiting of minimized window)
     * @li set window to be painted with translucency
     * @li set window to be transformed
     * @li request the window to be divided into multiple parts
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    /**
     * This is the main method for painting windows.
     * In this method you can:
     * @li do various transformations
     * @li change opacity of the window
     * @li change brightness and/or saturation, if it's supported
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    /**
     * Called for every window after all painting has been finished.
     * In this method you can:
     * @li schedule next repaint for individual window(s) in case of animations
     * You shouldn't paint anything here.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
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
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void paintEffectFrame(EffectFrame* frame, const QRegion &region, double opacity, double frameOpacity);

    /**
     * Called on Transparent resizes.
     * return true if your effect substitutes questioned feature
     */
    virtual bool provides(Feature);

    /**
     * Performs the @p feature with the @p arguments.
     *
     * This allows to have specific protocols between KWin core and an Effect.
     *
     * The method is supposed to return @c true if it performed the features,
     * @c false otherwise.
     *
     * The default implementation returns @c false.
     * @since 5.8
     */
    virtual bool perform(Feature feature, const QVariantList &arguments);

    /**
     * Can be called to draw multiple copies (e.g. thumbnails) of a window.
     * You can change window's opacity/brightness/etc here, but you can't
     *  do any transformations.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void drawWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data);

    /**
     * Define new window quads so that they can be transformed by other effects.
     * It's up to the effect to keep track of them.
     */
    virtual void buildQuads(EffectWindow* w, WindowQuadList& quadList);

    virtual void windowInputMouseEvent(QEvent* e);
    virtual void grabbedKeyboardEvent(QKeyEvent* e);

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
     */
    virtual bool isActive() const;

    /**
     * Reimplement this method to provide online debugging.
     * This could be as trivial as printing specific detail information about the effect state
     * but could also be used to move the effect in and out of a special debug modes, clear bogus
     * data, etc.
     * Notice that the functions is const by intent! Whenever you alter the state of the object
     * due to random user input, you should do so with greatest care, hence const_cast<> your
     * object - signalling "let me alone, i know what i'm doing"
     * @param parameter A freeform string user input for your effect to interpret.
     * @since 4.11
     */
    virtual QString debug(const QString &parameter) const;

    /**
     * Reimplement this method to indicate where in the Effect chain the Effect should be placed.
     *
     * A low number indicates early chain position, thus before other Effects got called, a high
     * number indicates a late position. The returned number should be in the interval [0, 100].
     * The default value is 0.
     *
     * In KWin4 this information was provided in the Effect's desktop file as property
     * X-KDE-Ordering. In the case of Scripted Effects this property is still used.
     *
     * @since 5.0
     */
    virtual int requestedEffectChainPosition() const;


    /**
     * A touch point was pressed.
     *
     * If the effect wants to exclusively use the touch event it should return @c true.
     * If @c false is returned the touch event is passed to further effects.
     *
     * In general an Effect should only return @c true if it is the exclusive effect getting
     * input events. E.g. has grabbed mouse events.
     *
     * Default implementation returns @c false.
     *
     * @param id The unique id of the touch point
     * @param pos The position of the touch point in global coordinates
     * @param time Timestamp
     *
     * @see touchMotion
     * @see touchUp
     * @since 5.8
     */
    virtual bool touchDown(qint32 id, const QPointF &pos, quint32 time);
    /**
     * A touch point moved.
     *
     * If the effect wants to exclusively use the touch event it should return @c true.
     * If @c false is returned the touch event is passed to further effects.
     *
     * In general an Effect should only return @c true if it is the exclusive effect getting
     * input events. E.g. has grabbed mouse events.
     *
     * Default implementation returns @c false.
     *
     * @param id The unique id of the touch point
     * @param pos The position of the touch point in global coordinates
     * @param time Timestamp
     *
     * @see touchDown
     * @see touchUp
     * @since 5.8
     */
    virtual bool touchMotion(qint32 id, const QPointF &pos, quint32 time);
    /**
     * A touch point was released.
     *
     * If the effect wants to exclusively use the touch event it should return @c true.
     * If @c false is returned the touch event is passed to further effects.
     *
     * In general an Effect should only return @c true if it is the exclusive effect getting
     * input events. E.g. has grabbed mouse events.
     *
     * Default implementation returns @c false.
     *
     * @param id The unique id of the touch point
     * @param time Timestamp
     *
     * @see touchDown
     * @see touchMotion
     * @since 5.8
     */
    virtual bool touchUp(qint32 id, quint32 time);

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
     * @overload Use this variant if animation time is provided through a KConfigXT generated class
     * having a property called "duration".
     */
    template <typename T>
    int animationTime(int defaultDuration);
    /**
     * Linearly interpolates between @p x and @p y.
     *
     * Returns @p x when @p a = 0; returns @p y when @p a = 1.
     */
    static double interpolate(double x, double y, double a) {
        return x * (1 - a) + y * a;
    }
    /** Helper to set WindowPaintData and QRegion to necessary transformations so that
     * a following drawWindow() would put the window at the requested geometry (useful for thumbnails)
     */
    static void setPositionTransformations(WindowPaintData& data, QRect& region, EffectWindow* w,
                                           const QRect& r, Qt::AspectRatioMode aspect);

public Q_SLOTS:
    virtual bool borderActivated(ElectricBorder border);

protected:
    xcb_connection_t *xcbConnection() const;
    xcb_window_t x11RootWindow() const;

    /**
     * An implementing class can call this with it's kconfig compiled singleton class.
     * This method will perform the instance on the class.
     * @since 5.9
     */
    template <typename T>
    void initConfig();
};


/**
 * Prefer the KWIN_EFFECT_FACTORY macros.
 */
class KWINEFFECTS_EXPORT EffectPluginFactory : public KPluginFactory
{
    Q_OBJECT
public:
    EffectPluginFactory();
    ~EffectPluginFactory() override;
    /**
     * Returns whether the Effect is supported.
     *
     * An Effect can implement this method to determine at runtime whether the Effect is supported.
     *
     * If the current compositing backend is not supported it should return @c false.
     *
     * This method is optional, by default @c true is returned.
     */
    virtual bool isSupported() const;
    /**
     * Returns whether the Effect should get enabled by default.
     *
     * This function provides a way for an effect to override the default at runtime,
     * e.g. based on the capabilities of the hardware.
     *
     * This method is optional; the effect doesn't have to provide it.
     *
     * Note that this function is only called if the supported() function returns true,
     * and if X-KDE-PluginInfo-EnabledByDefault is set to true in the .desktop file.
     *
     * This method is optional, by default @c true is returned.
     */
    virtual bool enabledByDefault() const;
    /**
     * This method returns the created Effect.
     */
    virtual KWin::Effect *createEffect() const = 0;
};

/**
 * Defines an EffectPluginFactory sub class with customized isSupported and enabledByDefault methods.
 *
 * If the Effect to be created does not need the isSupported or enabledByDefault methods prefer
 * the simplified KWIN_EFFECT_FACTORY, KWIN_EFFECT_FACTORY_SUPPORTED or KWIN_EFFECT_FACTORY_ENABLED
 * macros which create an EffectPluginFactory with a useable default value.
 *
 * The macro also adds a useable K_EXPORT_PLUGIN_VERSION to the definition. KWin will not load
 * any Effect with a non-matching plugin version. This API is not providing binary compatibility
 * and thus the effect plugin must be compiled against the same kwineffects library version as
 * KWin.
 *
 * @param factoryName The name to be used for the EffectPluginFactory
 * @param className The class name of the Effect sub class which is to be created by the factory
 * @param jsonFile Name of the json file to be compiled into the plugin as metadata
 * @param supported Source code to go into the isSupported() method, must return a boolean
 * @param enabled Source code to go into the enabledByDefault() method, must return a boolean
 */
#define KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED( factoryName, className, jsonFile, supported, enabled ) \
    class factoryName : public KWin::EffectPluginFactory \
    { \
        Q_OBJECT \
        Q_PLUGIN_METADATA(IID KPluginFactory_iid FILE jsonFile) \
        Q_INTERFACES(KPluginFactory) \
    public: \
        explicit factoryName() {} \
        ~factoryName() {} \
        bool isSupported() const override { \
            supported \
        } \
        bool enabledByDefault() const override { \
            enabled \
        } \
        KWin::Effect *createEffect() const override { \
            return new className(); \
        } \
    }; \
    K_EXPORT_PLUGIN_VERSION(quint32(KWIN_EFFECT_API_VERSION))

#define KWIN_EFFECT_FACTORY_ENABLED( factoryName, className, jsonFile, enabled ) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED( factoryName, className, jsonFile, return true;, enabled )

#define KWIN_EFFECT_FACTORY_SUPPORTED( factoryName, className, jsonFile, supported ) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED( factoryName, className, jsonFile, supported, return true; )

#define KWIN_EFFECT_FACTORY( factoryName, className, jsonFile ) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED( factoryName, className, jsonFile, return true;, return true; )



/**
 * @short Manager class that handles all the effects.
 *
 * This class creates Effect objects and calls it's appropriate methods.
 *
 * Effect objects can call methods of this class to interact with the
 *  workspace, e.g. to activate or move a specific window, change current
 *  desktop or create a special input window to receive mouse and keyboard
 *  events.
 */
class KWINEFFECTS_EXPORT EffectsHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentDesktop READ currentDesktop WRITE setCurrentDesktop NOTIFY desktopChanged)
    Q_PROPERTY(QString currentActivity READ currentActivity NOTIFY currentActivityChanged)
    Q_PROPERTY(KWin::EffectWindow *activeWindow READ activeWindow WRITE activateWindow NOTIFY windowActivated)
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize)
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth)
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight)
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)
    /**
     * The number of desktops currently used. Minimum number of desktops is 1, maximum 20.
     */
    Q_PROPERTY(int desktops READ numberOfDesktops WRITE setNumberOfDesktops NOTIFY numberDesktopsChanged)
    Q_PROPERTY(bool optionRollOverDesktops READ optionRollOverDesktops)
    Q_PROPERTY(int activeScreen READ activeScreen)
    Q_PROPERTY(int numScreens READ numScreens NOTIFY numberScreensChanged)
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
     */
    Q_PROPERTY(bool decorationsHaveAlpha READ decorationsHaveAlpha)
    /**
     * Whether the window decorations support blurring behind the decoration.
     */
    Q_PROPERTY(bool decorationSupportsBlurBehind READ decorationSupportsBlurBehind)
    Q_PROPERTY(CompositingType compositingType READ compositingType CONSTANT)
    Q_PROPERTY(QPoint cursorPos READ cursorPos)
    Q_PROPERTY(QSize virtualScreenSize READ virtualScreenSize NOTIFY virtualScreenSizeChanged)
    Q_PROPERTY(QRect virtualScreenGeometry READ virtualScreenGeometry NOTIFY virtualScreenGeometryChanged)
    Q_PROPERTY(bool hasActiveFullScreenEffect READ hasActiveFullScreenEffect NOTIFY hasActiveFullScreenEffectChanged)

    /**
     * The status of the session i.e if the user is logging out
     * @since 5.18
     */
    Q_PROPERTY(KWin::SessionState sessionState READ sessionState NOTIFY sessionStateChanged)

    friend class Effect;
public:
    explicit EffectsHandler(CompositingType type);
    ~EffectsHandler() override;
    // for use by effects
    virtual void prePaintScreen(ScreenPrePaintData& data, int time) = 0;
    virtual void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) = 0;
    virtual void postPaintScreen() = 0;
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) = 0;
    virtual void paintWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data) = 0;
    virtual void postPaintWindow(EffectWindow* w) = 0;
    virtual void paintEffectFrame(EffectFrame* frame, const QRegion &region, double opacity, double frameOpacity) = 0;
    virtual void drawWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data) = 0;
    virtual void buildQuads(EffectWindow* w, WindowQuadList& quadList) = 0;
    virtual QVariant kwinOption(KWinOption kwopt) = 0;
    /**
     * Sets the cursor while the mouse is intercepted.
     * @see startMouseInterception
     * @since 4.11
     */
    virtual void defineCursor(Qt::CursorShape shape) = 0;
    virtual QPoint cursorPos() const = 0;
    virtual bool grabKeyboard(Effect* effect) = 0;
    virtual void ungrabKeyboard() = 0;
    /**
     * Ensures that all mouse events are sent to the @p effect.
     * No window will get the mouse events. Only fullscreen effects providing a custom user interface should
     * be using this method. The input events are delivered to Effect::windowInputMouseEvent.
     *
     * @note This method does not perform an X11 mouse grab. On X11 a fullscreen input window is raised above
     * all other windows, but no grab is performed.
     *
     * @param effect The effect
     * @param shape Sets the cursor to be used while the mouse is intercepted
     * @see stopMouseInterception
     * @see Effect::windowInputMouseEvent
     * @since 4.11
     */
    virtual void startMouseInterception(Effect *effect, Qt::CursorShape shape) = 0;
    /**
     * Releases the hold mouse interception for @p effect
     * @see startMouseInterception
     * @since 4.11
     */
    virtual void stopMouseInterception(Effect *effect) = 0;

    /**
     * @brief Registers a global shortcut with the provided @p action.
     *
     * @param shortcut The global shortcut which should trigger the action
     * @param action The action which gets triggered when the shortcut matches
     */
    virtual void registerGlobalShortcut(const QKeySequence &shortcut, QAction *action) = 0;
    /**
     * @brief Registers a global pointer shortcut with the provided @p action.
     *
     * @param modifiers The keyboard modifiers which need to be holded
     * @param pointerButtons The pointer buttons which need to be pressed
     * @param action The action which gets triggered when the shortcut matches
     */
    virtual void registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action) = 0;
    /**
     * @brief Registers a global axis shortcut with the provided @p action.
     *
     * @param modifiers The keyboard modifiers which need to be holded
     * @param axis The direction in which the axis needs to be moved
     * @param action The action which gets triggered when the shortcut matches
     */
    virtual void registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action) = 0;

    /**
     * @brief Registers a global touchpad swipe gesture shortcut with the provided @p action.
     *
     * @param direction The direction for the swipe
     * @param action The action which gets triggered when the gesture triggers
     * @since 5.10
     */
    virtual void registerTouchpadSwipeShortcut(SwipeDirection direction, QAction *action) = 0;

    /**
     * Retrieve the proxy class for an effect if it has one. Will return NULL if
     * the effect isn't loaded or doesn't have a proxy class.
     */
    virtual void* getProxy(QString name) = 0;

    // Mouse polling
    virtual void startMousePolling() = 0;
    virtual void stopMousePolling() = 0;

    virtual void reserveElectricBorder(ElectricBorder border, Effect *effect) = 0;
    virtual void unreserveElectricBorder(ElectricBorder border, Effect *effect) = 0;

    /**
     * Registers the given @p action for the given @p border to be activated through
     * a touch swipe gesture.
     *
     * If the @p border gets triggered through a touch swipe gesture the QAction::triggered
     * signal gets invoked.
     *
     * To unregister the touch screen action either delete the @p action or
     * invoke unregisterTouchBorder.
     *
     * @see unregisterTouchBorder
     * @since 5.10
     */
    virtual void registerTouchBorder(ElectricBorder border, QAction *action) = 0;
    /**
     * Unregisters the given @p action for the given touch @p border.
     *
     * @see registerTouchBorder
     * @since 5.10
     */
    virtual void unregisterTouchBorder(ElectricBorder border, QAction *action) = 0;

    // functions that allow controlling windows/desktop
    virtual void activateWindow(KWin::EffectWindow* c) = 0;
    virtual KWin::EffectWindow* activeWindow() const = 0 ;
    Q_SCRIPTABLE virtual void moveWindow(KWin::EffectWindow* w, const QPoint& pos, bool snap = false, double snapAdjust = 1.0) = 0;

    /**
     * Moves the window to the specific desktop
     * Setting desktop to NET::OnAllDesktops will set the window on all desktops
     */
    Q_SCRIPTABLE virtual void windowToDesktop(KWin::EffectWindow* w, int desktop) = 0;

    /**
     * Moves a window to the given desktops
     * On X11, the window will end up on the last window in the list
     * Setting this to an empty list will set the window on all desktops
     *
     * @arg desktopIds a list of desktops the window should be placed on. NET::OnAllDesktops is not a valid desktop X11Id
     */
    Q_SCRIPTABLE virtual void windowToDesktops(KWin::EffectWindow* w, const QVector<uint> &desktopIds) = 0;

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
     * The bounding size of all screens combined. Overlapping areas
     * are not counted multiple times.
     *
     * @see virtualScreenGeometry()
     * @see virtualScreenSizeChanged()
     * @since 5.0
     */
    virtual QSize virtualScreenSize() const = 0;
    /**
     * The bounding geometry of all outputs combined. Always starts at (0,0) and has
     * virtualScreenSize as it's size.
     *
     * @see virtualScreenSize()
     * @see virtualScreenGeometryChanged()
     * @since 5.0
     */
    virtual QRect virtualScreenGeometry() const = 0;
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
    Q_SCRIPTABLE virtual KWin::EffectWindow* findWindow(KWaylandServer::SurfaceInterface *surf) const = 0;
    /**
     * Finds the EffectWindow for the internal window @p w.
     * If there is no such window @c null is returned.
     *
     * On Wayland this returns the internal window. On X11 it returns an Unamanged with the
     * window id matching that of the provided window @p w.
     *
     * @since 5.16
     */
    Q_SCRIPTABLE virtual KWin::EffectWindow *findWindow(QWindow *w) const = 0;
    /**
     * Finds the EffectWindow for the Toplevel with KWin internal @p id.
     * If there is no such window @c null is returned.
     *
     * @since 5.16
     */
    Q_SCRIPTABLE virtual KWin::EffectWindow *findWindow(const QUuid &id) const = 0;
    virtual EffectWindowList stackingOrder() const = 0;
    // window will be temporarily painted as if being at the top of the stack
    Q_SCRIPTABLE virtual void setElevatedWindow(KWin::EffectWindow* w, bool set) = 0;

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
     */
    Q_SCRIPTABLE virtual void addRepaintFull() = 0;
    Q_SCRIPTABLE virtual void addRepaint(const QRect& r) = 0;
    Q_SCRIPTABLE virtual void addRepaint(const QRegion& r) = 0;
    Q_SCRIPTABLE virtual void addRepaint(int x, int y, int w, int h) = 0;

    CompositingType compositingType() const;
    /**
     * @brief Whether the Compositor is OpenGL based (either GL 1 or 2).
     *
     * @return bool @c true in case of OpenGL based Compositor, @c false otherwise
     */
    bool isOpenGLCompositing() const;
    virtual unsigned long xrenderBufferPicture() = 0;
    /**
     * @brief Provides access to the QPainter which is rendering to the back buffer.
     *
     * Only relevant for CompositingType QPainterCompositing. For all other compositing types
     * @c null is returned.
     *
     * @return QPainter* The Scene's QPainter or @c null.
     */
    virtual QPainter *scenePainter() = 0;
    virtual void reconfigure() = 0;

    virtual QByteArray readRootProperty(long atom, long type, int format) const = 0;
    /**
     * @brief Announces support for the feature with the given name. If no other Effect
     * has announced support for this feature yet, an X11 property will be installed on
     * the root window.
     *
     * The Effect will be notified for events through the signal propertyNotify().
     *
     * To remove the support again use removeSupportProperty. When an Effect is
     * destroyed it is automatically taken care of removing the support. It is not
     * required to call removeSupportProperty in the Effect's cleanup handling.
     *
     * @param propertyName The name of the property to announce support for
     * @param effect The effect which announces support
     * @return xcb_atom_t The created X11 atom
     * @see removeSupportProperty
     * @since 4.11
     */
    virtual xcb_atom_t announceSupportProperty(const QByteArray &propertyName, Effect *effect) = 0;
    /**
     * @brief Removes support for the feature with the given name. If there is no other Effect left
     * which has announced support for the given property, the property will be removed from the
     * root window.
     *
     * In case the Effect had not registered support, calling this function does not change anything.
     *
     * @param propertyName The name of the property to remove support for
     * @param effect The effect which had registered the property.
     * @see announceSupportProperty
     * @since 4.11
     */
    virtual void removeSupportProperty(const QByteArray &propertyName, Effect *effect) = 0;

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
     */
    virtual void reloadEffect(Effect *effect) = 0;

    /**
     * Whether the screen is currently considered as locked.
     * Note for technical reasons this is not always possible to detect. The screen will only
     * be considered as locked if the screen locking process implements the
     * org.freedesktop.ScreenSaver interface.
     *
     * @returns @c true if the screen is currently locked, @c false otherwise
     * @see screenLockingChanged
     * @since 4.11
     */
    virtual bool isScreenLocked() const = 0;

    /**
     * @brief Makes the OpenGL compositing context current.
     *
     * If the compositing backend is not using OpenGL, this method returns @c false.
     *
     * @return bool @c true if the context became current, @c false otherwise.
     */
    virtual bool makeOpenGLContextCurrent() = 0;
    /**
     * @brief Makes a null OpenGL context current resulting in no context
     * being current.
     *
     * If the compositing backend is not OpenGL based, this method is a noop.
     *
     * There is normally no reason for an Effect to call this method.
     */
    virtual void doneOpenGLContextCurrent() = 0;

    virtual xcb_connection_t *xcbConnection() const = 0;
    virtual xcb_window_t x11RootWindow() const = 0;

    /**
     * Interface to the Wayland display: this is relevant only
     * on Wayland, on X11 it will be nullptr
     * @since 5.5
     */
    virtual KWaylandServer::Display *waylandDisplay() const = 0;

    /**
     * Whether animations are supported by the Scene.
     * If this method returns @c false Effects are supposed to not
     * animate transitions.
     *
     * @returns Whether the Scene can drive animations
     * @since 5.8
     */
    virtual bool animationsSupported() const = 0;

    /**
     * The current cursor image of the Platform.
     * @see cursorPos
     * @since 5.9
     */
    virtual PlatformCursorImage cursorImage() const = 0;

    /**
     * The cursor image should be hidden.
     * @see showCursor
     * @since 5.9
     */
    virtual void hideCursor() = 0;

    /**
     * The cursor image should be shown again after having been hidden.
     * @see hideCursor
     * @since 5.9
     */
    virtual void showCursor() = 0;

    /**
     * Starts an interactive window selection process.
     *
     * Once the user selected a window the @p callback is invoked with the selected EffectWindow as
     * argument. In case the user cancels the interactive window selection or selecting a window is currently
     * not possible (e.g. screen locked) the @p callback is invoked with a @c nullptr argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor.
     *
     * @param callback The function to invoke once the interactive window selection ends
     * @since 5.9
     */
    virtual void startInteractiveWindowSelection(std::function<void(KWin::EffectWindow*)> callback) = 0;

    /**
     * Starts an interactive position selection process.
     *
     * Once the user selected a position on the screen the @p callback is invoked with
     * the selected point as argument. In case the user cancels the interactive position selection
     * or selecting a position is currently not possible (e.g. screen locked) the @p callback
     * is invoked with a point at @c -1 as x and y argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor.
     *
     * @param callback The function to invoke once the interactive position selection ends
     * @since 5.9
     */
    virtual void startInteractivePositionSelection(std::function<void(const QPoint &)> callback) = 0;

    /**
     * Shows an on-screen-message. To hide it again use hideOnScreenMessage.
     *
     * @param message The message to show
     * @param iconName The optional themed icon name
     * @see hideOnScreenMessage
     * @since 5.9
     */
    virtual void showOnScreenMessage(const QString &message, const QString &iconName = QString()) = 0;

    /**
     * Flags for how to hide a shown on-screen-message
     * @see hideOnScreenMessage
     * @since 5.9
     */
    enum class OnScreenMessageHideFlag {
        /**
         * The on-screen-message should skip the close window animation.
         * @see EffectWindow::skipsCloseAnimation
         */
        SkipsCloseAnimation = 1
    };
    Q_DECLARE_FLAGS(OnScreenMessageHideFlags, OnScreenMessageHideFlag)
    /**
     * Hides a previously shown on-screen-message again.
     * @param flags The flags for how to hide the message
     * @see showOnScreenMessage
     * @since 5.9
     */
    virtual void hideOnScreenMessage(OnScreenMessageHideFlags flags = OnScreenMessageHideFlags()) = 0;

    /*
     * @returns The configuration used by the EffectsHandler.
     * @since 5.10
     */
    virtual KSharedConfigPtr config() const = 0;

    /**
     * @returns The global input configuration (kcminputrc)
     * @since 5.10
     */
    virtual KSharedConfigPtr inputConfig() const = 0;

    /**
     * Returns if activeFullScreenEffect is set
     */
    virtual bool hasActiveFullScreenEffect() const = 0;

    /**
     * Render the supplied EffectQuickView onto the scene
     * It can be called at any point during the scene rendering
     * @since 5.18
     */
    virtual void renderEffectQuickView(EffectQuickView *effectQuickView) const = 0;

    /**
     * The status of the session i.e if the user is logging out
     * @since 5.18
     */
    virtual SessionState sessionState() const = 0;
Q_SIGNALS:
    /**
     * Signal emitted when the current desktop changed.
     * @param oldDesktop The previously current desktop
     * @param newDesktop The new current desktop
     * @param with The window which is taken over to the new desktop, can be NULL
     * @since 4.9
     */
    void desktopChanged(int oldDesktop, int newDesktop, KWin::EffectWindow *with);
    /**
     * @since 4.7
     * @deprecated
     */
    void desktopChanged(int oldDesktop, int newDesktop);
    /**
     * Signal emitted when a window moved to another desktop
     * NOTICE that this does NOT imply that the desktop has changed
     * The @param window which is moved to the new desktop
     * @param oldDesktop The previous desktop of the window
     * @param newDesktop The new desktop of the window
     * @since 4.11.4
     */
    void desktopPresenceChanged(KWin::EffectWindow *window, int oldDesktop, int newDesktop);
    /**
    * Signal emitted when the number of currently existing desktops is changed.
    * @param old The previous number of desktops in used.
    * @see EffectsHandler::numberOfDesktops.
    * @since 4.7
    */
    void numberDesktopsChanged(uint old);
    /**
     * Signal emitted when the number of screens changed.
     * @since 5.0
     */
    void numberScreensChanged();
    /**
     * Signal emitted when the desktop showing ("dashboard") state changed
     * The desktop is risen to the keepAbove layer, you may want to elevate
     * windows or such.
     * @since 5.3
     */
    void showingDesktopChanged(bool);
    /**
     * Signal emitted when a new window has been added to the Workspace.
     * @param w The added window
     * @since 4.7
     */
    void windowAdded(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window is being removed from the Workspace.
     * An effect which wants to animate the window closing should connect
     * to this signal and reference the window by using
     * refWindow
     * @param w The window which is being closed
     * @since 4.7
     */
    void windowClosed(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window get's activated.
     * @param w The new active window, or @c NULL if there is no active window.
     * @since 4.7
     */
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
     */
    void windowDeleted(KWin::EffectWindow *w);
    /**
     * Signal emitted when a user begins a window move or resize operation.
     * To figure out whether the user resizes or moves the window use
     * isUserMove or isUserResize.
     * Whenever the geometry is updated the signal @ref windowStepUserMovedResized
     * is emitted with the current geometry.
     * The move/resize operation ends with the signal @ref windowFinishUserMovedResized.
     * Only one window can be moved/resized by the user at the same time!
     * @param w The window which is being moved/resized
     * @see windowStepUserMovedResized
     * @see windowFinishUserMovedResized
     * @see EffectWindow::isUserMove
     * @see EffectWindow::isUserResize
     * @since 4.7
     */
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
     */
    void windowStepUserMovedResized(KWin::EffectWindow *w, const QRect &geometry);
    /**
     * Signal emitted when the user finishes move/resize of window @p w.
     * @param w The window which has been moved/resized
     * @see windowStartUserMovedResized
     * @see windowFinishUserMovedResized
     * @since 4.7
     */
    void windowFinishUserMovedResized(KWin::EffectWindow *w);
    /**
     * Signal emitted when the maximized state of the window @p w changed.
     * A window can be in one of four states:
     * @li restored: both @p horizontal and @p vertical are @c false
     * @li horizontally maximized: @p horizontal is @c true and @p vertical is @c false
     * @li vertically maximized: @p horizontal is @c false and @p vertical is @c true
     * @li completely maximized: both @p horizontal and @p vertical are @c true
     * @param w The window whose maximized state changed
     * @param horizontal If @c true maximized horizontally
     * @param vertical If @c true maximized vertically
     * @since 4.7
     */
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
     */
    void windowGeometryShapeChanged(KWin::EffectWindow *w, const QRect &old);
    /**
     * This signal is emitted when the frame geometry of a window changed.
     * @param window The window whose geometry changed
     * @param oldGeometry The previous geometry
     * @since 5.19
     */
    void windowFrameGeometryChanged(KWin::EffectWindow *window, const QRect &oldGeometry);
    /**
     * Signal emitted when the padding of a window changed. (eg. shadow size)
     * @param w The window whose geometry changed
     * @param old The previous expandedGeometry()
     * @since 4.9
     */
    void windowPaddingChanged(KWin::EffectWindow *w, const QRect &old);
    /**
     * Signal emitted when the windows opacity is changed.
     * @param w The window whose opacity level is changed.
     * @param oldOpacity The previous opacity level
     * @param newOpacity The new opacity level
     * @since 4.7
     */
    void windowOpacityChanged(KWin::EffectWindow *w, qreal oldOpacity, qreal newOpacity);
    /**
     * Signal emitted when a window got minimized.
     * @param w The window which was minimized
     * @since 4.7
     */
    void windowMinimized(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window got unminimized.
     * @param w The window which was unminimized
     * @since 4.7
     */
    void windowUnminimized(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window either becomes modal (ie. blocking for its main client) or looses that state.
     * @param w The window which was unminimized
     * @since 4.11
     */
    void windowModalityChanged(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window either became unresponsive (eg. app froze or crashed)
     * or respoonsive
     * @param w The window that became (un)responsive
     * @param unresponsive Whether the window is responsive or unresponsive
     * @since 5.10
     */
    void windowUnresponsiveChanged(KWin::EffectWindow *w, bool unresponsive);
    /**
     * Signal emitted when an area of a window is scheduled for repainting.
     * Use this signal in an effect if another area needs to be synced as well.
     * @param w The window which is scheduled for repainting
     * @param r Always empty.
     * @since 4.7
     */
    void windowDamaged(KWin::EffectWindow *w, const QRect &r);
    /**
     * Signal emitted when a tabbox is added.
     * An effect who wants to replace the tabbox with itself should use refTabBox.
     * @param mode The TabBoxMode.
     * @see refTabBox
     * @see tabBoxClosed
     * @see tabBoxUpdated
     * @see tabBoxKeyEvent
     * @since 4.7
     */
    void tabBoxAdded(int mode);
    /**
     * Signal emitted when the TabBox was closed by KWin core.
     * An effect which referenced the TabBox should use unrefTabBox to unref again.
     * @see unrefTabBox
     * @see tabBoxAdded
     * @since 4.7
     */
    void tabBoxClosed();
    /**
     * Signal emitted when the selected TabBox window changed or the TabBox List changed.
     * An effect should only response to this signal if it referenced the TabBox with refTabBox.
     * @see refTabBox
     * @see currentTabBoxWindowList
     * @see currentTabBoxDesktopList
     * @see currentTabBoxWindow
     * @see currentTabBoxDesktop
     * @since 4.7
     */
    void tabBoxUpdated();
    /**
     * Signal emitted when a key event, which is not handled by TabBox directly is, happens while
     * TabBox is active. An effect might use the key event to e.g. change the selected window.
     * An effect should only response to this signal if it referenced the TabBox with refTabBox.
     * @param event The key event not handled by TabBox directly
     * @see refTabBox
     * @since 4.7
     */
    void tabBoxKeyEvent(QKeyEvent* event);
    void currentTabAboutToChange(KWin::EffectWindow* from, KWin::EffectWindow* to);
    void tabAdded(KWin::EffectWindow* from, KWin::EffectWindow* to);   // from merged with to
    void tabRemoved(KWin::EffectWindow* c, KWin::EffectWindow* group);   // c removed from group
    /**
     * Signal emitted when mouse changed.
     * If an effect needs to get updated mouse positions, it needs to first call startMousePolling.
     * For a fullscreen effect it is better to use an input window and react on windowInputMouseEvent.
     * @param pos The new mouse position
     * @param oldpos The previously mouse position
     * @param buttons The pressed mouse buttons
     * @param oldbuttons The previously pressed mouse buttons
     * @param modifiers Pressed keyboard modifiers
     * @param oldmodifiers Previously pressed keyboard modifiers.
     * @see startMousePolling
     * @since 4.7
     */
    void mouseChanged(const QPoint& pos, const QPoint& oldpos,
                              Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                              Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    /**
     * Signal emitted when the cursor shape changed.
     * You'll likely want to query the current cursor as reaction: xcb_xfixes_get_cursor_image_unchecked
     * Connection to this signal is tracked, so if you don't need it anymore, disconnect from it to stop cursor event filtering
     */
    void cursorShapeChanged();
    /**
     * Receives events registered for using registerPropertyType.
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
     * Signal emitted after the screen geometry changed (e.g. add of a monitor).
     * Effects using displayWidth()/displayHeight() to cache information should
     * react on this signal and update the caches.
     * @param size The new screen size
     * @since 4.8
     */
    void screenGeometryChanged(const QSize &size);

    /**
     * This signal is emitted when the global
     * activity is changed
     * @param id id of the new current activity
     * @since 4.9
     */
    void currentActivityChanged(const QString &id);
    /**
     * This signal is emitted when a new activity is added
     * @param id id of the new activity
     * @since 4.9
     */
    void activityAdded(const QString &id);
    /**
     * This signal is emitted when the activity
     * is removed
     * @param id id of the removed activity
     * @since 4.9
     */
    void activityRemoved(const QString &id);
    /**
     * This signal is emitted when the screen got locked or unlocked.
     * @param locked @c true if the screen is now locked, @c false if it is now unlocked
     * @since 4.11
     */
    void screenLockingChanged(bool locked);

    /**
     * This signal is emitted just before the screen locker tries to grab keys and lock the screen
     * Effects should release any grabs immediately
     * @since 5.17
     */
    void screenAboutToLock();

    /**
     * This signels is emitted when ever the stacking order is change, ie. a window is risen
     * or lowered
     * @since 4.10
     */
    void stackingOrderChanged();
    /**
     * This signal is emitted when the user starts to approach the @p border with the mouse.
     * The @p factor describes how far away the mouse is in a relative mean. The values are in
     * [0.0, 1.0] with 0.0 being emitted when first entered and on leaving. The value 1.0 means that
     * the @p border is reached with the mouse. So the values are well suited for animations.
     * The signal is always emitted when the mouse cursor position changes.
     * @param border The screen edge which is being approached
     * @param factor Value in range [0.0,1.0] to describe how close the mouse is to the border
     * @param geometry The geometry of the edge which is being approached
     * @since 4.11
     */
    void screenEdgeApproaching(ElectricBorder border, qreal factor, const QRect &geometry);
    /**
     * Emitted whenever the virtualScreenSize changes.
     * @see virtualScreenSize()
     * @since 5.0
     */
    void virtualScreenSizeChanged();
    /**
     * Emitted whenever the virtualScreenGeometry changes.
     * @see virtualScreenGeometry()
     * @since 5.0
     */
    void virtualScreenGeometryChanged();

    /**
     * The window @p w gets shown again. The window was previously
     * initially shown with windowAdded and hidden with windowHidden.
     *
     * @see windowHidden
     * @see windowAdded
     * @since 5.8
     */
    void windowShown(KWin::EffectWindow *w);

    /**
     * The window @p w got hidden but not yet closed.
     * This can happen when a window is still being used and is supposed to be shown again
     * with windowShown. On X11 an example is autohiding panels. On Wayland every
     * window first goes through the window hidden state and might get shown again, or might
     * get closed the normal way.
     *
     * @see windowShown
     * @see windowClosed
     * @since 5.8
     */
    void windowHidden(KWin::EffectWindow *w);

    /**
     * This signal gets emitted when the data on EffectWindow @p w for @p role changed.
     *
     * An Effect can connect to this signal to read the new value and react on it.
     * E.g. an Effect which does not operate on windows grabbed by another Effect wants
     * to cancel the already scheduled animation if another Effect adds a grab.
     *
     * @param w The EffectWindow for which the data changed
     * @param role The data role which changed
     * @see EffectWindow::setData
     * @see EffectWindow::data
     * @since 5.8.4
     */
    void windowDataChanged(KWin::EffectWindow *w, int role);

    /**
     * The xcb connection changed, either a new xcbConnection got created or the existing one
     * got destroyed.
     * Effects can use this to refetch the properties they want to set.
     *
     * When the xcbConnection changes also the x11RootWindow becomes invalid.
     * @see xcbConnection
     * @see x11RootWindow
     * @since 5.11
     */
    void xcbConnectionChanged();

    /**
     * This signal is emitted when active fullscreen effect changed.
     *
     * @see activeFullScreenEffect
     * @see setActiveFullScreenEffect
     * @since 5.14
     */
    void activeFullScreenEffectChanged();

    /**
     * This signal is emitted when active fullscreen effect changed to being
     * set or unset
     *
     * @see activeFullScreenEffect
     * @see setActiveFullScreenEffect
     * @since 5.15
     */
    void hasActiveFullScreenEffectChanged();

    /**
     * This signal is emitted when the keep above state of @p w was changed.
     *
     * @param w The window whose the keep above state was changed.
     * @since 5.15
     */
    void windowKeepAboveChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the keep below state of @p was changed.
     *
     * @param w The window whose the keep below state was changed.
     * @since 5.15
     */
    void windowKeepBelowChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the full screen state of @p w was changed.
     *
     * @param w The window whose the full screen state was changed.
     * @since 5.15
     */
    void windowFullScreenChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the session state was changed
     * @since 5.18
     */
    void sessionStateChanged();

protected:
    QVector< EffectPair > loaded_effects;
    //QHash< QString, EffectFactory* > effect_factories;
    CompositingType compositing_type;
};


/**
 * @short Representation of a window used by/for Effect classes.
 *
 * The purpose is to hide internal data and also to serve as a single
 *  representation for the case when Client/Unmanaged becomes Deleted.
 */
class KWINEFFECTS_EXPORT EffectWindow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool alpha READ hasAlpha CONSTANT)
    Q_PROPERTY(QRect geometry READ geometry)
    Q_PROPERTY(QRect expandedGeometry READ expandedGeometry)
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
     * See _NET_WM_WINDOW_TYPE_DESKTOP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool desktopWindow READ isDesktop)
    /**
     * Returns whether the window is a dock (i.e. a panel).
     * See _NET_WM_WINDOW_TYPE_DOCK at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dock READ isDock)
    /**
     * Returns whether the window is a standalone (detached) toolbar window.
     * See _NET_WM_WINDOW_TYPE_TOOLBAR at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool toolbar READ isToolbar)
    /**
     * Returns whether the window is a torn-off menu.
     * See _NET_WM_WINDOW_TYPE_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool menu READ isMenu)
    /**
     * Returns whether the window is a "normal" window, i.e. an application or any other window
     * for which none of the specialized window types fit.
     * See _NET_WM_WINDOW_TYPE_NORMAL at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool normalWindow READ isNormalWindow)
    /**
     * Returns whether the window is a dialog window.
     * See _NET_WM_WINDOW_TYPE_DIALOG at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dialog READ isDialog)
    /**
     * Returns whether the window is a splashscreen. Note that many (especially older) applications
     * do not support marking their splash windows with this type.
     * See _NET_WM_WINDOW_TYPE_SPLASH at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool splash READ isSplash)
    /**
     * Returns whether the window is a utility window, such as a tool window.
     * See _NET_WM_WINDOW_TYPE_UTILITY at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool utility READ isUtility)
    /**
     * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
     * from the applications menubar).
     * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dropdownMenu READ isDropdownMenu)
    /**
     * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
     * See _NET_WM_WINDOW_TYPE_POPUP_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool popupMenu READ isPopupMenu)
    /**
     * Returns whether the window is a tooltip.
     * See _NET_WM_WINDOW_TYPE_TOOLTIP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool tooltip READ isTooltip)
    /**
     * Returns whether the window is a window with a notification.
     * See _NET_WM_WINDOW_TYPE_NOTIFICATION at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool notification READ isNotification)
    /**
     * Returns whether the window is a window with a critical notification.
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_CRITICAL_NOTIFICATION
     */
    Q_PROPERTY(bool criticalNotification READ isCriticalNotification)
    /**
     * Returns whether the window is an on screen display window
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY
     */
    Q_PROPERTY(bool onScreenDisplay READ isOnScreenDisplay)
    /**
     * Returns whether the window is a combobox popup.
     * See _NET_WM_WINDOW_TYPE_COMBO at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool comboBox READ isComboBox)
    /**
     * Returns whether the window is a Drag&Drop icon.
     * See _NET_WM_WINDOW_TYPE_DND at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dndIcon READ isDNDIcon)
    /**
     * Returns the NETWM window type
     * See https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(int windowType READ windowType)
    /**
     * Whether this EffectWindow is managed by KWin (it has control over its placement and other
     * aspects, as opposed to override-redirect windows that are entirely handled by the application).
     */
    Q_PROPERTY(bool managed READ isManaged)
    /**
     * Whether this EffectWindow represents an already deleted window and only kept for the compositor for animations.
     */
    Q_PROPERTY(bool deleted READ isDeleted)
    /**
     * Whether the window has an own shape
     */
    Q_PROPERTY(bool shaped READ hasOwnShape)
    /**
     * The Window's shape
     */
    Q_PROPERTY(QRegion shape READ shape)
    /**
     * The Caption of the window. Read from WM_NAME property together with a suffix for hostname and shortcut.
     */
    Q_PROPERTY(QString caption READ caption)
    /**
     * Whether the window is set to be kept above other windows.
     */
    Q_PROPERTY(bool keepAbove READ keepAbove)
    /**
     * Whether the window is set to be kept below other windows.
     */
    Q_PROPERTY(bool keepBelow READ keepBelow)
    /**
     * Whether the window is minimized.
     */
    Q_PROPERTY(bool minimized READ isMinimized WRITE setMinimized)
    /**
     * Whether the window represents a modal window.
     */
    Q_PROPERTY(bool modal READ isModal)
    /**
     * Whether the window is moveable. Even if it is not moveable, it might be possible to move
     * it to another screen.
     * @see moveableAcrossScreens
     */
    Q_PROPERTY(bool moveable READ isMovable)
    /**
     * Whether the window can be moved to another screen.
     * @see moveable
     */
    Q_PROPERTY(bool moveableAcrossScreens READ isMovableAcrossScreens)
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     */
    Q_PROPERTY(QSize basicUnit READ basicUnit)
    /**
     * Whether the window is currently being moved by the user.
     */
    Q_PROPERTY(bool move READ isUserMove)
    /**
     * Whether the window is currently being resized by the user.
     */
    Q_PROPERTY(bool resize READ isUserResize)
    /**
     * The optional geometry representing the minimized Client in e.g a taskbar.
     * See _NET_WM_ICON_GEOMETRY at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(QRect iconGeometry READ iconGeometry)
    /**
     * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
     * i.e. window types that usually don't have a window frame and the user does not use window
     * management (moving, raising,...) on them.
     */
    Q_PROPERTY(bool specialWindow READ isSpecialWindow)
    Q_PROPERTY(QIcon icon READ icon)
    /**
     * Whether the window should be excluded from window switching effects.
     */
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
    /**
     * Whether the decoration currently uses an alpha channel.
     * @since 4.10
     */
    Q_PROPERTY(bool decorationHasAlpha READ decorationHasAlpha)
    /**
     * Whether the window is currently visible to the user, that is:
     * <ul>
     * <li>Not minimized</li>
     * <li>On current desktop</li>
     * <li>On current activity</li>
     * </ul>
     * @since 4.11
     */
    Q_PROPERTY(bool visible READ isVisible)
    /**
     * Whether the window does not want to be animated on window close.
     * In case this property is @c true it is not useful to start an animation on window close.
     * The window will not be visible, but the animation hooks are executed.
     * @since 5.0
     */
    Q_PROPERTY(bool skipsCloseAnimation READ skipsCloseAnimation)

    /**
     * Interface to the corresponding wayland surface.
     * relevant only in Wayland, on X11 it will be nullptr
     */
    Q_PROPERTY(KWaylandServer::SurfaceInterface *surface READ surface)

    /**
     * Whether the window is fullscreen.
     * @since 5.6
     */
    Q_PROPERTY(bool fullScreen READ isFullScreen)

    /**
     * Whether this client is unresponsive.
     *
     * When an application failed to react on a ping request in time, it is
     * considered unresponsive. This usually indicates that the application froze or crashed.
     *
     * @since 5.10
     */
    Q_PROPERTY(bool unresponsive READ isUnresponsive)

    /**
     * Whether this is a Wayland client.
     * @since 5.15
     */
    Q_PROPERTY(bool waylandClient READ isWaylandClient CONSTANT)

    /**
     * Whether this is an X11 client.
     * @since 5.15
     */
    Q_PROPERTY(bool x11Client READ isX11Client CONSTANT)

    /**
     * Whether the window is a popup.
     *
     * A popup is a window that can be used to implement tooltips, combo box popups,
     * popup menus and other similar user interface concepts.
     *
     * @since 5.15
     */
    Q_PROPERTY(bool popupWindow READ isPopupWindow CONSTANT)

    /**
     * KWin internal window. Specific to Wayland platform.
     *
     * If the EffectWindow does not reference an internal window, this property is @c null.
     * @since 5.16
     */
    Q_PROPERTY(QWindow *internalWindow READ internalWindow CONSTANT)
    
    /**
     * Whether this EffectWindow represents the outline.
     *
     * When compositing is turned on, the outline is an actual window.
     *
     * @since 5.16
     */
    Q_PROPERTY(bool outline READ isOutline CONSTANT)

    /**
     * The PID of the application this window belongs to.
     *
     * @since 5.18
     */
    Q_PROPERTY(pid_t pid READ pid CONSTANT)

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
        /**  Deprecated, tab groups have been removed: Window will not be painted because it is not the active window in a client group */
        PAINT_DISABLED_BY_TAB_GROUP = 1 << 4,
        /**  Window will not be painted because it's not on the current activity  */
        PAINT_DISABLED_BY_ACTIVITY     = 1 << 5
    };

    explicit EffectWindow(QObject *parent = nullptr);
    ~EffectWindow() override;

    virtual void enablePainting(int reason) = 0;
    virtual void disablePainting(int reason) = 0;
    virtual bool isPaintingEnabled() = 0;
    Q_SCRIPTABLE virtual void addRepaint(const QRect &r) = 0;
    Q_SCRIPTABLE virtual void addRepaint(int x, int y, int w, int h) = 0;
    Q_SCRIPTABLE virtual void addRepaintFull() = 0;
    Q_SCRIPTABLE virtual void addLayerRepaint(const QRect &r) = 0;
    Q_SCRIPTABLE virtual void addLayerRepaint(int x, int y, int w, int h) = 0;

    virtual void refWindow() = 0;
    virtual void unrefWindow() = 0;

    virtual bool isDeleted() const = 0;

    virtual bool isMinimized() const = 0;
    virtual double opacity() const = 0;
    virtual bool hasAlpha() const = 0;

    bool isOnCurrentActivity() const;
    Q_SCRIPTABLE bool isOnActivity(const QString &id) const;
    bool isOnAllActivities() const;
    virtual QStringList activities() const = 0;

    Q_SCRIPTABLE bool isOnDesktop(int d) const;
    bool isOnCurrentDesktop() const;
    bool isOnAllDesktops() const;
    /**
     * The desktop this window is in. This makes sense only on X11
     * where desktops are mutually exclusive, on Wayland it's the last
     * desktop the window has been added to.
     * use desktops() instead.
     * @see desktops()
     * @deprecated
     */
#ifndef KWIN_NO_DEPRECATED
    virtual int KWIN_DEPRECATED desktop() const = 0; // prefer isOnXXX()
#endif
    /**
     * All the desktops by number that the window is in. On X11 this list will always have
     * a length of 1, on Wayland can be any subset.
     * If the list is empty it means the window is on all desktops
     */
    virtual QVector<uint> desktops() const = 0;

    virtual int x() const = 0;
    virtual int y() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     */
    virtual QSize basicUnit() const = 0;
    /**
     * @deprecated Use frameGeometry() instead.
     */
    virtual QRect geometry() const = 0;
    /**
     * Returns the geometry of the window excluding server-side and client-side
     * drop-shadows.
     *
     * @since 5.18
     */
    virtual QRect frameGeometry() const = 0;
    /**
     * Returns the geometry of the pixmap or buffer attached to this window.
     *
     * For X11 clients, this method returns server-side geometry of the Toplevel.
     *
     * For Wayland clients, this method returns rectangle that the main surface
     * occupies on the screen, in global screen coordinates.
     *
     * @since 5.18
     */
    virtual QRect bufferGeometry() const = 0;
    /**
     * Geometry of the window including decoration and potentially shadows.
     * May be different from geometry() if the window has a shadow.
     * @since 4.9
     */
    virtual QRect expandedGeometry() const = 0;
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
    /**
     * Geometry of the transparent rect in the decoration.
     * May be different from contentsRect() if the decoration is extended into the client area.
     * @since 4.5
     */
    virtual QRect decorationInnerRect() const = 0;
    bool hasDecoration() const;
    virtual bool decorationHasAlpha() const = 0;
    virtual QByteArray readProperty(long atom, long type, int format) const = 0;
    virtual void deleteProperty(long atom) const = 0;

    virtual QString caption() const = 0;
    virtual QIcon icon() const = 0;
    virtual QString windowClass() const = 0;
    virtual QString windowRole() const = 0;
    virtual const EffectWindowGroup* group() const = 0;

    /**
     * Returns whether the window is a desktop background window (the one with wallpaper).
     * See _NET_WM_WINDOW_TYPE_DESKTOP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isDesktop() const = 0;
    /**
     * Returns whether the window is a dock (i.e. a panel).
     * See _NET_WM_WINDOW_TYPE_DOCK at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isDock() const = 0;
    /**
     * Returns whether the window is a standalone (detached) toolbar window.
     * See _NET_WM_WINDOW_TYPE_TOOLBAR at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isToolbar() const = 0;
    /**
     * Returns whether the window is a torn-off menu.
     * See _NET_WM_WINDOW_TYPE_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isMenu() const = 0;
    /**
     * Returns whether the window is a "normal" window, i.e. an application or any other window
     * for which none of the specialized window types fit.
     * See _NET_WM_WINDOW_TYPE_NORMAL at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
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
     * See _NET_WM_WINDOW_TYPE_DIALOG at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isDialog() const = 0;
    /**
     * Returns whether the window is a splashscreen. Note that many (especially older) applications
     * do not support marking their splash windows with this type.
     * See _NET_WM_WINDOW_TYPE_SPLASH at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isSplash() const = 0;
    /**
     * Returns whether the window is a utility window, such as a tool window.
     * See _NET_WM_WINDOW_TYPE_UTILITY at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isUtility() const = 0;
    /**
     * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
     * from the applications menubar).
     * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isDropdownMenu() const = 0;
    /**
     * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
     * See _NET_WM_WINDOW_TYPE_POPUP_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isPopupMenu() const = 0; // a context popup, not dropdown, not torn-off
    /**
     * Returns whether the window is a tooltip.
     * See _NET_WM_WINDOW_TYPE_TOOLTIP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isTooltip() const = 0;
    /**
     * Returns whether the window is a window with a notification.
     * See _NET_WM_WINDOW_TYPE_NOTIFICATION at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isNotification() const = 0;
    /**
     * Returns whether the window is a window with a critical notification.
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_CRITICAL_NOTIFICATION
     */
    virtual bool isCriticalNotification() const = 0;
    /**
     * Returns whether the window is an on screen display window
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY
     */
    virtual bool isOnScreenDisplay() const = 0;
    /**
     * Returns whether the window is a combobox popup.
     * See _NET_WM_WINDOW_TYPE_COMBO at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isComboBox() const = 0;
    /**
     * Returns whether the window is a Drag&Drop icon.
     * See _NET_WM_WINDOW_TYPE_DND at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual bool isDNDIcon() const = 0;
    /**
     * Returns the NETWM window type
     * See https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    virtual NET::WindowType windowType() const = 0;
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
    /**
     * Returns whether the window is kept below all other windows.
     */
    virtual bool keepBelow() const = 0;

    virtual bool isModal() const = 0;
    Q_SCRIPTABLE virtual KWin::EffectWindow* findModal() = 0;
    Q_SCRIPTABLE virtual KWin::EffectWindow* transientFor() = 0;
    Q_SCRIPTABLE virtual QList<KWin::EffectWindow*> mainWindows() const = 0;

    /**
     * Returns whether the window should be excluded from window switching effects.
     * @since 4.5
     */
    virtual bool isSkipSwitcher() const = 0;

    /**
     * Returns the unmodified window quad list. Can also be used to force rebuilding.
     */
    virtual WindowQuadList buildQuads(bool force = false) const = 0;

    void setMinimized(bool minimize);
    virtual void minimize() = 0;
    virtual void unminimize() = 0;
    Q_SCRIPTABLE virtual void closeWindow() = 0;

    /// deprecated
    virtual bool isCurrentTab() const = 0;

    /**
     * @since 4.11
     */
    bool isVisible() const;

    /**
     * @since 5.0
     */
    virtual bool skipsCloseAnimation() const = 0;

    /**
     * @since 5.5
     */
    virtual KWaylandServer::SurfaceInterface *surface() const = 0;

    /**
     * @since 5.6
     */
    virtual bool isFullScreen() const = 0;

    /**
     * @since 5.10
     */
    virtual bool isUnresponsive() const = 0;

    /**
     * @since 5.15
     */
    virtual bool isWaylandClient() const = 0;

    /**
     * @since 5.15
     */
    virtual bool isX11Client() const = 0;

    /**
     * @since 5.15
     */
    virtual bool isPopupWindow() const = 0;

    /**
     * @since 5.16
     */
    virtual QWindow *internalWindow() const = 0;

    /**
     * @since 5.16
     */
    virtual bool isOutline() const = 0;

    /**
     * @since 5.18
     */
    virtual pid_t pid() const = 0;

    /**
     * Can be used to by effects to store arbitrary data in the EffectWindow.
     *
     * Invoking this method will emit the signal EffectsHandler::windowDataChanged.
     * @see EffectsHandler::windowDataChanged
     */
    Q_SCRIPTABLE virtual void setData(int role, const QVariant &data) = 0;
    Q_SCRIPTABLE virtual QVariant data(int role) const = 0;

    /**
     * @brief References the previous window pixmap to prevent discarding.
     *
     * This method allows to reference the previous window pixmap in case that a window changed
     * its size, which requires a new window pixmap. By referencing the previous (and then outdated)
     * window pixmap an effect can for example cross fade the current window pixmap with the previous
     * one. This allows for smoother transitions for window geometry changes.
     *
     * If an effect calls this method on a window it also needs to call unreferencePreviousWindowPixmap
     * once it does no longer need the previous window pixmap.
     *
     * Note: the window pixmap is not kept forever even when referenced. If the geometry changes again, so that
     * a new window pixmap is created, the previous window pixmap will be exchanged with the current one. This
     * means it's still possible to have rendering glitches. An effect is supposed to track for itself the changes
     * to the window's geometry and decide how the transition should continue in such a situation.
     *
     * @see unreferencePreviousWindowPixmap
     * @since 4.11
     */
    virtual void referencePreviousWindowPixmap() = 0;
    /**
     * @brief Unreferences the previous window pixmap. Only relevant after referencePreviousWindowPixmap had
     * been called.
     *
     * @see referencePreviousWindowPixmap
     * @since 4.11
     */
    virtual void unreferencePreviousWindowPixmap() = 0;

private:
    class Private;
    QScopedPointer<Private> d;
};

class KWINEFFECTS_EXPORT EffectWindowGroup
{
public:
    virtual ~EffectWindowGroup();
    virtual EffectWindowList members() const = 0;
};


struct GLVertex2D
{
    QVector2D position;
    QVector2D texcoord;
};

struct GLVertex3D
{
    QVector3D position;
    QVector2D texcoord;
};


/**
 * @short Vertex class
 *
 * A vertex is one position in a window. WindowQuad consists of four WindowVertex objects
 * and represents one part of a window.
 */
class KWINEFFECTS_EXPORT WindowVertex
{
public:
    WindowVertex();
    WindowVertex(const QPointF &position, const QPointF &textureCoordinate);
    WindowVertex(double x, double y, double tx, double ty);

    double x() const { return px; }
    double y() const { return py; }
    double u() const { return tx; }
    double v() const { return ty; }
    double originalX() const { return ox; }
    double originalY() const { return oy; }
    double textureX() const { return tx; }
    double textureY() const { return ty; }
    void move(double x, double y);
    void setX(double x);
    void setY(double y);

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
class KWINEFFECTS_EXPORT WindowQuad
{
public:
    explicit WindowQuad(WindowQuadType type, int id = -1);
    WindowQuad makeSubQuad(double x1, double y1, double x2, double y2) const;
    WindowVertex& operator[](int index);
    const WindowVertex& operator[](int index) const;
    WindowQuadType type() const;
    void setUVAxisSwapped(bool value) { uvSwapped = value; }
    bool uvAxisSwapped() const { return uvSwapped; }
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
    bool uvSwapped;
    int quadID;
};

class KWINEFFECTS_EXPORT WindowQuadList
    : public QVector<WindowQuad>
{
public:
    WindowQuadList splitAtX(double x) const;
    WindowQuadList splitAtY(double y) const;
    WindowQuadList makeGrid(int maxquadsize) const;
    WindowQuadList makeRegularGrid(int xSubdivisions, int ySubdivisions) const;
    WindowQuadList select(WindowQuadType type) const;
    WindowQuadList filterOut(WindowQuadType type) const;
    bool smoothNeeded() const;
    void makeInterleavedArrays(unsigned int type, GLVertex2D *vertices, const QMatrix4x4 &matrix) const;
    void makeArrays(float** vertices, float** texcoords, const QSizeF &size, bool yInverted) const;
    bool isTransformed() const;
};

class KWINEFFECTS_EXPORT WindowPrePaintData
{
public:
    int mask;
    /**
     * Region that will be painted, in screen coordinates.
     */
    QRegion paint;
    /**
     * The clip region will be subtracted from paint region of following windows.
     * I.e. window will definitely cover it's clip region
     */
    QRegion clip;
    WindowQuadList quads;
    /**
     * Simple helper that sets data to say the window will be painted as non-opaque.
     * Takes also care of changing the regions.
     */
    void setTranslucent();
    /**
     * Helper to mark that this window will be transformed
     */
    void setTransformed();
};

class KWINEFFECTS_EXPORT PaintData
{
public:
    virtual ~PaintData();
    /**
     * @returns scale factor in X direction.
     * @since 4.10
     */
    qreal xScale() const;
    /**
     * @returns scale factor in Y direction.
     * @since 4.10
     */
    qreal yScale() const;
    /**
     * @returns scale factor in Z direction.
     * @since 4.10
     */
    qreal zScale() const;
    /**
     * Sets the scale factor in X direction to @p scale
     * @param scale The scale factor in X direction
     * @since 4.10
     */
    void setXScale(qreal scale);
    /**
     * Sets the scale factor in Y direction to @p scale
     * @param scale The scale factor in Y direction
     * @since 4.10
     */
    void setYScale(qreal scale);
    /**
     * Sets the scale factor in Z direction to @p scale
     * @param scale The scale factor in Z direction
     * @since 4.10
     */
    void setZScale(qreal scale);
    /**
     * Sets the scale factor in X and Y direction.
     * @param scale The scale factor for X and Y direction
     * @since 4.10
     */
    void setScale(const QVector2D &scale);
    /**
     * Sets the scale factor in X, Y and Z direction
     * @param scale The scale factor for X, Y and Z direction
     * @since 4.10
     */
    void setScale(const QVector3D &scale);
    const QGraphicsScale &scale() const;
    const QVector3D &translation() const;
    /**
     * @returns the translation in X direction.
     * @since 4.10
     */
    qreal xTranslation() const;
    /**
     * @returns the translation in Y direction.
     * @since 4.10
     */
    qreal yTranslation() const;
    /**
     * @returns the translation in Z direction.
     * @since 4.10
     */
    qreal zTranslation() const;
    /**
     * Sets the translation in X direction to @p translate.
     * @since 4.10
     */
    void setXTranslation(qreal translate);
    /**
     * Sets the translation in Y direction to @p translate.
     * @since 4.10
     */
    void setYTranslation(qreal translate);
    /**
     * Sets the translation in Z direction to @p translate.
     * @since 4.10
     */
    void setZTranslation(qreal translate);
    /**
     * Performs a translation by adding the values component wise.
     * @param x Translation in X direction
     * @param y Translation in Y direction
     * @param z Translation in Z direction
     * @since 4.10
     */
    void translate(qreal x, qreal y = 0.0, qreal z = 0.0);
    /**
     * Performs a translation by adding the values component wise.
     * Overloaded method for convenience.
     * @param translate The translation
     * @since 4.10
     */
    void translate(const QVector3D &translate);

    /**
     * Sets the rotation angle.
     * @param angle The new rotation angle.
     * @since 4.10
     * @see rotationAngle()
     */
    void setRotationAngle(qreal angle);
    /**
     * Returns the rotation angle.
     * Initially 0.0.
     * @returns The current rotation angle.
     * @since 4.10
     * @see setRotationAngle
     */
    qreal rotationAngle() const;
    /**
     * Sets the rotation origin.
     * @param origin The new rotation origin.
     * @since 4.10
     * @see rotationOrigin()
     */
    void setRotationOrigin(const QVector3D &origin);
    /**
     * Returns the rotation origin. That is the point in space which is fixed during the rotation.
     * Initially this is 0/0/0.
     * @returns The rotation's origin
     * @since 4.10
     * @see setRotationOrigin()
     */
    QVector3D rotationOrigin() const;
    /**
     * Sets the rotation axis.
     * Set a component to 1.0 to rotate around this axis and to 0.0 to disable rotation around the
     * axis.
     * @param axis A vector holding information on which axis to rotate
     * @since 4.10
     * @see rotationAxis()
     */
    void setRotationAxis(const QVector3D &axis);
    /**
     * Sets the rotation axis.
     * Overloaded method for convenience.
     * @param axis The axis around which should be rotated.
     * @since 4.10
     * @see rotationAxis()
     */
    void setRotationAxis(Qt::Axis axis);
    /**
     * The current rotation axis.
     * By default the rotation is (0/0/1) which means a rotation around the z axis.
     * @returns The current rotation axis.
     * @since 4.10
     * @see setRotationAxis
     */
    QVector3D rotationAxis() const;

protected:
    PaintData();
    PaintData(const PaintData &other);

private:
    PaintDataPrivate * const d;
};

class KWINEFFECTS_EXPORT WindowPaintData : public PaintData
{
public:
    explicit WindowPaintData(EffectWindow* w);
    explicit WindowPaintData(EffectWindow* w, const QMatrix4x4 &screenProjectionMatrix);
    WindowPaintData(const WindowPaintData &other);
    ~WindowPaintData() override;
    /**
     * Scales the window by @p scale factor.
     * Multiplies all three components by the given factor.
     * @since 4.10
     */
    WindowPaintData& operator*=(qreal scale);
    /**
     * Scales the window by @p scale factor.
     * Performs a component wise multiplication on x and y components.
     * @since 4.10
     */
    WindowPaintData& operator*=(const QVector2D &scale);
    /**
     * Scales the window by @p scale factor.
     * Performs a component wise multiplication.
     * @since 4.10
     */
    WindowPaintData& operator*=(const QVector3D &scale);
    /**
     * Translates the window by the given @p translation and returns a reference to the ScreenPaintData.
     * @since 4.10
     */
    WindowPaintData& operator+=(const QPointF &translation);
    /**
     * Translates the window by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    WindowPaintData& operator+=(const QPoint &translation);
    /**
     * Translates the window by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    WindowPaintData& operator+=(const QVector2D &translation);
    /**
     * Translates the window by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    WindowPaintData& operator+=(const QVector3D &translation);
    /**
     * Window opacity, in range 0 = transparent to 1 = fully opaque
     * @see setOpacity
     * @since 4.10
     */
    qreal opacity() const;
    /**
     * Sets the window opacity to the new @p opacity.
     * If you want to modify the existing opacity level consider using multiplyOpacity.
     * @param opacity The new opacity level
     * @since 4.10
     */
    void setOpacity(qreal opacity);
    /**
     * Multiplies the current opacity with the @p factor.
     * @param factor Factor with which the opacity should be multiplied
     * @return New opacity level
     * @since 4.10
     */
    qreal multiplyOpacity(qreal factor);
    /**
     * Saturation of the window, in range [0; 1]
     * 1 means that the window is unchanged, 0 means that it's completely
     *  unsaturated (greyscale). 0.5 would make the colors less intense,
     *  but not completely grey
     * Use EffectsHandler::saturationSupported() to find out whether saturation
     * is supported by the system, otherwise this value has no effect.
     * @return The current saturation
     * @see setSaturation()
     * @since 4.10
     */
    qreal saturation() const;
    /**
     * Sets the window saturation level to @p saturation.
     * If you want to modify the existing saturation level consider using multiplySaturation.
     * @param saturation The new saturation level
     * @since 4.10
     */
    void setSaturation(qreal saturation) const;
    /**
     * Multiplies the current saturation with @p factor.
     * @param factor with which the saturation should be multiplied
     * @return New saturation level
     * @since 4.10
     */
    qreal multiplySaturation(qreal factor);
    /**
     * Brightness of the window, in range [0; 1]
     * 1 means that the window is unchanged, 0 means that it's completely
     * black. 0.5 would make it 50% darker than usual
     */
    qreal brightness() const;
    /**
     * Sets the window brightness level to @p brightness.
     * If you want to modify the existing brightness level consider using multiplyBrightness.
     * @param brightness The new brightness level
     */
    void setBrightness(qreal brightness);
    /**
     * Multiplies the current brightness level with @p factor.
     * @param factor with which the brightness should be multiplied.
     * @return New brightness level
     * @since 4.10
     */
    qreal multiplyBrightness(qreal factor);
    /**
     * The screen number for which the painting should be done.
     * This affects color correction (different screens may need different
     * color correction lookup tables because they have different ICC profiles).
     * @return screen for which painting should be done
     */
    int screen() const;
    /**
     * @param screen New screen number
     * A value less than 0 will indicate that a default profile should be done.
     */
    void setScreen(int screen) const;
    /**
     * @brief Sets the cross fading @p factor to fade over with previously sized window.
     * If @c 1.0 only the current window is used, if @c 0.0 only the previous window is used.
     *
     * By default only the current window is used. This factor can only make any visual difference
     * if the previous window get referenced.
     *
     * @param factor The cross fade factor between @c 0.0 (previous window) and @c 1.0 (current window)
     * @see crossFadeProgress
     */
    void setCrossFadeProgress(qreal factor);
    /**
     * @see setCrossFadeProgress
     */
    qreal crossFadeProgress() const;

    /**
     * Sets the projection matrix that will be used when painting the window.
     *
     * The default projection matrix can be overridden by setting this matrix
     * to a non-identity matrix.
     */
    void setProjectionMatrix(const QMatrix4x4 &matrix);

    /**
     * Returns the current projection matrix.
     *
     * The default value for this matrix is the identity matrix.
     */
    QMatrix4x4 projectionMatrix() const;

    /**
     * Returns a reference to the projection matrix.
     */
    QMatrix4x4 &rprojectionMatrix();

    /**
     * Sets the model-view matrix that will be used when painting the window.
     *
     * The default model-view matrix can be overridden by setting this matrix
     * to a non-identity matrix.
     */
    void setModelViewMatrix(const QMatrix4x4 &matrix);

    /**
     * Returns the current model-view matrix.
     *
     * The default value for this matrix is the identity matrix.
     */
    QMatrix4x4 modelViewMatrix() const;

    /**
     * Returns a reference to the model-view matrix.
     */
    QMatrix4x4 &rmodelViewMatrix();

    /**
     * Returns The projection matrix as used by the current screen painting pass
     * including screen transformations.
     *
     * @since 5.6
     */
    QMatrix4x4 screenProjectionMatrix() const;

    WindowQuadList quads;

    /**
     * Shader to be used for rendering, if any.
     */
    GLShader* shader;

private:
    WindowPaintDataPrivate * const d;
};

class KWINEFFECTS_EXPORT ScreenPaintData : public PaintData
{
public:
    ScreenPaintData();
    ScreenPaintData(const QMatrix4x4 &projectionMatrix, const QRect &outputGeometry = QRect(), const qreal screenScale = 1.0);
    ScreenPaintData(const ScreenPaintData &other);
    ~ScreenPaintData() override;
    /**
     * Scales the screen by @p scale factor.
     * Multiplies all three components by the given factor.
     * @since 4.10
     */
    ScreenPaintData& operator*=(qreal scale);
    /**
     * Scales the screen by @p scale factor.
     * Performs a component wise multiplication on x and y components.
     * @since 4.10
     */
    ScreenPaintData& operator*=(const QVector2D &scale);
    /**
     * Scales the screen by @p scale factor.
     * Performs a component wise multiplication.
     * @since 4.10
     */
    ScreenPaintData& operator*=(const QVector3D &scale);
    /**
     * Translates the screen by the given @p translation and returns a reference to the ScreenPaintData.
     * @since 4.10
     */
    ScreenPaintData& operator+=(const QPointF &translation);
    /**
     * Translates the screen by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    ScreenPaintData& operator+=(const QPoint &translation);
    /**
     * Translates the screen by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    ScreenPaintData& operator+=(const QVector2D &translation);
    /**
     * Translates the screen by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    ScreenPaintData& operator+=(const QVector3D &translation);
    ScreenPaintData& operator=(const ScreenPaintData &rhs);

    /**
     * The projection matrix used by the scene for the current rendering pass.
     * On non-OpenGL compositors it's set to Identity matrix.
     * @since 5.6
     */
    QMatrix4x4 projectionMatrix() const;

    /**
     * The geometry of the currently rendered output.
     * Only set for per-output rendering (e.g. Wayland).
     *
     * This geometry can be used as a hint about the native window the OpenGL context
     * is bound. OpenGL calls need to be translated to this geometry.
     * @since 5.9
     */
    QRect outputGeometry() const;

    /**
     * The scale factor for the output
     *
     * @since 5.19
     */
    qreal screenScale() const;
private:
    class Private;
    QScopedPointer<Private> d;
};

class KWINEFFECTS_EXPORT ScreenPrePaintData
{
public:
    int mask;
    QRegion paint;
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
class KWINEFFECTS_EXPORT PaintClipper
{
public:
    /**
     * Calls push().
     */
    explicit PaintClipper(const QRegion& allowed_area);
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
    class KWINEFFECTS_EXPORT Iterator
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
class KWINEFFECTS_EXPORT Motion
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
class KWINEFFECTS_EXPORT Motion1D : public Motion<double>
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
class KWINEFFECTS_EXPORT Motion2D : public Motion<QPointF>
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
class KWINEFFECTS_EXPORT WindowMotionManager
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
    inline void manage(const EffectWindowList &list) {
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
class KWINEFFECTS_EXPORT EffectFrame
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
    virtual void render(const QRegion &region = infiniteRegion(), double opacity = 1.0, double frameOpacity = 1.0) = 0;

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
    virtual void setIcon(const QIcon& icon) = 0;
    virtual const QIcon& icon() const = 0;
    virtual void setIconSize(const QSize& size) = 0;
    virtual const QSize& iconSize() const = 0;

    /**
     * Sets the geometry of a selection.
     * To remove the selection set a null rect.
     * @param selection The geometry of the selection in screen coordinates.
     */
    virtual void setSelection(const QRect& selection) = 0;

    /**
     * @param shader The GLShader for rendering.
     */
    virtual void setShader(GLShader* shader) = 0;
    /**
     * @returns The GLShader used for rendering or null if none.
     */
    virtual GLShader* shader() const = 0;

    /**
     * @returns The style of this EffectFrame.
     */
    virtual EffectFrameStyle style() const = 0;

    /**
     * If @p enable is @c true cross fading between icons and text is enabled
     * By default disabled. Use setCrossFadeProgress to cross fade.
     * Cross Fading is currently only available if OpenGL is used.
     * @param enable @c true enables cross fading, @c false disables it again
     * @see isCrossFade
     * @see setCrossFadeProgress
     * @since 4.6
     */
    void enableCrossFade(bool enable);
    /**
     * @returns @c true if cross fading is enabled, @c false otherwise
     * @see enableCrossFade
     * @since 4.6
     */
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
     */
    void setCrossFadeProgress(qreal progress);
    /**
     * @returns The current progress for cross fading
     * @see setCrossFadeProgress
     * @see enableCrossFade
     * @see isCrossFade
     * @since 4.6
     */
    qreal crossFadeProgress() const;

    /**
     * Returns The projection matrix as used by the current screen painting pass
     * including screen transformations.
     *
     * This matrix is only valid during a rendering pass started by render.
     *
     * @since 5.6
     * @see render
     * @see EffectsHandler::paintEffectFrame
     * @see Effect::paintEffectFrame
     */
    QMatrix4x4 screenProjectionMatrix() const;

protected:
    void setScreenProjectionMatrix(const QMatrix4x4 &projection);

private:
    EffectFramePrivate* const d;
};

/**
 * The TimeLine class is a helper for controlling animations.
 */
class KWINEFFECTS_EXPORT TimeLine
{
public:
    /**
     * Direction of the timeline.
     *
     * When the direction of the timeline is Forward, the progress
     * value will go from 0.0 to 1.0.
     *
     * When the direction of the timeline is Backward, the progress
     * value will go from 1.0 to 0.0.
     */
    enum Direction {
        Forward,
        Backward
    };

    /**
     * Constructs a new instance of TimeLine.
     *
     * @param duration Duration of the timeline, in milliseconds
     * @param direction Direction of the timeline
     * @since 5.14
     */
    explicit TimeLine(std::chrono::milliseconds duration = std::chrono::milliseconds(1000),
                      Direction direction = Forward);
    TimeLine(const TimeLine &other);
    ~TimeLine();

    /**
     * Returns the current value of the timeline.
     *
     * @since 5.14
     */
    qreal value() const;

    /**
     * Updates the progress of the timeline.
     *
     * @note The delta value should be a non-negative number, i.e. it
     * should be greater or equal to 0.
     *
     * @param delta The number milliseconds passed since last frame
     * @since 5.14
     */
    void update(std::chrono::milliseconds delta);

    /**
     * Returns the number of elapsed milliseconds.
     *
     * @see setElapsed
     * @since 5.14
     */
    std::chrono::milliseconds elapsed() const;

    /**
     * Sets the number of elapsed milliseconds.
     *
     * This method overwrites previous value of elapsed milliseconds.
     * If the new value of elapsed milliseconds is greater or equal
     * to duration of the timeline, the timeline will be finished, i.e.
     * proceeding TimeLine::done method calls will return @c true.
     * Please don't use it. Instead, use TimeLine::update.
     *
     * @note The new number of elapsed milliseconds should be a non-negative
     * number, i.e. it should be greater or equal to 0.
     *
     * @param elapsed The new number of elapsed milliseconds
     * @see elapsed
     * @since 5.14
     */
    void setElapsed(std::chrono::milliseconds elapsed);

    /**
     * Returns the duration of the timeline.
     *
     * @returns Duration of the timeline, in milliseconds
     * @see setDuration
     * @since 5.14
     */
    std::chrono::milliseconds duration() const;

    /**
     * Sets the duration of the timeline.
     *
     * In addition to setting new value of duration, the timeline will
     * try to retarget the number of elapsed milliseconds to match
     * as close as possible old progress value. If the new duration
     * is much smaller than old duration, there is a big chance that
     * the timeline will be finished after setting new duration.
     *
     * @note The new duration should be a positive number, i.e. it
     * should be greater or equal to 1.
     *
     * @param duration The new duration of the timeline, in milliseconds
     * @see duration
     * @since 5.14
     */
    void setDuration(std::chrono::milliseconds duration);

    /**
     * Returns the direction of the timeline.
     *
     * @returns Direction of the timeline(TimeLine::Forward or TimeLine::Backward)
     * @see setDirection
     * @see toggleDirection
     * @since 5.14
     */
    Direction direction() const;

    /**
     * Sets the direction of the timeline.
     *
     * @param direction The new direction of the timeline
     * @see direction
     * @see toggleDirection
     * @since 5.14
     */
    void setDirection(Direction direction);

    /**
     * Toggles the direction of the timeline.
     *
     * If the direction of the timeline was TimeLine::Forward, it becomes
     * TimeLine::Backward, and vice verca.
     *
     * @see direction
     * @see setDirection
     * @since 5.14
     */
    void toggleDirection();

    /**
     * Returns the easing curve of the timeline.
     *
     * @see setEasingCurve
     * @since 5.14
     */
    QEasingCurve easingCurve() const;

    /**
     * Sets new easing curve.
     *
     * @param easingCurve An easing curve to be set
     * @see easingCurve
     * @since 5.14
     */
    void setEasingCurve(const QEasingCurve &easingCurve);

    /**
     * Sets new easing curve by providing its type.
     *
     * @param type Type of the easing curve(e.g. QEasingCurve::InCubic, etc)
     * @see easingCurve
     * @since 5.14
     */
    void setEasingCurve(QEasingCurve::Type type);

    /**
     * Returns whether the timeline is currently in progress.
     *
     * @see done
     * @since 5.14
     */
    bool running() const;

    /**
     * Returns whether the timeline is finished.
     *
     * @see reset
     * @since 5.14
     */
    bool done() const;

    /**
     * Resets the timeline to initial state.
     *
     * @since 5.14
     */
    void reset();

    enum class RedirectMode {
        Strict,
        Relaxed
    };

    /**
     * Returns the redirect mode for the source position.
     *
     * The redirect mode controls behavior of the timeline when its direction is
     * changed at the source position, e.g. what should we do when the timeline
     * initially goes forward and we change its direction to go backward.
     *
     * In the strict mode, the timeline will stop.
     *
     * In the relaxed mode, the timeline will go in the new direction. For example,
     * if the timeline goes forward(from 0 to 1), then with the new direction it
     * will go backward(from 1 to 0).
     *
     * The default is RedirectMode::Relaxed.
     *
     * @see targetRedirectMode
     * @since 5.15
     */
    RedirectMode sourceRedirectMode() const;

    /**
     * Sets the redirect mode for the source position.
     *
     * @param mode The new mode.
     * @since 5.15
     */
    void setSourceRedirectMode(RedirectMode mode);

    /**
     * Returns the redirect mode for the target position.
     *
     * The redirect mode controls behavior of the timeline when its direction is
     * changed at the target position.
     *
     * In the strict mode, subsequent update calls won't have any effect on the
     * current value of the timeline.
     *
     * In the relaxed mode, the timeline will go in the new direction.
     *
     * The default is RedirectMode::Strict.
     *
     * @see sourceRedirectMode
     * @since 5.15
     */
    RedirectMode targetRedirectMode() const;

    /**
     * Sets the redirect mode for the target position.
     *
     * @param mode The new mode.
     * @since 5.15
     */
    void setTargetRedirectMode(RedirectMode mode);

    TimeLine &operator=(const TimeLine &other);

private:
    qreal progress() const;

private:
    class Data;
    QSharedDataPointer<Data> d;
};

/**
 * Pointer to the global EffectsHandler object.
 */
extern KWINEFFECTS_EXPORT EffectsHandler* effects;

/***************************************************************
 WindowVertex
***************************************************************/

inline
WindowVertex::WindowVertex()
    : px(0), py(0), ox(0), oy(0), tx(0), ty(0)
{
}

inline
WindowVertex::WindowVertex(double _x, double _y, double _tx, double _ty)
    : px(_x), py(_y), ox(_x), oy(_y), tx(_tx), ty(_ty)
{
}


inline
WindowVertex::WindowVertex(const QPointF &position, const QPointF &texturePosition)
    : px(position.x()), py(position.y()), ox(position.x()), oy(position.y()), tx(texturePosition.x()), ty(texturePosition.y())
{
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
    , uvSwapped(false)
    , quadID(id)
{
}

inline
WindowVertex& WindowQuad::operator[](int index)
{
    Q_ASSERT(index >= 0 && index < 4);
    return verts[ index ];
}

inline
const WindowVertex& WindowQuad::operator[](int index) const
{
    Q_ASSERT(index >= 0 && index < 4);
    return verts[ index ];
}

inline
WindowQuadType WindowQuad::type() const
{
    Q_ASSERT(quadType != WindowQuadError);
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
    Q_ASSERT(quadType != WindowQuadError);
    return quadType == WindowQuadDecoration;
}

inline
bool WindowQuad::effect() const
{
    Q_ASSERT(quadType != WindowQuadError);
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

/***************************************************************
 Effect
***************************************************************/
template <typename T>
int Effect::animationTime(int defaultDuration)
{
    return animationTime(T::duration() != 0 ? T::duration() : defaultDuration);
}

template <typename T>
void Effect::initConfig()
{
    T::instance(effects->config());
}

} // namespace
Q_DECLARE_METATYPE(KWin::EffectWindow*)
Q_DECLARE_METATYPE(QList<KWin::EffectWindow*>)
Q_DECLARE_METATYPE(KWin::TimeLine)
Q_DECLARE_METATYPE(KWin::TimeLine::Direction)

/** @} */

#endif // KWINEFFECTS_H
