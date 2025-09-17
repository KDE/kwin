/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "config-kwin.h"

#if !KWIN_BUILD_X11
#error Do not include on non-X11 builds
#endif

// kwin
#include "utils/xcbutils.h"
#include "window.h"
// Qt
#include <QElapsedTimer>
#include <QFlags>
#include <QPixmap>
#include <QPointer>
// X
#include <NETWM>
#include <xcb/res.h>
#include <xcb/sync.h>

// TODO: Cleanup the order of things in this .h file

class QTimer;
class KStartupInfoData;
class KStartupInfoId;

namespace KWin
{

class KillPrompt;
class XwaylandSurfaceV1Interface;

class KWIN_EXPORT X11Window : public Window
{
    Q_OBJECT

public:
    explicit X11Window();
    ~X11Window() override; ///< Use destroyWindow() or releaseWindow()

    void associate(XwaylandSurfaceV1Interface *shellSurface);

    xcb_window_t window() const;

    int desktopId() const;
    QByteArray sessionId() const;
    xcb_window_t wmClientLeader() const;
    QString wmCommand();

    QPointF framePosToClientPos(const QPointF &point) const override;
    QPointF nextFramePosToClientPos(const QPointF &point) const override;
    QPointF clientPosToFramePos(const QPointF &point) const override;
    QPointF nextClientPosToFramePos(const QPointF &point) const override;
    QSizeF frameSizeToClientSize(const QSizeF &size) const override;
    QSizeF nextFrameSizeToClientSize(const QSizeF &size) const override;
    QSizeF clientSizeToFrameSize(const QSizeF &size) const override;
    QSizeF nextClientSizeToFrameSize(const QSizeF &size) const override;
    QRectF nextFrameRectToBufferRect(const QRectF &rect) const;

    void blockGeometryUpdates(bool block);
    void blockGeometryUpdates();
    void unblockGeometryUpdates();
    bool areGeometryUpdatesBlocked() const;

    xcb_visualid_t visual() const;
    int depth() const;
    bool hasAlpha() const;
    QRegion opaqueRegion() const;
    QList<QRectF> shapeRegion() const;

    pid_t pid() const override;
    QString windowRole() const override;

    bool isTransient() const override;
    bool groupTransient() const override;
    QList<Window *> mainWindows() const override; // Call once before loop , is not indirect
    bool hasTransient(const Window *c, bool indirect) const override;
    void checkTransient(xcb_window_t w);
    const Group *group() const override;
    Group *group() override;
    void checkGroup(Group *gr = nullptr, bool force = false);
    void changeClientLeaderGroup(Group *gr);
    bool supportsWindowRules() const override;
    void updateWindowRules(Rules::Types selection) override;
    void applyWindowRules() override;
    void updateFullscreenMonitors(NETFullscreenMonitors topology);

    bool hasNETSupport() const;

    QSizeF minSize() const override;
    QSizeF maxSize() const override;
    QSizeF basicUnit() const;

    bool windowEvent(xcb_generic_event_t *e);
    WindowType windowType() const override;

    bool track(xcb_window_t w);
    bool manage(xcb_window_t w, bool isMapped);

    void releaseWindow(bool on_shutdown = false);
    bool hasScheduledRelease() const;

    void destroyWindow() override;

    QStringList activities() const override;
    void doSetOnActivities(const QStringList &newActivitiesList) override;
    void updateActivities(bool includeTransients) override;

    bool isMaximizable() const override;
    MaximizeMode maximizeMode() const override;
    void maximize(MaximizeMode mode, const QRectF &restore = QRectF()) override;

    bool isMinimizable() const override;

    bool isFullScreenable() const override;
    void setFullScreen(bool set) override;
    bool isFullScreen() const override;
    int fullScreenMode() const
    {
        return m_fullscreenMode; // only for session saving
    }

    bool userNoBorder() const;
    bool noBorder() const override;
    void setNoBorder(bool set) override;
    bool userCanSetNoBorder() const override;
    void checkNoBorder() override;
    void checkActivities() override;

    int sessionStackingOrder() const;

    // Auxiliary functions, depend on the windowType
    bool wantsInput() const override;

    bool isResizable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isCloseable() const override; ///< May be closed by the user (May have a close button)

    bool takeFocus() override;

    void invalidateDecoration() override;


    /// resizeWithChecks() resizes according to gravity, and checks workarea position
    QRectF resizeWithChecks(const QRectF &geometry, const QSizeF &size) const override;
    QRectF resizeWithChecks(const QRectF &geometry, qreal w, qreal h, xcb_gravity_t gravity) const;
    QRectF resizeWithChecks(const QRectF &geometry, const QSizeF &s, xcb_gravity_t gravity) const;
    QSizeF constrainClientSize(const QSizeF &size, SizeMode mode = SizeModeAny) const override;

    bool providesContextHelp() const override;

    void updateVisibility();

    QString captionNormal() const override
    {
        return cap_normal;
    }
    QString captionSuffix() const override
    {
        return cap_suffix;
    }

    QPointF gravityAdjustment(xcb_gravity_t gravity) const;
    const QPointF calculateGravitation(bool invert) const;

    void NETMoveResize(qreal x_root, qreal y_root, NET::Direction direction, xcb_button_t button);
    void NETMoveResizeWindow(int flags, qreal x, qreal y, qreal width, qreal height);
    void GTKShowWindowMenu(qreal x_root, qreal y_root);
    void restackWindow(xcb_window_t above, int detail, NET::RequestSource source, xcb_timestamp_t timestamp);

    void gotPing(xcb_timestamp_t timestamp);

    void updateUserTime(xcb_timestamp_t time = XCB_TIME_CURRENT_TIME);
    xcb_timestamp_t userTime() const override;
    bool hasUserTimeSupport() const;

    /// Does 'delete c;'
    static void deleteClient(X11Window *c);

    static bool belongToSameApplication(const X11Window *c1, const X11Window *c2, SameApplicationChecks checks = SameApplicationChecks());
    static bool sameAppWindowRoleMatch(const X11Window *c1, const X11Window *c2, bool active_hack);

    void killWindow() override;
    void showContextHelp() override;
    void checkActiveModal();

    bool isClientSideDecorated() const;

    Xcb::StringProperty fetchPreferredColorScheme() const;
    QString readPreferredColorScheme(Xcb::StringProperty &property) const;
    QString preferredColorScheme() const override;

    // sets whether the client should be faked as being on all activities (and be shown during session save)
    void setSessionActivityOverride(bool needed);
    bool isClient() const override;
    bool isOutline() const override;
    bool isUnmanaged() const override;

    void cancelFocusOutTimer();

    Xcb::StringProperty fetchApplicationMenuServiceName() const;
    void readApplicationMenuServiceName(Xcb::StringProperty &property);
    void checkApplicationMenuServiceName();

    Xcb::StringProperty fetchApplicationMenuObjectPath() const;
    void readApplicationMenuObjectPath(Xcb::StringProperty &property);
    void checkApplicationMenuObjectPath();

    struct SyncRequest
    {
        xcb_sync_counter_t counter;
        xcb_sync_int64_t value;
        xcb_sync_alarm_t alarm;
        xcb_timestamp_t lastTimestamp;
        QTimer *timeout;
        bool enabled;
        bool pending;
        bool acked;
        bool interactiveResize;
    };
    const SyncRequest &syncRequest() const
    {
        return m_syncRequest;
    }
    void ackSync();
    void ackSyncTimeout();
    void finishSync();

    bool allowWindowActivation(xcb_timestamp_t time = -1U, bool focus_in = false);

    quint64 surfaceSerial() const;

    bool hitTest(const QPointF &point) const override;

public Q_SLOTS:
    void closeWindow() override;
    void updateCaption() override;

private:
    void mapRequestEvent(xcb_map_request_event_t *e);
    void unmapNotifyEvent(xcb_unmap_notify_event_t *e);
    void destroyNotifyEvent(xcb_destroy_notify_event_t *e);
    void configureNotifyEvent(xcb_configure_notify_event_t *e);
    void configureRequestEvent(xcb_configure_request_event_t *e);
    void propertyNotifyEvent(xcb_property_notify_event_t *e);
    void clientMessageEvent(xcb_client_message_event_t *e);
    void focusInEvent(xcb_focus_in_event_t *e);
    void focusOutEvent(xcb_focus_out_event_t *e);
    void shapeNotifyEvent(xcb_shape_notify_event_t *e);

protected:
    bool belongsToSameApplication(const Window *other, SameApplicationChecks checks) const override;
    void doSetActive() override;
    void doSetKeepAbove() override;
    void doSetKeepBelow() override;
    void doSetDesktop() override;
    void doMinimize() override;
    void doSetSkipPager() override;
    void doSetSkipTaskbar() override;
    void doSetSkipSwitcher() override;
    void doSetDemandsAttention() override;
    void doSetHidden() override;
    void doSetHiddenByShowDesktop() override;
    void doSetModal() override;
    bool belongsToDesktop() const override;
    bool isWaitingForInteractiveResizeSync() const override;
    void doInteractiveResizeSync(const QRectF &rect) override;
    QSizeF resizeIncrements() const override;
    bool acceptsFocus() const override;
    void doSetQuickTileMode() override;
    void moveResizeInternal(const QRectF &rect, MoveResizeMode mode) override;
    std::unique_ptr<WindowItem> createItem(Item *parentItem) override;
    void doSetNextTargetScale() override;
    void doSetSuspended() override;

Q_SIGNALS:
    void shapeChanged();

private:
    void exportMappingState(int s); // ICCCM 4.1.3.1, 4.1.4, NETWM 2.5.1
    bool isManaged() const; ///< Returns false if this client is not yet managed
    void updateAllowedActions(bool force = false);
    QRect fullscreenMonitorsArea(NETFullscreenMonitors topology) const;
    void getResourceClass();
    void getWmNormalHints();
    void getWmClientMachine();
    void getMotifHints();
    void getIcons();
    void getWmOpaqueRegion();
    void updateShapeRegion();
    void fetchName();
    void fetchIconicName();
    QString readName() const;
    void setCaption(const QString &s, bool force = false);
    bool hasTransientInternal(const X11Window *c, bool indirect, QList<const X11Window *> &set) const;
    void setShortcutInternal() override;
    Xcb::Property fetchWmClientLeader() const;
    void readWmClientLeader(Xcb::Property &p);
    void getWmClientLeader();
    Xcb::Property fetchSkipCloseAnimation() const;
    void readSkipCloseAnimation(Xcb::Property &prop);
    void getSkipCloseAnimation();

    void configureRequest(int value_mask, qreal rx, qreal ry, qreal rw, qreal rh, int gravity, bool from_tool);
    void getSyncCounter();
    void sendSyncRequest();

    void pingWindow();
    void killProcess(bool ask, xcb_timestamp_t timestamp = XCB_TIME_CURRENT_TIME);
    void updateUrgency();
    static void sendClientMessage(xcb_window_t w, xcb_atom_t a, xcb_atom_t protocol,
                                  uint32_t data1 = 0, uint32_t data2 = 0, uint32_t data3 = 0);

    void embedClient(xcb_window_t w, xcb_visualid_t visualid, xcb_colormap_t colormap, const QRect &nativeGeometry, uint8_t depth);
    void detectNoBorder();
    void updateFrameExtents();
    void setClientFrameExtents(const NETStrut &strut);

    void internalShow();
    void internalHide();
    void map();
    void unmap();

    void configure(const QRect &nativeGeometry);

    xcb_timestamp_t readUserTimeMapTimestamp(const KStartupInfoId *asn_id, const KStartupInfoData *asn_data,
                                             bool session) const;
    xcb_timestamp_t readUserCreationTime() const;
    void startupIdChanged();

    xcb_res_query_client_ids_cookie_t fetchPid() const;
    void readPid(xcb_res_query_client_ids_cookie_t cookie);

    void updateDecoration(bool check_workspace_pos, bool force = false);
    void createDecoration();
    void destroyDecoration();

    void checkOutput();
    void handleXwaylandScaleChanged();
    void handleCommitted();

    void setAllowCommits(bool allow);

    Xcb::Window m_client;
    qreal m_bufferScale = 1;
    xcb_window_t m_wmClientLeader = XCB_WINDOW_NONE;
    bool m_managed;

    Xcb::GeometryHints m_geometryHints;
    void sendSyntheticConfigureNotify();
    enum MappingState {
        Withdrawn, ///< Not handled, as per ICCCM WithdrawnState
        Mapped, ///< The frame is mapped
        Unmapped, ///< The frame is not mapped
    };
    MappingState mapping_state;

    Xcb::TransientFor fetchTransient() const;
    void readTransientProperty(Xcb::TransientFor &transientFor);
    void readTransient();
    xcb_window_t verifyTransientFor(xcb_window_t transient_for, bool set);
    void addTransient(Window *cl) override;
    void removeFromMainClients();
    void cleanGrouping();
    void checkGroupTransients();
    void setTransient(xcb_window_t new_transient_for_id);
    void setNetWmDesktop(VirtualDesktop *desktop);
    void updateNetWmDesktopId();

    NETWinInfo *info = nullptr;
    xcb_window_t m_transientForId;
    xcb_window_t m_originalTransientForId;
    Xcb::MotifHints m_motif;
    uint noborder : 1;
    uint app_noborder : 1; ///< App requested no border via window type, shape extension, etc.
    uint ignore_focus_stealing : 1; ///< Don't apply focus stealing prevention to this client
    bool is_shape = false;

    enum FullScreenMode {
        FullScreenNone,
        FullScreenNormal
    } m_fullscreenMode;

    MaximizeMode max_mode;
    QString cap_normal, cap_iconic, cap_suffix;
    Group *in_group;
    QTimer *ping_timer;
    std::unique_ptr<KillPrompt> m_killPrompt;
    xcb_timestamp_t m_pingTimestamp;
    xcb_timestamp_t m_userTime;
    pid_t m_pid = 0;
    NET::Actions allowed_actions;
    SyncRequest m_syncRequest;
    static bool check_active_modal; ///< \see X11Window::checkActiveModal()
    int sm_stacking_order;
    xcb_visualid_t m_visual = XCB_NONE;
    int bit_depth = 24;
    QRegion opaque_region;
    QList<QRectF> m_shapeRegion;
    friend struct ResetupRulesProcedure;

    friend bool performTransiencyCheck();

    Xcb::StringProperty fetchActivities() const;
    void readActivities(Xcb::StringProperty &property);
    bool activitiesDefined; // whether the x property was actually set

    bool sessionActivityOverride;

    QTimer *m_focusOutTimer;
    QTimer m_releaseTimer;
    QPointer<VirtualDesktop> m_netWmDesktop;

    QMetaObject::Connection m_edgeGeometryTrackingConnection;

    QMarginsF m_clientFrameExtents;
    int m_blockGeometryUpdates = 0; // > 0 = New geometry is remembered, but not actually set

    bool m_unmanaged = false;
    bool m_outline = false;
    quint64 m_surfaceSerial = 0;
    int m_inflightUnmaps = 0;
};

/**
 * Helper for X11Window::blockGeometryUpdates() being called in pairs (true/false)
 */
class X11GeometryUpdatesBlocker
{
public:
    explicit X11GeometryUpdatesBlocker(X11Window *c)
        : cl(c)
    {
        cl->blockGeometryUpdates(true);
    }
    ~X11GeometryUpdatesBlocker()
    {
        cl->blockGeometryUpdates(false);
    }

private:
    X11Window *cl;
};

inline xcb_visualid_t X11Window::visual() const
{
    return m_visual;
}

inline int X11Window::depth() const
{
    return bit_depth;
}

inline bool X11Window::hasAlpha() const
{
    return depth() == 32;
}

inline QRegion X11Window::opaqueRegion() const
{
    return opaque_region;
}

inline bool X11Window::isClientSideDecorated() const
{
    return !m_clientFrameExtents.isNull();
}

inline bool X11Window::groupTransient() const
{
    return m_transientForId == kwinApp()->x11RootWindow();
}

inline bool X11Window::isTransient() const
{
    return m_transientForId != XCB_WINDOW_NONE;
}

inline const Group *X11Window::group() const
{
    return in_group;
}

inline Group *X11Window::group()
{
    return in_group;
}

inline MaximizeMode X11Window::maximizeMode() const
{
    return max_mode;
}

inline bool X11Window::isFullScreen() const
{
    return m_fullscreenMode != FullScreenNone;
}

inline bool X11Window::hasNETSupport() const
{
    return info->hasNETSupport();
}

inline int X11Window::sessionStackingOrder() const
{
    return sm_stacking_order;
}

inline bool X11Window::isManaged() const
{
    return m_managed;
}

inline QRectF X11Window::resizeWithChecks(const QRectF &geometry, const QSizeF &s) const
{
    return resizeWithChecks(geometry, s.width(), s.height(), XCB_GRAVITY_BIT_FORGET);
}

inline QRectF X11Window::resizeWithChecks(const QRectF &geometry, const QSizeF &s, xcb_gravity_t gravity) const
{
    return resizeWithChecks(geometry, s.width(), s.height(), gravity);
}

inline bool X11Window::hasUserTimeSupport() const
{
    return info->userTime() != -1U;
}

inline quint64 X11Window::surfaceSerial() const
{
    return m_surfaceSerial;
}

inline bool X11Window::areGeometryUpdatesBlocked() const
{
    return m_blockGeometryUpdates != 0;
}

inline void X11Window::blockGeometryUpdates()
{
    m_blockGeometryUpdates++;
}

inline void X11Window::unblockGeometryUpdates()
{
    m_blockGeometryUpdates--;
}

} // namespace
Q_DECLARE_METATYPE(KWin::X11Window *)
Q_DECLARE_METATYPE(QList<KWin::X11Window *>)
