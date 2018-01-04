/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_SHELL_CLIENT_H
#define KWIN_SHELL_CLIENT_H

#include "abstract_client.h"
#include <KWayland/Server/xdgshell_interface.h>

namespace KWayland
{
namespace Server
{
class ShellSurfaceInterface;
class ServerSideDecorationInterface;
class ServerSideDecorationPaletteInterface;
class AppMenuInterface;
class PlasmaShellSurfaceInterface;
class QtExtendedSurfaceInterface;
}
}

namespace KWin
{

/**
 * @brief The reason for which the server pinged a client surface
 */
enum class PingReason {
    CloseWindow = 0,
    FocusWindow
};
    
class KWIN_EXPORT ShellClient : public AbstractClient
{
    Q_OBJECT
public:
    ShellClient(KWayland::Server::ShellSurfaceInterface *surface);
    ShellClient(KWayland::Server::XdgShellSurfaceInterface *surface);
    ShellClient(KWayland::Server::XdgShellPopupInterface *surface);
    virtual ~ShellClient();

    bool eventFilter(QObject *watched, QEvent *event) override;

    QStringList activities() const override;
    QPoint clientContentPos() const override;
    QSize clientSize() const override;
    QRect transparentRect() const override;
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    void debug(QDebug &stream) const override;
    double opacity() const override;
    void setOpacity(double opacity) override;
    QByteArray windowRole() const override;

    KWayland::Server::ShellSurfaceInterface *shellSurface() const {
        return m_shellSurface;
    }

    void blockActivityUpdates(bool b = true) override;
    QString captionNormal() const override {
        return m_caption;
    }
    QString captionSuffix() const override {
        return m_captionSuffix;
    }
    void closeWindow() override;
    AbstractClient *findModal(bool allow_itself = false) override;
    bool isCloseable() const override;
    bool isFullScreen() const override;
    bool isMaximizable() const override;
    bool isMinimizable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;
    bool isShown(bool shaded_is_shown) const override;
    bool isHiddenInternal() const override {
        return m_unmapped || m_hidden;
    }
    void hideClient(bool hide) override;
    MaximizeMode maximizeMode() const override;
    QRect geometryRestore() const override {
        return m_geomMaximizeRestore;
    }
    bool noBorder() const override;
    void setFullScreen(bool set, bool user = true) override;
    void setNoBorder(bool set) override;
    void updateDecoration(bool check_workspace_pos, bool force = false) override;
    void setOnAllActivities(bool set) override;
    void takeFocus() override;
    bool userCanSetFullScreen() const override;
    bool userCanSetNoBorder() const override;
    bool wantsInput() const override;
    bool dockWantsInput() const override;
    using AbstractClient::resizeWithChecks;
    void resizeWithChecks(int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    using AbstractClient::setGeometry;
    void setGeometry(int x, int y, int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    bool hasStrut() const override;

    void setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo) override;

    quint32 windowId() const override {
        return m_windowId;
    }

    /**
     * The process for this client.
     * Note that processes started by kwin will share its process id.
     * @since 5.11
     * @returns the process if for this client.
     **/
    pid_t pid() const override;

    bool isInternal() const;
    bool isLockScreen() const override;
    bool isInputMethod() const override;
    QWindow *internalWindow() const {
        return m_internalWindow;
    }

    void installPlasmaShellSurface(KWayland::Server::PlasmaShellSurfaceInterface *surface);
    void installQtExtendedSurface(KWayland::Server::QtExtendedSurfaceInterface *surface);
    void installServerSideDecoration(KWayland::Server::ServerSideDecorationInterface *decoration);
    void installAppMenu(KWayland::Server::AppMenuInterface *appmenu);
    void installPalette(KWayland::Server::ServerSideDecorationPaletteInterface *palette);

    bool isInitialPositionSet() const override;

    bool isTransient() const override;
    bool hasTransientPlacementHint() const override;
    QPoint transientPlacementHint() const override;

    QMatrix4x4 inputTransformation() const override;

    bool setupCompositing() override;
    void finishCompositing(ReleaseReason releaseReason = ReleaseReason::Release) override;

    void showOnScreenEdge() override;

    void killWindow() override;

    // TODO: const-ref
    void placeIn(QRect &area);

    bool hasPopupGrab() const override;
    void popupDone() override;

    void updateColorScheme() override;

protected:
    void addDamage(const QRegion &damage) override;
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const override;
    void doSetActive() override;
    Layer layerForDock() const override;
    void changeMaximize(bool horizontal, bool vertical, bool adjust) override;
    void setGeometryRestore(const QRect &geo) override {
        m_geomMaximizeRestore = geo;
    }
    void doResizeSync() override;
    bool isWaitingForMoveResizeSync() const override;
    bool acceptsFocus() const override;
    void doMinimize() override;
    void doMove(int x, int y) override;
    void updateCaption() override;

private Q_SLOTS:
    void clientFullScreenChanged(bool fullScreen);

private:
    void init();
    template <class T>
    void initSurface(T *shellSurface);
    void requestGeometry(const QRect &rect);
    void doSetGeometry(const QRect &rect);
    void createDecoration(const QRect &oldgeom);
    void destroyClient();
    void unmap();
    void createWindowId();
    void findInternalWindow();
    void updateInternalWindowGeometry();
    void syncGeometryToInternalWindow();
    void updateIcon();
    void markAsMapped();
    void setTransient();
    bool shouldExposeToWindowManagement();
    void updateClientOutputs();
    KWayland::Server::XdgShellSurfaceInterface::States xdgSurfaceStates() const;
    void updateShowOnScreenEdge();
    static void deleteClient(ShellClient *c);

    KWayland::Server::ShellSurfaceInterface *m_shellSurface;
    KWayland::Server::XdgShellSurfaceInterface *m_xdgShellSurface;
    KWayland::Server::XdgShellPopupInterface *m_xdgShellPopup;
    QSize m_clientSize;

    ClearablePoint m_positionAfterResize; // co-ordinates saved from a requestGeometry call, real geometry will be updated after the next damage event when the client has resized
    QRect m_geomFsRestore; //size and position of the window before it was set to fullscreen
    bool m_closing = false;
    quint32 m_windowId = 0;
    QWindow *m_internalWindow = nullptr;
    Qt::WindowFlags m_internalWindowFlags = Qt::WindowFlags();
    bool m_unmapped = true;
    MaximizeMode m_maximizeMode = MaximizeRestore;
    QRect m_geomMaximizeRestore; // size and position of the window before it was set to maximize
    NET::WindowType m_windowType = NET::Normal;
    QPointer<KWayland::Server::PlasmaShellSurfaceInterface> m_plasmaShellSurface;
    QPointer<KWayland::Server::QtExtendedSurfaceInterface> m_qtExtendedSurface;
    QPointer<KWayland::Server::AppMenuInterface> m_appMenuInterface;
    QPointer<KWayland::Server::ServerSideDecorationPaletteInterface> m_paletteInterface;
    KWayland::Server::ServerSideDecorationInterface *m_serverDecoration = nullptr;
    bool m_userNoBorder = false;
    bool m_fullScreen = false;
    bool m_transient = false;
    bool m_hidden = false;
    bool m_internal;
    bool m_hasPopupGrab = false;
    qreal m_opacity = 1.0;

    class RequestGeometryBlocker {
    public:
        RequestGeometryBlocker(ShellClient *client)
            : m_client(client)
        {
            m_client->m_requestGeometryBlockCounter++;
        }
        ~RequestGeometryBlocker()
        {
            m_client->m_requestGeometryBlockCounter--;
            if (m_client->m_requestGeometryBlockCounter == 0) {
                if (m_client->m_blockedRequestGeometry.isValid()) {
                    m_client->requestGeometry(m_client->m_blockedRequestGeometry);
                }
            }
        }
    private:
        ShellClient *m_client;
    };
    friend class RequestGeometryBlocker;
    int m_requestGeometryBlockCounter = 0;
    QRect m_blockedRequestGeometry;
    QString m_caption;
    QString m_captionSuffix;
    QHash<qint32, PingReason> m_pingSerials;

    bool m_compositingSetup = false;
};

}

Q_DECLARE_METATYPE(KWin::ShellClient*)

#endif
