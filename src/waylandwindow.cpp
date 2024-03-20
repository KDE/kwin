/*
    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandwindow.h"
#include "core/pixelgrid.h"
#include "scene/windowitem.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QFileInfo>

#include <csignal>

#include <sys/types.h>
#include <unistd.h>

namespace KWin
{

enum WaylandGeometryType {
    WaylandGeometryClient = 0x1,
    WaylandGeometryFrame = 0x2,
    WaylandGeometryBuffer = 0x4,
};
Q_DECLARE_FLAGS(WaylandGeometryTypes, WaylandGeometryType)

WaylandWindow::WaylandWindow(SurfaceInterface *surface)
    : m_isScreenLocker(surface->client() == waylandServer()->screenLockerClientConnection())
{
    setSurface(surface);

    connect(surface, &SurfaceInterface::shadowChanged,
            this, &WaylandWindow::updateShadow);
    connect(this, &WaylandWindow::frameGeometryChanged,
            this, &WaylandWindow::updateClientOutputs);
    connect(workspace(), &Workspace::outputsChanged, this, &WaylandWindow::updateClientOutputs);

    updateResourceName();
    updateShadow();
}

std::unique_ptr<WindowItem> WaylandWindow::createItem(Item *parentItem)
{
    return std::make_unique<WindowItemWayland>(this, parentItem);
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
    return surface() ? surface()->client()->processId() : -1;
}

bool WaylandWindow::isClient() const
{
    return true;
}

bool WaylandWindow::isLockScreen() const
{
    return m_isScreenLocker;
}

bool WaylandWindow::isLocalhost() const
{
    return true;
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
    if (isDeleted()) {
        return;
    }
    const auto rect = frameGeometry().toAlignedRect();
    if (rect.isEmpty()) {
        return;
    }

    surface()->setOutputs(waylandServer()->display()->outputsIntersecting(rect),
                          waylandServer()->display()->largestIntersectingOutput(rect));
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
    const QString suffix = shortcutCaptionSuffix();
    if (m_captionSuffix != suffix) {
        m_captionSuffix = suffix;
        Q_EMIT captionChanged();
    }
}

void WaylandWindow::setCaption(const QString &caption)
{
    const QString simplified = caption.simplified();
    if (m_captionNormal != simplified) {
        m_captionNormal = simplified;
        Q_EMIT captionNormalChanged();
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

void WaylandWindow::cleanGrouping()
{
    // We want to break parent-child relationships, but preserve stacking
    // order constraints at the same time for window closing animations.

    if (transientFor()) {
        transientFor()->removeTransientFromList(this);
        setTransientFor(nullptr);
    }

    const auto children = transients();
    for (Window *transient : children) {
        removeTransientFromList(transient);
        transient->setTransientFor(nullptr);
    }
}

QRectF WaylandWindow::frameRectToBufferRect(const QRectF &rect) const
{
    return QRectF(rect.topLeft(), snapToPixels(surface()->size(), targetScale()));
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
        Q_EMIT bufferGeometryChanged(oldBufferGeometry);
    }
    if (changedGeometries & WaylandGeometryClient) {
        Q_EMIT clientGeometryChanged(oldClientGeometry);
    }
    if (changedGeometries & WaylandGeometryFrame) {
        Q_EMIT frameGeometryChanged(oldFrameGeometry);
    }
    if (oldOutput != m_output) {
        Q_EMIT outputChanged();
    }
}

void WaylandWindow::markAsMapped()
{
    if (Q_UNLIKELY(!ready_for_painting)) {
        setupCompositing();
        setReadyForPainting();
    }
}

} // namespace KWin

#include "moc_waylandwindow.cpp"
