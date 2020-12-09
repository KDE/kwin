/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_PLASMA_WINDOW_MANAGEMENT_INTERFACE_H
#define WAYLAND_SERVER_PLASMA_WINDOW_MANAGEMENT_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

class QSize;

namespace KWaylandServer
{

class Display;
class PlasmaWindowInterface;
class SurfaceInterface;
class PlasmaVirtualDesktopManagementInterface;
class PlasmaWindowManagementInterfacePrivate;
class PlasmaWindowInterfacePrivate;

/**
 * @todo Add documentation
 */
class KWAYLANDSERVER_EXPORT PlasmaWindowManagementInterface : public QObject
{
    Q_OBJECT

public:
    explicit PlasmaWindowManagementInterface(Display *display, QObject *parent = nullptr);
    ~PlasmaWindowManagementInterface() override;
    enum class ShowingDesktopState {
        Disabled,
        Enabled
    };
    void setShowingDesktopState(ShowingDesktopState state);

    PlasmaWindowInterface *createWindow(QObject *parent, const QUuid &uuid);
    QList<PlasmaWindowInterface*> windows() const;

    /**
     * Unmaps the @p window previously created with {@link createWindow}.
     * The window will be unmapped and removed from the list of {@link windows}.
     *
     * Unmapping a @p window indicates to the client that it should destroy the
     * resource created for the window. Once all resources for the @p window are
     * destroyed, the @p window will get deleted automatically. There is no need
     * to manually delete the @p window. A manual delete will trigger the unmap
     * and resource destroy at the same time and can result in protocol errors on
     * client side if it still accesses the resource before receiving the unmap event.
     *
     * @see createWindow
     * @see windows
     * @since 5.23
     **/
    void unmapWindow(PlasmaWindowInterface *window);

    /**
     * Associate a PlasmaVirtualDesktopManagementInterface to this window management.
     * It's necessary to associate one in orderto use the plasma virtual desktop features
     * of PlasmaWindowInterface, as a window must know what are the deasktops available
     * @since 5.48
     */
    void setPlasmaVirtualDesktopManagementInterface(PlasmaVirtualDesktopManagementInterface *manager);

    /**
     * @returns the PlasmaVirtualDesktopManagementInterface associated to this PlasmaWindowManagementInterface
     * @since 5.48
     */
    PlasmaVirtualDesktopManagementInterface *plasmaVirtualDesktopManagementInterface() const;


    /**
     * Associate stacking order to this window management
     * @since 5.70
     */
    void setStackingOrder(const QVector<quint32> &stackingOrder);

Q_SIGNALS:
    void requestChangeShowingDesktop(ShowingDesktopState requestedState);

private:
    QScopedPointer<PlasmaWindowManagementInterfacePrivate> d;
};

/**
 * @todo Add documentation
 */
class KWAYLANDSERVER_EXPORT PlasmaWindowInterface : public QObject
{
    Q_OBJECT
public:
    ~PlasmaWindowInterface() override;

    void setTitle(const QString &title);
    void setAppId(const QString &appId);
    void setPid(quint32 pid);
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 52)
    /**
     * @deprecated Since 5.52, use addPlasmaVirtualDesktop and removePlasmaVirtualDesktop
     */
    KWAYLANDSERVER_DEPRECATED_VERSION(5, 52, "Use PlasmaWindowManagementInterface::addPlasmaVirtualDesktop(const QString&) and PlasmaWindowManagementInterface::removePlasmaVirtualDesktop(const QString&)")
    void setVirtualDesktop(quint32 desktop);
#endif
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
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 28)
    /**
     * @deprecated since 5.28 use setIcon
     **/
    KWAYLANDSERVER_DEPRECATED_VERSION(5, 28, "Use PlasmaWindowManagementInterface::setIcon(const QIcon&)")
    void setThemedIconName(const QString &iconName);
#endif
    /**
     * @since 5.22
     */
    void setShadeable(bool set);
    /**
     * @since 5.22
     */
    void setShaded(bool set);
    /**
     * @since 5.22
     */
    void setMovable(bool set);
    /**
     * @since 5.22
     */
    void setResizable(bool set);
    /**
     * FIXME: still relevant with new desktops?
     * @since 5.22
     */
    void setVirtualDesktopChangeable(bool set);

    /**
     * This method removes the Window and the Client is supposed to release the resource
     * bound for this Window. Once all resources are released the Window gets deleted.
     *
     * Prefer using {@link PlasmaWindowManagementInterface::unmapWindow}.
     * @see PlasmaWindowManagementInterface::unmapWindow
     **/
    void unmap();

    /**
     * @returns Geometries of the taskbar entries, indicized by the
     *          surface of the panels
     * @since 5.5
     */
    QHash<SurfaceInterface*, QRect> minimizedGeometries() const;

    /**
     * Sets this PlasmaWindowInterface as a transient window to @p parentWindow.
     * If @p parentWindow is @c nullptr, the PlasmaWindowInterface is a toplevel
     * window and does not have a parent window.
     * @since 5.24
     **/
    void setParentWindow(PlasmaWindowInterface *parentWindow);

    /**
     * Sets the window @p geometry of this PlasmaWindow.
     *
     * @param geometry The geometry in absolute coordinates
     * @since 5.25
     **/
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
     * @since 5.28
     **/
    void setIcon(const QIcon &icon);

    /**
     * Adds a new desktop to this window: a window can be on
     * an arbitrary subset of virtual desktops.
     * If it's on none it will be considered on all desktops.
     *
     * @since 5.48
     */
    void addPlasmaVirtualDesktop(const QString &id);

    /**
     * Removes a visrtual desktop from a window
     *
     * @since 5.48
     */
    void removePlasmaVirtualDesktop(const QString &id);

    /**
     * The ids of all the desktops currently associated with this window.
     * When a desktop is deleted it will be automatically removed from this list
     *
     * @since 5.48
     */
    QStringList plasmaVirtualDesktops() const;

    /**
     * Set the application menu D-BUS service name and object path for the window.
     *
     * @since 5.69
     */
    void setApplicationMenuPaths(const QString &serviceName, const QString &objectPath);

    /**
     * Return the window internal id
     *
     * @since 5.70
     */
    quint32 internalId() const;

    /**
     * @return a unique string that identifies this window
     *
     * @since Plasma 5.20
     */
    QByteArray uuid() const;

Q_SIGNALS:
    void closeRequested();
    /**
     * @since 5.22
     */
    void moveRequested();
    /**
     * @since 5.22
     */
    void resizeRequested();
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 52)
    /**
     * @deprecated Since 5.52, use enterPlasmaVirtualDesktopRequested and leavePlasmaVirtualDesktopRequested instead
     */
    KWAYLANDSERVER_DEPRECATED_VERSION(5, 52, "Use PlasmaWindowManagementInterface::enterPlasmaVirtualDesktopRequested(const QString&) and PlasmaWindowManagementInterface::leavePlasmaVirtualDesktopRequested(const QString&)")
    void virtualDesktopRequested(quint32 desktop);
#endif
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
    /**
     * @since 5.22
     */
    void shadeableRequested(bool set);
    /**
     * @since 5.22
     */
    void shadedRequested(bool set);
    /**
     * @since 5.22
     */
    void movableRequested(bool set);
    /**
     * @since 5.22
     */
    void resizableRequested(bool set);
    /**
     * FIXME: still relevant with new virtual desktops?
     * @since 5.22
     */
    void virtualDesktopChangeableRequested(bool set);

    /**
     * Emitted when the client wishes this window to enter in a new virtual desktop.
     * The server will decide whether to consent this request
     * @since 5.52
     */
    void enterPlasmaVirtualDesktopRequested(const QString &desktop);

    /**
     * Emitted when the client wishes this window to enter in
     * a new virtual desktop to be created for it.
     * The server will decide whether to consent this request
     * @since 5.52
     */
    void enterNewPlasmaVirtualDesktopRequested();

    /**
     * Emitted when the client wishes to remove this window from a virtual desktop.
     * The server will decide whether to consent this request
     * @since 5.52
     */
    void leavePlasmaVirtualDesktopRequested(const QString &desktop);

private:
    friend class PlasmaWindowManagementInterface;
    friend class PlasmaWindowInterfacePrivate;
    friend class PlasmaWindowManagementInterfacePrivate;
    explicit PlasmaWindowInterface(PlasmaWindowManagementInterface *wm, QObject *parent);

    QScopedPointer<PlasmaWindowInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::PlasmaWindowManagementInterface::ShowingDesktopState)

#endif
