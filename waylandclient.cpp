/*
    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandclient.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif

#include <KWaylandServer/display.h>
#include <KWaylandServer/clientconnection.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/buffer_interface.h>

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

WaylandClient::WaylandClient(SurfaceInterface *surface)
{
    setSurface(surface);
    setupCompositing();

    m_windowId = waylandServer()->createWindowId(surface);

    connect(surface, &SurfaceInterface::shadowChanged,
            this, &WaylandClient::updateShadow);
    connect(this, &WaylandClient::frameGeometryChanged,
            this, &WaylandClient::updateClientOutputs);
    connect(this, &WaylandClient::desktopFileNameChanged,
            this, &WaylandClient::updateIcon);
    connect(screens(), &Screens::changed, this,
            &WaylandClient::updateClientOutputs);

    updateResourceName();
    updateIcon();
}

QString WaylandClient::captionNormal() const
{
    return m_captionNormal;
}

QString WaylandClient::captionSuffix() const
{
    return m_captionSuffix;
}

QStringList WaylandClient::activities() const
{
    return QStringList();
}

void WaylandClient::setOnAllActivities(bool set)
{
    Q_UNUSED(set)
}

void WaylandClient::blockActivityUpdates(bool b)
{
    Q_UNUSED(b)
}

QPoint WaylandClient::clientContentPos() const
{
    return -clientPos();
}

QRect WaylandClient::transparentRect() const
{
    return QRect();
}

quint32 WaylandClient::windowId() const
{
    return m_windowId;
}

pid_t WaylandClient::pid() const
{
    return surface()->client()->processId();
}

bool WaylandClient::isLockScreen() const
{
    return surface()->client() == waylandServer()->screenLockerClientConnection();
}

bool WaylandClient::isLocalhost() const
{
    return true;
}

double WaylandClient::opacity() const
{
    return m_opacity;
}

void WaylandClient::setOpacity(double opacity)
{
    const qreal newOpacity = qBound(0.0, opacity, 1.0);
    if (newOpacity == m_opacity) {
        return;
    }
    const qreal oldOpacity = m_opacity;
    m_opacity = newOpacity;
    addRepaintFull();
    emit opacityChanged(this, oldOpacity);
}

AbstractClient *WaylandClient::findModal(bool allow_itself)
{
    Q_UNUSED(allow_itself)
    return nullptr;
}

void WaylandClient::resizeWithChecks(const QSize &size, ForceGeometry_t force)
{
    const QRect area = workspace()->clientArea(WorkArea, this);

    int width = size.width();
    int height = size.height();

    // don't allow growing larger than workarea
    if (width > area.width()) {
        width = area.width();
    }
    if (height > area.height()) {
        height = area.height();
    }
    setFrameGeometry(QRect(x(), y(), width, height), force);
}

void WaylandClient::killWindow()
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

QByteArray WaylandClient::windowRole() const
{
    return QByteArray();
}

bool WaylandClient::belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const
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

bool WaylandClient::belongsToDesktop() const
{
    const auto clients = waylandServer()->clients();

    return std::any_of(clients.constBegin(), clients.constEnd(),
        [this](const AbstractClient *client) {
            if (belongsToSameApplication(client, SameApplicationChecks())) {
                return client->isDesktop();
            }
            return false;
        }
    );
}

void WaylandClient::updateClientOutputs()
{
    QVector<OutputInterface *> clientOutputs;
    const auto outputs = waylandServer()->display()->outputs();
    for (const auto output : outputs) {
        if (output->isEnabled()) {
            const QRect outputGeometry(output->globalPosition(), output->pixelSize() / output->scale());
            if (frameGeometry().intersects(outputGeometry)) {
                clientOutputs << output;
            }
        }
    }
    surface()->setOutputs(clientOutputs);
}

void WaylandClient::updateIcon()
{
    const QString waylandIconName = QStringLiteral("wayland");
    const QString dfIconName = iconFromDesktopFile();
    const QString iconName = dfIconName.isEmpty() ? waylandIconName : dfIconName;
    if (iconName == icon().name()) {
        return;
    }
    setIcon(QIcon::fromTheme(iconName));
}

void WaylandClient::updateResourceName()
{
    const QFileInfo fileInfo(surface()->client()->executablePath());
    if (fileInfo.exists()) {
        setResourceClass(fileInfo.fileName().toUtf8());
    }
}

void WaylandClient::updateCaption()
{
    const QString oldSuffix = m_captionSuffix;
    const auto shortcut = shortcutCaptionSuffix();
    m_captionSuffix = shortcut;
    if ((!isSpecialWindow() || isToolbar()) && findClientWithSameCaption()) {
        int i = 2;
        do {
            m_captionSuffix = shortcut + QLatin1String(" <") + QString::number(i) + QLatin1Char('>');
            i++;
        } while (findClientWithSameCaption());
    }
    if (m_captionSuffix != oldSuffix) {
        emit captionChanged();
    }
}

void WaylandClient::setCaption(const QString &caption)
{
    const QString oldSuffix = m_captionSuffix;
    m_captionNormal = caption.simplified();
    updateCaption();
    if (m_captionSuffix == oldSuffix) {
        // Don't emit caption change twice it already got emitted by the changing suffix.
        emit captionChanged();
    }
}

void WaylandClient::doSetActive()
{
    if (isActive()) { // TODO: Xwayland clients must be unfocused somewhere else.
        StackingUpdatesBlocker blocker(workspace());
        workspace()->focusToNull();
    }
}

void WaylandClient::updateDepth()
{
    if (surface()->buffer()->hasAlphaChannel() && !isDesktop()) {
        setDepth(32);
    } else {
        setDepth(24);
    }
}

void WaylandClient::cleanGrouping()
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

void WaylandClient::cleanTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    if (tabBox->isDisplayed() && tabBox->currentClient() == this) {
        tabBox->nextPrev(true);
    }
#endif
}

bool WaylandClient::isShown(bool shaded_is_shown) const
{
    Q_UNUSED(shaded_is_shown)
    return !isZombie() && !isHidden() && !isMinimized();
}

bool WaylandClient::isHiddenInternal() const
{
    return isHidden();
}

void WaylandClient::hideClient(bool hide)
{
    if (hide) {
        internalHide();
    } else {
        internalShow();
    }
}

bool WaylandClient::isHidden() const
{
    return m_isHidden;
}

void WaylandClient::internalShow()
{
    if (!isHidden()) {
        return;
    }
    m_isHidden = false;
    addRepaintFull();
    emit windowShown(this);
}

void WaylandClient::internalHide()
{
    if (isHidden()) {
        return;
    }
    if (isMoveResize()) {
        leaveMoveResize();
    }
    m_isHidden = true;
    addWorkspaceRepaint(visibleRect());
    workspace()->clientHidden(this);
    emit windowHidden(this);
}

QRect WaylandClient::frameRectToBufferRect(const QRect &rect) const
{
    return QRect(rect.topLeft(), surface()->size());
}

QRect WaylandClient::requestedFrameGeometry() const
{
    return m_requestedFrameGeometry;
}

QPoint WaylandClient::requestedPos() const
{
    return m_requestedFrameGeometry.topLeft();
}

QSize WaylandClient::requestedSize() const
{
    return m_requestedFrameGeometry.size();
}

QRect WaylandClient::requestedClientGeometry() const
{
    return m_requestedClientGeometry;
}

QRect WaylandClient::bufferGeometry() const
{
    return m_bufferGeometry;
}

QSize WaylandClient::requestedClientSize() const
{
    return requestedClientGeometry().size();
}

void WaylandClient::setFrameGeometry(const QRect &rect, ForceGeometry_t force)
{
    m_requestedFrameGeometry = rect;

    if (isShade()) {
        if (m_requestedFrameGeometry.height() == borderTop() + borderBottom()) {
            qCDebug(KWIN_CORE) << "Passed shaded frame geometry to setFrameGeometry()";
        } else {
            m_requestedClientGeometry = frameRectToClientRect(m_requestedFrameGeometry);
            m_requestedFrameGeometry.setHeight(borderTop() + borderBottom());
        }
    } else {
        m_requestedClientGeometry = frameRectToClientRect(m_requestedFrameGeometry);
    }

    if (areGeometryUpdatesBlocked()) {
        m_frameGeometry = m_requestedFrameGeometry;
        if (pendingGeometryUpdate() == PendingGeometryForced) {
            return;
        }
        if (force == ForceGeometrySet) {
            setPendingGeometryUpdate(PendingGeometryForced);
        } else {
            setPendingGeometryUpdate(PendingGeometryNormal);
        }
        return;
    }

    m_frameGeometry = frameGeometryBeforeUpdateBlocking();

    if (requestedClientSize() != clientSize()) {
        requestGeometry(requestedFrameGeometry());
    } else {
        updateGeometry(requestedFrameGeometry());
        return;
    }

    QRect updateRect = m_frameGeometry;
    if (m_positionSyncMode == SyncMode::Sync) {
        updateRect.moveTopLeft(requestedPos());
    }
    if (m_sizeSyncMode == SyncMode::Sync) {
        updateRect.setSize(requestedSize());
    }
    updateGeometry(updateRect);
}

void WaylandClient::move(int x, int y, ForceGeometry_t force)
{
    Q_ASSERT(pendingGeometryUpdate() == PendingGeometryNone || areGeometryUpdatesBlocked());
    QPoint p(x, y);
    if (!areGeometryUpdatesBlocked() && p != rules()->checkPosition(p)) {
        qCDebug(KWIN_CORE) << "forced position fail:" << p << ":" << rules()->checkPosition(p);
    }
    m_requestedFrameGeometry.moveTopLeft(p);
    m_requestedClientGeometry.moveTopLeft(framePosToClientPos(p));
    if (force == NormalGeometrySet && m_frameGeometry.topLeft() == p) {
        return;
    }
    m_frameGeometry.moveTopLeft(m_requestedFrameGeometry.topLeft());
    if (areGeometryUpdatesBlocked()) {
        if (pendingGeometryUpdate() == PendingGeometryForced) {
            return;
        }
        if (force == ForceGeometrySet) {
            setPendingGeometryUpdate(PendingGeometryForced);
        } else {
            setPendingGeometryUpdate(PendingGeometryNormal);
        }
        return;
    }
    const QRect oldBufferGeometry = bufferGeometryBeforeUpdateBlocking();
    const QRect oldClientGeometry = clientGeometryBeforeUpdateBlocking();
    const QRect oldFrameGeometry = frameGeometryBeforeUpdateBlocking();
    m_clientGeometry.moveTopLeft(m_requestedClientGeometry.topLeft());
    m_bufferGeometry = frameRectToBufferRect(m_frameGeometry);
    updateGeometryBeforeUpdateBlocking();
    updateWindowRules(Rules::Position);
    screens()->setCurrent(this);
    workspace()->updateStackingOrder();
    emit bufferGeometryChanged(this, oldBufferGeometry);
    emit clientGeometryChanged(this, oldClientGeometry);
    emit frameGeometryChanged(this, oldFrameGeometry);
    addRepaintDuringGeometryUpdates();
}

void WaylandClient::requestGeometry(const QRect &rect)
{
    m_requestedFrameGeometry = rect;
    m_requestedClientGeometry = frameRectToClientRect(rect);
}

void WaylandClient::updateGeometry(const QRect &rect)
{
    const QRect oldClientGeometry = m_clientGeometry;
    const QRect oldFrameGeometry = m_frameGeometry;
    const QRect oldBufferGeometry = m_bufferGeometry;

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

    updateWindowRules(Rules::Position | Rules::Size);
    updateGeometryBeforeUpdateBlocking();

    if (changedGeometries & WaylandGeometryBuffer) {
        emit bufferGeometryChanged(this, oldBufferGeometry);
    }
    if (changedGeometries & WaylandGeometryClient) {
        emit clientGeometryChanged(this, oldClientGeometry);
    }
    if (changedGeometries & WaylandGeometryFrame) {
        emit frameGeometryChanged(this, oldFrameGeometry);
    }
    emit geometryShapeChanged(this, oldFrameGeometry);

    addRepaintDuringGeometryUpdates();
}

void WaylandClient::setPositionSyncMode(SyncMode syncMode)
{
    m_positionSyncMode = syncMode;
}

void WaylandClient::setSizeSyncMode(SyncMode syncMode)
{
    m_sizeSyncMode = syncMode;
}

} // namespace KWin
