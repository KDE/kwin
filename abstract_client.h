/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_ABSTRACT_CLIENT_H
#define KWIN_ABSTRACT_CLIENT_H

#include "toplevel.h"
#include "options.h"
#include "rules.h"
#include "cursor.h"

#include <memory>

#include <QElapsedTimer>
#include <QPointer>

namespace KWaylandServer
{
class PlasmaWindowInterface;
}

namespace KDecoration2
{
class Decoration;
}

namespace KWin
{
class Group;

namespace TabBox
{
class TabBoxClientImpl;
}

namespace Decoration
{
class DecoratedClientImpl;
class DecorationPalette;
}

class KWIN_EXPORT AbstractClient : public Toplevel
{
    Q_OBJECT

    /**
     * Whether this Client is fullScreen. A Client might either be fullScreen due to the _NET_WM property
     * or through a legacy support hack. The fullScreen state can only be changed if the Client does not
     * use the legacy hack. To be sure whether the state changed, connect to the notify signal.
     */
    Q_PROPERTY(bool fullScreen READ isFullScreen WRITE setFullScreen NOTIFY fullScreenChanged)

    /**
     * Whether the Client can be set to fullScreen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool fullScreenable READ isFullScreenable)

    /**
     * Whether this Client is active or not. Use Workspace::activateClient() to activate a Client.
     * @see Workspace::activateClient
     */
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)

    /**
     * The desktop this Client is on. If the Client is on all desktops the property has value -1.
     * This is a legacy property, use x11DesktopIds instead
     */
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)

    /**
     * Whether the Client is on all desktops. That is desktop is -1.
     */
    Q_PROPERTY(bool onAllDesktops READ isOnAllDesktops WRITE setOnAllDesktops NOTIFY desktopChanged)

    /**
     * The x11 ids for all desktops this client is in. On X11 this list will always have a length of 1
     */
    Q_PROPERTY(QVector<uint> x11DesktopIds READ x11DesktopIds NOTIFY x11DesktopIdsChanged)

    /**
     * Indicates that the window should not be included on a taskbar.
     */
    Q_PROPERTY(bool skipTaskbar READ skipTaskbar WRITE setSkipTaskbar NOTIFY skipTaskbarChanged)

    /**
     * Indicates that the window should not be included on a Pager.
     */
    Q_PROPERTY(bool skipPager READ skipPager WRITE setSkipPager NOTIFY skipPagerChanged)

    /**
     * Whether the Client should be excluded from window switching effects.
     */
    Q_PROPERTY(bool skipSwitcher READ skipSwitcher WRITE setSkipSwitcher NOTIFY skipSwitcherChanged)

    /**
     * Whether the window can be closed by the user. The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(bool closeable READ isCloseable)

    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)

    /**
     * Whether the Client is set to be kept above other windows.
     */
    Q_PROPERTY(bool keepAbove READ keepAbove WRITE setKeepAbove NOTIFY keepAboveChanged)

    /**
     * Whether the Client is set to be kept below other windows.
     */
    Q_PROPERTY(bool keepBelow READ keepBelow WRITE setKeepBelow NOTIFY keepBelowChanged)

    /**
     * Whether the Client can be shaded. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool shadeable READ isShadeable)

    /**
     * Whether the Client is shaded.
     */
    Q_PROPERTY(bool shade READ isShade WRITE setShade NOTIFY shadeChanged)

    /**
     * Whether the Client can be minimized. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool minimizable READ isMinimizable)

    /**
     * Whether the Client is minimized.
     */
    Q_PROPERTY(bool minimized READ isMinimized WRITE setMinimized NOTIFY minimizedChanged)

    /**
     * The optional geometry representing the minimized Client in e.g a taskbar.
     * See _NET_WM_ICON_GEOMETRY at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(QRect iconGeometry READ iconGeometry)

    /**
     * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
     * i.e. window types that usually don't have a window frame and the user does not use window
     * management (moving, raising,...) on them.
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(bool specialWindow READ isSpecialWindow)

    /**
     * Whether window state _NET_WM_STATE_DEMANDS_ATTENTION is set. This state indicates that some
     * action in or with the window happened. For example, it may be set by the Window Manager if
     * the window requested activation but the Window Manager refused it, or the application may set
     * it if it finished some work. This state may be set by both the Client and the Window Manager.
     * It should be unset by the Window Manager when it decides the window got the required attention
     * (usually, that it got activated).
     */
    Q_PROPERTY(bool demandsAttention READ isDemandingAttention WRITE demandAttention NOTIFY demandsAttentionChanged)

    /**
     * The Caption of the Client. Read from WM_NAME property together with a suffix for hostname and shortcut.
     * To read only the caption as provided by WM_NAME, use the getter with an additional @c false value.
     */
    Q_PROPERTY(QString caption READ caption NOTIFY captionChanged)

    /**
     * Minimum size as specified in WM_NORMAL_HINTS
     */
    Q_PROPERTY(QSize minSize READ minSize)

    /**
     * Maximum size as specified in WM_NORMAL_HINTS
     */
    Q_PROPERTY(QSize maxSize READ maxSize)

    /**
     * Whether the Client can accept keyboard focus.
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(bool wantsInput READ wantsInput)

    /**
     * Whether the Client is a transient Window to another Window.
     * @see transientFor
     */
    Q_PROPERTY(bool transient READ isTransient NOTIFY transientChanged)

    /**
     * The Client to which this Client is a transient if any.
     */
    Q_PROPERTY(KWin::AbstractClient *transientFor READ transientFor NOTIFY transientChanged)

    /**
     * Whether the Client represents a modal window.
     */
    Q_PROPERTY(bool modal READ isModal NOTIFY modalChanged)

    /**
     * The geometry of this Client. Be aware that depending on resize mode the frameGeometryChanged
     * signal might be emitted at each resize step or only at the end of the resize operation.
     *
     * @deprecated Use frameGeometry
     */
    Q_PROPERTY(QRect geometry READ frameGeometry WRITE setFrameGeometry)

    /**
     * The geometry of this Client. Be aware that depending on resize mode the frameGeometryChanged
     * signal might be emitted at each resize step or only at the end of the resize operation.
     */
    Q_PROPERTY(QRect frameGeometry READ frameGeometry WRITE setFrameGeometry)

    /**
     * Whether the Client is currently being moved by the user.
     * Notify signal is emitted when the Client starts or ends move/resize mode.
     */
    Q_PROPERTY(bool move READ isMove NOTIFY moveResizedChanged)

    /**
     * Whether the Client is currently being resized by the user.
     * Notify signal is emitted when the Client starts or ends move/resize mode.
     */
    Q_PROPERTY(bool resize READ isResize NOTIFY moveResizedChanged)

    /**
     * Whether the decoration is currently using an alpha channel.
     */
    Q_PROPERTY(bool decorationHasAlpha READ decorationHasAlpha)

    /**
     * Whether the window has a decoration or not.
     * This property is not allowed to be set by applications themselves.
     * The decision whether a window has a border or not belongs to the window manager.
     * If this property gets abused by application developers, it will be removed again.
     */
    Q_PROPERTY(bool noBorder READ noBorder WRITE setNoBorder)

    /**
     * Whether the Client provides context help. Mostly needed by decorations to decide whether to
     * show the help button or not.
     */
    Q_PROPERTY(bool providesContextHelp READ providesContextHelp CONSTANT)

    /**
     * Whether the Client can be maximized both horizontally and vertically.
     * The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool maximizable READ isMaximizable)

    /**
     * Whether the Client is moveable. Even if it is not moveable, it might be possible to move
     * it to another screen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     * @see moveableAcrossScreens
     */
    Q_PROPERTY(bool moveable READ isMovable)

    /**
     * Whether the Client can be moved to another screen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     * @see moveable
     */
    Q_PROPERTY(bool moveableAcrossScreens READ isMovableAcrossScreens)

    /**
     * Whether the Client can be resized. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool resizeable READ isResizable)

    /**
     * The desktop file name of the application this AbstractClient belongs to.
     *
     * This is either the base name without full path and without file extension of the
     * desktop file for the window's application (e.g. "org.kde.foo").
     *
     * The application's desktop file name can also be the full path to the desktop file
     * (e.g. "/opt/kde/share/org.kde.foo.desktop") in case it's not in a standard location.
     */
    Q_PROPERTY(QByteArray desktopFileName READ desktopFileName NOTIFY desktopFileNameChanged)

    /**
     * Whether an application menu is available for this Client
     */
    Q_PROPERTY(bool hasApplicationMenu READ hasApplicationMenu NOTIFY hasApplicationMenuChanged)

    /**
     * Whether the application menu for this Client is currently opened
     */
    Q_PROPERTY(bool applicationMenuActive READ applicationMenuActive NOTIFY applicationMenuActiveChanged)

    /**
     * Whether this client is unresponsive.
     *
     * When an application failed to react on a ping request in time, it is
     * considered unresponsive. This usually indicates that the application froze or crashed.
     */
    Q_PROPERTY(bool unresponsive READ unresponsive NOTIFY unresponsiveChanged)

    /**
     * The color scheme set on this client
     * Absolute file path, or name of palette in the user's config directory following KColorSchemes format.
     * An empty string indicates the default palette from kdeglobals is used.
     * @note this indicates the colour scheme requested, which might differ from the theme applied if the colorScheme cannot be found
     */
    Q_PROPERTY(QString colorScheme READ colorScheme NOTIFY colorSchemeChanged)

public:
    ~AbstractClient() override;

    QWeakPointer<TabBox::TabBoxClientImpl> tabBoxClient() const {
        return m_tabBoxClient.toWeakRef();
    }
    bool isFirstInTabBox() const {
        return m_firstInTabBox;
    }
    bool skipSwitcher() const {
        return m_skipSwitcher;
    }
    void setSkipSwitcher(bool set);

    bool skipTaskbar() const {
        return m_skipTaskbar;
    }
    void setSkipTaskbar(bool set);
    void setOriginalSkipTaskbar(bool set);
    bool originalSkipTaskbar() const {
        return m_originalSkipTaskbar;
    }

    bool skipPager() const {
        return m_skipPager;
    }
    void setSkipPager(bool set);

    const QIcon &icon() const {
        return m_icon;
    }

    bool isZombie() const;
    bool isActive() const {
        return m_active;
    }
    /**
     * Sets the client's active state to \a act.
     *
     * This function does only change the visual appearance of the client,
     * it does not change the focus setting. Use
     * Workspace::activateClient() or Workspace::requestFocus() instead.
     *
     * If a client receives or looses the focus, it calls setActive() on
     * its own.
     */
    void setActive(bool);

    bool keepAbove() const {
        return m_keepAbove;
    }
    void setKeepAbove(bool);
    bool keepBelow() const {
        return m_keepBelow;
    }
    void setKeepBelow(bool);

    void demandAttention(bool set = true);
    bool isDemandingAttention() const {
        return m_demandsAttention;
    }

    void cancelAutoRaise();

    bool wantsTabFocus() const;

    QMargins frameMargins() const override;
    QPoint clientPos() const override {
        return QPoint(borderLeft(), borderTop());
    }

    virtual void updateMouseGrab();
    /**
     * @returns The caption consisting of captionNormal and captionSuffix
     * @see captionNormal
     * @see captionSuffix
     */
    QString caption() const;
    /**
     * @returns The caption as set by the AbstractClient without any suffix.
     * @see caption
     * @see captionSuffix
     */
    virtual QString captionNormal() const = 0;
    /**
     * @returns The suffix added to the caption (e.g. shortcut, machine name, etc.)
     * @see caption
     * @see captionNormal
     */
    virtual QString captionSuffix() const = 0;
    virtual bool isPlaceable() const;
    virtual bool isCloseable() const = 0;
    // TODO: remove boolean trap
    virtual bool isShown(bool shaded_is_shown) const = 0;
    virtual bool isHiddenInternal() const = 0;
    // TODO: remove boolean trap
    virtual void hideClient(bool hide) = 0;
    virtual bool isFullScreenable() const;
    virtual bool isFullScreen() const;
    // TODO: remove boolean trap
    virtual AbstractClient *findModal(bool allow_itself = false) = 0;
    virtual bool isTransient() const;
    /**
     * @returns Whether there is a hint available to place the AbstractClient on it's parent, default @c false.
     * @see transientPlacementHint
     */
    virtual bool hasTransientPlacementHint() const;
    /**
     * Only valid id hasTransientPlacementHint is true
     * @returns The position the transient wishes to position itself
     */
    virtual QRect transientPlacement(const QRect &bounds) const;
    const AbstractClient* transientFor() const;
    AbstractClient* transientFor();
    /**
     * @returns @c true if c is the transient_for window for this client,
     *  or recursively the transient_for window
     * @todo: remove boolean trap
     */
    virtual bool hasTransient(const AbstractClient* c, bool indirect) const;
    const QList<AbstractClient*>& transients() const; // Is not indirect
    virtual void addTransient(AbstractClient *client);
    virtual void removeTransient(AbstractClient* cl);
    virtual QList<AbstractClient*> mainClients() const; // Call once before loop , is not indirect
    QList<AbstractClient*> allMainClients() const; // Call once before loop , is indirect
    /**
     * Returns true for "special" windows and false for windows which are "normal"
     * (normal=window which has a border, can be moved by the user, can be closed, etc.)
     * true for Desktop, Dock, Splash, Override and TopMenu (and Toolbar??? - for now)
     * false for Normal, Dialog, Utility and Menu (and Toolbar??? - not yet) TODO
     */
    bool isSpecialWindow() const;
    void sendToScreen(int screen);
    const QKeySequence &shortcut() const {
        return _shortcut;
    }
    void setShortcut(const QString &cut);
    bool performMouseCommand(Options::MouseCommand, const QPoint &globalPos);
    void setOnAllDesktops(bool set);
    void setDesktop(int);
    void enterDesktop(VirtualDesktop *desktop);
    void leaveDesktop(VirtualDesktop *desktop);

    /**
     * Set the window as being on the attached list of desktops
     * On X11 it will be set to the last entry
     */
    void setDesktops(QVector<VirtualDesktop *> desktops);

    int desktop() const override {
        return m_desktops.isEmpty() ? (int)NET::OnAllDesktops : m_desktops.last()->x11DesktopNumber();
    }
    QVector<VirtualDesktop *> desktops() const override {
        return m_desktops;
    }
    QVector<uint> x11DesktopIds() const;

    void setMinimized(bool set);
    /**
     * Minimizes this client plus its transients
     */
    void minimize(bool avoid_animation = false);
    void unminimize(bool avoid_animation = false);
    bool isMinimized() const {
        return m_minimized;
    }
    virtual void setFullScreen(bool set, bool user = true);

    virtual void setClientShown(bool shown);

    QRect geometryRestore() const;
    virtual MaximizeMode maximizeMode() const;
    virtual MaximizeMode requestedMaximizeMode() const;
    void maximize(MaximizeMode);
    /**
     * Sets the maximization according to @p vertically and @p horizontally.
     */
    Q_INVOKABLE void setMaximize(bool vertically, bool horizontally);
    virtual bool noBorder() const;
    virtual void setNoBorder(bool set);
    virtual void blockActivityUpdates(bool b = true) = 0;
    QPalette palette() const;
    const Decoration::DecorationPalette *decorationPalette() const;
    /**
     * Returns whether the window is resizable or has a fixed size.
     */
    virtual bool isResizable() const = 0;
    /**
     * Returns whether the window is moveable or has a fixed position.
     */
    virtual bool isMovable() const = 0;
    /**
     * Returns whether the window can be moved to another screen.
     */
    virtual bool isMovableAcrossScreens() const = 0;
    /**
     * @c true only for @c ShadeNormal
     */
    bool isShade() const {
        return shadeMode() == ShadeNormal;
    }
    ShadeMode shadeMode() const; // Prefer isShade()
    void setShade(bool set);
    void setShade(ShadeMode mode);
    void toggleShade();
    void cancelShadeHoverTimer();
    /**
     * Whether the Client can be shaded. Default implementation returns @c false.
     */
    virtual bool isShadeable() const;
    virtual bool isMaximizable() const;
    virtual bool isMinimizable() const;
    virtual QRect iconGeometry() const;
    virtual bool userCanSetFullScreen() const;
    virtual bool userCanSetNoBorder() const;
    virtual void checkNoBorder();
    virtual void setOnActivities(QStringList newActivitiesList);
    virtual void setOnAllActivities(bool set) = 0;
    const WindowRules* rules() const {
        return &m_rules;
    }
    void removeRule(Rules* r);
    void setupWindowRules(bool ignore_temporary);
    void evaluateWindowRules();
    virtual void applyWindowRules();
    virtual bool takeFocus() = 0;
    virtual bool wantsInput() const = 0;
    /**
     * Whether a dock window wants input.
     *
     * By default KWin doesn't pass focus to a dock window unless a force activate
     * request is provided.
     *
     * This method allows to have dock windows take focus also through flags set on
     * the window.
     *
     * The default implementation returns @c false.
     */
    virtual bool dockWantsInput() const;
    void checkWorkspacePosition(QRect oldGeometry = QRect(), int oldDesktop = -2,  QRect oldClientGeometry = QRect());
    virtual xcb_timestamp_t userTime() const;
    virtual void updateWindowRules(Rules::Types selection);

    void growHorizontal();
    void shrinkHorizontal();
    void growVertical();
    void shrinkVertical();
    void updateMoveResize(const QPointF &currentGlobalCursor);
    /**
     * Ends move resize when all pointer buttons are up again.
     */
    void endMoveResize();
    void keyPressEvent(uint key_code);

    void enterEvent(const QPoint &globalPos);
    void leaveEvent();

    /**
     * These values represent positions inside an area
     */
    enum Position {
        // without prefix, they'd conflict with Qt::TopLeftCorner etc. :(
        PositionCenter         = 0x00,
        PositionLeft           = 0x01,
        PositionRight          = 0x02,
        PositionTop            = 0x04,
        PositionBottom         = 0x08,
        PositionTopLeft        = PositionLeft | PositionTop,
        PositionTopRight       = PositionRight | PositionTop,
        PositionBottomLeft     = PositionLeft | PositionBottom,
        PositionBottomRight    = PositionRight | PositionBottom
    };
    Position titlebarPosition() const;
    bool titlebarPositionUnderMouse() const;

    // a helper for the workspace window packing. tests for screen validity and updates since in maximization case as with normal moving
    void packTo(int left, int top);

    /**
     * Sets the quick tile mode ("snap") of this window.
     * This will also handle preserving and restoring of window geometry as necessary.
     * @param mode The tile mode (left/right) to give this window.
     * @param keyboard Defines whether to take keyboard cursor into account.
     */
    void setQuickTileMode(QuickTileMode mode, bool keyboard = false);
    QuickTileMode quickTileMode() const {
        return QuickTileMode(m_quickTileMode);
    }
    Layer layer() const override;
    void updateLayer();

    void placeIn(const QRect &area);

    enum ForceGeometry_t { NormalGeometrySet, ForceGeometrySet };
    virtual void move(int x, int y, ForceGeometry_t force = NormalGeometrySet);
    void move(const QPoint &p, ForceGeometry_t force = NormalGeometrySet);
    virtual void resizeWithChecks(const QSize& s, ForceGeometry_t force = NormalGeometrySet) = 0;
    void keepInArea(QRect area, bool partial = false);
    virtual QSize minSize() const;
    virtual QSize maxSize() const;
    virtual void setFrameGeometry(const QRect &rect, ForceGeometry_t force = NormalGeometrySet) = 0;

    /**
     * How to resize the window in order to obey constraints (mainly aspect ratios).
     */
    enum SizeMode {
        SizeModeAny,
        SizeModeFixedW, ///< Try not to affect width
        SizeModeFixedH, ///< Try not to affect height
        SizeModeMax ///< Try not to make it larger in either direction
    };

    virtual QSize constrainClientSize(const QSize &size, SizeMode mode = SizeModeAny) const;
    QSize constrainFrameSize(const QSize &size, SizeMode mode = SizeModeAny) const;
    QSize adjustedSize() const;

    /**
     * Calculates the matching client position for the given frame position @p point.
     */
    virtual QPoint framePosToClientPos(const QPoint &point) const;
    /**
     * Calculates the matching frame position for the given client position @p point.
     */
    virtual QPoint clientPosToFramePos(const QPoint &point) const;
    /**
     * Calculates the matching client size for the given frame size @p size.
     *
     * Notice that size constraints won't be applied.
     *
     * Default implementation returns the frame size with frame margins being excluded.
     */
    virtual QSize frameSizeToClientSize(const QSize &size) const;
    /**
     * Calculates the matching frame size for the given client size @p size.
     *
     * Notice that size constraints won't be applied.
     *
     * Default implementation returns the client size with frame margins being included.
     */
    virtual QSize clientSizeToFrameSize(const QSize &size) const;
    /**
     * Calculates the matching client rect for the given frame rect @p rect.
     *
     * Notice that size constraints won't be applied.
     */
    QRect frameRectToClientRect(const QRect &rect) const;
    /**
     * Calculates the matching frame rect for the given client rect @p rect.
     *
     * Notice that size constraints won't be applied.
     */
    QRect clientRectToFrameRect(const QRect &rect) const;

    /**
     * Returns @c true if the Client is being interactively moved; otherwise @c false.
     */
    bool isMove() const {
        return isMoveResize() && moveResizePointerMode() == PositionCenter;
    }
    /**
     * Returns @c true if the Client is being interactively resized; otherwise @c false.
     */
    bool isResize() const {
        return isMoveResize() && moveResizePointerMode() != PositionCenter;
    }
    /**
     * Cursor shape for move/resize mode.
     */
    CursorShape cursor() const {
        return m_moveResize.cursor;
    }

    virtual StrutRect strutRect(StrutArea area) const;
    StrutRects strutRects() const;
    virtual bool hasStrut() const;

    void setModal(bool modal);
    bool isModal() const;

    /**
     * Determines the mouse command for the given @p button in the current state.
     *
     * The @p handled argument specifies whether the button was handled or not.
     * This value should be used to determine whether the mouse button should be
     * passed to the AbstractClient or being filtered out.
     */
    Options::MouseCommand getMouseCommand(Qt::MouseButton button, bool *handled) const;
    Options::MouseCommand getWheelCommand(Qt::Orientation orientation, bool *handled) const;

    // decoration related
    KDecoration2::Decoration *decoration() {
        return m_decoration.decoration;
    }
    const KDecoration2::Decoration *decoration() const {
        return m_decoration.decoration;
    }
    bool isDecorated() const {
        return m_decoration.decoration != nullptr;
    }
    QPointer<Decoration::DecoratedClientImpl> decoratedClient() const;
    void setDecoratedClient(QPointer<Decoration::DecoratedClientImpl> client);
    bool decorationHasAlpha() const;
    void triggerDecorationRepaint();
    virtual void layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const;
    void processDecorationMove(const QPoint &localPos, const QPoint &globalPos);
    bool processDecorationButtonPress(QMouseEvent *event, bool ignoreMenu = false);
    void processDecorationButtonRelease(QMouseEvent *event);

    /**
     * TODO: fix boolean traps
     */
    virtual void updateDecoration(bool check_workspace_pos, bool force = false);

    /**
     * Returns whether the window provides context help or not. If it does,
     * you should show a help menu item or a help button like '?' and call
     * contextHelp() if this is invoked.
     *
     * Default implementation returns @c false.
     * @see showContextHelp;
     */
    virtual bool providesContextHelp() const;

    /**
     * Invokes context help on the window. Only works if the window
     * actually provides context help.
     *
     * Default implementation does nothing.
     *
     * @see providesContextHelp()
     */
    virtual void showContextHelp();

    QRect inputGeometry() const override;

    /**
     * @returns the geometry of the virtual keyboard
     * This geometry is in global coordinates
     */
    QRect virtualKeyboardGeometry() const;

    /**
     * Sets the geometry of the virtual keyboard, The window may resize itself in order to make space for the keybaord
     * This geometry is in global coordinates
     */
    virtual void setVirtualKeyboardGeometry(const QRect &geo);

    /**
     * Restores the AbstractClient after it had been hidden due to show on screen edge functionality.
     * The AbstractClient also gets raised (e.g. Panel mode windows can cover) and the AbstractClient
     * gets informed in a window specific way that it is shown and raised again.
     */
    virtual void showOnScreenEdge();

    QByteArray desktopFileName() const {
        return m_desktopFileName;
    }

    /**
     * Tries to terminate the process of this AbstractClient.
     *
     * Implementing subclasses can perform a windowing system solution for terminating.
     */
    virtual void killWindow() = 0;
    virtual void destroyClient() = 0;

    enum class SameApplicationCheck {
        RelaxedForActive = 1 << 0,
        AllowCrossProcesses = 1 << 1
    };
    Q_DECLARE_FLAGS(SameApplicationChecks, SameApplicationCheck)
    static bool belongToSameApplication(const AbstractClient* c1, const AbstractClient* c2, SameApplicationChecks checks = SameApplicationChecks());

    bool hasApplicationMenu() const;
    bool applicationMenuActive() const {
        return m_applicationMenuActive;
    }
    void setApplicationMenuActive(bool applicationMenuActive);

    QString applicationMenuServiceName() const {
        return m_applicationMenuServiceName;
    }
    QString applicationMenuObjectPath() const {
        return m_applicationMenuObjectPath;
    }

    virtual QString preferredColorScheme() const;
    QString colorScheme() const;
    void setColorScheme(const QString &colorScheme);

    /**
     * Request showing the application menu bar
     * @param actionId The DBus menu ID of the action that should be highlighted, 0 for the root menu
     */
    void showApplicationMenu(int actionId);

    bool unresponsive() const;

    virtual bool isInitialPositionSet() const {
        return false;
    }

    /**
     * Default implementation returns @c null.
     * Mostly intended for X11 clients, from EWMH:
     * @verbatim
     * If the WM_TRANSIENT_FOR property is set to None or Root window, the window should be
     * treated as a transient for all other windows in the same group. It has been noted that this
     * is a slight ICCCM violation, but as this behavior is pretty standard for many toolkits and
     * window managers, and is extremely unlikely to break anything, it seems reasonable to document
     * it as standard.
     * @endverbatim
     */
    virtual bool groupTransient() const;
    /**
     * Default implementation returns @c null.
     *
     * Mostly for X11 clients, holds the client group
     */
    virtual const Group *group() const;
    /**
     * Default implementation returns @c null.
     *
     * Mostly for X11 clients, holds the client group
     */
    virtual Group *group();

    /**
     * Returns whether this is an internal client.
     *
     * Internal clients are created by KWin and used for special purpose windows,
     * like the task switcher, etc.
     *
     * Default implementation returns @c false.
     */
    virtual bool isInternal() const;

    /**
     * Returns whether window rules can be applied to this client.
     *
     * Default implementation returns @c false.
     */
    virtual bool supportsWindowRules() const;

    /**
     * Return window management interface
     */
    KWaylandServer::PlasmaWindowInterface *windowManagementInterface() const {
        return m_windowManagementInterface;
    }

public Q_SLOTS:
    virtual void closeWindow() = 0;

Q_SIGNALS:
    void fullScreenChanged();
    void skipTaskbarChanged();
    void skipPagerChanged();
    void skipSwitcherChanged();
    void iconChanged();
    void activeChanged();
    void keepAboveChanged(bool);
    void keepBelowChanged(bool);
    /**
     * Emitted whenever the demands attention state changes.
     */
    void demandsAttentionChanged();
    void desktopPresenceChanged(KWin::AbstractClient*, int); // to be forwarded by Workspace
    void desktopChanged();
    void x11DesktopIdsChanged();
    void shadeChanged();
    void minimizedChanged();
    void clientMinimized(KWin::AbstractClient* client, bool animate);
    void clientUnminimized(KWin::AbstractClient* client, bool animate);
    void paletteChanged(const QPalette &p);
    void colorSchemeChanged();
    void captionChanged();
    void clientMaximizedStateChanged(KWin::AbstractClient*, MaximizeMode);
    void clientMaximizedStateChanged(KWin::AbstractClient* c, bool h, bool v);
    void transientChanged();
    void modalChanged();
    void quickTileModeChanged();
    void moveResizedChanged();
    void moveResizeCursorChanged(CursorShape);
    void clientStartUserMovedResized(KWin::AbstractClient*);
    void clientStepUserMovedResized(KWin::AbstractClient *, const QRect&);
    void clientFinishUserMovedResized(KWin::AbstractClient*);
    void closeableChanged(bool);
    void minimizeableChanged(bool);
    void shadeableChanged(bool);
    void maximizeableChanged(bool);
    void desktopFileNameChanged();
    void applicationMenuChanged();
    void hasApplicationMenuChanged(bool);
    void applicationMenuActiveChanged(bool);
    void unresponsiveChanged(bool);

protected:
    AbstractClient();
    void setFirstInTabBox(bool enable) {
        m_firstInTabBox = enable;
    }
    void setIcon(const QIcon &icon);
    void startAutoRaise();
    void autoRaise();
    bool isMostRecentlyRaised() const;
    void markAsZombie();
    /**
     * Whether the window accepts focus.
     * The difference to wantsInput is that the implementation should not check rules and return
     * what the window effectively supports.
     */
    virtual bool acceptsFocus() const = 0;
    /**
     * Called from setActive once the active value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetActive();
    /**
     * Called from setKeepAbove once the keepBelow value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetKeepAbove();
    /**
     * Called from setKeepBelow once the keepBelow value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetKeepBelow();
    /**
     * Called from setShade() once the shadeMode value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetShade(ShadeMode previousShadeMode);
    /**
     * Called from setDeskop once the desktop value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetDesktop();
    /**
     * Called from @ref minimize and @ref unminimize once the minimized value got updated, but before the
     * changed signal is emitted.
     *
     * Default implementation does nothig.
     */
    virtual void doMinimize();
    virtual bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const = 0;

    virtual void doSetSkipTaskbar();
    virtual void doSetSkipPager();
    virtual void doSetSkipSwitcher();
    virtual void doSetDemandsAttention();
    virtual void doSetQuickTileMode();

    void setupWindowManagementInterface();
    void destroyWindowManagementInterface();
    void updateColorScheme();
    void setTransientFor(AbstractClient *transientFor);
    /**
     * Just removes the @p cl from the transients without any further checks.
     */
    void removeTransientFromList(AbstractClient* cl);

    virtual Layer belongsToLayer() const;
    virtual bool belongsToDesktop() const;
    void invalidateLayer();
    bool isActiveFullScreen() const;
    virtual Layer layerForDock() const;

    // electric border / quick tiling
    void setElectricBorderMode(QuickTileMode mode);
    QuickTileMode electricBorderMode() const {
        return m_electricMode;
    }
    void setElectricBorderMaximizing(bool maximizing);
    bool isElectricBorderMaximizing() const {
        return m_electricMaximizing;
    }
    QRect electricBorderMaximizeGeometry(QPoint pos, int desktop);
    void updateQuickTileMode(QuickTileMode newMode) {
        m_quickTileMode = newMode;
    }

    // geometry handling
    void checkOffscreenPosition(QRect *geom, const QRect &screenArea);
    int borderLeft() const;
    int borderRight() const;
    int borderTop() const;
    int borderBottom() const;
    virtual void changeMaximize(bool horizontal, bool vertical, bool adjust);
    void setGeometryRestore(const QRect &rect);

    /**
     * Called from move after updating the geometry. Can be reimplemented to perform specific tasks.
     * The base implementation does nothing.
     */
    virtual void doMove(int x, int y);
    void blockGeometryUpdates(bool block);
    void blockGeometryUpdates();
    void unblockGeometryUpdates();
    bool areGeometryUpdatesBlocked() const;
    enum PendingGeometry_t {
        PendingGeometryNone,
        PendingGeometryNormal,
        PendingGeometryForced
    };
    PendingGeometry_t pendingGeometryUpdate() const;
    void setPendingGeometryUpdate(PendingGeometry_t update);
    QRect bufferGeometryBeforeUpdateBlocking() const;
    QRect frameGeometryBeforeUpdateBlocking() const;
    QRect clientGeometryBeforeUpdateBlocking() const;
    void updateGeometryBeforeUpdateBlocking();
    /**
     * Schedules a repaint for the visibleRect before and after a
     * geometry update. The current visibleRect is stored for the
     * next time this method is called as the before geometry.
     */
    void addRepaintDuringGeometryUpdates();

    /**
     * @returns whether the Client is currently in move resize mode
     */
    bool isMoveResize() const {
        return m_moveResize.enabled;
    }
    /**
     * Sets whether the Client is in move resize mode to @p enabled.
     */
    void setMoveResize(bool enabled) {
        m_moveResize.enabled = enabled;
    }
    /**
     * @returns whether the move resize mode is unrestricted.
     */
    bool isUnrestrictedMoveResize() const {
        return m_moveResize.unrestricted;
    }
    /**
     * Sets whether move resize mode is unrestricted to @p set.
     */
    void setUnrestrictedMoveResize(bool set) {
        m_moveResize.unrestricted = set;
    }
    QPoint moveOffset() const {
        return m_moveResize.offset;
    }
    void setMoveOffset(const QPoint &offset) {
        m_moveResize.offset = offset;
    }
    QPoint invertedMoveOffset() const {
        return m_moveResize.invertedOffset;
    }
    void setInvertedMoveOffset(const QPoint &offset) {
        m_moveResize.invertedOffset = offset;
    }
    QRect initialMoveResizeGeometry() const {
        return m_moveResize.initialGeometry;
    }
    /**
     * Sets the initial move resize geometry to the current geometry.
     */
    void updateInitialMoveResizeGeometry();
    QRect moveResizeGeometry() const {
        return m_moveResize.geometry;
    }
    void setMoveResizeGeometry(const QRect &geo) {
        m_moveResize.geometry = geo;
    }
    Position moveResizePointerMode() const {
        return m_moveResize.pointer;
    }
    void setMoveResizePointerMode(Position mode) {
        m_moveResize.pointer = mode;
    }
    bool isMoveResizePointerButtonDown() const {
        return m_moveResize.buttonDown;
    }
    void setMoveResizePointerButtonDown(bool down) {
        m_moveResize.buttonDown = down;
    }
    int moveResizeStartScreen() const {
        return m_moveResize.startScreen;
    }
    void checkUnrestrictedMoveResize();
    /**
     * Sets an appropriate cursor shape for the logical mouse position.
     */
    void updateCursor();
    void startDelayedMoveResize();
    void stopDelayedMoveResize();
    bool startMoveResize();
    /**
     * Called from startMoveResize.
     *
     * Implementing classes should return @c false if starting move resize should
     * get aborted. In that case startMoveResize will also return @c false.
     *
     * Base implementation returns @c true.
     */
    virtual bool doStartMoveResize();
    virtual void doFinishMoveResize();
    void finishMoveResize(bool cancel);
    /**
     * Leaves the move resize mode.
     *
     * Inheriting classes must invoke the base implementation which
     * ensures that the internal mode is properly ended.
     */
    virtual void leaveMoveResize();
    virtual void positionGeometryTip();
    void performMoveResize();
    /**
     * Called from performMoveResize() after actually performing the change of geometry.
     * Implementing subclasses can perform windowing system specific handling here.
     *
     * Default implementation does nothing.
     */
    virtual void doPerformMoveResize();
    /*
     * Checks if the mouse cursor is near the edge of the screen and if so
     * activates quick tiling or maximization
     */
    void checkQuickTilingMaximizationZones(int xroot, int yroot);
    /**
     * Whether a sync request is still pending.
     * Default implementation returns @c false.
     */
    virtual bool isWaitingForMoveResizeSync() const;
    /**
     * Called during handling a resize. Implementing subclasses can use this
     * method to perform windowing system specific syncing.
     *
     * Default implementation does nothing.
     */
    virtual void doResizeSync();
    void handleMoveResize(int x, int y, int x_root, int y_root);
    void handleMoveResize(const QPoint &local, const QPoint &global);
    void dontMoveResize();

    virtual QSize resizeIncrements() const;

    /**
     * Returns the position depending on the Decoration's section under mouse.
     * If no decoration it returns PositionCenter.
     */
    Position mousePosition() const;

    static bool haveResizeEffect() {
        return s_haveResizeEffect;
    }
    static void updateHaveResizeEffect();
    static void resetHaveResizeEffect() {
        s_haveResizeEffect = false;
    }

    void setDecoration(KDecoration2::Decoration *decoration) {
        m_decoration.decoration = decoration;
    }
    virtual void createDecoration(const QRect &oldGeometry);
    virtual void destroyDecoration();
    void startDecorationDoubleClickTimer();
    void invalidateDecorationDoubleClickTimer();

    void setDesktopFileName(QByteArray name);
    QString iconFromDesktopFile() const;

    void updateApplicationMenuServiceName(const QString &serviceName);
    void updateApplicationMenuObjectPath(const QString &objectPath);

    void setUnresponsive(bool unresponsive);

    virtual void setShortcutInternal();
    QString shortcutCaptionSuffix() const;
    virtual void updateCaption() = 0;

    /**
     * Looks for another AbstractClient with same captionNormal and captionSuffix.
     * If no such AbstractClient exists @c nullptr is returned.
     */
    AbstractClient *findClientWithSameCaption() const;

    void finishWindowRules();
    void discardTemporaryRules();

    bool tabTo(AbstractClient *other, bool behind, bool activate);

    void startShadeHoverTimer();
    void startShadeUnhoverTimer();

    // The geometry that the client should be restored when the virtual keyboard closes
    QRect keyboardGeometryRestore() const;
    void setKeyboardGeometryRestore(const QRect &geom);

    QRect m_virtualKeyboardGeometry;
private Q_SLOTS:
    void shadeHover();
    void shadeUnhover();

private:
    void handlePaletteChange();
    QSharedPointer<TabBox::TabBoxClientImpl> m_tabBoxClient;
    bool m_firstInTabBox = false;
    bool m_skipTaskbar = false;
    /**
     * Unaffected by KWin
     */
    bool m_originalSkipTaskbar = false;
    bool m_skipPager = false;
    bool m_skipSwitcher = false;
    QIcon m_icon;
    bool m_active = false;
    bool m_zombie = false;
    bool m_keepAbove = false;
    bool m_keepBelow = false;
    bool m_demandsAttention = false;
    bool m_minimized = false;
    QTimer *m_autoRaiseTimer = nullptr;
    QTimer *m_shadeHoverTimer = nullptr;
    ShadeMode m_shadeMode = ShadeNone;
    QVector <VirtualDesktop *> m_desktops;

    QString m_colorScheme;
    std::shared_ptr<Decoration::DecorationPalette> m_palette;
    static QHash<QString, std::weak_ptr<Decoration::DecorationPalette>> s_palettes;
    static std::shared_ptr<Decoration::DecorationPalette> s_defaultPalette;

    KWaylandServer::PlasmaWindowInterface *m_windowManagementInterface = nullptr;

    AbstractClient *m_transientFor = nullptr;
    QList<AbstractClient*> m_transients;
    bool m_modal = false;
    Layer m_layer = UnknownLayer;

    // electric border/quick tiling
    QuickTileMode m_electricMode = QuickTileFlag::None;
    bool m_electricMaximizing = false;
    // The quick tile mode of this window.
    int m_quickTileMode = int(QuickTileFlag::None);
    QTimer *m_electricMaximizingDelay = nullptr;

    // geometry
    int m_blockGeometryUpdates = 0; // > 0 = New geometry is remembered, but not actually set
    PendingGeometry_t m_pendingGeometryUpdate = PendingGeometryNone;
    friend class GeometryUpdatesBlocker;
    QRect m_visibleRectBeforeGeometryUpdate;
    QRect m_bufferGeometryBeforeUpdateBlocking;
    QRect m_frameGeometryBeforeUpdateBlocking;
    QRect m_clientGeometryBeforeUpdateBlocking;
    QRect m_keyboardGeometryRestore;
    QRect m_maximizeGeometryRestore;

    struct {
        bool enabled = false;
        bool unrestricted = false;
        QPoint offset;
        QPoint invertedOffset;
        QRect initialGeometry;
        QRect geometry;
        Position pointer = PositionCenter;
        bool buttonDown = false;
        CursorShape cursor = Qt::ArrowCursor;
        int startScreen = 0;
        QTimer *delayedTimer = nullptr;
    } m_moveResize;

    struct {
        KDecoration2::Decoration *decoration = nullptr;
        QPointer<Decoration::DecoratedClientImpl> client;
        QElapsedTimer doubleClickTimer;
    } m_decoration;
    QByteArray m_desktopFileName;

    bool m_applicationMenuActive = false;
    QString m_applicationMenuServiceName;
    QString m_applicationMenuObjectPath;

    bool m_unresponsive = false;

    QKeySequence _shortcut;

    WindowRules m_rules;

    static bool s_haveResizeEffect;
};

/**
 * Helper for AbstractClient::blockGeometryUpdates() being called in pairs (true/false)
 */
class GeometryUpdatesBlocker
{
public:
    explicit GeometryUpdatesBlocker(AbstractClient* c)
        : cl(c) {
        cl->blockGeometryUpdates(true);
    }
    ~GeometryUpdatesBlocker() {
        cl->blockGeometryUpdates(false);
    }

private:
    AbstractClient* cl;
};

inline void AbstractClient::move(const QPoint& p, ForceGeometry_t force)
{
    move(p.x(), p.y(), force);
}

inline const QList<AbstractClient*>& AbstractClient::transients() const
{
    return m_transients;
}

inline bool AbstractClient::areGeometryUpdatesBlocked() const
{
    return m_blockGeometryUpdates != 0;
}

inline void AbstractClient::blockGeometryUpdates()
{
    m_blockGeometryUpdates++;
}

inline void AbstractClient::unblockGeometryUpdates()
{
    m_blockGeometryUpdates--;
}

inline AbstractClient::PendingGeometry_t AbstractClient::pendingGeometryUpdate() const
{
    return m_pendingGeometryUpdate;
}

inline void AbstractClient::setPendingGeometryUpdate(PendingGeometry_t update)
{
    m_pendingGeometryUpdate = update;
}

}

Q_DECLARE_METATYPE(KWin::AbstractClient*)
Q_DECLARE_METATYPE(QList<KWin::AbstractClient*>)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::AbstractClient::SameApplicationChecks)

#endif
