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
#include "internal_client.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>
#include <KWayland/Server/surface_interface.h>

#include <QOpenGLFramebufferObject>

Q_DECLARE_METATYPE(NET::WindowType)

static const QByteArray s_skipClosePropertyName = QByteArrayLiteral("KWIN_SKIP_CLOSE_ANIMATION");

namespace KWin
{

InternalClient::InternalClient(KWayland::Server::ShellSurfaceInterface *surface)
    : ShellClient(surface)
{
    findInternalWindow();
    updateInternalWindowGeometry();
    updateDecoration(true);
}

InternalClient::InternalClient(KWayland::Server::XdgShellSurfaceInterface *surface)
    : ShellClient(surface)
{
}

InternalClient::InternalClient(KWayland::Server::XdgShellPopupInterface *surface)
    : ShellClient(surface)
{
}

InternalClient::~InternalClient() = default;

void InternalClient::findInternalWindow()
{
    const QWindowList windows = kwinApp()->topLevelWindows();
    for (QWindow *w: windows) {
        auto s = KWayland::Client::Surface::fromWindow(w);
        if (!s) {
            continue;
        }
        if (s->id() != surface()->id()) {
            continue;
        }
        m_internalWindow = w;
        m_windowId = m_internalWindow->winId();
        m_internalWindowFlags = m_internalWindow->flags();
        connect(m_internalWindow, &QWindow::xChanged, this, &InternalClient::updateInternalWindowGeometry);
        connect(m_internalWindow, &QWindow::yChanged, this, &InternalClient::updateInternalWindowGeometry);
        connect(m_internalWindow, &QWindow::destroyed, this, [this] { m_internalWindow = nullptr; });
        connect(m_internalWindow, &QWindow::opacityChanged, this, &InternalClient::setOpacity);

        const QVariant windowType = m_internalWindow->property("kwin_windowType");
        if (!windowType.isNull()) {
            m_windowType = windowType.value<NET::WindowType>();
        }
        setOpacity(m_internalWindow->opacity());

        // skip close animation support
        setSkipCloseAnimation(m_internalWindow->property(s_skipClosePropertyName).toBool());
        m_internalWindow->installEventFilter(this);
        return;
    }
}

bool InternalClient::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_internalWindow && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (pe->propertyName() == s_skipClosePropertyName) {
            setSkipCloseAnimation(m_internalWindow->property(s_skipClosePropertyName).toBool());
        }
        if (pe->propertyName() == "kwin_windowType") {
            m_windowType = m_internalWindow->property("kwin_windowType").value<NET::WindowType>();
            workspace()->updateClientArea();
        }
    }
    return false;
}

NET::WindowType InternalClient::windowType(bool direct, int supported_types) const
{
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return m_windowType;
}

void InternalClient::killWindow()
{
    // we don't kill our internal windows
}

bool InternalClient::isPopupWindow() const
{
    if (Toplevel::isPopupWindow()) {
        return true;
    }
    return m_internalWindowFlags.testFlag(Qt::Popup);
}

void InternalClient::setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo)
{
    if (fbo.isNull()) {
        unmap();
        return;
    }

    setClientSize(fbo->size() / surface()->scale());
    markAsMapped();
    doSetGeometry(QRect(geom.topLeft(), clientSize()));
    Toplevel::setInternalFramebufferObject(fbo);
    Toplevel::addDamage(QRegion(0, 0, width(), height()));
}

void InternalClient::closeWindow()
{
    if (m_internalWindow) {
        m_internalWindow->hide();
    }
}

bool InternalClient::isCloseable() const
{
    return true;
}

bool InternalClient::isMaximizable() const
{
    return false;
}

bool InternalClient::isMinimizable() const
{
    return false;
}

bool InternalClient::isMovable() const
{
    return true;
}

bool InternalClient::isMovableAcrossScreens() const
{
    return true;
}

bool InternalClient::isResizable() const
{
    return true;
}

bool InternalClient::noBorder() const
{
    return m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalClient::userCanSetNoBorder() const
{
    return !m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalClient::wantsInput() const
{
    return false;
}

bool InternalClient::acceptsFocus() const
{
    return false;
}

bool InternalClient::isInternal() const
{
    return true;
}

bool InternalClient::isLockScreen() const
{
    if (m_internalWindow) {
        return m_internalWindow->property("org_kde_ksld_emergency").toBool();
    }
    return false;
}

bool InternalClient::isInputMethod() const
{
    if (m_internalWindow) {
        return m_internalWindow->property("__kwin_input_method").toBool();
    }
    return false;
}

bool InternalClient::isOutline() const
{
    if (m_internalWindow) {
        return m_internalWindow->property("__kwin_outline").toBool();
    }
    return false;
}

quint32 InternalClient::windowId() const
{
    return m_windowId;
}

void InternalClient::updateInternalWindowGeometry()
{
    if (!m_internalWindow) {
        return;
    }
    doSetGeometry(QRect(m_internalWindow->geometry().topLeft() - QPoint(borderLeft(), borderTop()),
                        m_internalWindow->geometry().size() + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
}

bool InternalClient::requestGeometry(const QRect &rect)
{
    if (!ShellClient::requestGeometry(rect)) {
        return false;
    }
    if (m_internalWindow) {
        m_internalWindow->setGeometry(QRect(rect.topLeft() + QPoint(borderLeft(), borderTop()), rect.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    }
    return true;
}

void InternalClient::doSetGeometry(const QRect &rect)
{
    if (geom == rect && pendingGeometryUpdate() == PendingGeometryNone) {
        return;
    }
    if (!isUnmapped()) {
        addWorkspaceRepaint(visibleRect());
    }
    geom = rect;

    if (isUnmapped() && geometryRestore().isEmpty() && !geom.isEmpty()) {
        // use first valid geometry as restore geometry
        setGeometryRestore(geom);
    }

    if (!isUnmapped()) {
        addWorkspaceRepaint(visibleRect());
    }
    syncGeometryToInternalWindow();
    if (hasStrut()) {
        workspace()->updateClientArea();
    }
    const auto old = geometryBeforeUpdateBlocking();
    updateGeometryBeforeUpdateBlocking();
    emit geometryShapeChanged(this, old);

    if (isResize()) {
        performMoveResize();
    }
}

void InternalClient::doMove(int x, int y)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    syncGeometryToInternalWindow();
}

void InternalClient::syncGeometryToInternalWindow()
{
    if (!m_internalWindow) {
        return;
    }
    const QRect windowRect = QRect(geom.topLeft() + QPoint(borderLeft(), borderTop()),
                                    geom.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
    if (m_internalWindow->geometry() != windowRect) {
        // delay to end of cycle to prevent freeze, see BUG 384441
        QTimer::singleShot(0, m_internalWindow, std::bind(static_cast<void (QWindow::*)(const QRect&)>(&QWindow::setGeometry), m_internalWindow, windowRect));
    }
}

void InternalClient::resizeWithChecks(int w, int h, ForceGeometry_t force)
{
    Q_UNUSED(force)
    if (!m_internalWindow) {
        return;
    }
    QRect area = workspace()->clientArea(WorkArea, this);
    // don't allow growing larger than workarea
    if (w > area.width()) {
        w = area.width();
    }
    if (h > area.height()) {
        h = area.height();
    }
    m_internalWindow->setGeometry(QRect(pos() + QPoint(borderLeft(), borderTop()), QSize(w, h) - QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
}

void InternalClient::doResizeSync()
{
    if (!m_internalWindow) {
        return;
    }
    const auto rect = moveResizeGeometry();
    m_internalWindow->setGeometry(QRect(rect.topLeft() + QPoint(borderLeft(), borderTop()), rect.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
}

QWindow *InternalClient::internalWindow() const
{
    return m_internalWindow;
}

}
