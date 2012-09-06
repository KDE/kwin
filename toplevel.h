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

#include <config-X11.h>

#include <assert.h>
#include <QObject>
#include <kdecoration.h>
#include <kdebug.h>

#include "utils.h"
#include "workspace.h"

#include <X11/extensions/Xdamage.h>

class NETWinInfo2;

namespace KWin
{

class Workspace;
class EffectWindowImpl;
class Shadow;

class Toplevel
    : public QObject, public KDecorationDefines
{
    Q_OBJECT
    Q_PROPERTY(bool alpha READ hasAlpha CONSTANT)
    Q_PROPERTY(qulonglong frameId READ frameId)
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(QRect visibleRect READ visibleRect)
    Q_PROPERTY(int height READ height)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(QPoint pos READ pos)
    Q_PROPERTY(int screen READ screen)
    Q_PROPERTY(QSize size READ size)
    Q_PROPERTY(int width READ width)
    Q_PROPERTY(qulonglong windowId READ window CONSTANT)
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
    Q_PROPERTY(QByteArray resourceName READ resourceName)
    Q_PROPERTY(QByteArray resourceClass READ resourceClass)
    Q_PROPERTY(QByteArray windowRole READ windowRole)
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
    Q_PROPERTY(QStringList activities READ activities)
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
public:
    Toplevel(Workspace *ws);
    Window frameId() const;
    Window window() const;
    Workspace* workspace() const;
    QRect geometry() const;
    QSize size() const;
    QPoint pos() const;
    QRect rect() const;
    int x() const;
    int y() const;
    int width() const;
    int height() const;
    bool isOnScreen(int screen) const;   // true if it's at least partially there
    int screen() const; // the screen where the center is
    virtual QPoint clientPos() const = 0; // inside of geometry()
    virtual QSize clientSize() const = 0;
    virtual QRect visibleRect() const; // the area the window occupies on the screen
    virtual QRect decorationRect() const; // rect including the decoration shadows
    virtual QRect transparentRect() const = 0;
    virtual QRegion decorationPendingRegion() const; // decoration region that needs to be repainted
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
    bool isComboBox() const;
    bool isDNDIcon() const;

    virtual int desktop() const = 0;
    virtual QStringList activities() const = 0;
    bool isOnDesktop(int d) const;
    bool isOnActivity(const QString &activity) const;
    bool isOnCurrentDesktop() const;
    bool isOnCurrentActivity() const;
    bool isOnAllDesktops() const;
    bool isOnAllActivities() const;

    QByteArray windowRole() const;
    QByteArray sessionId() const;
    QByteArray resourceName() const;
    QByteArray resourceClass() const;
    QByteArray wmCommand();
    QByteArray wmClientMachine(bool use_localhost) const;
    Window wmClientLeader() const;
    pid_t pid() const;
    static bool resourceMatch(const Toplevel* c1, const Toplevel* c2);

    Pixmap windowPixmap(bool allow_create = true);   // may return None (e.g. at a bad moment while resizing)
    bool readyForPainting() const; // true if the window has been already painted its contents
    Visual* visual() const;
    bool shape() const;
    void setOpacity(double opacity);
    double opacity() const;
    int depth() const;
    bool hasAlpha() const;
    virtual bool setupCompositing();
    virtual void finishCompositing();
    bool updateUnredirectedState();
    bool unredirected() const;
    void suspendUnredirect(bool suspend);
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
    void resetDamage(const QRect& r);
    EffectWindowImpl* effectWindow();
    const EffectWindowImpl* effectWindow() const;

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
     * This method returns the area that the Toplevel window reports to be opaque.
     * It is supposed to only provide valuable information if @link hasAlpha is @c true .
     * @see hasAlpha
     **/
    const QRegion& opaqueRegion() const;

    virtual Layer layer() const = 0;

signals:
    void opacityChanged(KWin::Toplevel* toplevel, qreal oldOpacity);
    void damaged(KWin::Toplevel* toplevel, const QRect& damage);
    void propertyNotify(KWin::Toplevel* toplevel, long a);
    void geometryChanged();
    void geometryShapeChanged(KWin::Toplevel* toplevel, const QRect& old);
    void paddingChanged(KWin::Toplevel* toplevel, const QRect& old);
    void windowClosed(KWin::Toplevel* toplevel, KWin::Deleted* deleted);
    void windowShown(KWin::Toplevel* toplevel);
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

protected:
    virtual ~Toplevel();
    void setWindowHandles(Window client, Window frame);
    void detectShape(Window id);
    virtual void propertyNotifyEvent(XPropertyEvent* e);
    virtual void damageNotifyEvent(XDamageNotifyEvent* e);
    Pixmap createWindowPixmap();
    void discardWindowPixmap();
    void addDamage(const QRect& r);
    void addDamage(int x, int y, int w, int h);
    void addDamageFull();
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
    void getWindowRole();
    virtual void debug(QDebug& stream) const = 0;
    void copyToDeleted(Toplevel* c);
    void disownDataPassedToDeleted();
    friend QDebug& operator<<(QDebug& stream, const Toplevel*);
    void deleteEffectWindow();
    virtual bool shouldUnredirect() const = 0;
    QRect geom;
    Visual* vis;
    int bit_depth;
    NETWinInfo2* info;
    bool ready_for_painting;
    QTimer *m_readyForPaintingTimer;
    QRegion repaints_region; // updating, repaint just requires repaint of that area
    QRegion layer_repaints_region;
protected slots:
    void setReadyForPainting();
private:
    static QByteArray staticWindowRole(WId);
    static QByteArray staticSessionId(WId);
    static QByteArray staticWmCommand(WId);
    static QByteArray staticWmClientMachine(WId);
    static Window staticWmClientLeader(WId);
    // when adding new data members, check also copyToDeleted()
    Window client;
    Window frame;
    Workspace* wspace;
    Pixmap window_pix;
    Damage damage_handle;
    QRegion damage_region; // damage is really damaged window (XDamage) and texture needs
    float damageRatio;
    bool is_shape;
    EffectWindowImpl* effect_window;
    QByteArray resource_name;
    QByteArray resource_class;
    QByteArray client_machine;
    WId wmClientLeaderWin;
    QByteArray window_role;
    bool unredirect;
    bool unredirectSuspend; // when unredirected, but pixmap is needed temporarily
    QRegion opaque_region;
    // when adding new data members, check also copyToDeleted()
};

inline Window Toplevel::window() const
{
    return client;
}

inline Window Toplevel::frameId() const
{
    return frame;
}

inline void Toplevel::setWindowHandles(Window w, Window f)
{
    assert(client == None && w != None);
    client = w;
    assert(frame == None && f != None);
    frame = f;
}

inline Workspace* Toplevel::workspace() const
{
    return wspace;
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

inline QRegion Toplevel::decorationPendingRegion() const
{
    return QRegion();
}

inline bool Toplevel::readyForPainting() const
{
    return ready_for_painting;
}

inline Visual* Toplevel::visual() const
{
    return vis;
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

inline bool Toplevel::isComboBox() const
{
    return windowType() == NET::ComboBox;
}

inline bool Toplevel::isDNDIcon() const
{
    return windowType() == NET::DNDIcon;
}

inline Pixmap Toplevel::windowPixmap(bool allow_create)
{
    if (window_pix == None && allow_create)
        window_pix = createWindowPixmap();
    return window_pix;
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
    return isOnDesktop(workspace()->currentDesktop());
}

inline bool Toplevel::isOnCurrentActivity() const
{
    return isOnActivity(Workspace::self()->currentActivity());
}

inline QByteArray Toplevel::resourceName() const
{
    return resource_name; // it is always lowercase
}

inline QByteArray Toplevel::resourceClass() const
{
    return resource_class; // it is always lowercase
}

inline QByteArray Toplevel::windowRole() const
{
    return window_role;
}

inline pid_t Toplevel::pid() const
{
    return info->pid();
}

inline bool Toplevel::unredirected() const
{
    return unredirect;
}

QDebug& operator<<(QDebug& stream, const Toplevel*);
QDebug& operator<<(QDebug& stream, const ToplevelList&);
QDebug& operator<<(QDebug& stream, const ConstToplevelList&);

KWIN_COMPARE_PREDICATE(WindowMatchPredicate, Toplevel, Window, cl->window() == value);
KWIN_COMPARE_PREDICATE(FrameIdMatchPredicate, Toplevel, Window, cl->frameId() == value);

} // namespace

#endif
