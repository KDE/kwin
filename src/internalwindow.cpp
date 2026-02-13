/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "internalwindow.h"
#include "decorations/decorationbridge.h"
#include "scene/windowitem.h"
#include "workspace.h"

#include <KDecoration3/Decoration>

#include <QMouseEvent>
#include <QWindow>

#include <qpa/qwindowsysteminterface.h>

Q_DECLARE_METATYPE(NET::WindowType)

static const QByteArray s_skipClosePropertyName = QByteArrayLiteral("KWIN_SKIP_CLOSE_ANIMATION");
static const QByteArray s_shadowEnabledPropertyName = QByteArrayLiteral("kwin_shadow_enabled");

namespace KWin
{

InternalWindow::InternalWindow(QWindow *handle)
    : m_handle(handle)
    , m_internalWindowFlags(handle->flags())
{
    connect(m_handle, &QWindow::windowTitleChanged, this, &InternalWindow::setCaption);
    connect(m_handle, &QWindow::opacityChanged, this, &InternalWindow::setOpacity);
    connect(m_handle, &QWindow::destroyed, this, &InternalWindow::destroyWindow);

    setOutput(workspace()->activeOutput());
    setMoveResizeOutput(workspace()->activeOutput());
    setCaption(m_handle->title());
    setIcon(QIcon::fromTheme(QStringLiteral("kwin")));
    setOnAllDesktops(true);
    setOpacity(m_handle->opacity());
    setSkipCloseAnimation(m_handle->property(s_skipClosePropertyName).toBool());
    updateColorScheme();
    updateShadow();

    setMoveResizeGeometry(m_handle->geometry());
    commitGeometry(m_handle->geometry());

    updateDecoration(true);

    m_handle->installEventFilter(this);
}

InternalWindow::~InternalWindow()
{
}

std::unique_ptr<WindowItem> InternalWindow::createItem(Item *parentItem)
{
    return std::make_unique<WindowItemInternal>(this, parentItem);
}

bool InternalWindow::isClient() const
{
    return true;
}

bool InternalWindow::hitTest(const QPointF &point) const
{
    if (!Window::hitTest(point)) {
        return false;
    }

    const QRegion mask = m_handle->mask();
    if (!mask.isEmpty() && !mask.contains(mapToLocal(point).toPoint())) {
        return false;
    } else if (m_handle->property("outputOnly").toBool()) {
        return false;
    }

    return true;
}

void InternalWindow::pointerEnterEvent(const QPointF &globalPos)
{
    Window::pointerEnterEvent(globalPos);

    QEnterEvent enterEvent(pos(), pos(), globalPos);
    QCoreApplication::sendEvent(m_handle, &enterEvent);
}

void InternalWindow::pointerLeaveEvent()
{
    if (!m_handle) {
        return;
    }
    Window::pointerLeaveEvent();

    QEvent event(QEvent::Leave);
    QCoreApplication::sendEvent(m_handle, &event);
}

bool InternalWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_handle && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (pe->propertyName() == s_skipClosePropertyName) {
            setSkipCloseAnimation(m_handle->property(s_skipClosePropertyName).toBool());
        }
        if (pe->propertyName() == s_shadowEnabledPropertyName) {
            updateShadow();
        }
    }
    return false;
}

qreal InternalWindow::bufferScale() const
{
    if (m_handle) {
        return m_handle->devicePixelRatio();
    }
    return 1;
}

void InternalWindow::doSetNextTargetScale()
{
    setTargetScale(nextTargetScale());
}

QString InternalWindow::captionNormal() const
{
    return m_captionNormal;
}

QString InternalWindow::captionSuffix() const
{
    return m_captionSuffix;
}

QSizeF InternalWindow::minSize() const
{
    return m_handle->minimumSize();
}

QSizeF InternalWindow::maxSize() const
{
    return m_handle->maximumSize();
}

WindowType InternalWindow::windowType() const
{
    return WindowType::Normal;
}

void InternalWindow::killWindow()
{
    // We don't kill our internal windows.
}

bool InternalWindow::isPopupWindow() const
{
    if (Window::isPopupWindow()) {
        return true;
    }
    return m_internalWindowFlags.testFlag(Qt::Popup);
}

QString InternalWindow::windowRole() const
{
    return QString();
}

void InternalWindow::closeWindow()
{
    if (!isDeleted()) {
        QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::AsynchronousDelivery>(m_handle);
    }
}

bool InternalWindow::isCloseable() const
{
    return true;
}

bool InternalWindow::isMovable() const
{
    if (!options->interactiveWindowMoveEnabled()) {
        return false;
    }
    return !m_internalWindowFlags.testFlag(Qt::BypassWindowManagerHint) && !m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalWindow::isMovableAcrossScreens() const
{
    return !m_internalWindowFlags.testFlag(Qt::BypassWindowManagerHint) && !m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalWindow::isResizable() const
{
    return true;
}

bool InternalWindow::isPlaceable() const
{
    return !m_internalWindowFlags.testFlag(Qt::BypassWindowManagerHint) && !m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalWindow::wantsInput() const
{
    return false;
}

bool InternalWindow::isInternal() const
{
    return true;
}

bool InternalWindow::isLockScreen() const
{
    if (m_handle) {
        return m_handle->property("org_kde_ksld_emergency").toBool();
    }
    return false;
}

bool InternalWindow::isOutline() const
{
    if (m_handle) {
        return m_handle->property("__kwin_outline").toBool();
    }
    return false;
}

RectF InternalWindow::resizeWithChecks(const RectF &geometry, const QSizeF &size) const
{
    if (!m_handle) {
        return geometry;
    }
    const RectF area = workspace()->clientArea(WorkArea, this, geometry.center());
    return RectF(moveResizeGeometry().topLeft(), size.boundedTo(area.size()));
}

void InternalWindow::moveResizeInternal(const RectF &rect, MoveResizeMode mode)
{
    const QSize requestedSize = nextFrameSizeToClientSize(rect.size()).toSize();
    if (clientSize().toSize() == requestedSize) {
        commitGeometry(rect);
    }

    const QRect nativeRect = nextFrameRectToClientRect(rect).toRect();
    if (m_handle->geometry() != nativeRect) {
        const QRect oldNativeRect = m_handle->geometry();

        QWindowSystemInterface::handleGeometryChange(m_handle, nativeRect);
        if (m_handle->isExposed() && oldNativeRect.size() != nativeRect.size()) {
            QWindowSystemInterface::handleExposeEvent(m_handle, QRect(QPoint(), nativeRect.size()));
        }
    }
}

DecorationPolicy InternalWindow::decorationPolicy() const
{
    return m_decorationPolicy;
}

void InternalWindow::setDecorationPolicy(DecorationPolicy policy)
{
    if (m_decorationPolicy == policy) {
        return;
    }
    m_decorationPolicy = policy;
    updateDecoration(true);
    Q_EMIT decorationPolicyChanged();
}

void InternalWindow::createDecoration(const RectF &oldGeometry)
{
    std::shared_ptr<KDecoration3::Decoration> decoration(Workspace::self()->decorationBridge()->createDecoration(this));
    if (decoration) {
        decoration->apply(decoration->nextState()->clone());
        connect(decoration.get(), &KDecoration3::Decoration::nextStateChanged, this, [this](auto state) {
            if (!isDeleted()) {
                m_decoration.decoration->apply(state->clone());
            }
        });
    }

    setDecoration(decoration);
    moveResize(RectF(oldGeometry.topLeft(), nextClientSizeToFrameSize(clientSize())));
}

void InternalWindow::destroyDecoration()
{
    const QSizeF clientSize = nextFrameSizeToClientSize(moveResizeGeometry().size());
    setDecoration(nullptr);
    resize(clientSize);
}

DecorationMode InternalWindow::preferredDecorationMode() const
{
    if (!Decoration::DecorationBridge::hasPlugin()) {
        return DecorationMode::Client;
    } else if (isRequestedFullScreen()) {
        return DecorationMode::None;
    }

    switch (m_decorationPolicy) {
    case DecorationPolicy::None:
        return DecorationMode::None;
    case DecorationPolicy::Server:
        return DecorationMode::Server;
    case DecorationPolicy::PreferredByClient:
        if (m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup)) {
            return DecorationMode::Client;
        } else {
            return DecorationMode::Server;
        }
    }

    return DecorationMode::Client;
}

void InternalWindow::updateDecoration(bool check_workspace_pos, bool force)
{
    const bool wantsDecoration = preferredDecorationMode() == DecorationMode::Server;
    if (!force && isDecorated() == wantsDecoration) {
        return;
    }

    const RectF oldFrameGeometry = frameGeometry();
    if (force) {
        destroyDecoration();
    }

    if (wantsDecoration) {
        createDecoration(oldFrameGeometry);
    } else {
        destroyDecoration();
    }

    updateShadow();

    if (check_workspace_pos) {
        checkWorkspacePosition(oldFrameGeometry);
    }
}

void InternalWindow::invalidateDecoration()
{
    updateDecoration(true, true);
}

void InternalWindow::destroyWindow()
{
    m_handle->removeEventFilter(this);
    m_handle->disconnect(this);

    markAsDeleted();
    stopDelayedInteractiveMoveResize();
    if (isInteractiveMoveResize()) {
        leaveInteractiveMoveResize();
        Q_EMIT interactiveMoveResizeFinished();
    }

    Q_EMIT closed();

    workspace()->removeInternalWindow(this);
    m_handle = nullptr;

    unref();
}

bool InternalWindow::hasPopupGrab() const
{
    return !m_handle->flags().testFlag(Qt::WindowTransparentForInput) && m_handle->flags().testFlag(Qt::Popup) && !m_handle->flags().testFlag(Qt::ToolTip);
}

void InternalWindow::popupDone()
{
    m_handle->close();
}

GraphicsBuffer *InternalWindow::graphicsBuffer() const
{
    return m_graphicsBufferRef.buffer();
}

OutputTransform InternalWindow::bufferTransform() const
{
    return m_bufferTransform;
}

void InternalWindow::present(const InternalWindowFrame &frame)
{
    RectF geometry(clientRectToFrameRect(m_handle->geometry()));
    if (isInteractiveResize()) {
        geometry = interactiveMoveResizeGravity().apply(geometry, moveResizeGeometry());
    }

    commitGeometry(geometry);

    m_graphicsBufferRef = frame.buffer;
    m_bufferTransform = frame.bufferTransform;

    Q_EMIT presented(frame);

    markAsMapped();
}

QWindow *InternalWindow::handle() const
{
    return m_handle;
}

bool InternalWindow::acceptsFocus() const
{
    return false;
}

bool InternalWindow::belongsToSameApplication(const Window *other, SameApplicationChecks checks) const
{
    const InternalWindow *otherInternal = qobject_cast<const InternalWindow *>(other);
    if (!otherInternal) {
        return false;
    }
    if (otherInternal == this) {
        return true;
    }
    return otherInternal->handle()->isAncestorOf(handle()) || handle()->isAncestorOf(otherInternal->handle());
}

void InternalWindow::updateCaption()
{
    const QString suffix = shortcutCaptionSuffix();
    if (m_captionSuffix != suffix) {
        m_captionSuffix = suffix;
        Q_EMIT captionChanged();
    }
}

void InternalWindow::commitGeometry(const RectF &rect)
{
    // The client geometry and the buffer geometry are the same.
    const RectF oldClientGeometry = m_clientGeometry;
    const RectF oldFrameGeometry = m_frameGeometry;
    const LogicalOutput *oldOutput = m_output;

    Q_EMIT frameGeometryAboutToChange();

    m_clientGeometry = frameRectToClientRect(rect);
    m_frameGeometry = rect;
    m_bufferGeometry = m_clientGeometry;

    if (oldClientGeometry == m_clientGeometry && oldFrameGeometry == m_frameGeometry) {
        return;
    }

    m_output = workspace()->outputAt(rect.center());

    if (oldClientGeometry != m_clientGeometry) {
        Q_EMIT bufferGeometryChanged(oldClientGeometry);
        Q_EMIT clientGeometryChanged(oldClientGeometry);
    }
    if (oldFrameGeometry != m_frameGeometry) {
        Q_EMIT frameGeometryChanged(oldFrameGeometry);
    }
    if (oldOutput != m_output) {
        Q_EMIT outputChanged();
    }
}

void InternalWindow::setCaption(const QString &caption)
{
    if (m_captionNormal == caption) {
        return;
    }

    m_captionNormal = caption;
    Q_EMIT captionNormalChanged();
    Q_EMIT captionChanged();
}

void InternalWindow::markAsMapped()
{
    if (!ready_for_painting) {
        setupItem();
        setReadyForPainting();
        workspace()->addInternalWindow(this);
    }
}

}

#include "moc_internalwindow.cpp"
