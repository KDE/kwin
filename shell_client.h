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

namespace KWayland
{
namespace Server
{
class ShellSurfaceInterface;
}
}

namespace KWin
{

class ShellClient : public AbstractClient
{
    Q_OBJECT
public:
    ShellClient(KWayland::Server::ShellSurfaceInterface *surface);
    virtual ~ShellClient();

    QStringList activities() const override;
    QPoint clientPos() const override;
    QSize clientSize() const override;
    Layer layer() const override;
    QRect transparentRect() const override;
    bool shouldUnredirect() const override;
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    void debug(QDebug &stream) const override;
    double opacity() const override;
    void setOpacity(double opacity) override;
    QByteArray windowRole() const override;

    KWayland::Server::ShellSurfaceInterface *shellSurface() const {
        return m_shellSurface;
    }

    void blockActivityUpdates(bool b = true) override;
    QString caption(bool full = true, bool stripped = false) const override;
    void checkWorkspacePosition(QRect oldGeometry = QRect(), int oldDesktop = -2) override;
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
    void maximize(MaximizeMode) override;
    MaximizeMode maximizeMode() const override;
    bool noBorder() const override;
    const WindowRules *rules() const override;
    bool performMouseCommand(Options::MouseCommand cmd, const QPoint &globalPos) override;
    void sendToScreen(int screen) override;
    void setFullScreen(bool set, bool user = true) override;
    void setNoBorder(bool set) override;
    void setOnAllActivities(bool set) override;
    void setQuickTileMode(QuickTileMode mode, bool keyboard = false) override;
    void setShortcut(const QString &cut) override;
    const QKeySequence &shortcut() const override;
    void takeFocus() override;
    void updateWindowRules(Rules::Types selection) override;
    bool userCanSetFullScreen() const override;
    bool userCanSetNoBorder() const override;
    bool wantsInput() const override;

    quint32 windowId() const {
        return m_windowId;
    }

protected:
    void addDamage(const QRegion &damage) override;
    bool belongsToSameApplication(const AbstractClient *other, bool active_hack) const override;

private:
    void setGeometry(const QRect &rect);
    void destroyClient();
    void createWindowId();
    void findInternalWindow();
    void updateInternalWindowGeometry();
    static void deleteClient(ShellClient *c);

    KWayland::Server::ShellSurfaceInterface *m_shellSurface;
    QSize m_clientSize;
    bool m_closing = false;
    quint32 m_windowId = 0;
    QWindow *m_internalWindow = nullptr;
};

}

#endif
