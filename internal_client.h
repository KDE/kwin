/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 Martin Flöser <mgraesslin@kde.org>

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
#pragma once

#include "shell_client.h"


namespace KWin
{

class KWIN_EXPORT InternalClient : public ShellClient
{
    Q_OBJECT
public:
    InternalClient(KWayland::Server::ShellSurfaceInterface *surface);
    // needed for template <class T> void WaylandServer::createSurface(T *surface)
    InternalClient(KWayland::Server::XdgShellSurfaceInterface *surface);
    // needed for template <class T> void WaylandServer::createSurface(T *surface)
    InternalClient(KWayland::Server::XdgShellPopupInterface *surface);
    ~InternalClient() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    void killWindow() override;
    bool isPopupWindow() const override;
    void setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo) override;
    void closeWindow() override;
    bool isCloseable() const override;
    bool isMaximizable() const override;
    bool isMinimizable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;
    bool noBorder() const override;
    bool userCanSetNoBorder() const override;
    bool wantsInput() const override;
    bool isInternal() const override;
    bool isLockScreen() const override;
    bool isInputMethod() const override;
    bool isOutline() const override;
    quint32 windowId() const override;
    using AbstractClient::resizeWithChecks;
    void resizeWithChecks(int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    QWindow *internalWindow() const override;
    bool supportsWindowRules() const override;

protected:
    bool acceptsFocus() const override;
    void doMove(int x, int y) override;
    void doResizeSync() override;
    bool requestGeometry(const QRect &rect) override;
    void doSetGeometry(const QRect &rect) override;

private:
    void findInternalWindow();
    void updateInternalWindowGeometry();
    void syncGeometryToInternalWindow();

    NET::WindowType m_windowType = NET::Normal;
    quint32 m_windowId = 0;
    QWindow *m_internalWindow = nullptr;
    Qt::WindowFlags m_internalWindowFlags = Qt::WindowFlags();
};

}
