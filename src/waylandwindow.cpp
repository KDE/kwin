/*
    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandwindow.h"
#include "scene/windowitem.h"
#include "wayland/clientbuffer.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QFileInfo>

#include <csignal>

#include <sys/types.h>
#include <unistd.h>

using namespace KWaylandServer;

namespace KWin
{

enum WaylandGeometryType {
    WaylandGeometryClient = 0x1,
    WaylandGeometryFrame = 0x2,
    WaylandGeometryBuffer = 0x4,
};
Q_DECLARE_FLAGS(WaylandGeometryTypes, WaylandGeometryType)

WaylandWindow::WaylandWindow(SurfaceInterface *surface)
{
    setSurface(surface);
    setupCompositing();

    connect(surface, &SurfaceInterface::shadowChanged,
            this, &WaylandWindow::updateShadow);
    connect(this, &WaylandWindow::frameGeometryChanged,
            this, &WaylandWindow::updateClientOutputs);
    connect(this, &WaylandWindow::desktopFileNameChanged,
            this, &WaylandWindow::updateIcon);
    connect(workspace(), &Workspace::outputsChanged, this, &WaylandWindow::updateClientOutputs);
    connect(surface->client(), &ClientConnection::aboutToBeDestroyed,
            this, &WaylandWindow::destroyWindow);

    updateResourceName();
    updateIcon();
}

std::unique_ptr<WindowItem> WaylandWindow::createItem(Scene *scene)
{
    return std::make_unique<WindowItemWayland>(this, scene);
}

QString WaylandWindow::captionNormal() const
{
    return m_captionNormal;
}

QString WaylandWindow::captionSuffix() const
{
    return m_captionSuffix;
}

pid_t WaylandWindow::pid() const
{
    return surface()->client()->processId();
}

bool WaylandWindow::isClient() const
{
    return true;
}

bool WaylandWindow::isLockScreen() const
{
    return surface()->client() == waylandServer()->screenLockerClientConnection();
}

bool WaylandWindow::isLocalhost() const
{
    return true;
}

Window *WaylandWindow::findModal(bool allow_itself)
{
    return nullptr;
}

QRectF WaylandWindow::resizeWithChecks(const QRectF &geometry, const QSizeF &size)
{
    const QRectF area = workspace()->clientArea(WorkArea, this, geometry.center());

    qreal width = size.width();
    qreal height = size.height();

    // don't allow growing larger than workarea
    if (width > area.width()) {
        width = area.width();
    }
    if (height > area.height()) {
        height = area.height();
    }
    return QRectF(geometry.topLeft(), QSizeF(width, height));
}

void WaylandWindow::killWindow()
{
    if (!surface()) {
        return;
    }
    auto c = surface()->client();
    if (c->processId() == getpid() || c->processId() == 0) {
        c->destroy();
        return;
    }
    ::kill(c->processId(), SIGTERM);
    // give it time to terminate and only if terminate fails, try destroy Wayland connection
    QTimer::singleShot(5000, c, &ClientConnection::destroy);
}

QString WaylandWindow::windowRole() const
{
    return QString();
}

bool WaylandWindow::belongsToSameApplication(const Window *other, SameApplicationChecks checks) const
{
    if (checks.testFlag(SameApplicationCheck::AllowCrossProcesses)) {
        if (other->desktopFileName() == desktopFileName()) {
            return true;
        }
    }
    if (auto s = other->surface()) {
        return s->client() == surface()->client();
    }
    return false;
}

bool WaylandWindow::belongsToDesktop() const
{
    const auto clients = waylandServer()->windows();

    return std::any_of(clients.constBegin(), clients.constEnd(),
                       [this](const Window *client) {
                           if (belongsToSameApplication(client, SameApplicationChecks())) {
                               return client->isDesktop();
                           }
                           return false;
                       });
}

void WaylandWindow::updateClientOutputs()
{
    surface()->setOutputs(waylandServer()->display()->outputsIntersecting(frameGeometry().toAlignedRect()));
    if (output()) {
        surface()->setPreferredScale(output()->scale());
    }
}

void WaylandWindow::updateIcon()
{
    const QString waylandIconName = QStringLiteral("wayland");
    const QString dfIconName = iconFromDesktopFile();
    const QString iconName = dfIconName.isEmpty() ? waylandIconName : dfIconName;
    if (iconName == icon().name()) {
        return;
    }
    setIcon(QIcon::fromTheme(iconName));
}

void WaylandWindow::updateResourceName()
{
    const QFileInfo fileInfo(surface()->client()->executablePath());
    if (fileInfo.exists()) {
        const QByteArray executableFileName = fileInfo.fileName().toUtf8();
        setResourceClass(executableFileName, executableFileName);
    }
}

void WaylandWindow::updateCaption()
{
    const QString oldSuffix = m_captionSuffix;
    const auto shortcut = shortcutCaptionSuffix();
    m_captionSuffix = shortcut;
    if ((!isSpecialWindow() || isToolbar()) && findWindowWithSameCaption()) {
        int i = 2;
        do {
            m_captionSuffix = shortcut + QLatin1String(" <") + QString::number(i) + QLatin1Char('>');
            i++;
        } while (findWindowWithSameCaption());
    }
    if (m_captionSuffix != oldSuffix) {
        Q_EMIT captionChanged();
    }
}

void WaylandWindow::setCaption(const QString &caption)
{
    const QString oldSuffix = m_captionSuffix;
    m_captionNormal = caption.simplified();
    updateCaption();
    if (m_captionSuffix == oldSuffix) {
        // Don't emit caption change twice it already got emitted by the changing suffix.
        Q_EMIT captionChanged();
    }
}

void WaylandWindow::doSetActive()
{
    if (isActive()) { // TODO: Xwayland clients must be unfocused somewhere else.
        StackingUpdatesBlocker blocker(workspace());
        workspace()->focusToNull();
    }
}

void WaylandWindow::updateDepth()
{
    if (surface()->buffer()->hasAlphaChannel()) {
        setDepth(32);
    } else {
        setDepth(24);
    }
}

void WaylandWindow::cleanGrouping()
{
    if (transientFor()) {
        transientFor()->removeTransient(this);
    }
    for (auto it = transients().constBegin(); it != transients().constEnd();) {
        if ((*it)->transientFor() == this) {
            removeTransient(*it);
            it = transients().constBegin(); // restart, just in case something more has changed with the list
        } else {
            ++it;
        }
    }
}

bool WaylandWindow::isShown() const
{
    return !isZombie() && !isHidden() && !isMinimized();
}

bool WaylandWindow::isHiddenInternal() const
{
    return isHidden();
}

bool WaylandWindow::isHidden() const
{
    return m_isHidden;
}

void WaylandWindow::showClient()
{
    if (!isHidden()) {
        return;
    }
    m_isHidden = false;
    Q_EMIT windowShown(this);
}

void WaylandWindow::hideClient()
{
    if (isHidden()) {
        return;
    }
    if (isInteractiveMoveResize()) {
        leaveInteractiveMoveResize();
    }
    m_isHidden = true;
    workspace()->windowHidden(this);
    Q_EMIT windowHidden(this);
}

QRectF WaylandWindow::frameRectToBufferRect(const QRectF &rect) const
{
    return QRectF(rect.topLeft(), surface()->size());
}

void WaylandWindow::updateGeometry(const QRectF &rect)
{
    const QRectF oldClientGeometry = m_clientGeometry;
    const QRectF oldFrameGeometry = m_frameGeometry;
    const QRectF oldBufferGeometry = m_bufferGeometry;
    const Output *oldOutput = m_output;

    m_clientGeometry = frameRectToClientRect(rect);
    m_frameGeometry = rect;
    m_bufferGeometry = frameRectToBufferRect(rect);

    WaylandGeometryTypes changedGeometries;

    if (m_clientGeometry != oldClientGeometry) {
        changedGeometries |= WaylandGeometryClient;
    }
    if (m_frameGeometry != oldFrameGeometry) {
        changedGeometries |= WaylandGeometryFrame;
    }
    if (m_bufferGeometry != oldBufferGeometry) {
        changedGeometries |= WaylandGeometryBuffer;
    }

    if (!changedGeometries) {
        return;
    }

    m_output = workspace()->outputAt(rect.center());
    updateWindowRules(Rules::Position | Rules::Size);

    if (changedGeometries & WaylandGeometryBuffer) {
        Q_EMIT bufferGeometryChanged(this, oldBufferGeometry);
    }
    if (changedGeometries & WaylandGeometryClient) {
        Q_EMIT clientGeometryChanged(this, oldClientGeometry);
    }
    if (changedGeometries & WaylandGeometryFrame) {
        Q_EMIT frameGeometryChanged(this, oldFrameGeometry);
    }
    if (oldOutput != m_output) {
        Q_EMIT screenChanged();
    }
    Q_EMIT geometryShapeChanged(this, oldFrameGeometry);
}

} // namespace KWin
