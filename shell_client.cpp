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
#include "shell_client.h"
#include "deleted.h"
#include "wayland_server.h"
#include "virtualdesktops.h"

#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/buffer_interface.h>

using namespace KWayland::Server;

namespace KWin
{

ShellClient::ShellClient(ShellSurfaceInterface *surface)
    : AbstractClient()
    , m_shellSurface(surface)
{
    setSurface(surface->surface());
    setupCompositing();
    if (surface->surface()->buffer()) {
        setReadyForPainting();
        m_clientSize = surface->surface()->buffer()->size();
    } else {
        ready_for_painting = false;
    }
    setGeometry(QRect(QPoint(0, 0), m_clientSize));

    setDesktop(VirtualDesktopManager::self()->current());

    connect(surface->surface(), &SurfaceInterface::sizeChanged, this,
        [this] {
            m_clientSize = m_shellSurface->surface()->buffer()->size();
            setGeometry(QRect(QPoint(0, 0), m_clientSize));
        }
    );
    connect(surface, &ShellSurfaceInterface::destroyed, this, &ShellClient::destroyClient);
    connect(surface->surface(), &SurfaceInterface::unmapped, this, &ShellClient::destroyClient);
}

ShellClient::~ShellClient() = default;

void ShellClient::destroyClient()
{
    m_closing = true;
    Deleted *del = Deleted::create(this);
    emit windowClosed(this, del);
    waylandServer()->removeClient(this);

    del->unrefWindow();
    m_shellSurface = nullptr;
    deleteClient(this);
}

void ShellClient::deleteClient(ShellClient *c)
{
    delete c;
}

QStringList ShellClient::activities() const
{
    // TODO: implement
    return QStringList();
}

QPoint ShellClient::clientPos() const
{
    return QPoint(0, 0);
}

QSize ShellClient::clientSize() const
{
    // TODO: connect for changes
    return m_clientSize;
}

void ShellClient::debug(QDebug &stream) const
{
    // TODO: implement
    Q_UNUSED(stream)
}

Layer ShellClient::layer() const
{
    // TODO: implement
    return KWin::NormalLayer;
}

bool ShellClient::shouldUnredirect() const
{
    // TODO: unredirect for fullscreen
    return false;
}

QRect ShellClient::transparentRect() const
{
    // TODO: implement
    return QRect();
}

NET::WindowType ShellClient::windowType(bool direct, int supported_types) const
{
    // TODO: implement
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return NET::Normal;
}

double ShellClient::opacity() const
{
    return 1.0;
}

void ShellClient::setOpacity(double opacity)
{
    Q_UNUSED(opacity)
}

void ShellClient::addDamage(const QRegion &damage)
{
    setReadyForPainting();
    if (m_shellSurface->surface()->buffer()->size().isValid()) {
        m_clientSize = m_shellSurface->surface()->buffer()->size();
        setGeometry(QRect(QPoint(0, 0), m_clientSize));
    }
    Toplevel::addDamage(damage);
}

void ShellClient::setGeometry(const QRect &rect)
{
    if (geom == rect) {
        return;
    }
    const QRect old = geom;
    geom = rect;
    emit geometryChanged();
    emit geometryShapeChanged(this, old);
}

QByteArray ShellClient::windowRole() const
{
    return QByteArray();
}

bool ShellClient::belongsToSameApplication(const AbstractClient *other, bool active_hack) const
{
    Q_UNUSED(active_hack)
    if (auto s = other->surface()) {
        return s->client() == surface()->client();
    }
    return false;
}

void ShellClient::blockActivityUpdates(bool b)
{
    Q_UNUSED(b)
}

QString ShellClient::caption(bool full, bool stripped) const
{
    Q_UNUSED(full)
    Q_UNUSED(stripped)
    return QString();
}

void ShellClient::checkWorkspacePosition(QRect oldGeometry, int oldDesktop)
{
    Q_UNUSED(oldGeometry)
    Q_UNUSED(oldDesktop)
}

void ShellClient::closeWindow()
{
}

AbstractClient *ShellClient::findModal(bool allow_itself)
{
    Q_UNUSED(allow_itself)
    return nullptr;
}

bool ShellClient::isCloseable() const
{
    return false;
}

bool ShellClient::isFullScreen() const
{
    return false;
}

bool ShellClient::isMaximizable() const
{
    return false;
}

bool ShellClient::isMinimizable() const
{
    return false;
}

bool ShellClient::isMovable() const
{
    return false;
}

bool ShellClient::isMovableAcrossScreens() const
{
    return false;
}

bool ShellClient::isResizable() const
{
    return false;
}

bool ShellClient::isShown(bool shaded_is_shown) const
{
    Q_UNUSED(shaded_is_shown)
    return !m_closing;
}

void ShellClient::maximize(MaximizeMode)
{
}

MaximizeMode ShellClient::maximizeMode() const
{
    return KWin::MaximizeRestore;
}

bool ShellClient::noBorder() const
{
    return true;
}

bool ShellClient::performMouseCommand(Options::MouseCommand cmd, const QPoint &globalPos)
{
    Q_UNUSED(cmd)
    Q_UNUSED(globalPos)
    return false;
}

const WindowRules *ShellClient::rules() const
{
    static WindowRules s_rules;
    return &s_rules;
}

void ShellClient::sendToScreen(int screen)
{
    Q_UNUSED(screen)
}

void ShellClient::setFullScreen(bool set, bool user)
{
    Q_UNUSED(set)
    Q_UNUSED(user)
}

void ShellClient::setNoBorder(bool set)
{
    Q_UNUSED(set)
}

void ShellClient::setOnAllActivities(bool set)
{
    Q_UNUSED(set)
}

void ShellClient::setQuickTileMode(AbstractClient::QuickTileMode mode, bool keyboard)
{
    Q_UNUSED(mode)
    Q_UNUSED(keyboard)
}

void ShellClient::setShortcut(const QString &cut)
{
    Q_UNUSED(cut)
}

const QKeySequence &ShellClient::shortcut() const
{
    static QKeySequence seq;
    return seq;
}

void ShellClient::takeFocus()
{
    setActive(true);
}

void ShellClient::updateWindowRules(Rules::Types selection)
{
    Q_UNUSED(selection)
}

bool ShellClient::userCanSetFullScreen() const
{
    return false;
}

bool ShellClient::userCanSetNoBorder() const
{
    return false;
}

bool ShellClient::wantsInput() const
{
    return true;
}

}
