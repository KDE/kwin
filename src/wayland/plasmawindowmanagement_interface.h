/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef WAYLAND_SERVER_PLASMA_WINDOW_MANAGEMENT_INTERFACE_H
#define WAYLAND_SERVER_PLASMA_WINDOW_MANAGEMENT_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

#include "global.h"
#include "resource.h"

class QSize;

namespace KWayland
{
namespace Server
{

class Display;
class PlasmaWindowInterface;
class SurfaceInterface;

/**
 * @todo Add documentation
 */
class KWAYLANDSERVER_EXPORT PlasmaWindowManagementInterface : public Global
{
    Q_OBJECT
public:
    virtual ~PlasmaWindowManagementInterface();
    enum class ShowingDesktopState {
        Disabled,
        Enabled
    };
    void setShowingDesktopState(ShowingDesktopState state);

    PlasmaWindowInterface *createWindow(QObject *parent);
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

Q_SIGNALS:
    void requestChangeShowingDesktop(ShowingDesktopState requestedState);

private:
    friend class Display;
    explicit PlasmaWindowManagementInterface(Display *display, QObject *parent);
    class Private;
    Private *d_func() const;
};

/**
 * @todo Add documentation
 */
class KWAYLANDSERVER_EXPORT PlasmaWindowInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~PlasmaWindowInterface();

    void setTitle(const QString &title);
    void setAppId(const QString &appId);
    void setPid(quint32 pid);
    void setVirtualDesktop(quint32 desktop);
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
    /**
     * @deprecated since 5.28 use setIcon
     * @see setIcon
     **/
#ifndef KWAYLANDSERVER_NO_DEPRECATED
    void KWAYLANDSERVER_DEPRECATED setThemedIconName(const QString &iconName);
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
    void virtualDesktopRequested(quint32 desktop);
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
     * @since 5.22
     */
    void virtualDesktopChangeableRequested(bool set);

private:
    friend class PlasmaWindowManagementInterface;
    explicit PlasmaWindowInterface(PlasmaWindowManagementInterface *wm, QObject *parent);

    class Private;
    const QScopedPointer<Private> d;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::PlasmaWindowManagementInterface::ShowingDesktopState)

#endif
