/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"

#include <QRegion>

#include <KPluginFactory>
#include <KSharedConfig>

class QKeyEvent;

namespace KWin
{

class EffectWindow;
class Output;
class PaintDataPrivate;
class RenderTarget;
class RenderViewport;
struct TabletToolProximityEvent;
struct TabletToolTipEvent;
struct TabletToolAxisEvent;
class WindowPaintDataPrivate;

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
 * plugin factory. This macro will take the embedded json metadata filename as the second argument.
 *
 * @subsection creating-buildsystem Buildsystem
 * To build the effect, you can use the kcoreaddons_add_plugin cmake macro which
 *  takes care of creating the library and installing it.
 *  The first parameter is the name of the library, this is the same as the id of the plugin.
 *  If our effect's source is in cooleffect.cpp, we'd use following:
 * @code
 *     kcoreaddons_add_plugin(cooleffect SOURCES cooleffect.cpp INSTALL_NAMESPACE "kwin/effects/plugins")
 * @endcode
 *
 * @subsection creating-json-metadata Effect's .json file for embedded metadata
 * The format follows the one of the @see KPluginMetaData class.
 *
 * Example cooleffect.json file:
 * @code
{
    "KPlugin": {
        "Authors": [
            {
                "Email": "my@email.here",
                "Name": "My Name"
            }
        ],
        "Category": "Misc",
        "Description": "The coolest effect you've ever seen",
        "Icon": "preferences-system-windows-effect-cooleffect",
        "Name": "Cool Effect"
    }
}
 * @endcode
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

#define KWIN_EFFECT_API_MAKE_VERSION(major, minor) ((major) << 8 | (minor))
#define KWIN_EFFECT_API_VERSION_MAJOR 0
#define KWIN_EFFECT_API_VERSION_MINOR 236
#define KWIN_EFFECT_API_VERSION KWIN_EFFECT_API_MAKE_VERSION( \
    KWIN_EFFECT_API_VERSION_MAJOR, KWIN_EFFECT_API_VERSION_MINOR)

class KWIN_EXPORT PaintData
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
    const QVector3D &scale() const;
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

    /**
     * Returns the corresponding transform matrix.
     *
     * The transform matrix is converted to device coordinates using the
     * supplied deviceScale.
     */
    QMatrix4x4 toMatrix(qreal deviceScale) const;

protected:
    PaintData();
    PaintData(const PaintData &other);

private:
    const std::unique_ptr<PaintDataPrivate> d;
};

class KWIN_EXPORT WindowPrePaintData
{
public:
    int mask;
    /**
     * Region that will be painted, in screen coordinates.
     */
    QRegion paint;
    /**
     * Region indicating the opaque content. It can be used to avoid painting
     * windows occluded by the opaque region.
     */
    QRegion opaque;
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

class KWIN_EXPORT WindowPaintData : public PaintData
{
public:
    WindowPaintData();
    WindowPaintData(const WindowPaintData &other);
    ~WindowPaintData() override;
    /**
     * Scales the window by @p scale factor.
     * Multiplies all three components by the given factor.
     * @since 4.10
     */
    WindowPaintData &operator*=(qreal scale);
    /**
     * Scales the window by @p scale factor.
     * Performs a component wise multiplication on x and y components.
     * @since 4.10
     */
    WindowPaintData &operator*=(const QVector2D &scale);
    /**
     * Scales the window by @p scale factor.
     * Performs a component wise multiplication.
     * @since 4.10
     */
    WindowPaintData &operator*=(const QVector3D &scale);
    /**
     * Translates the window by the given @p translation and returns a reference to the ScreenPaintData.
     * @since 4.10
     */
    WindowPaintData &operator+=(const QPointF &translation);
    /**
     * Translates the window by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    WindowPaintData &operator+=(const QPoint &translation);
    /**
     * Translates the window by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    WindowPaintData &operator+=(const QVector2D &translation);
    /**
     * Translates the window by the given @p translation and returns a reference to the ScreenPaintData.
     * Overloaded method for convenience.
     * @since 4.10
     */
    WindowPaintData &operator+=(const QVector3D &translation);
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

private:
    const std::unique_ptr<WindowPaintDataPrivate> d;
};

class KWIN_EXPORT ScreenPrePaintData
{
public:
    int mask;
    QRegion paint;
    Output *screen = nullptr;
};

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
 *  is called for every window while the screen method is usually called just
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
class KWIN_EXPORT Effect : public QObject
{
    Q_OBJECT
public:
    /** Flags controlling how painting is done. */
    // TODO: is that ok here?
    enum {
        /**
         * Window (or at least part of it) will be painted opaque.
         */
        PAINT_WINDOW_OPAQUE = 1 << 0,
        /**
         * Window (or at least part of it) will be painted translucent.
         */
        PAINT_WINDOW_TRANSLUCENT = 1 << 1,
        /**
         * Window will be painted with transformed geometry.
         */
        PAINT_WINDOW_TRANSFORMED = 1 << 2,
        /**
         * Paint only a region of the screen (can be optimized, cannot
         * be used together with TRANSFORMED flags).
         */
        PAINT_SCREEN_REGION = 1 << 3,
        /**
         * The whole screen will be painted with transformed geometry.
         * Forces the entire screen to be painted.
         */
        PAINT_SCREEN_TRANSFORMED = 1 << 4,
        /**
         * At least one window will be painted with transformed geometry.
         * Forces the entire screen to be painted.
         */
        PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
        /**
         * Clear whole background as the very first step, without optimizing it
         */
        PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6,
    };

    enum Feature {
        Nothing = 0,
        ScreenInversion,
        Blur,
        Contrast,
        HighlightWindows,
        SystemBell,
    };

    /**
     * Constructs new Effect object.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when the Effect is constructed.
     */
    Effect(QObject *parent = nullptr);
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
     * Called before starting to paint the screen.
     * In this method you can:
     * @li set whether the windows or the entire screen will be transformed
     * @li change the region of the screen that will be painted
     * @li do various housekeeping tasks such as initing your effect's variables
            for the upcoming paint pass or updating animation's progress
     *
     * @a presentTime specifies the expected monotonic time when the rendered frame
     * will be displayed on the screen.
    */
    virtual void prePaintScreen(ScreenPrePaintData &data,
                                std::chrono::milliseconds presentTime);
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
    virtual void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen);
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
     *
     * @a presentTime specifies the expected monotonic time when the rendered frame
     * will be displayed on the screen.
     */
    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data,
                                std::chrono::milliseconds presentTime);
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
    virtual void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, QRegion region, WindowPaintData &data);
    /**
     * Called for every window after all painting has been finished.
     * In this method you can:
     * @li schedule next repaint for individual window(s) in case of animations
     * You shouldn't paint anything here.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void postPaintWindow(EffectWindow *w);

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
    virtual void drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data);

    virtual void windowInputMouseEvent(QEvent *e);
    virtual void grabbedKeyboardEvent(QKeyEvent *e);

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
    virtual bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time);
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
    virtual bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time);
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
    virtual bool touchUp(qint32 id, std::chrono::microseconds time);
    /**
     * All touch points were canceled
     * @since 6.3
     */
    virtual void touchCancel();

    /**
     * There has been a proximity tablet tool event.
     */
    virtual bool tabletToolProximity(TabletToolProximityEvent *event);

    /**
     * There has been an axis tablet tool event.
     */
    virtual bool tabletToolAxis(TabletToolAxisEvent *event);

    /**
     * There has been a tip tablet tool event.
     */
    virtual bool tabletToolTip(TabletToolTipEvent *event);

    /**
     * There has been an event from a button on a drawing tablet tool
     *
     * @param button which button
     * @param pressed true if pressed, false when released
     * @param toolId the identifier of the tool id
     *
     * @since 5.25
     */
    virtual bool tabletToolButtonEvent(uint button, bool pressed, quint64 toolId);

    /**
     * There has been an event from a button on a drawing tablet pad
     *
     * @param button which button
     * @param pressed true if pressed, false when released
     * @param device the identifier of the tool id
     *
     * @since 5.25
     */
    virtual bool tabletPadButtonEvent(uint button, bool pressed, void *device);

    /**
     * There has been an event from a input strip on a drawing tablet pad
     *
     * @param number which strip
     * @param position the value within the strip that was selected
     * @param isFinger if it was activated with a finger
     * @param device the identifier of the tool id
     *
     * @since 5.25
     */
    virtual bool tabletPadStripEvent(int number, int position, bool isFinger, void *device);

    /**
     * There has been an event from a input ring on a drawing tablet pad
     *
     * @param number which ring
     * @param position the value within the ring that was selected
     * @param isFinger if it was activated with a finger
     * @param device the identifier of the tool id
     *
     * @since 5.25
     */
    virtual bool tabletPadRingEvent(int number, int position, bool isFinger, void *device);

    static QPointF cursorPos();

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
    static double animationTime(const KConfigGroup &cfg, const QString &key, std::chrono::milliseconds defaultTime);
    /**
     * @overload Use this variant if the animation time is hardcoded and not configurable
     * in the effect itself.
     */
    static double animationTime(std::chrono::milliseconds defaultTime);
    /**
     * @overload Use this variant if animation time is provided through a KConfigXT generated class
     * having a property called "duration".
     */
    template<typename T>
    int animationTime(std::chrono::milliseconds defaultDuration);
    /**
     * Linearly interpolates between @p x and @p y.
     *
     * Returns @p x when @p a = 0; returns @p y when @p a = 1.
     */
    static double interpolate(double x, double y, double a)
    {
        return x * (1 - a) + y * a;
    }
    /** Helper to set WindowPaintData and QRegion to necessary transformations so that
     * a following drawWindow() would put the window at the requested geometry (useful for thumbnails)
     */
    static void setPositionTransformations(WindowPaintData &data, QRect &region, EffectWindow *w,
                                           const QRect &r, Qt::AspectRatioMode aspect);

    /**
     * overwrite this method to return false if your effect does not need to be drawn over opaque fullscreen windows
     */
    virtual bool blocksDirectScanout() const;

public Q_SLOTS:
    virtual bool borderActivated(ElectricBorder border);
};

template<typename T>
int Effect::animationTime(std::chrono::milliseconds defaultDuration)
{
    return animationTime(T::duration() != 0 ? std::chrono::milliseconds(T::duration()) : defaultDuration);
}

/**
 * Prefer the KWIN_EFFECT_FACTORY macros.
 */
class KWIN_EXPORT EffectPluginFactory : public KPluginFactory
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

#define EffectPluginFactory_iid "org.kde.kwin.EffectPluginFactory" KWIN_PLUGIN_VERSION_STRING
#define KWIN_PLUGIN_FACTORY_NAME KPLUGINFACTORY_PLUGIN_CLASS_INTERNAL_NAME

/**
 * Defines an EffectPluginFactory sub class with customized isSupported and enabledByDefault methods.
 *
 * If the Effect to be created does not need the isSupported or enabledByDefault methods prefer
 * the simplified KWIN_EFFECT_FACTORY, KWIN_EFFECT_FACTORY_SUPPORTED or KWIN_EFFECT_FACTORY_ENABLED
 * macros which create an EffectPluginFactory with a useable default value.
 *
 * This API is not providing binary compatibility and thus the effect plugin must be compiled against
 * the same kwineffects library version as KWin.
 *
 * @param factoryName The name to be used for the EffectPluginFactory
 * @param className The class name of the Effect sub class which is to be created by the factory
 * @param jsonFile Name of the json file to be compiled into the plugin as metadata
 * @param supported Source code to go into the isSupported() method, must return a boolean
 * @param enabled Source code to go into the enabledByDefault() method, must return a boolean
 */
#define KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(className, jsonFile, supported, enabled) \
    class KWIN_PLUGIN_FACTORY_NAME : public KWin::EffectPluginFactory                  \
    {                                                                                  \
        Q_OBJECT                                                                       \
        Q_PLUGIN_METADATA(IID EffectPluginFactory_iid FILE jsonFile)                   \
        Q_INTERFACES(KPluginFactory)                                                   \
    public:                                                                            \
        explicit KWIN_PLUGIN_FACTORY_NAME()                                            \
        {                                                                              \
        }                                                                              \
        ~KWIN_PLUGIN_FACTORY_NAME()                                                    \
        {                                                                              \
        }                                                                              \
        bool isSupported() const override                                              \
        {                                                                              \
            supported                                                                  \
        }                                                                              \
        bool enabledByDefault() const override{                                        \
            enabled} KWin::Effect *createEffect() const override                       \
        {                                                                              \
            return new className();                                                    \
        }                                                                              \
    };

#define KWIN_EFFECT_FACTORY_ENABLED(className, jsonFile, enabled) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(className, jsonFile, return true;, enabled)

#define KWIN_EFFECT_FACTORY_SUPPORTED(className, jsonFile, supported) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(className, jsonFile, supported, return true;)

#define KWIN_EFFECT_FACTORY(className, jsonFile) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(className, jsonFile, return true;, return true;)

} // namespace KWin
