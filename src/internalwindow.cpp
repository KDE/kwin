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
    // TODO: The geometry data flow is error-prone. Consider adopting the configure event design from xdg-shell.
    connect(m_handle, &QWindow::xChanged, this, &InternalWindow::updateInternalWindowGeometry);
    connect(m_handle, &QWindow::yChanged, this, &InternalWindow::updateInternalWindowGeometry);
    connect(m_handle, &QWindow::widthChanged, this, &InternalWindow::updateInternalWindowGeometry);
    connect(m_handle, &QWindow::heightChanged, this, &InternalWindow::updateInternalWindowGeometry);
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

bool InternalWindow::noBorder() const
{
    return m_userNoBorder || m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalWindow::userCanSetNoBorder() const
{
    return !m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
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

QRectF InternalWindow::resizeWithChecks(const QRectF &geometry, const QSizeF &size)
{
    if (!m_handle) {
        return geometry;
    }
    const QRectF area = workspace()->clientArea(WorkArea, this, geometry.center());
    return QRectF(moveResizeGeometry().topLeft(), size.boundedTo(area.size()));
}

void InternalWindow::moveResizeInternal(const QRectF &rect, MoveResizeMode mode)
{
    const QSize requestedClientSize = nextFrameSizeToClientSize(rect.size()).toSize();
    if (clientSize() == requestedClientSize) {
        commitGeometry(rect);
    } else {
        requestGeometry(rect);
    }
}

bool InternalWindow::takeFocus()
{
    return false;
}

void InternalWindow::setNoBorder(bool set)
{
    if (!userCanSetNoBorder()) {
        return;
    }
    if (m_userNoBorder == set) {
        return;
    }
    m_userNoBorder = set;
    updateDecoration(true);
    Q_EMIT noBorderChanged();
}

void InternalWindow::createDecoration(const QRectF &oldGeometry)
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
    moveResize(QRectF(oldGeometry.topLeft(), nextClientSizeToFrameSize(clientSize())));
}

void InternalWindow::destroyDecoration()
{
    const QSizeF clientSize = nextFrameSizeToClientSize(moveResizeGeometry().size());
    setDecoration(nullptr);
    resize(clientSize);
}

void InternalWindow::updateDecoration(bool check_workspace_pos, bool force)
{
    if (!force && isDecorated() == !noBorder()) {
        return;
    }

    const QRectF oldFrameGeometry = frameGeometry();
    if (force) {
        destroyDecoration();
    }

    if (!noBorder()) {
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

    commitTile(nullptr);
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
    const QSize bufferSize = frame.buffer->size() / bufferScale();
    QRectF geometry(pos(), clientSizeToFrameSize(bufferSize));
    if (isInteractiveResize()) {
        geometry = gravitateGeometry(geometry, moveResizeGeometry(), interactiveMoveResizeGravity());
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

void InternalWindow::doInteractiveResizeSync(const QRectF &rect)
{
    moveResize(rect);
}

void InternalWindow::updateCaption()
{
    const QString suffix = shortcutCaptionSuffix();
    if (m_captionSuffix != suffix) {
        m_captionSuffix = suffix;
        Q_EMIT captionChanged();
    }
}

void InternalWindow::requestGeometry(const QRectF &rect)
{
    if (m_handle) {
        m_handle->setGeometry(nextFrameRectToClientRect(rect).toRect());
    }
}

void InternalWindow::commitGeometry(const QRectF &rect)
{
    // The client geometry and the buffer geometry are the same.
    const QRectF oldClientGeometry = m_clientGeometry;
    const QRectF oldFrameGeometry = m_frameGeometry;
    const Output *oldOutput = m_output;

    Q_EMIT frameGeometryAboutToChange();

    m_clientGeometry = frameRectToClientRect(rect);
    m_frameGeometry = rect;
    m_bufferGeometry = m_clientGeometry;

    if (oldClientGeometry == m_clientGeometry && oldFrameGeometry == m_frameGeometry) {
        return;
    }

    m_output = workspace()->outputAt(rect.center());
    syncGeometryToInternalWindow();

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
        setupCompositing();
        setReadyForPainting();
        workspace()->addInternalWindow(this);
    }
}

void InternalWindow::syncGeometryToInternalWindow()
{
    if (m_handle->geometry() == frameRectToClientRect(frameGeometry()).toRect()) {
        return;
    }

    QTimer::singleShot(0, this, [this] {
        requestGeometry(frameGeometry());
    });
}

void InternalWindow::updateInternalWindowGeometry()
{
    if (!isInteractiveMoveResize()) {
        const QRectF rect = clientRectToFrameRect(m_handle->geometry());
        setMoveResizeGeometry(rect);
        commitGeometry(rect);
    }
}

}

#include "moc_internalwindow.cpp"
