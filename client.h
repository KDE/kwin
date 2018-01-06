/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_CLIENT_H
#define KWIN_CLIENT_H

// kwin
#include "options.h"
#include "rules.h"
#include "tabgroup.h"
#include "abstract_client.h"
#include "xcbutils.h"
// Qt
#include <QElapsedTimer>
#include <QFlags>
#include <QPointer>
#include <QPixmap>
#include <QWindow>
// X
#include <xcb/sync.h>

// TODO: Cleanup the order of things in this .h file

class QTimer;
class KStartupInfoData;
class KStartupInfoId;

namespace KWin
{


/**
 * @brief Defines Predicates on how to search for a Client.
 *
 * Used by Workspace::findClient.
 */
enum class Predicate {
    WindowMatch,
    WrapperIdMatch,
    FrameIdMatch,
    InputIdMatch
};

class KWIN_EXPORT Client
    : public AbstractClient
{
    Q_OBJECT
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(QSize basicUnit READ basicUnit)
    /**
     * The "Window Tabs" Group this Client belongs to.
     **/
    Q_PROPERTY(KWin::TabGroup* tabGroup READ tabGroup NOTIFY tabGroupChanged SCRIPTABLE false)
    /**
     * A client can block compositing. That is while the Client is alive and the state is set,
     * Compositing is suspended and is resumed when there are no Clients blocking compositing any
     * more.
     *
     * This is actually set by a window property, unfortunately not used by the target application
     * group. For convenience it's exported as a property to the scripts.
     *
     * Use with care!
     **/
    Q_PROPERTY(bool blocksCompositing READ isBlockingCompositing WRITE setBlockingCompositing NOTIFY blockingCompositingChanged)
    /**
     * Whether the Client uses client side window decorations.
     * Only GTK+ are detected.
     **/
    Q_PROPERTY(bool clientSideDecorated READ isClientSideDecorated NOTIFY clientSideDecoratedChanged)
public:
    explicit Client();
    xcb_window_t wrapperId() const;
    xcb_window_t inputId() const { return m_decoInputExtent; }
    virtual xcb_window_t frameId() const override;

    bool isTransient() const override;
    bool groupTransient() const;
    bool wasOriginallyGroupTransient() const;
    QList<AbstractClient*> mainClients() const override; // Call once before loop , is not indirect
    bool hasTransient(const AbstractClient* c, bool indirect) const override;
    void checkTransient(xcb_window_t w);
    AbstractClient* findModal(bool allow_itself = false) override;
    const Group* group() const;
    Group* group();
    void checkGroup(Group* gr = NULL, bool force = false);
    void changeClientLeaderGroup(Group* gr);
    void updateWindowRules(Rules::Types selection) override;
    void updateFullscreenMonitors(NETFullscreenMonitors topology);

    bool hasNETSupport() const;

    QSize minSize() const override;
    QSize maxSize() const override;
    QSize basicUnit() const;
    virtual QSize clientSize() const;
    QPoint inputPos() const { return input_offset; } // Inside of geometry()

    bool windowEvent(xcb_generic_event_t *e);
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const;

    bool manage(xcb_window_t w, bool isMapped);
    void releaseWindow(bool on_shutdown = false);
    void destroyClient();

    virtual QStringList activities() const;
    void setOnActivity(const QString &activity, bool enable);
    void setOnAllActivities(bool set) override;
    void setOnActivities(QStringList newActivitiesList) override;
    void updateActivities(bool includeTransients);
    void blockActivityUpdates(bool b = true) override;

    /// Is not minimized and not hidden. I.e. normally visible on some virtual desktop.
    bool isShown(bool shaded_is_shown) const override;
    bool isHiddenInternal() const override; // For compositing

    ShadeMode shadeMode() const override; // Prefer isShade()
    void setShade(ShadeMode mode) override;
    bool isShadeable() const override;

    bool isMaximizable() const override;
    QRect geometryRestore() const override;
    MaximizeMode maximizeMode() const override;

    bool isMinimizable() const override;
    QRect iconGeometry() const override;

    void setFullScreen(bool set, bool user = true) override;
    bool isFullScreen() const override;
    bool userCanSetFullScreen() const override;
    QRect geometryFSRestore() const {
        return geom_fs_restore;    // Only for session saving
    }
    int fullScreenMode() const {
        return fullscreen_mode;    // only for session saving
    }

    bool noBorder() const override;
    void setNoBorder(bool set) override;
    bool userCanSetNoBorder() const override;
    void checkNoBorder() override;

    int sessionStackingOrder() const;

    // Auxiliary functions, depend on the windowType
    bool wantsInput() const override;

    bool isResizable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isCloseable() const override; ///< May be closed by the user (May have a close button)

    void takeFocus() override;

    void updateDecoration(bool check_workspace_pos, bool force = false) override;

    void updateShape();

    using AbstractClient::setGeometry;
    void setGeometry(int x, int y, int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    /// plainResize() simply resizes
    void plainResize(int w, int h, ForceGeometry_t force = NormalGeometrySet);
    void plainResize(const QSize& s, ForceGeometry_t force = NormalGeometrySet);
    /// resizeWithChecks() resizes according to gravity, and checks workarea position
    using AbstractClient::resizeWithChecks;
    void resizeWithChecks(int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    void resizeWithChecks(int w, int h, xcb_gravity_t gravity, ForceGeometry_t force = NormalGeometrySet);
    void resizeWithChecks(const QSize& s, xcb_gravity_t gravity, ForceGeometry_t force = NormalGeometrySet);
    QSize sizeForClientSize(const QSize&, Sizemode mode = SizemodeAny, bool noframe = false) const override;

    bool providesContextHelp() const override;

    Options::WindowOperation mouseButtonToWindowOperation(Qt::MouseButtons button);
    bool performMouseCommand(Options::MouseCommand, const QPoint& globalPos) override;

    QRect adjustedClientArea(const QRect& desktop, const QRect& area) const;

    xcb_colormap_t colormap() const;

    /// Updates visibility depending on being shaded, virtual desktop, etc.
    void updateVisibility();
    /// Hides a client - Basically like minimize, but without effects, it's simply hidden
    void hideClient(bool hide) override;
    bool hiddenPreview() const; ///< Window is mapped in order to get a window pixmap

    virtual bool setupCompositing();
    void finishCompositing(ReleaseReason releaseReason = ReleaseReason::Release) override;
    void setBlockingCompositing(bool block);
    inline bool isBlockingCompositing() { return blocks_compositing; }

    QString captionNormal() const override {
        return cap_normal;
    }
    QString captionSuffix() const override {
        return cap_suffix;
    }

    using AbstractClient::keyPressEvent;
    void keyPressEvent(uint key_code, xcb_timestamp_t time);   // FRAME ??
    void updateMouseGrab() override;
    xcb_window_t moveResizeGrabWindow() const;

    const QPoint calculateGravitation(bool invert, int gravity = 0) const;   // FRAME public?

    void NETMoveResize(int x_root, int y_root, NET::Direction direction);
    void NETMoveResizeWindow(int flags, int x, int y, int width, int height);
    void restackWindow(xcb_window_t above, int detail, NET::RequestSource source, xcb_timestamp_t timestamp,
                       bool send_event = false);

    void gotPing(xcb_timestamp_t timestamp);

    void updateUserTime(xcb_timestamp_t time = XCB_TIME_CURRENT_TIME);
    xcb_timestamp_t userTime() const override;
    bool hasUserTimeSupport() const;

    /// Does 'delete c;'
    static void deleteClient(Client* c);

    static bool belongToSameApplication(const Client* c1, const Client* c2, SameApplicationChecks checks = SameApplicationChecks());
    static bool sameAppWindowRoleMatch(const Client* c1, const Client* c2, bool active_hack);

    void killWindow() override;
    void toggleShade();
    void showContextHelp() override;
    void cancelShadeHoverTimer();
    void checkActiveModal();
    StrutRect strutRect(StrutArea area) const;
    StrutRects strutRects() const;
    bool hasStrut() const override;

    // Tabbing functions
    TabGroup* tabGroup() const override; // Returns a pointer to client_group
    Q_INVOKABLE inline bool tabBefore(Client *other, bool activate) { return tabTo(other, false, activate); }
    Q_INVOKABLE inline bool tabBehind(Client *other, bool activate) { return tabTo(other, true, activate); }
    /**
     * Syncs the *dynamic* @param property @param fromThisClient or the @link currentTab() to
     * all members of the @link tabGroup() (if there is one)
     *
     * eg. if you call:
     * client->setProperty("kwin_tiling_floats", true);
     * client->syncTabGroupFor("kwin_tiling_floats", true)
     * all clients in this tabGroup will have ::property("kwin_tiling_floats").toBool() == true
     *
     * WARNING: non dynamic properties are ignored - you're not supposed to alter/update such explicitly
     */
    Q_INVOKABLE void syncTabGroupFor(QString property, bool fromThisClient = false);
    Q_INVOKABLE bool untab(const QRect &toGeometry = QRect(), bool clientRemoved = false) override;
    /**
     * Set tab group - this is to be invoked by TabGroup::add/remove(client) and NO ONE ELSE
     */
    void setTabGroup(TabGroup* group);
    /*
    *   If shown is true the client is mapped and raised, if false
    *   the client is unmapped and hidden, this function is called
    *   when the tabbing group of the client switches its visible
    *   client.
    */
    void setClientShown(bool shown);
    /*
    *   When a click is done in the decoration and it calls the group
    *   to change the visible client it starts to move-resize the new
    *   client, this function stops it.
    */
    bool isCurrentTab() const override;

    /**
     * Whether or not the window has a strut that expands through the invisible area of
     * an xinerama setup where the monitors are not the same resolution.
     */
    bool hasOffscreenXineramaStrut() const;

    // Decorations <-> Effects
    QRect decorationRect() const;

    QRect transparentRect() const;

    bool isClientSideDecorated() const;
    bool wantsShadowToBeRendered() const override;

    void layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const override;

    Xcb::Property fetchFirstInTabBox() const;
    void readFirstInTabBox(Xcb::Property &property);
    void updateFirstInTabBox();
    Xcb::StringProperty fetchColorScheme() const;
    void readColorScheme(Xcb::StringProperty &property);
    void updateColorScheme() override;

    //sets whether the client should be faked as being on all activities (and be shown during session save)
    void setSessionActivityOverride(bool needed);
    virtual bool isClient() const;

    template <typename T>
    void print(T &stream) const;

    void cancelFocusOutTimer();

    /**
     * Restores the Client after it had been hidden due to show on screen edge functionality.
     * In addition the property gets deleted so that the Client knows that it is visible again.
     **/
    void showOnScreenEdge() override;

    Xcb::StringProperty fetchApplicationMenuServiceName() const;
    void readApplicationMenuServiceName(Xcb::StringProperty &property);
    void checkApplicationMenuServiceName();

    Xcb::StringProperty fetchApplicationMenuObjectPath() const;
    void readApplicationMenuObjectPath(Xcb::StringProperty &property);
    void checkApplicationMenuObjectPath();

    struct SyncRequest {
        xcb_sync_counter_t counter;
        xcb_sync_int64_t value;
        xcb_sync_alarm_t alarm;
        xcb_timestamp_t lastTimestamp;
        QTimer *timeout, *failsafeTimeout;
        bool isPending;
    };
    const SyncRequest &getSyncRequest() const {
        return syncRequest;
    }
    void handleSync();

    static void cleanupX11();

public Q_SLOTS:
    void closeWindow() override;
    void updateCaption() override;

private Q_SLOTS:
    void shadeHover();
    void shadeUnhover();

private:
    // Use Workspace::createClient()
    virtual ~Client(); ///< Use destroyClient() or releaseWindow()

    // Handlers for X11 events
    bool mapRequestEvent(xcb_map_request_event_t *e);
    void unmapNotifyEvent(xcb_unmap_notify_event_t *e);
    void destroyNotifyEvent(xcb_destroy_notify_event_t *e);
    void configureRequestEvent(xcb_configure_request_event_t *e);
    virtual void propertyNotifyEvent(xcb_property_notify_event_t *e) override;
    void clientMessageEvent(xcb_client_message_event_t *e) override;
    void enterNotifyEvent(xcb_enter_notify_event_t *e);
    void leaveNotifyEvent(xcb_leave_notify_event_t *e);
    void focusInEvent(xcb_focus_in_event_t *e);
    void focusOutEvent(xcb_focus_out_event_t *e);
    virtual void damageNotifyEvent();

    bool buttonPressEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root, xcb_timestamp_t time = XCB_CURRENT_TIME);
    bool buttonReleaseEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root);
    bool motionNotifyEvent(xcb_window_t w, int state, int x, int y, int x_root, int y_root);

    Client* findAutogroupCandidate() const;

protected:
    virtual void debug(QDebug& stream) const;
    void addDamage(const QRegion &damage) override;
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const override;
    void doSetActive() override;
    void doSetKeepAbove() override;
    void doSetKeepBelow() override;
    void doSetDesktop(int desktop, int was_desk) override;
    void doMinimize() override;
    void doSetSkipPager() override;
    void doSetSkipTaskbar() override;
    bool belongsToDesktop() const override;
    void setGeometryRestore(const QRect &geo) override;
    void updateTabGroupStates(TabGroup::States states) override;
    void doMove(int x, int y) override;
    bool doStartMoveResize() override;
    void doPerformMoveResize() override;
    bool isWaitingForMoveResizeSync() const override;
    void doResizeSync() override;
    QSize resizeIncrements() const override;
    bool acceptsFocus() const override;

    //Signals for the scripting interface
    //Signals make an excellent way for communication
    //in between objects as compared to simple function
    //calls
Q_SIGNALS:
    void clientManaging(KWin::Client*);
    void clientFullScreenSet(KWin::Client*, bool, bool);

    /**
     * Emitted whenever the Client's TabGroup changed. That is whenever the Client is moved to
     * another group, but not when a Client gets added or removed to the Client's ClientGroup.
     **/
    void tabGroupChanged();

    /**
     * Emitted whenever the Client want to show it menu
     */
    void showRequest();
    /**
     * Emitted whenever the Client's menu is closed
     */
    void menuHidden();
    /**
     * Emitted whenever the Client's menu is available
     **/
    void appMenuAvailable();
    /**
     * Emitted whenever the Client's menu is unavailable
     */
    void appMenuUnavailable();

    /**
     * Emitted whenever the Client's block compositing state changes.
     **/
    void blockingCompositingChanged(KWin::Client *client);
    void clientSideDecoratedChanged();

private:
    void exportMappingState(int s);   // ICCCM 4.1.3.1, 4.1.4, NETWM 2.5.1
    bool isManaged() const; ///< Returns false if this client is not yet managed
    void updateAllowedActions(bool force = false);
    QRect fullscreenMonitorsArea(NETFullscreenMonitors topology) const;
    void changeMaximize(bool horizontal, bool vertical, bool adjust) override;
    int checkFullScreenHack(const QRect& geom) const;   // 0 - None, 1 - One xinerama screen, 2 - Full area
    void updateFullScreenHack(const QRect& geom);
    void getWmNormalHints();
    void getMotifHints();
    void getIcons();
    void fetchName();
    void fetchIconicName();
    QString readName() const;
    void setCaption(const QString& s, bool force = false);
    bool hasTransientInternal(const Client* c, bool indirect, ConstClientList& set) const;
    void setShortcutInternal() override;

    void configureRequest(int value_mask, int rx, int ry, int rw, int rh, int gravity, bool from_tool);
    NETExtendedStrut strut() const;
    int checkShadeGeometry(int w, int h);
    void getSyncCounter();
    void sendSyncRequest();
    void leaveMoveResize() override;
    void positionGeometryTip() override;
    void grabButton(int mod);
    void ungrabButton(int mod);
    void resizeDecoration();
    void createDecoration(const QRect &oldgeom);

    void pingWindow();
    void killProcess(bool ask, xcb_timestamp_t timestamp = XCB_TIME_CURRENT_TIME);
    void updateUrgency();
    static void sendClientMessage(xcb_window_t w, xcb_atom_t a, xcb_atom_t protocol,
                                  uint32_t data1 = 0, uint32_t data2 = 0, uint32_t data3 = 0,
                                  xcb_timestamp_t timestamp = xTime());

    void embedClient(xcb_window_t w, xcb_visualid_t visualid, xcb_colormap_t colormap, uint8_t depth);
    void detectNoBorder();
    Xcb::Property fetchGtkFrameExtents() const;
    void readGtkFrameExtents(Xcb::Property &prop);
    void detectGtkFrameExtents();
    void destroyDecoration() override;
    void updateFrameExtents();

    void internalShow();
    void internalHide();
    void internalKeep();
    void map();
    void unmap();
    void updateHiddenPreview();

    void updateInputShape();

    xcb_timestamp_t readUserTimeMapTimestamp(const KStartupInfoId* asn_id, const KStartupInfoData* asn_data,
                                  bool session) const;
    xcb_timestamp_t readUserCreationTime() const;
    void startupIdChanged();

    void updateInputWindow();

    bool tabTo(Client *other, bool behind, bool activate);

    Xcb::Property fetchShowOnScreenEdge() const;
    void readShowOnScreenEdge(Xcb::Property &property);
    /**
     * Reads the property and creates/destroys the screen edge if required
     * and shows/hides the client.
     **/
    void updateShowOnScreenEdge();

    Xcb::Window m_client;
    Xcb::Window m_wrapper;
    Xcb::Window m_frame;
    QStringList activityList;
    int m_activityUpdatesBlocked;
    bool m_blockedActivityUpdatesRequireTransients;
    Xcb::Window m_moveResizeGrabWindow;
    bool move_resize_has_keyboard_grab;
    bool m_managed;

    Xcb::GeometryHints m_geometryHints;
    void sendSyntheticConfigureNotify();
    enum MappingState {
        Withdrawn, ///< Not handled, as per ICCCM WithdrawnState
        Mapped, ///< The frame is mapped
        Unmapped, ///< The frame is not mapped
        Kept ///< The frame should be unmapped, but is kept (For compositing)
    };
    MappingState mapping_state;

    Xcb::TransientFor fetchTransient() const;
    void readTransientProperty(Xcb::TransientFor &transientFor);
    void readTransient();
    xcb_window_t verifyTransientFor(xcb_window_t transient_for, bool set);
    void addTransient(AbstractClient* cl) override;
    void removeTransient(AbstractClient* cl) override;
    void removeFromMainClients();
    void cleanGrouping();
    void checkGroupTransients();
    void setTransient(xcb_window_t new_transient_for_id);
    xcb_window_t m_transientForId;
    xcb_window_t m_originalTransientForId;
    ShadeMode shade_mode;
    Client *shade_below;
    uint deleting : 1; ///< True when doing cleanup and destroying the client
    Xcb::MotifHints m_motif;
    uint hidden : 1; ///< Forcibly hidden by calling hide()
    uint noborder : 1;
    uint app_noborder : 1; ///< App requested no border via window type, shape extension, etc.
    uint ignore_focus_stealing : 1; ///< Don't apply focus stealing prevention to this client
    bool blocks_compositing;
    // DON'T reorder - Saved to config files !!!
    enum FullScreenMode {
        FullScreenNone,
        FullScreenNormal,
        FullScreenHack ///< Non-NETWM fullscreen (noborder and size of desktop)
    };
    FullScreenMode fullscreen_mode;
    MaximizeMode max_mode;
    QRect geom_restore;
    QRect geom_fs_restore;
    QTimer* shadeHoverTimer;
    xcb_colormap_t m_colormap;
    QString cap_normal, cap_iconic, cap_suffix;
    Group* in_group;
    TabGroup* tab_group;
    QTimer* ping_timer;
    qint64 m_killHelperPID;
    xcb_timestamp_t m_pingTimestamp;
    xcb_timestamp_t m_userTime;
    NET::Actions allowed_actions;
    QSize client_size;
    bool shade_geometry_change;
    SyncRequest syncRequest;
    static bool check_active_modal; ///< \see Client::checkActiveModal()
    int sm_stacking_order;
    friend struct ResetupRulesProcedure;

    friend bool performTransiencyCheck();

    Xcb::StringProperty fetchActivities() const;
    void readActivities(Xcb::StringProperty &property);
    void checkActivities();
    bool activitiesDefined; //whether the x property was actually set

    bool sessionActivityOverride;
    bool needsXWindowMove;

    Xcb::Window m_decoInputExtent;
    QPoint input_offset;

    QTimer *m_focusOutTimer;

    QList<QMetaObject::Connection> m_connections;
    bool m_clientSideDecorated;

    QMetaObject::Connection m_edgeRemoveConnection;
    QMetaObject::Connection m_edgeGeometryTrackingConnection;
};

inline xcb_window_t Client::wrapperId() const
{
    return m_wrapper;
}

inline bool Client::isClientSideDecorated() const
{
    return m_clientSideDecorated;
}

inline bool Client::groupTransient() const
{
    return m_transientForId == rootWindow();
}

// Needed because verifyTransientFor() may set transient_for_id to root window,
// if the original value has a problem (window doesn't exist, etc.)
inline bool Client::wasOriginallyGroupTransient() const
{
    return m_originalTransientForId == rootWindow();
}

inline bool Client::isTransient() const
{
    return m_transientForId != XCB_WINDOW_NONE;
}

inline const Group* Client::group() const
{
    return in_group;
}

inline Group* Client::group()
{
    return in_group;
}

inline TabGroup* Client::tabGroup() const
{
    return tab_group;
}

inline bool Client::isShown(bool shaded_is_shown) const
{
    return !isMinimized() && (!isShade() || shaded_is_shown) && !hidden &&
           (!tabGroup() || tabGroup()->current() == this);
}

inline bool Client::isHiddenInternal() const
{
    return hidden;
}

inline ShadeMode Client::shadeMode() const
{
    return shade_mode;
}

inline QRect Client::geometryRestore() const
{
    return geom_restore;
}

inline void Client::setGeometryRestore(const QRect &geo)
{
    geom_restore = geo;
}

inline MaximizeMode Client::maximizeMode() const
{
    return max_mode;
}

inline bool Client::isFullScreen() const
{
    return fullscreen_mode != FullScreenNone;
}

inline bool Client::hasNETSupport() const
{
    return info->hasNETSupport();
}

inline xcb_colormap_t Client::colormap() const
{
    return m_colormap;
}

inline int Client::sessionStackingOrder() const
{
    return sm_stacking_order;
}

inline bool Client::isManaged() const
{
    return m_managed;
}

inline QSize Client::clientSize() const
{
    return client_size;
}

inline void Client::plainResize(const QSize& s, ForceGeometry_t force)
{
    plainResize(s.width(), s.height(), force);
}

inline void Client::resizeWithChecks(int w, int h, AbstractClient::ForceGeometry_t force)
{
    resizeWithChecks(w, h, XCB_GRAVITY_BIT_FORGET, force);
}

inline void Client::resizeWithChecks(const QSize& s, xcb_gravity_t gravity, ForceGeometry_t force)
{
    resizeWithChecks(s.width(), s.height(), gravity, force);
}

inline bool Client::hasUserTimeSupport() const
{
    return info->userTime() != -1U;
}

inline xcb_window_t Client::moveResizeGrabWindow() const
{
    return m_moveResizeGrabWindow;
}

inline bool Client::hiddenPreview() const
{
    return mapping_state == Kept;
}

template <typename T>
inline void Client::print(T &stream) const
{
    stream << "\'ID:" << window() << ";WMCLASS:" << resourceClass() << ":"
           << resourceName() << ";Caption:" << caption() << "\'";
}

} // namespace
Q_DECLARE_METATYPE(KWin::Client*)
Q_DECLARE_METATYPE(QList<KWin::Client*>)

#endif
