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

#include "libkwineffects/effect.h"

#include <QEasingCurve>
#include <QIcon>
#include <QPair>
#include <QRect>
#include <QRegion>
#include <QSet>
#include <QVector2D>
#include <QVector3D>

#include <QHash>
#include <QList>
#include <QLoggingCategory>
#include <QStack>

#include <netwm.h>

#include <climits>
#include <cmath>
#include <functional>
#include <optional>
#include <span>

class KConfigGroup;
class QFont;
class QKeyEvent;
class QMatrix4x4;
class QMouseEvent;
class QWheelEvent;
class QAction;
class QTabletEvent;
class QQmlEngine;

/**
 * Logging category to be used inside the KWin effects.
 * Do not use in this library.
 */
Q_DECLARE_LOGGING_CATEGORY(KWINEFFECTS)

namespace KDecoration2
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
class TabletEvent;
class TabletPadId;
class TabletToolId;
class Window;
class WindowItem;
class WindowPropertyNotifyX11Filter;
class WindowQuad;
class WindowQuadList;
class WorkspaceScene;
class VirtualDesktop;

typedef QPair<QString, Effect *> EffectPair;
typedef QList<KWin::EffectWindow *> EffectWindowList;

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
    Q_PROPERTY(KWin::EffectWindowList stackingOrder READ stackingOrder)
    /**
     * Whether window decorations use the alpha channel.
     */
    Q_PROPERTY(bool decorationsHaveAlpha READ decorationsHaveAlpha)
    Q_PROPERTY(CompositingType compositingType READ compositingType CONSTANT)
    Q_PROPERTY(QPointF cursorPos READ cursorPos)
    Q_PROPERTY(QSize virtualScreenSize READ virtualScreenSize NOTIFY virtualScreenSizeChanged)
    Q_PROPERTY(QRect virtualScreenGeometry READ virtualScreenGeometry NOTIFY virtualScreenGeometryChanged)
    Q_PROPERTY(bool hasActiveFullScreenEffect READ hasActiveFullScreenEffect NOTIFY hasActiveFullScreenEffectChanged)

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

    // Mouse polling
    void startMousePolling();
    void stopMousePolling();

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
    EffectWindowList stackingOrder() const;
    // window will be temporarily painted as if being at the top of the stack
    Q_SCRIPTABLE void setElevatedWindow(KWin::EffectWindow *w, bool set);

    void setTabBoxWindow(EffectWindow *);
    EffectWindowList currentTabBoxWindowList() const;
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

    xcb_connection_t *xcbConnection() const;
    xcb_window_t x11RootWindow() const;

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
     * Returns if activeFullScreenEffect is set
     */
    bool hasActiveFullScreenEffect() const;

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

    /**
     * Renders @p screen in the current render target
     */
    void renderScreen(Output *screen);

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

    bool tabletToolEvent(KWin::TabletEvent *event);
    bool tabletToolButtonEvent(uint button, bool pressed, const KWin::TabletToolId &tabletToolId, std::chrono::microseconds time);
    bool tabletPadButtonEvent(uint button, bool pressed, const KWin::TabletPadId &tabletPadId, std::chrono::microseconds time);
    bool tabletPadStripEvent(int number, int position, bool isFinger, const KWin::TabletPadId &tabletPadId, std::chrono::microseconds time);
    bool tabletPadRingEvent(int number, int position, bool isFinger, const KWin::TabletPadId &tabletPadId, std::chrono::microseconds time);

    void highlightWindows(const QList<EffectWindow *> &windows);

    bool isPropertyTypeRegistered(xcb_atom_t atom) const
    {
        return registered_atoms.contains(atom);
    }

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
    void connectNotify(const QMetaMethod &signal) override;
    void disconnectNotify(const QMetaMethod &signal) override;
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
    QHash<long, int> registered_atoms;
    QList<EffectPair> loaded_effects;
    CompositingType compositing_type;
    EffectsList m_activeEffects;
    EffectsIterator m_currentDrawWindowIterator;
    EffectsIterator m_currentPaintWindowIterator;
    EffectsIterator m_currentPaintScreenIterator;
    typedef QHash<QByteArray, QList<Effect *>> PropertyEffectMap;
    PropertyEffectMap m_propertiesForEffects;
    QHash<QByteArray, qulonglong> m_managedProperties;
    Compositor *m_compositor;
    WorkspaceScene *m_scene;
    QList<Effect *> m_grabbedMouseEffects;
    EffectLoader *m_effectLoader;
    int m_trackingCursorChanges;
    std::unique_ptr<WindowPropertyNotifyX11Filter> m_x11WindowPropertyNotify;
};

class EffectWindowVisibleRef;
/**
 * @short Representation of a window used by/for Effect classes.
 *
 * The purpose is to hide internal data and also to serve as a single
 *  representation for the case when Client/Unmanaged becomes Deleted.
 */
class KWIN_EXPORT EffectWindow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF geometry READ frameGeometry)
    Q_PROPERTY(QRectF expandedGeometry READ expandedGeometry)
    Q_PROPERTY(qreal height READ height)
    Q_PROPERTY(qreal opacity READ opacity)
    Q_PROPERTY(QPointF pos READ pos)
    Q_PROPERTY(KWin::Output *screen READ screen)
    Q_PROPERTY(QSizeF size READ size)
    Q_PROPERTY(qreal width READ width)
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(QList<KWin::VirtualDesktop *> desktops READ desktops)
    Q_PROPERTY(bool onAllDesktops READ isOnAllDesktops)
    Q_PROPERTY(bool onCurrentDesktop READ isOnCurrentDesktop)
    Q_PROPERTY(QRectF rect READ rect)
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
    Q_PROPERTY(QSizeF basicUnit READ basicUnit)
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
    Q_PROPERTY(QRectF iconGeometry READ iconGeometry)
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
    Q_PROPERTY(QRectF contentsRect READ contentsRect)
    /**
     * Geometry of the transparent rect in the decoration.
     * May be different from contentsRect if the decoration is extended into the client area.
     */
    Q_PROPERTY(QRectF decorationInnerRect READ decorationInnerRect)
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

    /**
     * Whether this EffectWindow represents the screenlocker greeter.
     *
     * @since 5.22
     */
    Q_PROPERTY(bool lockScreen READ isLockScreen CONSTANT)

    /**
     * Whether this EffectWindow is hidden because the show desktop mode is active.
     */
    Q_PROPERTY(bool hiddenByShowDesktop READ isHiddenByShowDesktop)

public:
    /**  Flags explaining why painting should be disabled  */
    enum {
        /**  Window will not be painted  */
        PAINT_DISABLED = 1 << 0,
        /**  Window will not be painted because of which desktop it's on  */
        PAINT_DISABLED_BY_DESKTOP = 1 << 1,
        /**  Window will not be painted because it is minimized  */
        PAINT_DISABLED_BY_MINIMIZE = 1 << 2,
        /**  Window will not be painted because it's not on the current activity  */
        PAINT_DISABLED_BY_ACTIVITY = 1 << 3,
    };

    explicit EffectWindow(WindowItem *windowItem);
    ~EffectWindow() override;

    Q_SCRIPTABLE void addRepaint(const QRect &r);
    Q_SCRIPTABLE void addRepaint(int x, int y, int w, int h);
    Q_SCRIPTABLE void addRepaintFull();
    Q_SCRIPTABLE void addLayerRepaint(const QRect &r);
    Q_SCRIPTABLE void addLayerRepaint(int x, int y, int w, int h);

    void refWindow();
    void unrefWindow();

    bool isDeleted() const;
    bool isHidden() const;
    bool isHiddenByShowDesktop() const;

    bool isMinimized() const;
    double opacity() const;

    bool isOnCurrentActivity() const;
    Q_SCRIPTABLE bool isOnActivity(const QString &id) const;
    bool isOnAllActivities() const;
    QStringList activities() const;

    Q_SCRIPTABLE bool isOnDesktop(KWin::VirtualDesktop *desktop) const;
    bool isOnCurrentDesktop() const;
    bool isOnAllDesktops() const;
    /**
     * All the desktops by number that the window is in. On X11 this list will always have
     * a length of 1, on Wayland can be any subset.
     * If the list is empty it means the window is on all desktops
     */
    QList<KWin::VirtualDesktop *> desktops() const;

    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     */
    QSizeF basicUnit() const;
    /**
     * Returns the geometry of the window excluding server-side and client-side
     * drop-shadows.
     *
     * @since 5.18
     */
    QRectF frameGeometry() const;
    /**
     * Returns the geometry of the pixmap or buffer attached to this window.
     *
     * For X11 clients, this method returns server-side geometry of the Window.
     *
     * For Wayland clients, this method returns rectangle that the main surface
     * occupies on the screen, in global screen coordinates.
     *
     * @since 5.18
     */
    QRectF bufferGeometry() const;
    QRectF clientGeometry() const;
    /**
     * Geometry of the window including decoration and potentially shadows.
     * May be different from geometry() if the window has a shadow.
     * @since 4.9
     */
    QRectF expandedGeometry() const;
    Output *screen() const;
    QPointF pos() const;
    QSizeF size() const;
    QRectF rect() const;
    bool isMovable() const;
    bool isMovableAcrossScreens() const;
    bool isUserMove() const;
    bool isUserResize() const;
    QRectF iconGeometry() const;

    /**
     * Geometry of the actual window contents inside the whole (including decorations) window.
     */
    QRectF contentsRect() const;
    /**
     * Geometry of the transparent rect in the decoration.
     * May be different from contentsRect() if the decoration is extended into the client area.
     * @since 4.5
     */
    QRectF decorationInnerRect() const;
    bool hasDecoration() const;
    bool decorationHasAlpha() const;
    /**
     * Returns the decoration
     * @since 5.25
     */
    KDecoration2::Decoration *decoration() const;
    QByteArray readProperty(long atom, long type, int format) const;
    void deleteProperty(long atom) const;

    QString caption() const;
    QIcon icon() const;
    QString windowClass() const;
    QString windowRole() const;
    const EffectWindowGroup *group() const;

    /**
     * Returns whether the window is a desktop background window (the one with wallpaper).
     * See _NET_WM_WINDOW_TYPE_DESKTOP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDesktop() const;
    /**
     * Returns whether the window is a dock (i.e. a panel).
     * See _NET_WM_WINDOW_TYPE_DOCK at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDock() const;
    /**
     * Returns whether the window is a standalone (detached) toolbar window.
     * See _NET_WM_WINDOW_TYPE_TOOLBAR at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isToolbar() const;
    /**
     * Returns whether the window is a torn-off menu.
     * See _NET_WM_WINDOW_TYPE_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isMenu() const;
    /**
     * Returns whether the window is a "normal" window, i.e. an application or any other window
     * for which none of the specialized window types fit.
     * See _NET_WM_WINDOW_TYPE_NORMAL at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
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
     * See _NET_WM_WINDOW_TYPE_DIALOG at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDialog() const;
    /**
     * Returns whether the window is a splashscreen. Note that many (especially older) applications
     * do not support marking their splash windows with this type.
     * See _NET_WM_WINDOW_TYPE_SPLASH at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isSplash() const;
    /**
     * Returns whether the window is a utility window, such as a tool window.
     * See _NET_WM_WINDOW_TYPE_UTILITY at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isUtility() const;
    /**
     * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
     * from the applications menubar).
     * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDropdownMenu() const;
    /**
     * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
     * See _NET_WM_WINDOW_TYPE_POPUP_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isPopupMenu() const; // a context popup, not dropdown, not torn-off
    /**
     * Returns whether the window is a tooltip.
     * See _NET_WM_WINDOW_TYPE_TOOLTIP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isTooltip() const;
    /**
     * Returns whether the window is a window with a notification.
     * See _NET_WM_WINDOW_TYPE_NOTIFICATION at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isNotification() const;
    /**
     * Returns whether the window is a window with a critical notification.
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_CRITICAL_NOTIFICATION
     */
    bool isCriticalNotification() const;
    /**
     * Returns whether the window is a window used for applet popups.
     */
    bool isAppletPopup() const;
    /**
     * Returns whether the window is an on screen display window
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY
     */
    bool isOnScreenDisplay() const;
    /**
     * Returns whether the window is a combobox popup.
     * See _NET_WM_WINDOW_TYPE_COMBO at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isComboBox() const;
    /**
     * Returns whether the window is a Drag&Drop icon.
     * See _NET_WM_WINDOW_TYPE_DND at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDNDIcon() const;
    /**
     * Returns the NETWM window type
     * See https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
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
    /**
     * Returns whether the window is kept below all other windows.
     */
    bool keepBelow() const;

    bool isModal() const;
    Q_SCRIPTABLE KWin::EffectWindow *findModal();
    Q_SCRIPTABLE KWin::EffectWindow *transientFor();
    Q_SCRIPTABLE KWin::EffectWindowList mainWindows() const;

    /**
     * Returns whether the window should be excluded from window switching effects.
     * @since 4.5
     */
    bool isSkipSwitcher() const;

    void setMinimized(bool minimize);
    void minimize();
    void unminimize();
    Q_SCRIPTABLE void closeWindow();

    /**
     * @since 4.11
     */
    bool isVisible() const;

    /**
     * @since 5.0
     */
    bool skipsCloseAnimation() const;

    /**
     * @since 5.5
     */
    SurfaceInterface *surface() const;

    /**
     * @since 5.6
     */
    bool isFullScreen() const;

    /**
     * @since 5.10
     */
    bool isUnresponsive() const;

    /**
     * @since 5.15
     */
    bool isWaylandClient() const;

    /**
     * @since 5.15
     */
    bool isX11Client() const;

    /**
     * @since 5.15
     */
    bool isPopupWindow() const;

    /**
     * @since 5.16
     */
    QWindow *internalWindow() const;

    /**
     * @since 5.16
     */
    bool isOutline() const;

    /**
     * @since 5.22
     */
    bool isLockScreen() const;

    /**
     * @since 5.18
     */
    pid_t pid() const;

    /**
     * @since 5.21
     */
    qlonglong windowId() const;
    /**
     * Returns the internal id of the window that uniquely identifies it. The main difference
     * between internalId() and windowId() is that the latter one works as expected only on X11,
     * while the former is unique regardless of the window system.
     *
     * Note that the internaId() has special meaning only to kwin.
     * @since 5.24
     */
    QUuid internalId() const;

    /**
     * @since 6.0
     */
    bool isInputMethod() const;

    /**
     * Can be used to by effects to store arbitrary data in the EffectWindow.
     *
     * Invoking this method will emit the signal EffectsHandler::windowDataChanged.
     * @see EffectsHandler::windowDataChanged
     */
    Q_SCRIPTABLE void setData(int role, const QVariant &data);
    Q_SCRIPTABLE QVariant data(int role) const;

    Window *window() const;
    WindowItem *windowItem() const;
    void elevate(bool elevate);

Q_SIGNALS:
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
     */
    void windowStepUserMovedResized(KWin::EffectWindow *w, const QRectF &geometry);
    /**
     * Signal emitted when the user finishes move/resize of window @p w.
     * @param w The window which has been moved/resized
     * @see windowStartUserMovedResized
     * @see windowFinishUserMovedResized
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
     */
    void windowMaximizedStateChanged(KWin::EffectWindow *w, bool horizontal, bool vertical);

    /**
     * Signal emitted when the maximized state of the window @p w is about to change,
     * but before windowMaximizedStateChanged is emitted or any geometry change.
     * Useful for OffscreenEffect to grab a window image before any actual change happens
     *
     * A window can be in one of four states:
     * @li restored: both @p horizontal and @p vertical are @c false
     * @li horizontally maximized: @p horizontal is @c true and @p vertical is @c false
     * @li vertically maximized: @p horizontal is @c false and @p vertical is @c true
     * @li completely maximized: both @p horizontal and @p vertical are @c true
     * @param w The window whose maximized state changed
     * @param horizontal If @c true maximized horizontally
     * @param vertical If @c true maximized vertically
     */
    void windowMaximizedStateAboutToChange(KWin::EffectWindow *w, bool horizontal, bool vertical);

    /**
     * This signal is emitted when the frame geometry of a window changed.
     * @param window The window whose geometry changed
     * @param oldGeometry The previous geometry
     */
    void windowFrameGeometryChanged(KWin::EffectWindow *window, const QRectF &oldGeometry);

    /**
     * This signal is emitted when the frame geometry is about to change, the new one is not known yet.
     * Useful for OffscreenEffect to grab a window image before any actual change happens.
     *
     * @param window The window whose geometry is about to change
     */
    void windowFrameGeometryAboutToChange(KWin::EffectWindow *window);

    /**
     * Signal emitted when the windows opacity is changed.
     * @param w The window whose opacity level is changed.
     * @param oldOpacity The previous opacity level
     * @param newOpacity The new opacity level
     */
    void windowOpacityChanged(KWin::EffectWindow *w, qreal oldOpacity, qreal newOpacity);
    /**
     * Signal emitted when a window got minimized.
     * @param w The window which was minimized
     */
    void windowMinimized(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window got unminimized.
     * @param w The window which was unminimized
     */
    void windowUnminimized(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window either becomes modal (ie. blocking for its main client) or looses that state.
     * @param w The window which was unminimized
     */
    void windowModalityChanged(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window either became unresponsive (eg. app froze or crashed)
     * or respoonsive
     * @param w The window that became (un)responsive
     * @param unresponsive Whether the window is responsive or unresponsive
     */
    void windowUnresponsiveChanged(KWin::EffectWindow *w, bool unresponsive);
    /**
     * Signal emitted when an area of a window is scheduled for repainting.
     * Use this signal in an effect if another area needs to be synced as well.
     * @param w The window which is scheduled for repainting
     */
    void windowDamaged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the keep above state of @p w was changed.
     *
     * @param w The window whose the keep above state was changed.
     */
    void windowKeepAboveChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the keep below state of @p was changed.
     *
     * @param w The window whose the keep below state was changed.
     */
    void windowKeepBelowChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the full screen state of @p w was changed.
     *
     * @param w The window whose the full screen state was changed.
     */
    void windowFullScreenChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when decoration of @p was changed.
     *
     * @param w The window for which decoration changed
     */
    void windowDecorationChanged(KWin::EffectWindow *window);

    /**
     * This signal is emitted when the visible geometry of a window changed.
     */
    void windowExpandedGeometryChanged(KWin::EffectWindow *window);

    /**
     * This signal is emitted when a window enters or leaves a virtual desktop.
     */
    void windowDesktopsChanged(KWin::EffectWindow *window);

    /**
     * The window @p w gets shown again. The window was previously
     * initially shown with windowAdded and hidden with windowHidden.
     *
     * @see windowHidden
     * @see windowAdded
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
     */
    void windowHidden(KWin::EffectWindow *w);

protected:
    friend EffectWindowVisibleRef;
    void refVisible(const EffectWindowVisibleRef *holder);
    void unrefVisible(const EffectWindowVisibleRef *holder);

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * The EffectWindowDeletedRef provides a convenient way to prevent deleting a closed
 * window until an effect has finished animating it.
 */
class KWIN_EXPORT EffectWindowDeletedRef
{
public:
    EffectWindowDeletedRef()
        : m_window(nullptr)
    {
    }

    explicit EffectWindowDeletedRef(EffectWindow *window)
        : m_window(window)
    {
        m_window->refWindow();
    }

    EffectWindowDeletedRef(const EffectWindowDeletedRef &other)
        : m_window(other.m_window)
    {
        if (m_window) {
            m_window->refWindow();
        }
    }

    ~EffectWindowDeletedRef()
    {
        if (m_window) {
            m_window->unrefWindow();
        }
    }

    EffectWindowDeletedRef &operator=(const EffectWindowDeletedRef &other)
    {
        if (other.m_window) {
            other.m_window->refWindow();
        }
        if (m_window) {
            m_window->unrefWindow();
        }
        m_window = other.m_window;
        return *this;
    }

    bool isNull() const
    {
        return m_window == nullptr;
    }

private:
    EffectWindow *m_window;
};

/**
 * The EffectWindowVisibleRef provides a convenient way to force the visible status of a
 * window until an effect is finished animating it.
 */
class KWIN_EXPORT EffectWindowVisibleRef
{
public:
    EffectWindowVisibleRef()
        : m_window(nullptr)
        , m_reason(0)
    {
    }

    explicit EffectWindowVisibleRef(EffectWindow *window, int reason)
        : m_window(window)
        , m_reason(reason)
    {
        m_window->refVisible(this);
    }

    EffectWindowVisibleRef(const EffectWindowVisibleRef &other)
        : m_window(other.m_window)
        , m_reason(other.m_reason)
    {
        if (m_window) {
            m_window->refVisible(this);
        }
    }

    ~EffectWindowVisibleRef()
    {
        if (m_window) {
            m_window->unrefVisible(this);
        }
    }

    int reason() const
    {
        return m_reason;
    }

    EffectWindowVisibleRef &operator=(const EffectWindowVisibleRef &other)
    {
        if (other.m_window) {
            other.m_window->refVisible(&other);
        }
        if (m_window) {
            m_window->unrefVisible(this);
        }
        m_window = other.m_window;
        m_reason = other.m_reason;
        return *this;
    }

    bool isNull() const
    {
        return m_window == nullptr;
    }

private:
    EffectWindow *m_window;
    int m_reason;
};

class KWIN_EXPORT EffectWindowGroup
{
public:
    explicit EffectWindowGroup(Group *group);
    virtual ~EffectWindowGroup();

    EffectWindowList members() const;

private:
    Group *m_group;
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
class KWIN_EXPORT WindowVertex
{
public:
    WindowVertex();
    WindowVertex(const QPointF &position, const QPointF &textureCoordinate);
    WindowVertex(double x, double y, double tx, double ty);

    double x() const
    {
        return px;
    }
    double y() const
    {
        return py;
    }
    double u() const
    {
        return tx;
    }
    double v() const
    {
        return ty;
    }
    void move(double x, double y);
    void setX(double x);
    void setY(double y);

private:
    friend class WindowQuad;
    friend class WindowQuadList;
    double px, py; // position
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
    WindowQuad();
    WindowQuad makeSubQuad(double x1, double y1, double x2, double y2) const;
    WindowVertex &operator[](int index);
    const WindowVertex &operator[](int index) const;
    double left() const;
    double right() const;
    double top() const;
    double bottom() const;
    QRectF bounds() const;

private:
    friend class WindowQuadList;
    WindowVertex verts[4];
};

class KWIN_EXPORT WindowQuadList
    : public QList<WindowQuad>
{
public:
    WindowQuadList splitAtX(double x) const;
    WindowQuadList splitAtY(double y) const;
    WindowQuadList makeGrid(int maxquadsize) const;
    WindowQuadList makeRegularGrid(int xSubdivisions, int ySubdivisions) const;
};

/**
 * A helper class for render geometry in device coordinates.
 *
 * This mostly represents a vector of vertices, with some convenience methods
 * for easily converting from WindowQuad and related classes to lists of
 * GLVertex2D. This class assumes rendering happens as unindexed triangles.
 */
class KWIN_EXPORT RenderGeometry : public QList<GLVertex2D>
{
public:
    /**
     * In what way should vertices snap to integer device coordinates?
     *
     * Vertices are converted to device coordinates before being sent to the
     * rendering system. Depending on scaling factors, this may lead to device
     * coordinates with fractional parts. For some cases, this may not be ideal
     * as fractional coordinates need to be interpolated and can lead to
     * "blurry" rendering. To avoid that, we can snap the vertices to integer
     * device coordinates when they are added.
     */
    enum class VertexSnappingMode {
        None, //< No rounding, device coordinates containing fractional parts
              //  are passed directly to the rendering system.
        Round, //< Perform a simple rounding, device coordinates will not have
               //  any fractional parts.
    };

    /**
     * The vertex snapping mode to use for this geometry.
     *
     * By default, this is VertexSnappingMode::Round.
     */
    inline VertexSnappingMode vertexSnappingMode() const
    {
        return m_vertexSnappingMode;
    }
    /**
     * Set the vertex snapping mode to use for this geometry.
     *
     * Note that this doesn't change vertices retroactively, so you should set
     * this before adding any vertices, or clear and rebuild the geometry after
     * setting it.
     *
     * @param mode The new rounding mode.
     */
    void setVertexSnappingMode(VertexSnappingMode mode)
    {
        m_vertexSnappingMode = mode;
    }
    /**
     * Copy geometry data into another buffer.
     *
     * This is primarily intended for copying into a vertex buffer for rendering.
     *
     * @param destination The destination buffer. This needs to be at least large
     *                    enough to contain all elements.
     */
    void copy(std::span<GLVertex2D> destination);
    /**
     * Append a WindowVertex as a geometry vertex.
     *
     * WindowVertex is assumed to be in logical coordinates. It will be converted
     * to device coordinates using the specified device scale and then rounded
     * so it fits correctly on the device pixel grid.
     *
     * @param windowVertex The WindowVertex instance to append.
     * @param deviceScale The scaling factor to use to go from logical to device
     *                    coordinates.
     */
    void appendWindowVertex(const WindowVertex &windowVertex, qreal deviceScale);
    /**
     * Append a WindowQuad as two triangles.
     *
     * This will append the corners of the specified WindowQuad in the right
     * order so they make two triangles that can be rendered by OpenGL. The
     * corners are converted to device coordinates and rounded, just like
     * `appendWindowVertex()` does.
     *
     * @param quad The WindowQuad instance to append.
     * @param deviceScale The scaling factor to use to go from logical to device
     *                    coordinates.
     */
    void appendWindowQuad(const WindowQuad &quad, qreal deviceScale);
    /**
     * Append a sub-quad of a WindowQuad as two triangles.
     *
     * This will append the sub-quad specified by `intersection` as two
     * triangles. The quad is expected to be in logical coordinates, while the
     * intersection is expected to be in device coordinates. The texture
     * coordinates of the resulting vertices are based upon those of the quad,
     * using bilinear interpolation for interpolating how much of the original
     * texture coordinates to use.
     *
     * @param quad The WindowQuad instance to use a sub-quad of.
     * @param subquad The sub-quad to append.
     * @param deviceScale The scaling factor used to convert from logical to
     *                    device coordinates.
     */
    void appendSubQuad(const WindowQuad &quad, const QRectF &subquad, qreal deviceScale);
    /**
     * Modify this geometry's texture coordinates based on a matrix.
     *
     * This is primarily intended to convert from non-normalised to normalised
     * texture coordinates.
     *
     * @param textureMatrix The texture matrix to use for modifying the
     *                      texture coordinates. Note that only the 2D scale and
     *                      translation are used.
     */
    void postProcessTextureCoordinates(const QMatrix4x4 &textureMatrix);

private:
    VertexSnappingMode m_vertexSnappingMode = VertexSnappingMode::Round;
};

/**
 * Pointer to the global EffectsHandler object.
 */
extern KWIN_EXPORT EffectsHandler *effects;

/***************************************************************
 WindowVertex
***************************************************************/

inline WindowVertex::WindowVertex()
    : px(0)
    , py(0)
    , tx(0)
    , ty(0)
{
}

inline WindowVertex::WindowVertex(double _x, double _y, double _tx, double _ty)
    : px(_x)
    , py(_y)
    , tx(_tx)
    , ty(_ty)
{
}

inline WindowVertex::WindowVertex(const QPointF &position, const QPointF &texturePosition)
    : px(position.x())
    , py(position.y())
    , tx(texturePosition.x())
    , ty(texturePosition.y())
{
}

inline void WindowVertex::move(double x, double y)
{
    px = x;
    py = y;
}

inline void WindowVertex::setX(double x)
{
    px = x;
}

inline void WindowVertex::setY(double y)
{
    py = y;
}

/***************************************************************
 WindowQuad
***************************************************************/

inline WindowQuad::WindowQuad()
{
}

inline WindowVertex &WindowQuad::operator[](int index)
{
    Q_ASSERT(index >= 0 && index < 4);
    return verts[index];
}

inline const WindowVertex &WindowQuad::operator[](int index) const
{
    Q_ASSERT(index >= 0 && index < 4);
    return verts[index];
}

inline double WindowQuad::left() const
{
    return std::min(verts[0].px, std::min(verts[1].px, std::min(verts[2].px, verts[3].px)));
}

inline double WindowQuad::right() const
{
    return std::max(verts[0].px, std::max(verts[1].px, std::max(verts[2].px, verts[3].px)));
}

inline double WindowQuad::top() const
{
    return std::min(verts[0].py, std::min(verts[1].py, std::min(verts[2].py, verts[3].py)));
}

inline double WindowQuad::bottom() const
{
    return std::max(verts[0].py, std::max(verts[1].py, std::max(verts[2].py, verts[3].py)));
}

inline QRectF WindowQuad::bounds() const
{
    return QRectF(QPointF(left(), top()), QPointF(right(), bottom()));
}

/***************************************************************
 EffectWindow
***************************************************************/

inline void EffectWindow::addRepaint(int x, int y, int w, int h)
{
    addRepaint(QRect(x, y, w, h));
}

inline void EffectWindow::addLayerRepaint(int x, int y, int w, int h)
{
    addLayerRepaint(QRect(x, y, w, h));
}

} // namespace
Q_DECLARE_METATYPE(KWin::EffectWindow *)
Q_DECLARE_METATYPE(KWin::EffectWindowList)

/** @} */
