/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

class QSize;

namespace KWaylandServer
{
class Display;
class OutputInterface;
class PlasmaWindowActivationFeedbackInterfacePrivate;
class PlasmaWindowInterface;
class PlasmaVirtualDesktopManagementInterface;
class PlasmaWindowActivationInterfacePrivate;
class PlasmaWindowManagementInterfacePrivate;
class PlasmaWindowInterfacePrivate;
class SurfaceInterface;

class KWIN_EXPORT PlasmaWindowActivationInterface
{
public:
    ~PlasmaWindowActivationInterface();

    void sendAppId(const QString &id);

private:
    friend class PlasmaWindowActivationFeedbackInterface;
    explicit PlasmaWindowActivationInterface();

    std::unique_ptr<PlasmaWindowActivationInterfacePrivate> d;
};

class KWIN_EXPORT PlasmaWindowActivationFeedbackInterface : public QObject
{
    Q_OBJECT

public:
    explicit PlasmaWindowActivationFeedbackInterface(Display *display, QObject *parent = nullptr);
    ~PlasmaWindowActivationFeedbackInterface() override;

    /**
     * Notify about a new application with @p app_id being started
     *
     * @returns an instance of @class PlasmaWindowActivationInterface to
     * be destroyed as the activation process ends.
     */
    std::unique_ptr<PlasmaWindowActivationInterface> createActivation(const QString &app_id);

private:
    std::unique_ptr<PlasmaWindowActivationFeedbackInterfacePrivate> d;
};

class KWIN_EXPORT PlasmaWindowManagementInterface : public QObject
{
    Q_OBJECT

public:
    explicit PlasmaWindowManagementInterface(Display *display, QObject *parent = nullptr);
    ~PlasmaWindowManagementInterface() override;
    enum class ShowingDesktopState {
        Disabled,
        Enabled,
    };
    void setShowingDesktopState(ShowingDesktopState state);

    PlasmaWindowInterface *createWindow(QObject *parent, const QUuid &uuid);
    QList<PlasmaWindowInterface *> windows() const;

    /**
     * Associate a PlasmaVirtualDesktopManagementInterface to this window management.
     * It's necessary to associate one in orderto use the plasma virtual desktop features
     * of PlasmaWindowInterface, as a window must know what are the deasktops available
     */
    void setPlasmaVirtualDesktopManagementInterface(PlasmaVirtualDesktopManagementInterface *manager);

    /**
     * @returns the PlasmaVirtualDesktopManagementInterface associated to this PlasmaWindowManagementInterface
     */
    PlasmaVirtualDesktopManagementInterface *plasmaVirtualDesktopManagementInterface() const;

    /**
     * Associate stacking order to this window management
     */
    void setStackingOrder(const QVector<quint32> &stackingOrder);

    void setStackingOrderUuids(const QVector<QString> &stackingOrderUuids);

Q_SIGNALS:
    void requestChangeShowingDesktop(ShowingDesktopState requestedState);

private:
    std::unique_ptr<PlasmaWindowManagementInterfacePrivate> d;
};

/**
 * @todo Add documentation
 */
class KWIN_EXPORT PlasmaWindowInterface : public QObject
{
    Q_OBJECT
public:
    ~PlasmaWindowInterface() override;

    void setTitle(const QString &title);
    void setAppId(const QString &appId);
    void setPid(quint32 pid);
    void setActive(bool set);
    void setMinimized(bool set);
    void setMaximized(bool set);
    void setFullscreen(bool set);
    void setKeepAbove(bool set);
    void setKeepBelow(bool set);
    void setOnAllDesktops(bool set);
    void setDemandsAttention(bool set);
    void setCloseable(bool set);
    void setMinimizeable(bool set);
    void setMaximizeable(bool set);
    void setFullscreenable(bool set);
    void setSkipTaskbar(bool skip);
    void setSkipSwitcher(bool skip);
    void setShadeable(bool set);
    void setShaded(bool set);
    void setMovable(bool set);
    void setResizable(bool set);
    void setResourceName(const QString &resourceName);
    /**
     * FIXME: still relevant with new desktops?
     */
    void setVirtualDesktopChangeable(bool set);

    /**
     * This method removes the Window and the Client is supposed to release the resource
     * bound for this Window.
     *
     * No more events should be sent afterwards.
     */
    void unmap();

    /**
     * @returns Geometries of the taskbar entries, indicized by the
     *          surface of the panels
     */
    QHash<SurfaceInterface *, QRect> minimizedGeometries() const;

    /**
     * Sets this PlasmaWindowInterface as a transient window to @p parentWindow.
     * If @p parentWindow is @c nullptr, the PlasmaWindowInterface is a toplevel
     * window and does not have a parent window.
     */
    void setParentWindow(PlasmaWindowInterface *parentWindow);

    /**
     * Sets the window @p geometry of this PlasmaWindow.
     *
     * @param geometry The geometry in absolute coordinates
     */
    void setGeometry(const QRect &geometry);

    /**
     * Set the icon of the PlasmaWindowInterface.
     *
     * In case the icon has a themed name, only the name is sent to the client.
     * Otherwise the client is only informed that there is an icon and the client
     * can request the icon in an asynchronous way by passing a file descriptor
     * into which the icon will be serialized.
     *
     * @param icon The new icon
     */
    void setIcon(const QIcon &icon);

    /**
     * Adds a new desktop to this window: a window can be on
     * an arbitrary subset of virtual desktops.
     * If it's on none it will be considered on all desktops.
     */
    void addPlasmaVirtualDesktop(const QString &id);

    /**
     * Removes a visrtual desktop from a window
     */
    void removePlasmaVirtualDesktop(const QString &id);

    /**
     * The ids of all the desktops currently associated with this window.
     * When a desktop is deleted it will be automatically removed from this list
     */
    QStringList plasmaVirtualDesktops() const;

    /**
     * Adds an activity to this window: a window can be on
     * an arbitrary subset of activities.
     * If it's on none it will be considered on all activities.
     */
    void addPlasmaActivity(const QString &id);

    /**
     * Removes an activity from a window
     */
    void removePlasmaActivity(const QString &id);

    /**
     * The ids of all the activities currently associated with this window.
     * When an activity is deleted it will be automatically removed from this list
     */
    QStringList plasmaActivities() const;

    /**
     * Set the application menu D-BUS service name and object path for the window.
     */
    void setApplicationMenuPaths(const QString &serviceName, const QString &objectPath);

    /**
     * Return the window internal id
     */
    quint32 internalId() const;

    /**
     * @return a unique string that identifies this window
     */
    QString uuid() const;

Q_SIGNALS:
    void closeRequested();
    void moveRequested();
    void resizeRequested();
    void activeRequested(bool set);
    void minimizedRequested(bool set);
    void maximizedRequested(bool set);
    void fullscreenRequested(bool set);
    void keepAboveRequested(bool set);
    void keepBelowRequested(bool set);
    void demandsAttentionRequested(bool set);
    void closeableRequested(bool set);
    void minimizeableRequested(bool set);
    void maximizeableRequested(bool set);
    void fullscreenableRequested(bool set);
    void skipTaskbarRequested(bool set);
    void skipSwitcherRequested(bool set);
    QRect minimizedGeometriesChanged();
    void shadeableRequested(bool set);
    void shadedRequested(bool set);
    void movableRequested(bool set);
    void resizableRequested(bool set);
    /**
     * FIXME: still relevant with new virtual desktops?
     */
    void virtualDesktopChangeableRequested(bool set);

    /**
     * Emitted when the client wishes this window to enter in a new virtual desktop.
     * The server will decide whether to consent this request
     */
    void enterPlasmaVirtualDesktopRequested(const QString &desktop);

    /**
     * Emitted when the client wishes this window to enter in
     * a new virtual desktop to be created for it.
     * The server will decide whether to consent this request
     */
    void enterNewPlasmaVirtualDesktopRequested();

    /**
     * Emitted when the client wishes to remove this window from a virtual desktop.
     * The server will decide whether to consent this request
     */
    void leavePlasmaVirtualDesktopRequested(const QString &desktop);

    /**
     * Emitted when the client wishes this window to enter an activity.
     * The server will decide whether to consent this request
     */
    void enterPlasmaActivityRequested(const QString &activity);

    /**
     * Emitted when the client wishes to remove this window from an activity.
     * The server will decide whether to consent this request
     */
    void leavePlasmaActivityRequested(const QString &activity);

    /**
     * Requests sending the window to @p output
     */
    void sendToOutput(KWaylandServer::OutputInterface *output);

private:
    friend class PlasmaWindowManagementInterface;
    friend class PlasmaWindowInterfacePrivate;
    friend class PlasmaWindowManagementInterfacePrivate;
    explicit PlasmaWindowInterface(PlasmaWindowManagementInterface *wm, QObject *parent);

    std::unique_ptr<PlasmaWindowInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::PlasmaWindowManagementInterface::ShowingDesktopState)
