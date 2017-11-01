/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_TOPLEVEL_H
#define KWIN_TOPLEVEL_H

// kwin
#include "input.h"
#include "utils.h"
#include "virtualdesktops.h"
#include "xcbutils.h"
// KDE
#include <NETWM>
// Qt
#include <QObject>
#include <QMatrix4x4>
// xcb
#include <xcb/damage.h>
#include <xcb/xfixes.h>
// XLib
#include <X11/Xlib.h>
#include <fixx11h.h>
// system
#include <assert.h>
// c++
#include <functional>

class QOpenGLFramebufferObject;

namespace KWayland
{
namespace Server
{
class SurfaceInterface;
}
}

namespace KWin
{

class ClientMachine;
class EffectWindowImpl;
class Shadow;

/**
 * Enum to describe the reason why a Toplevel has to be released.
 */
enum class ReleaseReason {
    Release, ///< Normal Release after e.g. an Unmap notify event (window still valid)
    Destroyed, ///< Release after an Destroy notify event (window no longer valid)
    KWinShutsDown ///< Release on KWin Shutdown (window still valid)
};

class KWIN_EXPORT Toplevel
    : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool alpha READ hasAlpha NOTIFY hasAlphaChanged)
    Q_PROPERTY(qulonglong frameId READ frameId)
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(QRect visibleRect READ visibleRect)
    Q_PROPERTY(int height READ height)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(QPoint pos READ pos)
    Q_PROPERTY(int screen READ screen NOTIFY screenChanged)
    Q_PROPERTY(QSize size READ size)
    Q_PROPERTY(int width READ width)
    Q_PROPERTY(qulonglong windowId READ windowId CONSTANT)
    Q_PROPERTY(int x READ x)
    Q_PROPERTY(int y READ y)
    Q_PROPERTY(int desktop READ desktop)
    /**
     * Whether the window is on all desktops. That is desktop is -1.
     **/
    Q_PROPERTY(bool onAllDesktops READ isOnAllDesktops)
    Q_PROPERTY(QRect rect READ rect)
    Q_PROPERTY(QPoint clientPos READ clientPos)
    Q_PROPERTY(QSize clientSize READ clientSize)
    Q_PROPERTY(QByteArray resourceName READ resourceName NOTIFY windowClassChanged)
    Q_PROPERTY(QByteArray resourceClass READ resourceClass NOTIFY windowClassChanged)
    Q_PROPERTY(QByteArray windowRole READ windowRole NOTIFY windowRoleChanged)
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
     * Returns whether the window is an On Screen Display.
     */
    Q_PROPERTY(bool onScreenDisplay READ isOnScreenDisplay)
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
    Q_PROPERTY(QStringList activities READ activities NOTIFY activitiesChanged)
    /**
     * Whether this Toplevel is managed by KWin (it has control over its placement and other
     * aspects, as opposed to override-redirect windows that are entirely handled by the application).
     **/
    Q_PROPERTY(bool managed READ isClient CONSTANT)
    /**
     * Whether this Toplevel represents an already deleted window and only kept for the compositor for animations.
     **/
    Q_PROPERTY(bool deleted READ isDeleted CONSTANT)
    /**
     * Whether the window has an own shape
     **/
    Q_PROPERTY(bool shaped READ shape NOTIFY shapedChanged)
    /**
     * Whether the window does not want to be animated on window close.
     * There are legit reasons for this like a screenshot application which does not want it's
     * window being captured.
     **/
    Q_PROPERTY(bool skipsCloseAnimation READ skipsCloseAnimation WRITE setSkipCloseAnimation NOTIFY skipCloseAnimationChanged)
    /**
     * The Id of the Wayland Surface associated with this Toplevel.
     * On X11 only setups the value is @c 0.
     **/
    Q_PROPERTY(quint32 surfaceId READ surfaceId NOTIFY surfaceIdChanged)

    /**
     * Interface to the Wayland Surface.
     * Relevant only in Wayland, in X11 it will be nullptr
     */
    Q_PROPERTY(KWayland::Server::SurfaceInterface *surface READ surface)

public:
    explicit Toplevel();
    virtual xcb_window_t frameId() const;
    xcb_window_t window() const;
    /**
     * @return a unique identifier for the Toplevel. On X11 same as @link {window}
     **/
    virtual quint32 windowId() const;
    QRect geometry() const;
    /**
     * The geometry of the Toplevel which accepts input events. This might be larger
     * than the actual geometry, e.g. to support resizing outside the window.
     *
     * Default implementation returns same as geometry.
     **/
    virtual QRect inputGeometry() const;
    QSize size() const;
    QPoint pos() const;
    QRect rect() const;
    int x() const;
    int y() const;
    int width() const;
    int height() const;
    bool isOnScreen(int screen) const;   // true if it's at least partially there
    bool isOnActiveScreen() const;
    int screen() const; // the screen where the center is
    /**
     * The scale of the screen this window is currently on
     * @Note: The buffer scale can be different.
     * @since 5.12
     */
    qreal screenScale() const; //
    virtual QPoint clientPos() const = 0; // inside of geometry()
    /**
     * Describes how the client's content maps to the window geometry including the frame.
     * The default implementation is a 1:1 mapping meaning the frame is part of the content.
     **/
    virtual QPoint clientContentPos() const;
    virtual QSize clientSize() const = 0;
    virtual QRect visibleRect() const; // the area the window occupies on the screen
    virtual QRect decorationRect() const; // rect including the decoration shadows
    virtual QRect transparentRect() const = 0;
    virtual bool isClient() const;
    virtual bool isDeleted() const;

    // prefer isXXX() instead
    // 0 for supported types means default for managed/unmanaged types
    virtual NET::WindowType windowType(bool direct = false, int supported_types = 0) const = 0;
    bool hasNETSupport() const;
    bool isDesktop() const;
    bool isDock() const;
    bool isToolbar() const;
    bool isMenu() const;
    bool isNormalWindow() const; // normal as in 'NET::Normal or NET::Unknown non-transient'
    bool isDialog() const;
    bool isSplash() const;
    bool isUtility() const;
    bool isDropdownMenu() const;
    bool isPopupMenu() const; // a context popup, not dropdown, not torn-off
    bool isTooltip() const;
    bool isNotification() const;
    bool isOnScreenDisplay() const;
    bool isComboBox() const;
    bool isDNDIcon() const;

    virtual bool isLockScreen() const;
    virtual bool isInputMethod() const;

    /**
     * Returns the virtual desktop within the workspace() the client window
     * is located in, 0 if it isn't located on any special desktop (not mapped yet),
     * or NET::OnAllDesktops. Do not use desktop() directly, use
     * isOnDesktop() instead.
     */
    virtual int desktop() const = 0;
    virtual QStringList activities() const = 0;
    bool isOnDesktop(int d) const;
    bool isOnActivity(const QString &activity) const;
    bool isOnCurrentDesktop() const;
    bool isOnCurrentActivity() const;
    bool isOnAllDesktops() const;
    bool isOnAllActivities() const;

    virtual QByteArray windowRole() const;
    QByteArray sessionId() const;
    QByteArray resourceName() const;
    QByteArray resourceClass() const;
    QByteArray wmCommand();
    QByteArray wmClientMachine(bool use_localhost) const;
    const ClientMachine *clientMachine() const;
    Window wmClientLeader() const;
    virtual pid_t pid() const;
    static bool resourceMatch(const Toplevel* c1, const Toplevel* c2);

    bool readyForPainting() const; // true if the window has been already painted its contents
    xcb_visualid_t visual() const;
    bool shape() const;
    QRegion inputShape() const;
    virtual void setOpacity(double opacity);
    virtual double opacity() const;
    int depth() const;
    bool hasAlpha() const;
    virtual bool setupCompositing();
    virtual void finishCompositing(ReleaseReason releaseReason = ReleaseReason::Release);
    Q_INVOKABLE void addRepaint(const QRect& r);
    Q_INVOKABLE void addRepaint(const QRegion& r);
    Q_INVOKABLE void addRepaint(int x, int y, int w, int h);
    Q_INVOKABLE void addLayerRepaint(const QRect& r);
    Q_INVOKABLE void addLayerRepaint(const QRegion& r);
    Q_INVOKABLE void addLayerRepaint(int x, int y, int w, int h);
    Q_INVOKABLE virtual void addRepaintFull();
    // these call workspace->addRepaint(), but first transform the damage if needed
    void addWorkspaceRepaint(const QRect& r);
    void addWorkspaceRepaint(int x, int y, int w, int h);
    QRegion repaints() const;
    void resetRepaints();
    QRegion damage() const;
    void resetDamage();
    EffectWindowImpl* effectWindow();
    const EffectWindowImpl* effectWindow() const;
    /**
     * Window will be temporarily painted as if being at the top of the stack.
     * Only available if Compositor is active, if not active, this method is a no-op.
     **/
    void elevate(bool elevate);

    /**
     * @returns Whether the Toplevel has a Shadow or not
     * @see shadow
     **/
    bool hasShadow() const;
    /**
     * Returns the pointer to the Toplevel's Shadow. A Shadow
     * is only available if Compositing is enabled and the corresponding X window
     * has the Shadow property set.
     * If a shadow is available @link hasShadow returns @c true.
     * @returns The Shadow belonging to this Toplevel, may be @c NULL.
     * @see hasShadow
     **/
    const Shadow *shadow() const;
    Shadow *shadow();
    /**
     * Updates the Shadow associated with this Toplevel from X11 Property.
     * Call this method when the Property changes or Compositing is started.
     **/
    void getShadow();
    /**
     * Whether the Toplevel currently wants the shadow to be rendered. Default
     * implementation always returns @c true.
     **/
    virtual bool wantsShadowToBeRendered() const;

    /**
     * This method returns the area that the Toplevel window reports to be opaque.
     * It is supposed to only provide valuable information if @link hasAlpha is @c true .
     * @see hasAlpha
     **/
    const QRegion& opaqueRegion() const;

    virtual Layer layer() const = 0;

    /**
     * Resets the damage state and sends a request for the damage region.
     * A call to this function must be followed by a call to getDamageRegionReply(),
     * or the reply will be leaked.
     *
     * Returns true if the window was damaged, and false otherwise.
     */
    bool resetAndFetchDamage();

    /**
     * Gets the reply from a previous call to resetAndFetchDamage().
     * Calling this function is a no-op if there is no pending reply.
     * Call damage() to return the fetched region.
     */
    void getDamageRegionReply();

    bool skipsCloseAnimation() const;
    void setSkipCloseAnimation(bool set);

    quint32 surfaceId() const;
    KWayland::Server::SurfaceInterface *surface() const;
    void setSurface(KWayland::Server::SurfaceInterface *surface);

    virtual void setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo);
    const QSharedPointer<QOpenGLFramebufferObject> &internalFramebufferObject() const;

    /**
     * @returns Transformation to map from global to window coordinates.
     *
     * Default implementation returns a translation on negative pos().
     * @see pos
     **/
    virtual QMatrix4x4 inputTransformation() const;

    /**
     * The window has a popup grab. This means that when it got mapped the
     * parent window had an implicit (pointer) grab.
     *
     * Normally this is only relevant for transient windows.
     *
     * Once the popup grab ends (e.g. pointer press outside of any Toplevel of
     * the client), the method popupDone should be invoked.
     *
     * The default implementation returns @c false.
     * @see popupDone
     * @since 5.10
     **/
    virtual bool hasPopupGrab() const {
        return false;
    }
    /**
     * This method should be invoked for Toplevels with a popup grab when
     * the grab ends.
     *
     * The default implementation does nothing.
     * @see hasPopupGrab
     * @since 5.10
     **/
    virtual void popupDone() {};

    /**
     * @brief Finds the Toplevel matching the condition expressed in @p func in @p list.
     *
     * The method is templated to operate on either a list of Toplevels or on a list of
     * a subclass type of Toplevel.
     * @param list The list to search in
     * @param func The condition function (compare std::find_if)
     * @return T* The found Toplevel or @c null if there is no matching Toplevel
     */
    template <class T, class U>
    static T *findInList(const QList<T*> &list, std::function<bool (const U*)> func);

Q_SIGNALS:
    void opacityChanged(KWin::Toplevel* toplevel, qreal oldOpacity);
    void damaged(KWin::Toplevel* toplevel, const QRect& damage);
    void geometryChanged();
    void geometryShapeChanged(KWin::Toplevel* toplevel, const QRect& old);
    void paddingChanged(KWin::Toplevel* toplevel, const QRect& old);
    void windowClosed(KWin::Toplevel* toplevel, KWin::Deleted* deleted);
    void windowShown(KWin::Toplevel* toplevel);
    void windowHidden(KWin::Toplevel* toplevel);
    /**
     * Signal emitted when the window's shape state changed. That is if it did not have a shape
     * and received one or if the shape was withdrawn. Think of Chromium enabling/disabling KWin's
     * decoration.
     **/
    void shapedChanged();
    /**
     * Emitted whenever the state changes in a way, that the Compositor should
     * schedule a repaint of the scene.
     **/
    void needsRepaint();
    void activitiesChanged(KWin::Toplevel* toplevel);
    /**
     * Emitted whenever the Toplevel's screen changes. This can happen either in consequence to
     * a screen being removed/added or if the Toplevel's geometry changes.
     * @since 4.11
     **/
    void screenChanged();
    void skipCloseAnimationChanged();
    /**
     * Emitted whenever the window role of the window changes.
     * @since 5.0
     **/
    void windowRoleChanged();
    /**
     * Emitted whenever the window class name or resource name of the window changes.
     * @since 5.0
     **/
    void windowClassChanged();
    /**
     * Emitted when a Wayland Surface gets associated with this Toplevel.
     * @since 5.3
     **/
    void surfaceIdChanged(quint32);
    /**
     * @since 5.4
     **/
    void hasAlphaChanged();

    /**
     * Emitted whenever the Surface for this Toplevel changes.
     **/
    void surfaceChanged();

    /*
     * Emitted when the client's screen changes onto a screen of a different scale
     * or the screen we're on changes
     * @since 5.12
     */
    void screenScaleChanged();

protected Q_SLOTS:
    /**
     * Checks whether the screen number for this Toplevel changed and updates if needed.
     * Any method changing the geometry of the Toplevel should call this method.
     **/
    void checkScreen();
    void setupCheckScreenConnection();
    void removeCheckScreenConnection();
    void setReadyForPainting();

protected:
    virtual ~Toplevel();
    void setWindowHandles(xcb_window_t client);
    void detectShape(Window id);
    virtual void propertyNotifyEvent(xcb_property_notify_event_t *e);
    virtual void damageNotifyEvent();
    virtual void clientMessageEvent(xcb_client_message_event_t *e);
    void discardWindowPixmap();
    void addDamageFull();
    virtual void addDamage(const QRegion &damage);
    Xcb::Property fetchWmClientLeader() const;
    void readWmClientLeader(Xcb::Property &p);
    void getWmClientLeader();
    void getWmClientMachine();
    /**
     * @returns Whether there is a compositor and it is active.
     **/
    bool compositing() const;

    /**
     * This function fetches the opaque region from this Toplevel.
     * Will only be called on corresponding property changes and for initialization.
     **/
    void getWmOpaqueRegion();

    void getResourceClass();
    void setResourceClass(const QByteArray &name, const QByteArray &className = QByteArray());
    Xcb::Property fetchSkipCloseAnimation() const;
    void readSkipCloseAnimation(Xcb::Property &prop);
    void getSkipCloseAnimation();
    virtual void debug(QDebug& stream) const = 0;
    void copyToDeleted(Toplevel* c);
    void disownDataPassedToDeleted();
    friend QDebug& operator<<(QDebug& stream, const Toplevel*);
    void deleteEffectWindow();
    void setDepth(int depth);
    QRect geom;
    xcb_visualid_t m_visual;
    int bit_depth;
    NETWinInfo* info;
    bool ready_for_painting;
    QRegion repaints_region; // updating, repaint just requires repaint of that area
    QRegion layer_repaints_region;

protected:
    bool m_isDamaged;

private:
    // when adding new data members, check also copyToDeleted()
    Xcb::Window m_client;
    xcb_damage_damage_t damage_handle;
    QRegion damage_region; // damage is really damaged window (XDamage) and texture needs
    bool is_shape;
    EffectWindowImpl* effect_window;
    QByteArray resource_name;
    QByteArray resource_class;
    ClientMachine *m_clientMachine;
    WId wmClientLeaderWin;
    bool m_damageReplyPending;
    QRegion opaque_region;
    xcb_xfixes_fetch_region_cookie_t m_regionCookie;
    int m_screen;
    bool m_skipCloseAnimation;
    quint32 m_surfaceId = 0;
    KWayland::Server::SurfaceInterface *m_surface = nullptr;
    /**
     * An FBO object KWin internal windows might render to.
     **/
    QSharedPointer<QOpenGLFramebufferObject> m_internalFBO;
    // when adding new data members, check also copyToDeleted()
    qreal m_screenScale = 1.0;
};

inline xcb_window_t Toplevel::window() const
{
    return m_client;
}

inline void Toplevel::setWindowHandles(xcb_window_t w)
{
    assert(!m_client.isValid() && w != XCB_WINDOW_NONE);
    m_client.reset(w, false);
}

inline QRect Toplevel::geometry() const
{
    return geom;
}

inline QSize Toplevel::size() const
{
    return geom.size();
}

inline QPoint Toplevel::pos() const
{
    return geom.topLeft();
}

inline int Toplevel::x() const
{
    return geom.x();
}

inline int Toplevel::y() const
{
    return geom.y();
}

inline int Toplevel::width() const
{
    return geom.width();
}

inline int Toplevel::height() const
{
    return geom.height();
}

inline QRect Toplevel::rect() const
{
    return QRect(0, 0, width(), height());
}

inline bool Toplevel::readyForPainting() const
{
    return ready_for_painting;
}

inline xcb_visualid_t Toplevel::visual() const
{
    return m_visual;
}

inline bool Toplevel::isDesktop() const
{
    return windowType() == NET::Desktop;
}

inline bool Toplevel::isDock() const
{
    return windowType() == NET::Dock;
}

inline bool Toplevel::isMenu() const
{
    return windowType() == NET::Menu;
}

inline bool Toplevel::isToolbar() const
{
    return windowType() == NET::Toolbar;
}

inline bool Toplevel::isSplash() const
{
    return windowType() == NET::Splash;
}

inline bool Toplevel::isUtility() const
{
    return windowType() == NET::Utility;
}

inline bool Toplevel::isDialog() const
{
    return windowType() == NET::Dialog;
}

inline bool Toplevel::isNormalWindow() const
{
    return windowType() == NET::Normal;
}

inline bool Toplevel::isDropdownMenu() const
{
    return windowType() == NET::DropdownMenu;
}

inline bool Toplevel::isPopupMenu() const
{
    return windowType() == NET::PopupMenu;
}

inline bool Toplevel::isTooltip() const
{
    return windowType() == NET::Tooltip;
}

inline bool Toplevel::isNotification() const
{
    return windowType() == NET::Notification;
}

inline bool Toplevel::isOnScreenDisplay() const
{
    return windowType() == NET::OnScreenDisplay;
}

inline bool Toplevel::isComboBox() const
{
    return windowType() == NET::ComboBox;
}

inline bool Toplevel::isDNDIcon() const
{
    return windowType() == NET::DNDIcon;
}

inline bool Toplevel::isLockScreen() const
{
    return false;
}

inline bool Toplevel::isInputMethod() const
{
    return false;
}

inline QRegion Toplevel::damage() const
{
    return damage_region;
}

inline QRegion Toplevel::repaints() const
{
    return repaints_region.translated(pos()) | layer_repaints_region;
}

inline bool Toplevel::shape() const
{
    return is_shape;
}

inline int Toplevel::depth() const
{
    return bit_depth;
}

inline bool Toplevel::hasAlpha() const
{
    return depth() == 32;
}

inline const QRegion& Toplevel::opaqueRegion() const
{
    return opaque_region;
}

inline
EffectWindowImpl* Toplevel::effectWindow()
{
    return effect_window;
}

inline
const EffectWindowImpl* Toplevel::effectWindow() const
{
    return effect_window;
}

inline bool Toplevel::isOnAllDesktops() const
{
    return desktop() == NET::OnAllDesktops;
}

inline bool Toplevel::isOnAllActivities() const
{
    return activities().isEmpty();
}

inline bool Toplevel::isOnDesktop(int d) const
{
    return desktop() == d || /*desk == 0 ||*/ isOnAllDesktops();
}

inline bool Toplevel::isOnActivity(const QString &activity) const
{
    return activities().isEmpty() || activities().contains(activity);
}

inline bool Toplevel::isOnCurrentDesktop() const
{
    return isOnDesktop(VirtualDesktopManager::self()->current());
}

inline QByteArray Toplevel::resourceName() const
{
    return resource_name; // it is always lowercase
}

inline QByteArray Toplevel::resourceClass() const
{
    return resource_class; // it is always lowercase
}

inline const ClientMachine *Toplevel::clientMachine() const
{
    return m_clientMachine;
}

inline quint32 Toplevel::surfaceId() const
{
    return m_surfaceId;
}

inline KWayland::Server::SurfaceInterface *Toplevel::surface() const
{
    return m_surface;
}

inline const QSharedPointer<QOpenGLFramebufferObject> &Toplevel::internalFramebufferObject() const
{
    return m_internalFBO;
}

inline QPoint Toplevel::clientContentPos() const
{
    return QPoint(0, 0);
}

template <class T, class U>
inline T *Toplevel::findInList(const QList<T*> &list, std::function<bool (const U*)> func)
{
    static_assert(std::is_base_of<U, T>::value,
                 "U must be derived from T");
    const auto it = std::find_if(list.begin(), list.end(), func);
    if (it == list.end()) {
        return nullptr;
    }
    return *it;
}

QDebug& operator<<(QDebug& stream, const Toplevel*);
QDebug& operator<<(QDebug& stream, const ToplevelList&);

} // namespace
Q_DECLARE_METATYPE(KWin::Toplevel*)

#endif
