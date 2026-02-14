/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/region.h"
#include "effect/globals.h"

#include <KPluginFactory>
#include <KSharedConfig>

class QKeyEvent;

namespace KWin
{

class EffectWindow;
class LogicalOutput;
class PaintDataPrivate;
class PointerAxisEvent;
class RenderTarget;
class PointerButtonEvent;
class PointerMotionEvent;
class RenderViewport;
struct TabletToolProximityEvent;
struct TabletToolTipEvent;
struct TabletToolAxisEvent;
class WindowPaintDataPrivate;
class RenderView;

/*!
 * \page kwin-effects.html
 * \title KWin Effects
 *
 * KWin effects library contains necessary classes for creating new KWin
 * compositing effects.
 *
 * \section1 Creating new effects
 * This example will demonstrate the basics of creating an effect. We'll use
 *  CoolEffect as the class name, cooleffect as internal name and
 *  "Cool Effect" as user-visible name of the effect.
 *
 * This example doesn't demonstrate how to write the effect's code. For that,
 *  see the documentation of the Effect class.
 *
 * \section2 CoolEffect class
 * First you need to create CoolEffect class which has to be a subclass of
 *  KWin::Effect. In that class you can reimplement various virtual
 *  methods to control how and where the windows are drawn.
 *
 * \section2 KWIN_EFFECT_FACTORY macro
 * This library provides a specialized KPluginFactory subclass and macros to
 * create a sub class. This subclass of KPluginFactory has to be used, otherwise
 * KWin won't load the plugin. Use the KWIN_EFFECT_FACTORY macro to create the
 * plugin factory. This macro will take the embedded json metadata filename as the second argument.
 *
 * \section2 Buildsystem
 * To build the effect, you can use the kcoreaddons_add_plugin cmake macro which
 *  takes care of creating the library and installing it.
 *  The first parameter is the name of the library, this is the same as the id of the plugin.
 *  If our effect's source is in cooleffect.cpp, we'd use following:
 * \code
 *     kcoreaddons_add_plugin(cooleffect SOURCES cooleffect.cpp INSTALL_NAMESPACE "kwin/effects/plugins")
 * \endcode
 *
 * \section2 Effect's .json file for embedded metadata
 * The format follows the one of the KPluginMetaData class.
 *
 * Example cooleffect.json file:
 * \badcode
 * {
 *     "KPlugin": {
 *         "Authors": [
 *             {
 *                 "Email": "my@email.here",
 *                 "Name": "My Name"
 *             }
 *         ],
 *         "Category": "Misc",
 *         "Description": "The coolest effect you've ever seen",
 *         "Icon": "preferences-system-windows-effect-cooleffect",
 *         "Name": "Cool Effect"
 *     }
 * }
 * \endcode
 *
 * \section1 Accessing windows and workspace
 * Effects can gain access to the properties of windows and workspace via
 *  EffectWindow and EffectsHandler classes.
 *
 * There is one global EffectsHandler object which you can access using the
 *  \c effects pointer.
 *
 * For each window, there is an EffectWindow object which can be used to read
 *  window properties such as position and also to change them.
 *
 * For more information about this, see the documentation of the corresponding
 *  classes.
 *
 */

#define KWIN_EFFECT_API_MAKE_VERSION(major, minor) ((major) << 8 | (minor))
#define KWIN_EFFECT_API_VERSION_MAJOR 0
#define KWIN_EFFECT_API_VERSION_MINOR 237
#define KWIN_EFFECT_API_VERSION KWIN_EFFECT_API_MAKE_VERSION( \
    KWIN_EFFECT_API_VERSION_MAJOR, KWIN_EFFECT_API_VERSION_MINOR)

/*!
 * \class KWin::PaintData
 * \inmodule KWin
 * \inheaderfile effect/effect.h
 */
class KWIN_EXPORT PaintData
{
public:
    virtual ~PaintData();
    /*!
     * Returns scale factor in X direction.
     * \since 4.10
     */
    qreal xScale() const;
    /*!
     * Returns scale factor in Y direction.
     * \since 4.10
     */
    qreal yScale() const;
    /*!
     * Returns scale factor in Z direction.
     * \since 4.10
     */
    qreal zScale() const;
    /*!
     * Sets the scale factor in X direction to \a scale
     *
     * \a scale The scale factor in X direction
     * \since 4.10
     */
    void setXScale(qreal scale);
    /*!
     * Sets the scale factor in Y direction to \a scale
     *
     * \a scale The scale factor in Y direction
     * \since 4.10
     */
    void setYScale(qreal scale);
    /*!
     * Sets the scale factor in Z direction to \a scale
     *
     * \a scale The scale factor in Z direction
     * \since 4.10
     */
    void setZScale(qreal scale);
    /*!
     * Sets the scale factor in X and Y direction.
     *
     * \a scale The scale factor for X and Y direction
     * \since 4.10
     */
    void setScale(const QVector2D &scale);
    /*!
     * Sets the scale factor in X, Y and Z direction
     *
     * \a scale The scale factor for X, Y and Z direction
     * \since 4.10
     */
    void setScale(const QVector3D &scale);
    /*!
     *
     */
    const QVector3D &scale() const;
    /*!
     *
     */
    const QVector3D &translation() const;
    /*!
     * Returns the translation in X direction.
     * \since 4.10
     */
    qreal xTranslation() const;
    /*!
     * Returns the translation in Y direction.
     * \since 4.10
     */
    qreal yTranslation() const;
    /*!
     * Returns the translation in Z direction.
     * \since 4.10
     */
    qreal zTranslation() const;
    /*!
     * Sets the translation in X direction to \a translate.
     * \since 4.10
     */
    void setXTranslation(qreal translate);
    /*!
     * Sets the translation in Y direction to \a translate.
     * \since 4.10
     */
    void setYTranslation(qreal translate);
    /*!
     * Sets the translation in Z direction to \a translate.
     * \since 4.10
     */
    void setZTranslation(qreal translate);
    /*!
     * Performs a translation by adding the values component wise.
     *
     * \a x Translation in X direction
     *
     * \a y Translation in Y direction
     *
     * \a z Translation in Z direction
     *
     * \since 4.10
     */
    void translate(qreal x, qreal y = 0.0, qreal z = 0.0);
    /*!
     * Performs a translation by adding the values component wise.
     *
     * Overloaded method for convenience.
     *
     * \a translate The translation
     *
     * \since 4.10
     */
    void translate(const QVector3D &translate);

    /*!
     * Sets the rotation angle.
     *
     * \a angle The new rotation angle.
     *
     * \since 4.10
     * \sa rotationAngle()
     */
    void setRotationAngle(qreal angle);
    /*!
     * Returns the rotation angle.
     *
     * Initially 0.0.
     * \since 4.10
     * \sa setRotationAngle
     */
    qreal rotationAngle() const;
    /*!
     * Sets the rotation origin.
     *
     * \a origin The new rotation origin.
     *
     * \since 4.10
     * \sa rotationOrigin()
     */
    void setRotationOrigin(const QVector3D &origin);
    /*!
     * Returns the rotation origin.
     *
     * That is the point in space which is fixed during the rotation.
     *
     * Initially this is 0/0/0.
     * \since 4.10
     * \sa setRotationOrigin()
     */
    QVector3D rotationOrigin() const;
    /*!
     * Sets the rotation axis.
     *
     * Set a component to 1.0 to rotate around this axis and to 0.0 to disable rotation around the
     * axis.
     *
     * \a axis A vector holding information on which axis to rotate
     * \since 4.10
     * \sa rotationAxis()
     */
    void setRotationAxis(const QVector3D &axis);
    /*!
     * Sets the rotation axis.
     *
     * Overloaded method for convenience.
     *
     * \a axis The axis around which should be rotated.
     *
     * \since 4.10
     * \sa rotationAxis()
     */
    void setRotationAxis(Qt::Axis axis);
    /*!
     * The current rotation axis.
     *
     * By default the rotation is (0/0/1) which means a rotation around the z axis.
     *
     * Returns The current rotation axis.
     * \since 4.10
     * \sa setRotationAxis
     */
    QVector3D rotationAxis() const;

    /*!
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

/*!
 * \class KWin::WindowPrePaintData
 * \inmodule KWin
 * \inheaderfile effect/effect.h
 */
class KWIN_EXPORT WindowPrePaintData
{
public:
    /*!
     * \variable KWin::WindowPrePaintData::mask
     */
    int mask;
    /*!
     * \variable KWin::WindowPrePaintData::devicePaint
     * Region that will be painted, in device coordinates.
     */
    Region devicePaint;
    /*!
     * \variable KWin::WindowPrePaintData::deviceOpaque
     * Region indicating the opaque content. It can be used to avoid painting
     * windows occluded by the opaque region.
     */
    Region deviceOpaque;
    /*!
     * Simple helper that sets data to say the window will be painted as non-opaque.
     * Takes also care of changing the regions.
     */
    void setTranslucent();
    /*!
     * Helper to mark that this window will be transformed
     */
    void setTransformed();
};

/*!
 * \class KWin::WindowPaintData
 * \inmodule KWin
 * \inheaderfile effect/effect.h
 */
class KWIN_EXPORT WindowPaintData : public PaintData
{
public:
    /*!
     * Constructs an identity WindowPaintData.
     */
    WindowPaintData();
    WindowPaintData(const WindowPaintData &other);
    ~WindowPaintData() override;
    /*!
     * Scales the window by \a scale factor.
     *
     * Multiplies all three components by the given factor.
     * \since 4.10
     */
    WindowPaintData &operator*=(qreal scale);
    /*!
     * Scales the window by \a scale factor.
     *
     * Performs a component wise multiplication on x and y components.
     * \since 4.10
     */
    WindowPaintData &operator*=(const QVector2D &scale);
    /*!
     * Scales the window by \a scale factor.
     *
     * Performs a component wise multiplication.
     * \since 4.10
     */
    WindowPaintData &operator*=(const QVector3D &scale);
    /*!
     * Translates the window by the given \a translation and returns a reference to the ScreenPaintData.
     * \since 4.10
     */
    WindowPaintData &operator+=(const QPointF &translation);
    /*!
     * Translates the window by the given \a translation and returns a reference to the ScreenPaintData.
     *
     * Overloaded method for convenience.
     * \since 4.10
     */
    WindowPaintData &operator+=(const QPoint &translation);
    /*!
     * Translates the window by the given \a translation and returns a reference to the ScreenPaintData.
     *
     * Overloaded method for convenience.
     * \since 4.10
     */
    WindowPaintData &operator+=(const QVector2D &translation);
    /*!
     * Translates the window by the given \a translation and returns a reference to the ScreenPaintData.
     *
     * Overloaded method for convenience.
     * \since 4.10
     */
    WindowPaintData &operator+=(const QVector3D &translation);
    /*!
     * Window opacity, in range 0 = transparent to 1 = fully opaque
     * \sa setOpacity
     * \since 4.10
     */
    qreal opacity() const;
    /*!
     * Sets the window opacity to the new \a opacity.
     *
     * If you want to modify the existing opacity level consider using multiplyOpacity.
     *
     * \a opacity The new opacity level
     * \since 4.10
     */
    void setOpacity(qreal opacity);
    /*!
     * Multiplies the current opacity with the \a factor.
     *
     * \a factor Factor with which the opacity should be multiplied
     *
     * Returns the new opacity level
     * \since 4.10
     */
    qreal multiplyOpacity(qreal factor);
    /*!
     * Saturation of the window, in range [0; 1]
     *
     * 1 means that the window is unchanged, 0 means that it's completely
     *  unsaturated (greyscale). 0.5 would make the colors less intense,
     *  but not completely grey
     *
     * Use EffectsHandler::saturationSupported() to find out whether saturation
     * is supported by the system, otherwise this value has no effect.
     *
     * Returns the current saturation
     * \sa setSaturation()
     * \since 4.10
     */
    qreal saturation() const;
    /*!
     * Sets the window saturation level to \a saturation.
     *
     * If you want to modify the existing saturation level consider using multiplySaturation.
     *
     * \a saturation The new saturation level
     * \since 4.10
     */
    void setSaturation(qreal saturation) const;
    /*!
     * Multiplies the current saturation with \a factor.
     *
     * \a factor with which the saturation should be multiplied
     *
     * Returns the new saturation level
     * \since 4.10
     */
    qreal multiplySaturation(qreal factor);
    /*!
     * Brightness of the window, in range [0; 1]
     *
     * 1 means that the window is unchanged, 0 means that it's completely
     * black. 0.5 would make it 50% darker than usual
     */
    qreal brightness() const;
    /*!
     * Sets the window brightness level to \a brightness.
     *
     * If you want to modify the existing brightness level consider using multiplyBrightness.
     *
     * \a brightness The new brightness level
     */
    void setBrightness(qreal brightness);
    /*!
     * Multiplies the current brightness level with \a factor.
     *
     * \a factor with which the brightness should be multiplied.
     *
     * Returns the new brightness level
     * \since 4.10
     */
    qreal multiplyBrightness(qreal factor);
    /*!
     * Sets the cross fading \a factor to fade over with previously sized window.
     *
     * If \c 1.0 only the current window is used, if \c 0.0 only the previous window is used.
     *
     * By default only the current window is used. This factor can only make any visual difference
     * if the previous window get referenced.
     *
     * \a factor The cross fade factor between \c 0.0 (previous window) and \c 1.0 (current window)
     * \sa crossFadeProgress
     */
    void setCrossFadeProgress(qreal factor);
    /*!
     * \sa setCrossFadeProgress
     */
    qreal crossFadeProgress() const;

private:
    const std::unique_ptr<WindowPaintDataPrivate> d;
};

/*!
 * \class KWin::ScreenPrePaintData
 * \inmodule KWin
 * \inheaderfile effect/effect.h
 */
class KWIN_EXPORT ScreenPrePaintData
{
public:
    /*!
     * \variable KWin::ScreenPrePaintData::mask
     */
    int mask;

    /*!
     * \variable KWin::ScreenPrePaintData::paint
     */
    Region paint;

    /*!
     * \variable KWin::ScreenPrePaintData::screen
     */
    LogicalOutput *screen = nullptr;

    /*!
     * \variable KWin::ScreenPrePaintData::view
     */
    RenderView *view = nullptr;
};

/*!
 * \class KWin::Effect
 * \inmodule KWin
 * \inheaderfile effect/effect.h
 *
 * \brief Base class for all KWin effects.
 *
 * This is the base class for all effects. By reimplementing virtual methods
 * of this class, you can customize how the windows are painted.
 *
 * The virtual methods are used for painting and need to be implemented for
 * custom painting.
 *
 * In order to react to state changes (e.g. a window gets closed) the effect
 * should provide slots for the signals emitted by the EffectsHandler.
 *
 * \section1 Chaining
 * Most methods of this class are called in chain style. This means that when
 *  effects A and B area active then first e.g. A::paintWindow() is called and
 *  then from within that method B::paintWindow() is called (although
 *  indirectly). To achieve this, you need to make sure to call corresponding
 *  method in EffectsHandler class from each such method (using \c effects
 *  pointer):
 * \code
 *  void MyEffect::postPaintScreen()
 *  {
 *      // Do your own processing here
 *      ...
 *      // Call corresponding EffectsHandler method
 *      effects->postPaintScreen();
 *  }
 * \endcode
 *
 * \section1 Effects pointer
 * \c effects pointer points to the global EffectsHandler object that you can
 *  use to interact with the windows.
 *
 * \section1 Painting stages
 * Painting of windows is done in three stages:
 * \list
 * \li First, the prepaint pass.
 *  Here you can specify how the windows will be painted, e.g. that they will
 *  be translucent and transformed.
 * \li Second, the paint pass.
 *  Here the actual painting takes place. You can change attributes such as
 *  opacity of windows as well as apply transformations to them. You can also
 *  paint something onto the screen yourself.
 * \li Finally, the postpaint pass.
 *  Here you can mark windows, part of windows or even the entire screen for
 *  repainting to create animations.
 *
 * For each stage there are *Screen() and *Window() methods. The window method
 *  is called for every window while the screen method is usually called just
 *  once.
 *
 * \section1 OpenGL
 * Effects can use OpenGL if EffectsHandler::isOpenGLCompositing() returns \c true.
 *
 * The OpenGL context may not always be current when code inside the effect is
 * executed. The framework ensures that the OpenGL context is current when the Effect
 * gets created, destroyed or reconfigured and during the painting stages. All virtual
 * methods which have the OpenGL context current are documented.
 *
 * If OpenGL code is going to be executed outside the painting stages, e.g. in reaction
 * to a global shortcut, it is the task of the Effect to make the OpenGL context current:
 * \code
 * effects->makeOpenGLContextCurrent();
 * \endcode
 *
 * There is in general no need to call the matching doneCurrent method.
 */
class KWIN_EXPORT Effect : public QObject
{
    Q_OBJECT
public:
    // TODO: is that ok here?
    /*!
     * Flags controlling how painting is done.
     *
     * \value PAINT_WINDOW_OPAQUE Window (or at least part of it) will be painted opaque
     * \value PAINT_WINDOW_TRANSLUCENT Window (or at least part of it) will be painted translucent
     * \value PAINT_WINDOW_TRANSFORMED Window will be painted with transformed geometry
     * \value PAINT_SCREEN_REGION Paint only a region of the screen (can be optimized, cannot be used together with TRANSFORMED flags)
     * \value PAINT_SCREEN_TRANSFORMED The whole screen will be painted with transformed geometry. Forces the entire screen to be painted
     * \value PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS At least one window will be painted with transformed geometry. Forces the entire screen to be painted
     * \value PAINT_SCREEN_BACKGROUND_FIRST Clear whole background as the very first step, without optimizing it
     */
    enum {
        PAINT_WINDOW_OPAQUE = 1 << 0,
        PAINT_WINDOW_TRANSLUCENT = 1 << 1,
        PAINT_WINDOW_TRANSFORMED = 1 << 2,
        PAINT_SCREEN_REGION = 1 << 3,
        PAINT_SCREEN_TRANSFORMED = 1 << 4,
        PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
        PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6,
    };

    /*!
     * \value Nothing
     * \value ScreenInversion
     * \value Blur
     * \value Contrast
     * \value HighlightWindows
     * \value SystemBell
     */
    enum Feature {
        Nothing = 0,
        ScreenInversion,
        Blur,
        Contrast,
        HighlightWindows,
        SystemBell,
    };

    /*!
     * Constructs new Effect object.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when the Effect is constructed.
     */
    Effect(QObject *parent = nullptr);
    /*!
     * Destructs the Effect object.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when the Effect is destroyed.
     */
    ~Effect() override;

    /*!
     * Flags describing which parts of configuration have changed.
     *
     * \value ReconfigureAll Everything needs to be reconfigured
     */
    enum ReconfigureFlag {
        ReconfigureAll = 1 << 0
    };
    Q_DECLARE_FLAGS(ReconfigureFlags, ReconfigureFlag)

    /*!
     * Called when configuration changes (either the effect's or KWin's global).
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when the Effect is reconfigured. If this method is called from within the Effect it is
     * required to ensure that the context is current if the implementation does OpenGL calls.
     */
    virtual void reconfigure(ReconfigureFlags flags);

    /*!
     * Called before starting to paint the screen.
     * In this method you can:
     * \list
     * \li set whether the windows or the entire screen will be transformed
     * \li change the region of the screen that will be painted
     * \li do various housekeeping tasks such as initing your effect's variables
     *      for the upcoming paint pass or updating animation's progress
     * \endlist
     *
     * \a presentTime specifies the expected monotonic time when the rendered frame
     * will be displayed on the screen.
     */
    virtual void prePaintScreen(ScreenPrePaintData &data,
                                std::chrono::milliseconds presentTime);
    /*!
     * In this method you can:
     * \list
     * \li paint something on top of the windows (by painting after calling
     *      effects->paintScreen())
     * \li paint multiple desktops and/or multiple copies of the same desktop
     *      by calling effects->paintScreen() multiple times
     * \endlist
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen);
    /*!
     * Called after all the painting has been finished.
     *
     * In this method you can:
     * \list
     * \li schedule next repaint in case of animations
     * \endlist
     *
     * You shouldn't paint anything here.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void postPaintScreen();

    /*!
     * Called for every window before the actual paint pass
     *
     * In this method you can:
     * \list
     * \li enable or disable painting of the window (e.g. enable painting of minimized window)
     * \li set window to be painted with translucency
     * \li set window to be transformed
     * \li request the window to be divided into multiple parts
     * \endlist
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     *
     * \a presentTime specifies the expected monotonic time when the rendered frame
     * will be displayed on the screen.
     */
    virtual void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data,
                                std::chrono::milliseconds presentTime);
    /*!
     * This is the main method for painting windows.
     *
     * In this method you can:
     * \list
     * \li do various transformations
     * \li change opacity of the window
     * \li change brightness and/or saturation, if it's supported
     * \endlist
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data);

    /*!
     * Called on Transparent resizes.
     *
     * return \c true if your effect substitutes questioned feature
     */
    virtual bool provides(Feature);

    /*!
     * Performs the \a feature with the \a arguments.
     *
     * This allows to have specific protocols between KWin core and an Effect.
     *
     * The method is supposed to return \c true if it performed the features,
     * \c false otherwise.
     *
     * The default implementation returns \c false.
     * \since 5.8
     */
    virtual bool perform(Feature feature, const QVariantList &arguments);

    /*!
     * Can be called to draw multiple copies (e.g. thumbnails) of a window.
     *
     * You can change window's opacity/brightness/etc here, but you can't
     *  do any transformations.
     *
     * In OpenGL based compositing, the frameworks ensures that the context is current
     * when this method is invoked.
     */
    virtual void drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data);

    /*!
     * This function can be overriden to handle grabbed keyboard input.
     */
    virtual void grabbedKeyboardEvent(QKeyEvent *e);

    /*!
     * Overwrite this method to indicate whether your effect will be doing something in
     * the next frame to be rendered.
     *
     * If the method returns \c false the effect will be
     * excluded from the chained methods in the next rendered frame.
     *
     * This method is called always directly before the paint loop begins. So it is totally
     * fine to e.g. react on a window event, issue a repaint to trigger an animation and
     * change a flag to indicate that this method returns \c true.
     *
     * As the method is called each frame, you should not perform complex calculations.
     * Best use just a boolean flag.
     *
     * The default implementation of this method returns \c true.
     * \since 4.8
     */
    virtual bool isActive() const;

    /*!
     * Reimplement this method to provide online debugging.
     *
     * This could be as trivial as printing specific detail information about the effect state
     * but could also be used to move the effect in and out of a special debug modes, clear bogus
     * data, etc.
     *
     * Notice that the functions is const by intent! Whenever you alter the state of the object
     * due to random user input, you should do so with greatest care, hence const_cast<> your
     * object - signalling "let me alone, i know what i'm doing"
     *
     * \a parameter A freeform string user input for your effect to interpret.
     * \since 4.11
     */
    virtual QString debug(const QString &parameter) const;

    /*!
     * Reimplement this method to indicate where in the Effect chain the Effect should be placed.
     *
     * A low number indicates early chain position, thus before other Effects got called, a high
     * number indicates a late position. The returned number should be in the interval [0, 100].
     * The default value is 0.
     *
     * In KWin4 this information was provided in the Effect's desktop file as property
     * X-KDE-Ordering. In the case of Scripted Effects this property is still used.
     *
     * \since 5.0
     */
    virtual int requestedEffectChainPosition() const;

    /*!
     * A pointer was moved
     *
     * \a event The PointerMotionEvent for the event
     *
     * \since 6.7
     */
    virtual void pointerMotion(PointerMotionEvent *event);
    /*!
     * A pointer button was moved
     *
     * \a event The PointerButtonEvent for the event
     *
     * \since 6.7
     */
    virtual void pointerButton(PointerButtonEvent *event);
    /*!
     * A pointer axis was scrolled
     *
     * \a event The PointerAxisEvent for the event
     *
     * \since 6.7
     */
    virtual void pointerAxis(PointerAxisEvent *event);

    /*!
     * A touch point was pressed.
     *
     * If the effect wants to exclusively use the touch event it should return \c true.
     * If \c false is returned the touch event is passed to further effects.
     *
     * In general an Effect should only return \c true if it is the exclusive effect getting
     * input events. E.g. has grabbed mouse events.
     *
     * Default implementation returns \c false.
     *
     * \a id The unique id of the touch point
     *
     * \a pos The position of the touch point in global coordinates
     *
     * \a time Timestamp
     *
     * \sa touchMotion
     * \sa touchUp
     * \since 5.8
     */
    virtual bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    /*!
     * A touch point moved.
     *
     * If the effect wants to exclusively use the touch event it should return \c true.
     * If \c false is returned the touch event is passed to further effects.
     *
     * In general an Effect should only return \c true if it is the exclusive effect getting
     * input events. E.g. has grabbed mouse events.
     *
     * Default implementation returns \c false.
     *
     * \a id The unique id of the touch point
     *
     * \a pos The position of the touch point in global coordinates
     *
     * \a time Timestamp
     *
     * \sa touchDown
     * \sa touchUp
     * \since 5.8
     */
    virtual bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    /*!
     * A touch point was released.
     *
     * If the effect wants to exclusively use the touch event it should return \c true.
     * If \c false is returned the touch event is passed to further effects.
     *
     * In general an Effect should only return \c true if it is the exclusive effect getting
     * input events. E.g. has grabbed mouse events.
     *
     * Default implementation returns \c false.
     *
     * \a id The unique id of the touch point
     *
     * \a time Timestamp
     *
     * \sa touchDown
     * \sa touchMotion
     * \since 5.8
     */
    virtual bool touchUp(qint32 id, std::chrono::microseconds time);
    /*!
     * All touch points were canceled
     * \since 6.3
     */
    virtual void touchCancel();

    /*!
     * There has been a proximity tablet tool event.
     */
    virtual bool tabletToolProximity(TabletToolProximityEvent *event);

    /*!
     * There has been an axis tablet tool event.
     */
    virtual bool tabletToolAxis(TabletToolAxisEvent *event);

    /*!
     * There has been a tip tablet tool event.
     */
    virtual bool tabletToolTip(TabletToolTipEvent *event);

    /*!
     * There has been an event from a button on a drawing tablet tool
     *
     * \a button which button
     *
     * \a pressed true if pressed, false when released
     *
     * \a toolId the identifier of the tool id
     *
     * \since 5.25
     */
    virtual bool tabletToolButtonEvent(uint button, bool pressed, quint64 toolId);

    /*!
     * There has been an event from a button on a drawing tablet pad
     *
     * \a button which button
     *
     * \a pressed true if pressed, false when released
     *
     * \a device the identifier of the tool id
     *
     * \since 5.25
     */
    virtual bool tabletPadButtonEvent(uint button, bool pressed, void *device);

    /*!
     * There has been an event from a input strip on a drawing tablet pad
     *
     * \a number which strip
     *
     * \a position the value within the strip that was selected
     *
     * \a isFinger if it was activated with a finger
     *
     * \a device the identifier of the tool id
     *
     * \since 5.25
     */
    virtual bool tabletPadStripEvent(int number, qreal position, bool isFinger, void *device);

    /*!
     * There has been an event from a input ring on a drawing tablet pad
     *
     * \a number which ring
     *
     * \a position the value within the ring that was selected
     *
     * \a isFinger if it was activated with a finger
     *
     * \a device the identifier of the tool id
     *
     * \since 5.25
     */
    virtual bool tabletPadRingEvent(int number, qreal position, bool isFinger, void *device);

    /*!
     * There has been an event from a input dial on a drawing tablet pad
     *
     * \a number which dial
     *
     * \a delta the delta value
     *
     * \a device the identifier of the tool id
     *
     * \since 6.4
     */
    virtual bool tabletPadDialEvent(int number, double delta, void *device);

    /*!
     * Returns the current pointer position.
     */
    static QPointF cursorPos();

    // return type is intentionally double so that one can divide using it without losing data
    /*!
     * Read animation time from the configuration and possibly adjust using animationTimeFactor().
     *
     * The configuration value in the effect should also have special value 'default' (set using
     * QSpinBox::setSpecialValueText()) with the value 0. This special value is adjusted
     * using the global animation speed, otherwise the exact time configured is returned.
     *
     * \a cfg configuration group to read value from
     *
     * \a key configuration key to read value from
     *
     * \a defaultTime default animation time in milliseconds
     */
    static double animationTime(const KConfigGroup &cfg, const QString &key, std::chrono::milliseconds defaultTime);
    /*!
     * \overload Use this variant if the animation time is hardcoded and not configurable
     * in the effect itself.
     */
    static double animationTime(std::chrono::milliseconds defaultTime);
    /*!
     * \overload Use this variant if animation time is provided through a KConfigXT generated class
     * having a property called "duration".
     */
    template<typename T>
    int animationTime(std::chrono::milliseconds defaultDuration);
    /*!
     * Linearly interpolates between \a x and \a y.
     *
     * Returns \a x when \a a = 0; returns \a y when \a a = 1.
     */
    static double interpolate(double x, double y, double a)
    {
        return x * (1 - a) + y * a;
    }
    /*!
     * Helper to set WindowPaintData and Region to necessary transformations so that
     * a following drawWindow() would put the window at the requested geometry (useful for thumbnails)
     */
    static void setPositionTransformations(WindowPaintData &data, Rect &logicalRegion, EffectWindow *w,
                                           const Rect &r, Qt::AspectRatioMode aspect);

    /*!
     * overwrite this method to return \c false if your effect does not need to be drawn over opaque fullscreen windows
     */
    virtual bool blocksDirectScanout() const;

public Q_SLOTS:
    /*!
     * This function gets called when a reserved screen edge for \a border gets called.
     */
    virtual bool borderActivated(ElectricBorder border);
};

template<typename T>
int Effect::animationTime(std::chrono::milliseconds defaultDuration)
{
    return animationTime(T::duration() != 0 ? std::chrono::milliseconds(T::duration()) : defaultDuration);
}

/*!
 * \class KWin::EffectPluginFactory
 * \inmodule KWin
 * \inheaderfile effect/effect.h
 *
 * Prefer the KWIN_EFFECT_FACTORY macros.
 */
class KWIN_EXPORT EffectPluginFactory : public KPluginFactory
{
    Q_OBJECT
public:
    /*!
     * Default constructs the plugin factory.
     */
    EffectPluginFactory();
    ~EffectPluginFactory() override;
    /*!
     * Returns whether the Effect is supported.
     *
     * An Effect can implement this method to determine at runtime whether the Effect is supported.
     *
     * If the current compositing backend is not supported it should return \c false.
     *
     * This method is optional, by default \c true is returned.
     */
    virtual bool isSupported() const;
    /*!
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
     * This method is optional, by default \c true is returned.
     */
    virtual bool enabledByDefault() const;
    /*!
     * This method returns the created Effect.
     */
    virtual KWin::Effect *createEffect() const = 0;
};

#define EffectPluginFactory_iid "org.kde.kwin.EffectPluginFactory" KWIN_PLUGIN_VERSION_STRING
#define KWIN_PLUGIN_FACTORY_NAME KPLUGINFACTORY_PLUGIN_CLASS_INTERNAL_NAME

/*!
 * \macro KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED
 * \relates KWin::Effect
 *
 * Defines an EffectPluginFactory sub class with customized isSupported and enabledByDefault methods.
 *
 * If the Effect to be created does not need the isSupported or enabledByDefault methods prefer
 * the simplified KWIN_EFFECT_FACTORY, KWIN_EFFECT_FACTORY_SUPPORTED or KWIN_EFFECT_FACTORY_ENABLED
 * macros which create an EffectPluginFactory with a usable default value.
 *
 * This API is not providing binary compatibility and thus the effect plugin must be compiled against
 * the same kwineffects library version as KWin.
 *
 * \a factoryName The name to be used for the EffectPluginFactory
 *
 * \a className The class name of the Effect sub class which is to be created by the factory
 *
 * \a jsonFile Name of the json file to be compiled into the plugin as metadata
 *
 * \a supported Source code to go into the isSupported() method, must return a boolean
 *
 * \a enabled Source code to go into the enabledByDefault() method, must return a boolean
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

/*!
 * \macro KWIN_EFFECT_FACTORY_ENABLED
 * \relates KWin::Effect
 */
#define KWIN_EFFECT_FACTORY_ENABLED(className, jsonFile, enabled) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(className, jsonFile, return true;, enabled)

/*!
 * \macro KWIN_EFFECT_FACTORY_SUPPORTED
 * \relates KWin::Effect
 */
#define KWIN_EFFECT_FACTORY_SUPPORTED(className, jsonFile, supported) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(className, jsonFile, supported, return true;)

/*!
 * \macro KWIN_EFFECT_FACTORY
 * \relates KWin::Effect
 */
#define KWIN_EFFECT_FACTORY(className, jsonFile) \
    KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(className, jsonFile, return true;, return true;)

} // namespace KWin
