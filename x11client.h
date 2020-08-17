/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// kwin
#include "options.h"
#include "rules.h"
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

class KWIN_EXPORT X11Client : public AbstractClient
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
     * A client can block compositing. That is while the Client is alive and the state is set,
     * Compositing is suspended and is resumed when there are no Clients blocking compositing any
     * more.
     *
     * This is actually set by a window property, unfortunately not used by the target application
     * group. For convenience it's exported as a property to the scripts.
     *
     * Use with care!
     */
    Q_PROPERTY(bool blocksCompositing READ isBlockingCompositing WRITE setBlockingCompositing NOTIFY blockingCompositingChanged)
    /**
     * Whether the Client uses client side window decorations.
     * Only GTK+ are detected.
     */
    Q_PROPERTY(bool clientSideDecorated READ isClientSideDecorated NOTIFY clientSideDecoratedChanged)
public:
    explicit X11Client();
    ~X11Client() override; ///< Use destroyClient() or releaseWindow()

    xcb_window_t wrapperId() const;
    xcb_window_t inputId() const { return m_decoInputExtent; }
    xcb_window_t frameId() const override;

    QRect inputGeometry() const override;
    QRect bufferGeometry() const override;
    QMargins bufferMargins() const override;

    QPoint framePosToClientPos(const QPoint &point) const override;
    QPoint clientPosToFramePos(const QPoint &point) const override;
    QSize frameSizeToClientSize(const QSize &size) const override;
    QSize clientSizeToFrameSize(const QSize &size) const override;
    QRect frameRectToBufferRect(const QRect &rect) const;

    QMatrix4x4 inputTransformation() const override;

    bool isTransient() const override;
    bool groupTransient() const override;
    bool wasOriginallyGroupTransient() const;
    QList<AbstractClient*> mainClients() const override; // Call once before loop , is not indirect
    bool hasTransient(const AbstractClient* c, bool indirect) const override;
    void checkTransient(xcb_window_t w);
    AbstractClient* findModal(bool allow_itself = false) override;
    const Group* group() const override;
    Group* group() override;
    void checkGroup(Group* gr = nullptr, bool force = false);
    void changeClientLeaderGroup(Group* gr);
    bool supportsWindowRules() const override;
    void updateWindowRules(Rules::Types selection) override;
    void applyWindowRules() override;
    void updateFullscreenMonitors(NETFullscreenMonitors topology);

    bool hasNETSupport() const;

    QSize minSize() const override;
    QSize maxSize() const override;
    QSize basicUnit() const;
    QPoint inputPos() const { return input_offset; } // Inside of geometry()

    bool windowEvent(xcb_generic_event_t *e);
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;

    bool manage(xcb_window_t w, bool isMapped);
    void releaseWindow(bool on_shutdown = false);
    void destroyClient() override;

    QStringList activities() const override;
    void setOnActivity(const QString &activity, bool enable);
    void setOnAllActivities(bool set) override;
    void setOnActivities(QStringList newActivitiesList) override;
    void updateActivities(bool includeTransients);
    void blockActivityUpdates(bool b = true) override;

    /// Is not minimized and not hidden. I.e. normally visible on some virtual desktop.
    bool isShown(bool shaded_is_shown) const override;
    bool isHiddenInternal() const override; // For compositing

    bool isShadeable() const override;
    bool isMaximizable() const override;
    MaximizeMode maximizeMode() const override;

    bool isMinimizable() const override;
    QRect iconGeometry() const override;

    bool isFullScreenable() const override;
    void setFullScreen(bool set, bool user = true) override;
    bool isFullScreen() const override;
    bool userCanSetFullScreen() const override;
    QRect geometryFSRestore() const {
        return geom_fs_restore;     // only for session saving
    }
    int fullScreenMode() const {
        return m_fullscreenMode;    // only for session saving
    }

    bool userNoBorder() const;
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

    bool takeFocus() override;

    void updateDecoration(bool check_workspace_pos, bool force = false) override;

    void updateShape();

    using AbstractClient::move;
    void move(int x, int y, ForceGeometry_t force = NormalGeometrySet) override;
    void setFrameGeometry(const QRect &rect, ForceGeometry_t force = NormalGeometrySet) override;
    /// plainResize() simply resizes
    void plainResize(int w, int h, ForceGeometry_t force = NormalGeometrySet);
    void plainResize(const QSize& s, ForceGeometry_t force = NormalGeometrySet);
    /// resizeWithChecks() resizes according to gravity, and checks workarea position
    void resizeWithChecks(const QSize &size, ForceGeometry_t force = NormalGeometrySet) override;
    void resizeWithChecks(int w, int h, xcb_gravity_t gravity, ForceGeometry_t force = NormalGeometrySet);
    void resizeWithChecks(const QSize& s, xcb_gravity_t gravity, ForceGeometry_t force = NormalGeometrySet);
    QSize constrainClientSize(const QSize &size, SizeMode mode = SizeModeAny) const override;

    bool providesContextHelp() const override;

    xcb_colormap_t colormap() const;

    /// Updates visibility depending on being shaded, virtual desktop, etc.
    void updateVisibility();
    /// Hides a client - Basically like minimize, but without effects, it's simply hidden
    void hideClient(bool hide) override;
    bool hiddenPreview() const; ///< Window is mapped in order to get a window pixmap

    bool setupCompositing() override;
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

    QPoint gravityAdjustment(xcb_gravity_t gravity) const;
    const QPoint calculateGravitation(bool invert) const;

    void NETMoveResize(int x_root, int y_root, NET::Direction direction);
    void NETMoveResizeWindow(int flags, int x, int y, int width, int height);
    void restackWindow(xcb_window_t above, int detail, NET::RequestSource source, xcb_timestamp_t timestamp,
                       bool send_event = false);

    void gotPing(xcb_timestamp_t timestamp);

    void updateUserTime(xcb_timestamp_t time = XCB_TIME_CURRENT_TIME);
    xcb_timestamp_t userTime() const override;
    bool hasUserTimeSupport() const;

    /// Does 'delete c;'
    static void deleteClient(X11Client *c);

    static bool belongToSameApplication(const X11Client *c1, const X11Client *c2, SameApplicationChecks checks = SameApplicationChecks());
    static bool sameAppWindowRoleMatch(const X11Client *c1, const X11Client *c2, bool active_hack);

    void killWindow() override;
    void showContextHelp() override;
    void checkActiveModal();

    StrutRect strutRect(StrutArea area) const override;
    bool hasStrut() const override;

    /**
     * If shown is true the client is mapped and raised, if false
     * the client is unmapped and hidden, this function is called
     * when the tabbing group of the client switches its visible
     * client.
     */
    void setClientShown(bool shown) override;

    QRect transparentRect() const override;

    bool isClientSideDecorated() const;
    bool wantsShadowToBeRendered() const override;

    void layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const override;

    Xcb::Property fetchFirstInTabBox() const;
    void readFirstInTabBox(Xcb::Property &property);
    void updateFirstInTabBox();
    Xcb::StringProperty fetchPreferredColorScheme() const;
    QString readPreferredColorScheme(Xcb::StringProperty &property) const;
    QString preferredColorScheme() const override;

    //sets whether the client should be faked as being on all activities (and be shown during session save)
    void setSessionActivityOverride(bool needed);
    bool isClient() const override;

    void cancelFocusOutTimer();

    /**
     * Restores the Client after it had been hidden due to show on screen edge functionality.
     * In addition the property gets deleted so that the Client knows that it is visible again.
     */
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
    const SyncRequest &syncRequest() const {
        return m_syncRequest;
    }
    virtual bool wantsSyncCounter() const;
    void handleSync();

    static void cleanupX11();

public Q_SLOTS:
    void closeWindow() override;
    void updateCaption() override;

private:
    // Handlers for X11 events
    bool mapRequestEvent(xcb_map_request_event_t *e);
    void unmapNotifyEvent(xcb_unmap_notify_event_t *e);
    void destroyNotifyEvent(xcb_destroy_notify_event_t *e);
    void configureRequestEvent(xcb_configure_request_event_t *e);
    void propertyNotifyEvent(xcb_property_notify_event_t *e) override;
    void clientMessageEvent(xcb_client_message_event_t *e) override;
    void enterNotifyEvent(xcb_enter_notify_event_t *e);
    void leaveNotifyEvent(xcb_leave_notify_event_t *e);
    void focusInEvent(xcb_focus_in_event_t *e);
    void focusOutEvent(xcb_focus_out_event_t *e);
    void damageNotifyEvent() override;

    bool buttonPressEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root, xcb_timestamp_t time = XCB_CURRENT_TIME);
    bool buttonReleaseEvent(xcb_window_t w, int button, int state, int x, int y, int x_root, int y_root);
    bool motionNotifyEvent(xcb_window_t w, int state, int x, int y, int x_root, int y_root);

protected:
    void addDamage(const QRegion &damage) override;
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const override;
    void doSetActive() override;
    void doSetKeepAbove() override;
    void doSetKeepBelow() override;
    void doSetShade(ShadeMode previousShadeMode) override;
    void doSetDesktop() override;
    void doMinimize() override;
    void doSetSkipPager() override;
    void doSetSkipTaskbar() override;
    void doSetSkipSwitcher() override;
    void doSetDemandsAttention() override;
    bool belongsToDesktop() const override;
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
    void clientManaging(KWin::X11Client *);
    void clientFullScreenSet(KWin::X11Client *, bool, bool);

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
     */
    void appMenuAvailable();
    /**
     * Emitted whenever the Client's menu is unavailable
     */
    void appMenuUnavailable();

    /**
     * Emitted whenever the Client's block compositing state changes.
     */
    void blockingCompositingChanged(KWin::X11Client *client);
    void clientSideDecoratedChanged();

private:
    void exportMappingState(int s);   // ICCCM 4.1.3.1, 4.1.4, NETWM 2.5.1
    bool isManaged() const; ///< Returns false if this client is not yet managed
    void updateAllowedActions(bool force = false);
    QRect fullscreenMonitorsArea(NETFullscreenMonitors topology) const;
    void changeMaximize(bool horizontal, bool vertical, bool adjust) override;
    void getWmNormalHints();
    void getMotifHints();
    void getIcons();
    void fetchName();
    void fetchIconicName();
    QString readName() const;
    void setCaption(const QString& s, bool force = false);
    bool hasTransientInternal(const X11Client *c, bool indirect, QList<const X11Client *> &set) const;
    void setShortcutInternal() override;

    void configureRequest(int value_mask, int rx, int ry, int rw, int rh, int gravity, bool from_tool);
    NETExtendedStrut strut() const;
    int checkShadeGeometry(int w, int h);
    void getSyncCounter();
    void sendSyncRequest();
    void leaveMoveResize() override;
    void positionGeometryTip() override;
    void establishCommandWindowGrab(uint8_t button);
    void establishCommandAllGrab(uint8_t button);
    void resizeDecoration();
    void createDecoration(const QRect &oldgeom) override;

    void pingWindow();
    void killProcess(bool ask, xcb_timestamp_t timestamp = XCB_TIME_CURRENT_TIME);
    void updateUrgency();
    static void sendClientMessage(xcb_window_t w, xcb_atom_t a, xcb_atom_t protocol,
                                  uint32_t data1 = 0, uint32_t data2 = 0, uint32_t data3 = 0);

    void embedClient(xcb_window_t w, xcb_visualid_t visualid, xcb_colormap_t colormap, uint8_t depth);
    void detectNoBorder();
    void destroyDecoration() override;
    void updateFrameExtents();
    void setClientFrameExtents(const NETStrut &strut);

    void internalShow();
    void internalHide();
    void internalKeep();
    void map();
    void unmap();
    void updateHiddenPreview();

    void updateInputShape();
    void updateServerGeometry();
    void updateWindowPixmap();

    xcb_timestamp_t readUserTimeMapTimestamp(const KStartupInfoId* asn_id, const KStartupInfoData* asn_data,
                                  bool session) const;
    xcb_timestamp_t readUserCreationTime() const;
    void startupIdChanged();

    void updateInputWindow();

    Xcb::Property fetchShowOnScreenEdge() const;
    void readShowOnScreenEdge(Xcb::Property &property);
    /**
     * Reads the property and creates/destroys the screen edge if required
     * and shows/hides the client.
     */
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
    X11Client *shade_below;
    Xcb::MotifHints m_motif;
    uint hidden : 1; ///< Forcibly hidden by calling hide()
    uint noborder : 1;
    uint app_noborder : 1; ///< App requested no border via window type, shape extension, etc.
    uint ignore_focus_stealing : 1; ///< Don't apply focus stealing prevention to this client
    bool blocks_compositing;

    enum FullScreenMode {
        FullScreenNone,
        FullScreenNormal
    } m_fullscreenMode;

    MaximizeMode max_mode;
    QRect m_bufferGeometry = QRect(0, 0, 100, 100);
    QRect geom_fs_restore;
    xcb_colormap_t m_colormap;
    QString cap_normal, cap_iconic, cap_suffix;
    Group* in_group;
    QTimer* ping_timer;
    qint64 m_killHelperPID;
    xcb_timestamp_t m_pingTimestamp;
    xcb_timestamp_t m_userTime;
    NET::Actions allowed_actions;
    bool shade_geometry_change;
    SyncRequest m_syncRequest;
    static bool check_active_modal; ///< \see X11Client::checkActiveModal()
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

    QMetaObject::Connection m_edgeRemoveConnection;
    QMetaObject::Connection m_edgeGeometryTrackingConnection;

    QMargins m_clientFrameExtents;
};

inline xcb_window_t X11Client::wrapperId() const
{
    return m_wrapper;
}

inline bool X11Client::isClientSideDecorated() const
{
    return !m_clientFrameExtents.isNull();
}

inline bool X11Client::groupTransient() const
{
    return m_transientForId == rootWindow();
}

// Needed because verifyTransientFor() may set transient_for_id to root window,
// if the original value has a problem (window doesn't exist, etc.)
inline bool X11Client::wasOriginallyGroupTransient() const
{
    return m_originalTransientForId == rootWindow();
}

inline bool X11Client::isTransient() const
{
    return m_transientForId != XCB_WINDOW_NONE;
}

inline const Group* X11Client::group() const
{
    return in_group;
}

inline Group* X11Client::group()
{
    return in_group;
}

inline bool X11Client::isShown(bool shaded_is_shown) const
{
    return !isMinimized() && (!isShade() || shaded_is_shown) && !hidden;
}

inline bool X11Client::isHiddenInternal() const
{
    return hidden;
}

inline MaximizeMode X11Client::maximizeMode() const
{
    return max_mode;
}

inline bool X11Client::isFullScreen() const
{
    return m_fullscreenMode != FullScreenNone;
}

inline bool X11Client::hasNETSupport() const
{
    return info->hasNETSupport();
}

inline xcb_colormap_t X11Client::colormap() const
{
    return m_colormap;
}

inline int X11Client::sessionStackingOrder() const
{
    return sm_stacking_order;
}

inline bool X11Client::isManaged() const
{
    return m_managed;
}

inline void X11Client::plainResize(const QSize& s, ForceGeometry_t force)
{
    plainResize(s.width(), s.height(), force);
}

inline void X11Client::resizeWithChecks(const QSize &s, AbstractClient::ForceGeometry_t force)
{
    resizeWithChecks(s.width(), s.height(), XCB_GRAVITY_BIT_FORGET, force);
}

inline void X11Client::resizeWithChecks(const QSize& s, xcb_gravity_t gravity, ForceGeometry_t force)
{
    resizeWithChecks(s.width(), s.height(), gravity, force);
}

inline bool X11Client::hasUserTimeSupport() const
{
    return info->userTime() != -1U;
}

inline xcb_window_t X11Client::moveResizeGrabWindow() const
{
    return m_moveResizeGrabWindow;
}

inline bool X11Client::hiddenPreview() const
{
    return mapping_state == Kept;
}

} // namespace
Q_DECLARE_METATYPE(KWin::X11Client *)
Q_DECLARE_METATYPE(QList<KWin::X11Client *>)
