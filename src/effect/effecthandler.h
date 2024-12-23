/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"
#include "effect/effect.h"
#include "effect/effectwindow.h"

#include <QEasingCurve>
#include <QIcon>
#include <QPair>
#include <QRect>
#include <QRegion>
#include <QSet>

#include <QHash>
#include <QList>
#include <QLoggingCategory>
#include <QStack>

#include <functional>

#if KWIN_BUILD_X11
#include <xcb/xcb.h>
#endif

class KConfigGroup;
class QFont;
class QKeyEvent;
class QMatrix4x4;
class QMouseEvent;
class QWheelEvent;
class QAction;
class QQmlEngine;

/**
 * Logging category to be used inside the KWin effects.
 * Do not use in this library.
 */
Q_DECLARE_LOGGING_CATEGORY(KWINEFFECTS)

namespace KDecoration3
{
class Decoration;
}

namespace KWin
{

class SurfaceInterface;
class Display;
class PaintDataPrivate;
class WindowPaintDataPrivate;

class Compositor;
class EffectLoader;
class EffectWindow;
class EffectWindowGroup;
class OffscreenQuickView;
class Group;
class Output;
class Effect;
struct TabletToolProximityEvent;
struct TabletToolAxisEvent;
struct TabletToolTipEvent;
class Window;
class WindowItem;
class WindowPropertyNotifyX11Filter;
class WorkspaceScene;
class VirtualDesktop;
class OpenGlContext;
class InputDevice;
class InputDeviceTabletTool;

typedef QPair<QString, Effect *> EffectPair;

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
    WindowForceBackgroundContrastRole, ///< For fullscreen effects to enforce the background contrast,
};

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
class KWIN_EXPORT EffectsHandler : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Effects")

    Q_PROPERTY(QStringList activeEffects READ activeEffects)
    Q_PROPERTY(QStringList loadedEffects READ loadedEffects)
    Q_PROPERTY(QStringList listOfEffects READ listOfEffects)

    Q_PROPERTY(KWin::VirtualDesktop *currentDesktop READ currentDesktop WRITE setCurrentDesktop NOTIFY desktopChanged)
    Q_PROPERTY(QString currentActivity READ currentActivity NOTIFY currentActivityChanged)
    Q_PROPERTY(KWin::EffectWindow *activeWindow READ activeWindow WRITE activateWindow NOTIFY windowActivated)
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize NOTIFY desktopGridSizeChanged)
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth NOTIFY desktopGridWidthChanged)
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight NOTIFY desktopGridHeightChanged)
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)
    Q_PROPERTY(QList<KWin::VirtualDesktop *> desktops READ desktops)
    Q_PROPERTY(bool optionRollOverDesktops READ optionRollOverDesktops)
    Q_PROPERTY(KWin::Output *activeScreen READ activeScreen)
    /**
     * Factor by which animation speed in the effect should be modified (multiplied).
     * If configurable in the effect itself, the option should have also 'default'
     * animation speed. The actual value should be determined using animationTime().
     * Note: The factor can be also 0, so make sure your code can cope with 0ms time
     * if used manually.
     */
    Q_PROPERTY(qreal animationTimeFactor READ animationTimeFactor)
    Q_PROPERTY(QList<EffectWindow *> stackingOrder READ stackingOrder)
    /**
     * Whether window decorations use the alpha channel.
     */
    Q_PROPERTY(bool decorationsHaveAlpha READ decorationsHaveAlpha)
    Q_PROPERTY(CompositingType compositingType READ compositingType CONSTANT)
    Q_PROPERTY(QPointF cursorPos READ cursorPos)
    Q_PROPERTY(QSize virtualScreenSize READ virtualScreenSize NOTIFY virtualScreenSizeChanged)
    Q_PROPERTY(QRect virtualScreenGeometry READ virtualScreenGeometry NOTIFY virtualScreenGeometryChanged)
    Q_PROPERTY(bool hasActiveFullScreenEffect READ hasActiveFullScreenEffect NOTIFY hasActiveFullScreenEffectChanged)
    Q_PROPERTY(bool colorPickerActive READ isColorPickerActive NOTIFY colorPickerActiveChanged)

    /**
     * The status of the session i.e if the user is logging out
     * @since 5.18
     */
    Q_PROPERTY(KWin::SessionState sessionState READ sessionState NOTIFY sessionStateChanged)

    Q_PROPERTY(KWin::EffectWindow *inputPanel READ inputPanel NOTIFY inputPanelChanged)

    friend class Effect;

public:
    using TouchBorderCallback = std::function<void(ElectricBorder border, const QPointF &, Output *screen)>;

    EffectsHandler(Compositor *compositor, WorkspaceScene *scene);
    ~EffectsHandler() override;

    // internal (used by kwin core or compositing code)
    void startPaint();

    // for use by effects
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime);
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen);
    void postPaintScreen();
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime);
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data);
    void postPaintWindow(EffectWindow *w);
    void drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data);
    void renderWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data);
    QVariant kwinOption(KWinOption kwopt);
    /**
     * Sets the cursor while the mouse is intercepted.
     * @see startMouseInterception
     * @since 4.11
     */
    virtual void defineCursor(Qt::CursorShape shape);
    QPointF cursorPos() const;
    bool grabKeyboard(Effect *effect);
    void ungrabKeyboard();
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
    void startMouseInterception(Effect *effect, Qt::CursorShape shape);
    /**
     * Releases the hold mouse interception for @p effect
     * @see startMouseInterception
     * @since 4.11
     */
    void stopMouseInterception(Effect *effect);
    bool isMouseInterception() const;

    bool checkInputWindowEvent(QMouseEvent *e);
    bool checkInputWindowEvent(QWheelEvent *e);
    void checkInputWindowStacking();

    void grabbedKeyboardEvent(QKeyEvent *e);
    bool hasKeyboardGrab() const;

    /**
     * @brief Registers a global pointer shortcut with the provided @p action.
     *
     * @param modifiers The keyboard modifiers which need to be holded
     * @param pointerButtons The pointer buttons which need to be pressed
     * @param action The action which gets triggered when the shortcut matches
     */
    void registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action);
    /**
     * @brief Registers a global axis shortcut with the provided @p action.
     *
     * @param modifiers The keyboard modifiers which need to be holded
     * @param axis The direction in which the axis needs to be moved
     * @param action The action which gets triggered when the shortcut matches
     */
    void registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action);

    /**
     * @brief Registers a global touchpad swipe gesture shortcut with the provided @p action.
     *
     * @param direction The direction for the swipe
     * @param action The action which gets triggered when the gesture triggers
     * @since 5.10
     */
    void registerTouchpadSwipeShortcut(SwipeDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback = {});

    void registerTouchpadPinchShortcut(PinchDirection dir, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback = {});

    /**
     * @brief Registers a global touchscreen swipe gesture shortcut with the provided @p action.
     *
     * @param direction The direction for the swipe
     * @param action The action which gets triggered when the gesture triggers
     * @since 5.25
     */
    void registerTouchscreenSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> progressCallback);

    void reserveElectricBorder(ElectricBorder border, Effect *effect);
    void unreserveElectricBorder(ElectricBorder border, Effect *effect);

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
    void registerTouchBorder(ElectricBorder border, QAction *action);

    /**
     * Registers the given @p action for the given @p border to be activated through
     * a touch swipe gesture.
     *
     * If the @p border gets triggered through a touch swipe gesture the QAction::triggered
     * signal gets invoked.
     *
     * progressCallback will be dinamically called each time the touch position is updated
     * to show the effect "partially" activated
     *
     * To unregister the touch screen action either delete the @p action or
     * invoke unregisterTouchBorder.
     *
     * @see unregisterTouchBorder
     * @since 5.25
     */
    void registerRealtimeTouchBorder(ElectricBorder border, QAction *action, TouchBorderCallback progressCallback);

    /**
     * Unregisters the given @p action for the given touch @p border.
     *
     * @see registerTouchBorder
     * @since 5.10
     */
    void unregisterTouchBorder(ElectricBorder border, QAction *action);

    // functions that allow controlling windows/desktop
    void activateWindow(KWin::EffectWindow *c);
    KWin::EffectWindow *activeWindow() const;
    Q_SCRIPTABLE void moveWindow(KWin::EffectWindow *w, const QPoint &pos, bool snap = false, double snapAdjust = 1.0);

    /**
     * Moves a window to the given desktops
     * On X11, the window will end up on the last window in the list
     * Setting this to an empty list will set the window on all desktops
     */
    Q_SCRIPTABLE void windowToDesktops(KWin::EffectWindow *w, const QList<KWin::VirtualDesktop *> &desktops);

    Q_SCRIPTABLE void windowToScreen(KWin::EffectWindow *w, Output *screen);
    void setShowingDesktop(bool showing);

    // Activities
    /**
     * @returns The ID of the current activity.
     */
    QString currentActivity() const;
    // Desktops
    /**
     * @returns The current desktop.
     */
    VirtualDesktop *currentDesktop() const;
    /**
     * @returns Total number of desktops currently in existence.
     */
    QList<VirtualDesktop *> desktops() const;
    /**
     * Set the current desktop to @a desktop.
     */
    void setCurrentDesktop(KWin::VirtualDesktop *desktop);
    /**
     * @returns The size of desktop layout in grid units.
     */
    QSize desktopGridSize() const;
    /**
     * @returns The width of desktop layout in grid units.
     */
    int desktopGridWidth() const;
    /**
     * @returns The height of desktop layout in grid units.
     */
    int desktopGridHeight() const;
    /**
     * @returns The width of desktop layout in pixels.
     */
    int workspaceWidth() const;
    /**
     * @returns The height of desktop layout in pixels.
     */
    int workspaceHeight() const;
    /**
     * @returns The desktop at the point @a coords or 0 if no desktop exists at that
     * point. @a coords is to be in grid units.
     */
    VirtualDesktop *desktopAtCoords(QPoint coords) const;
    /**
     * @returns The coords of the specified @a desktop in grid units.
     */
    QPoint desktopGridCoords(VirtualDesktop *desktop) const;
    /**
     * @returns The coords of the top-left corner of @a desktop in pixels.
     */
    QPoint desktopCoords(VirtualDesktop *desktop) const;
    /**
     * @returns The desktop above the given @a desktop. Wraps around to the bottom of
     * the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE KWin::VirtualDesktop *desktopAbove(KWin::VirtualDesktop *desktop = nullptr, bool wrap = true) const;
    /**
     * @returns The desktop to the right of the given @a desktop. Wraps around to the
     * left of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE KWin::VirtualDesktop *desktopToRight(KWin::VirtualDesktop *desktop = nullptr, bool wrap = true) const;
    /**
     * @returns The desktop below the given @a desktop. Wraps around to the top of the
     * layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE KWin::VirtualDesktop *desktopBelow(KWin::VirtualDesktop *desktop = nullptr, bool wrap = true) const;
    /**
     * @returns The desktop to the left of the given @a desktop. Wraps around to the
     * right of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE KWin::VirtualDesktop *desktopToLeft(KWin::VirtualDesktop *desktop = nullptr, bool wrap = true) const;
    Q_SCRIPTABLE QString desktopName(KWin::VirtualDesktop *desktop) const;
    bool optionRollOverDesktops() const;

    Output *activeScreen() const; // Xinerama
    QRectF clientArea(clientAreaOption, const Output *screen, const VirtualDesktop *desktop) const;
    QRectF clientArea(clientAreaOption, const EffectWindow *c) const;
    QRectF clientArea(clientAreaOption, const QPoint &p, const VirtualDesktop *desktop) const;

    /**
     * The bounding size of all screens combined. Overlapping areas
     * are not counted multiple times.
     *
     * @see virtualScreenGeometry()
     * @see virtualScreenSizeChanged()
     * @since 5.0
     */
    QSize virtualScreenSize() const;
    /**
     * The bounding geometry of all outputs combined. Always starts at (0,0) and has
     * virtualScreenSize as it's size.
     *
     * @see virtualScreenSize()
     * @see virtualScreenGeometryChanged()
     * @since 5.0
     */
    QRect virtualScreenGeometry() const;
    /**
     * Factor by which animation speed in the effect should be modified (multiplied).
     * If configurable in the effect itself, the option should have also 'default'
     * animation speed. The actual value should be determined using animationTime().
     * Note: The factor can be also 0, so make sure your code can cope with 0ms time
     * if used manually.
     */
    double animationTimeFactor() const;

    Q_SCRIPTABLE KWin::EffectWindow *findWindow(WId id) const;
    Q_SCRIPTABLE KWin::EffectWindow *findWindow(SurfaceInterface *surf) const;
    /**
     * Finds the EffectWindow for the internal window @p w.
     * If there is no such window @c null is returned.
     *
     * On Wayland this returns the internal window. On X11 it returns an Unamanged with the
     * window id matching that of the provided window @p w.
     *
     * @since 5.16
     */
    Q_SCRIPTABLE KWin::EffectWindow *findWindow(QWindow *w) const;
    /**
     * Finds the EffectWindow for the Window with KWin internal @p id.
     * If there is no such window @c null is returned.
     *
     * @since 5.16
     */
    Q_SCRIPTABLE KWin::EffectWindow *findWindow(const QUuid &id) const;
    QList<EffectWindow *> stackingOrder() const;
    // window will be temporarily painted as if being at the top of the stack
    Q_SCRIPTABLE void setElevatedWindow(KWin::EffectWindow *w, bool set);

    void setTabBoxWindow(EffectWindow *);
    QList<EffectWindow *> currentTabBoxWindowList() const;
    void refTabBox();
    void unrefTabBox();
    void closeTabBox();
    EffectWindow *currentTabBoxWindow() const;

    void setActiveFullScreenEffect(Effect *e);
    Effect *activeFullScreenEffect() const;

    /**
     * Schedules the entire workspace to be repainted next time.
     * If you call it during painting (including prepaint) then it does not
     *  affect the current painting.
     */
    Q_SCRIPTABLE void addRepaintFull();
    Q_SCRIPTABLE void addRepaint(const QRectF &r);
    Q_SCRIPTABLE void addRepaint(const QRect &r);
    Q_SCRIPTABLE void addRepaint(const QRegion &r);
    Q_SCRIPTABLE void addRepaint(int x, int y, int w, int h);

    CompositingType compositingType() const;
    /**
     * @brief Whether the Compositor is OpenGL based (either GL 1 or 2).
     *
     * @return bool @c true in case of OpenGL based Compositor, @c false otherwise
     */
    bool isOpenGLCompositing() const;
    OpenGlContext *openglContext() const;
    /**
     * @brief Provides access to the QPainter which is rendering to the back buffer.
     *
     * Only relevant for CompositingType QPainterCompositing. For all other compositing types
     * @c null is returned.
     *
     * @return QPainter* The Scene's QPainter or @c null.
     */
    QPainter *scenePainter();
    void reconfigure();

    QByteArray readRootProperty(long atom, long type, int format) const;

#if KWIN_BUILD_X11
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
    xcb_atom_t announceSupportProperty(const QByteArray &propertyName, Effect *effect);
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
    void removeSupportProperty(const QByteArray &propertyName, Effect *effect);
#endif

    /**
     * Returns @a true if the active window decoration has shadow API hooks.
     */
    bool hasDecorationShadows() const;

    /**
     * Returns @a true if the window decorations use the alpha channel, and @a false otherwise.
     * @since 4.5
     */
    bool decorationsHaveAlpha() const;

    /**
     * Allows an effect to trigger a reload of itself.
     * This can be used by an effect which needs to be reloaded when screen geometry changes.
     * It is possible that the effect cannot be loaded again as it's supported method does no longer
     * hold.
     * @param effect The effect to reload
     * @since 4.8
     */
    void reloadEffect(Effect *effect);
    Effect *provides(Effect::Feature ef);
    Effect *findEffect(const QString &name) const;
    QStringList loadedEffects() const;
    QStringList listOfEffects() const;
    void unloadAllEffects();
    QStringList activeEffects() const;
    bool isEffectActive(const QString &pluginId) const;

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
    bool isScreenLocked() const;

    /**
     * @brief Makes the OpenGL compositing context current.
     *
     * If the compositing backend is not using OpenGL, this method returns @c false.
     *
     * @return bool @c true if the context became current, @c false otherwise.
     */
    bool makeOpenGLContextCurrent();
    /**
     * @brief Makes a null OpenGL context current resulting in no context
     * being current.
     *
     * If the compositing backend is not OpenGL based, this method is a noop.
     *
     * There is normally no reason for an Effect to call this method.
     */
    void doneOpenGLContextCurrent();

#if KWIN_BUILD_X11
    xcb_connection_t *xcbConnection() const;
    xcb_window_t x11RootWindow() const;
#endif

    /**
     * Interface to the Wayland display: this is relevant only
     * on Wayland, on X11 it will be nullptr
     * @since 5.5
     */
    Display *waylandDisplay() const;

    /**
     * Whether animations are supported by the Scene.
     * If this method returns @c false Effects are supposed to not
     * animate transitions.
     *
     * @returns Whether the Scene can drive animations
     * @since 5.8
     */
    bool animationsSupported() const;

    /**
     * The current cursor image of the Platform.
     * @see cursorPos
     * @since 5.9
     */
    PlatformCursorImage cursorImage() const;

    /**
     * The cursor image should be hidden.
     * @see showCursor
     * @since 5.9
     */
    void hideCursor();

    /**
     * The cursor image should be shown again after having been hidden.
     * @see hideCursor
     * @since 5.9
     */
    void showCursor();

    /**
     * @returns Whether or not the cursor is currently hidden
     */
    bool isCursorHidden() const;

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
    void startInteractiveWindowSelection(std::function<void(KWin::EffectWindow *)> callback);

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
    void startInteractivePositionSelection(std::function<void(const QPointF &)> callback);

    /**
     * Shows an on-screen-message. To hide it again use hideOnScreenMessage.
     *
     * @param message The message to show
     * @param iconName The optional themed icon name
     * @see hideOnScreenMessage
     * @since 5.9
     */
    void showOnScreenMessage(const QString &message, const QString &iconName = QString());

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
    void hideOnScreenMessage(OnScreenMessageHideFlags flags = OnScreenMessageHideFlags());

    /*
     * @returns The configuration used by the EffectsHandler.
     * @since 5.10
     */
    KSharedConfigPtr config() const;

    /**
     * @returns The global input configuration (kcminputrc)
     * @since 5.10
     */
    KSharedConfigPtr inputConfig() const;

    /**
     * Returns true if activeFullScreenEffect is set
     */
    bool hasActiveFullScreenEffect() const;

    /**
     * Returns true if color picker effect is currently picking colors.
     */
    bool isColorPickerActive() const;

    /**
     * Render the supplied OffscreenQuickView onto the scene
     * It can be called at any point during the scene rendering
     * @since 5.18
     */
    void renderOffscreenQuickView(const RenderTarget &renderTarget, const RenderViewport &viewport, OffscreenQuickView *effectQuickView) const;

    /**
     * The status of the session i.e if the user is logging out
     * @since 5.18
     */
    SessionState sessionState() const;

    /**
     * Returns the list of all the screens connected to the system.
     */
    QList<Output *> screens() const;
    Output *screenAt(const QPoint &point) const;
    Output *findScreen(const QString &name) const;
    Output *findScreen(int screenId) const;

    KWin::EffectWindow *inputPanel() const;
    bool isInputPanelOverlay() const;

    QQmlEngine *qmlEngine() const;

    /**
     * @returns whether or not any effect is currently active where KWin should not use direct scanout
     */
    bool blocksDirectScanout() const;

    WorkspaceScene *scene() const
    {
        return m_scene;
    }

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time);
    bool touchUp(qint32 id, std::chrono::microseconds time);
    void touchCancel();

    bool tabletToolProximityEvent(KWin::TabletToolProximityEvent *event);
    bool tabletToolAxisEvent(KWin::TabletToolAxisEvent *event);
    bool tabletToolTipEvent(KWin::TabletToolTipEvent *event);
    bool tabletToolButtonEvent(uint button, bool pressed, InputDeviceTabletTool *tool, std::chrono::microseconds time);
    bool tabletPadButtonEvent(uint button, bool pressed, std::chrono::microseconds time, InputDevice *device);
    bool tabletPadStripEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device);
    bool tabletPadRingEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device);

    void highlightWindows(const QList<EffectWindow *> &windows);

#if KWIN_BUILD_X11
    bool isPropertyTypeRegistered(xcb_atom_t atom) const
    {
        return registered_atoms.contains(atom);
    }
#endif

Q_SIGNALS:
    /**
     * This signal is emitted whenever a new @a screen is added to the system.
     */
    void screenAdded(KWin::Output *screen);
    /**
     * This signal is emitted whenever a @a screen is removed from the system.
     */
    void screenRemoved(KWin::Output *screen);
    /**
     * Signal emitted when the current desktop changed.
     * @param oldDesktop The previously current desktop
     * @param newDesktop The new current desktop
     * @param with The window which is taken over to the new desktop, can be NULL
     * @since 4.9
     */
    void desktopChanged(KWin::VirtualDesktop *oldDesktop, KWin::VirtualDesktop *newDesktop, KWin::EffectWindow *with);

    /**
     * Signal emmitted while desktop is changing for animation.
     * @param currentDesktop The current desktop untiotherwise.
     * @param offset The current desktop offset.
     * offset.x() = .6 means 60% of the way to the desktop to the right.
     * Positive Values means Up and Right.
     */
    void desktopChanging(KWin::VirtualDesktop *currentDesktop, QPointF offset, KWin::EffectWindow *with);
    void desktopChangingCancelled();
    void desktopAdded(KWin::VirtualDesktop *desktop);
    void desktopRemoved(KWin::VirtualDesktop *desktop);

    /**
     * Emitted when the virtual desktop grid layout changes
     * @param size new size
     * @since 5.25
     */
    void desktopGridSizeChanged(const QSize &size);
    /**
     * Emitted when the virtual desktop grid layout changes
     * @param width new width
     * @since 5.25
     */
    void desktopGridWidthChanged(int width);
    /**
     * Emitted when the virtual desktop grid layout changes
     * @param height new height
     * @since 5.25
     */
    void desktopGridHeightChanged(int height);
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
    void tabBoxKeyEvent(QKeyEvent *event);
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
    void mouseChanged(const QPointF &pos, const QPointF &oldpos,
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
    void propertyNotify(KWin::EffectWindow *w, long atom);

    /**
     * emitted before the current activity actually changes
     * @since 6.3
     */
    void currentActivityAboutToChange();
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

#if KWIN_BUILD_X11
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
#endif

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

    void colorPickerActiveChanged();

    /**
     * This signal is emitted when the session state was changed
     * @since 5.18
     */
    void sessionStateChanged();

    void startupAdded(const QString &id, const QIcon &icon);
    void startupChanged(const QString &id, const QIcon &icon);
    void startupRemoved(const QString &id);

    void inputPanelChanged();

public Q_SLOTS:
    // slots for D-Bus interface
    Q_SCRIPTABLE void reconfigureEffect(const QString &name);
    Q_SCRIPTABLE bool loadEffect(const QString &name);
    Q_SCRIPTABLE void toggleEffect(const QString &name);
    Q_SCRIPTABLE void unloadEffect(const QString &name);
    Q_SCRIPTABLE bool isEffectLoaded(const QString &name) const;
    Q_SCRIPTABLE bool isEffectSupported(const QString &name);
    Q_SCRIPTABLE QList<bool> areEffectsSupported(const QStringList &names);
    Q_SCRIPTABLE QString supportInformation(const QString &name) const;
    Q_SCRIPTABLE QString debug(const QString &name, const QString &parameter = QString()) const;

protected:
    void effectsChanged();
    void setupWindowConnections(KWin::Window *window);

    /**
     * Default implementation does nothing and returns @c true.
     */
    virtual bool doGrabKeyboard();
    /**
     * Default implementation does nothing.
     */
    virtual void doUngrabKeyboard();

    /**
     * Default implementation sets Effects override cursor on the PointerInputRedirection.
     */
    virtual void doStartMouseInterception(Qt::CursorShape shape);

    /**
     * Default implementation removes the Effects override cursor on the PointerInputRedirection.
     */
    virtual void doStopMouseInterception();

    /**
     * Default implementation does nothing
     */
    virtual void doCheckInputWindowStacking();

    void registerPropertyType(long atom, bool reg);
    void destroyEffect(Effect *effect);
    void reconfigureEffects();

    typedef QList<Effect *> EffectsList;
    typedef EffectsList::const_iterator EffectsIterator;

    Effect *keyboard_grab_effect;
    Effect *fullscreen_effect;
    QMultiMap<int, EffectPair> effect_order;
#if KWIN_BUILD_X11
    QHash<long, int> registered_atoms;
#endif
    QList<EffectPair> loaded_effects;
    CompositingType compositing_type;
    EffectsList m_activeEffects;
    EffectsIterator m_currentDrawWindowIterator;
    EffectsIterator m_currentPaintWindowIterator;
    EffectsIterator m_currentPaintScreenIterator;
    typedef QHash<QByteArray, QList<Effect *>> PropertyEffectMap;
#if KWIN_BUILD_X11
    PropertyEffectMap m_propertiesForEffects;
#endif
    QHash<QByteArray, qulonglong> m_managedProperties;
    Compositor *m_compositor;
    WorkspaceScene *m_scene;
    QList<Effect *> m_grabbedMouseEffects;
    EffectLoader *m_effectLoader;
    std::unique_ptr<WindowPropertyNotifyX11Filter> m_x11WindowPropertyNotify;
};

/**
 * Pointer to the global EffectsHandler object.
 */
extern KWIN_EXPORT EffectsHandler *effects;

} // namespace

/** @} */
