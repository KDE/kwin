/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "internal_client.h"
#include "decorations/decorationbridge.h"
#include "deleted.h"
#include "surfaceitem.h"
#include "workspace.h"

#include <KDecoration2/Decoration>

#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QWindow>

Q_DECLARE_METATYPE(NET::WindowType)

static const QByteArray s_skipClosePropertyName = QByteArrayLiteral("KWIN_SKIP_CLOSE_ANIMATION");
static const QByteArray s_shadowEnabledPropertyName = QByteArrayLiteral("kwin_shadow_enabled");

namespace KWin
{

InternalClient::InternalClient(QWindow *window)
    : m_internalWindow(window)
    , m_internalWindowFlags(window->flags())
{
    connect(m_internalWindow, &QWindow::xChanged, this, &InternalClient::updateInternalWindowGeometry);
    connect(m_internalWindow, &QWindow::yChanged, this, &InternalClient::updateInternalWindowGeometry);
    connect(m_internalWindow, &QWindow::widthChanged, this, &InternalClient::updateInternalWindowGeometry);
    connect(m_internalWindow, &QWindow::heightChanged, this, &InternalClient::updateInternalWindowGeometry);
    connect(m_internalWindow, &QWindow::windowTitleChanged, this, &InternalClient::setCaption);
    connect(m_internalWindow, &QWindow::opacityChanged, this, &InternalClient::setOpacity);
    connect(m_internalWindow, &QWindow::destroyed, this, &InternalClient::destroyClient);

    const QVariant windowType = m_internalWindow->property("kwin_windowType");
    if (!windowType.isNull()) {
        m_windowType = windowType.value<NET::WindowType>();
    }

    setCaption(m_internalWindow->title());
    setIcon(QIcon::fromTheme(QStringLiteral("kwin")));
    setOnAllDesktops(true);
    setOpacity(m_internalWindow->opacity());
    setSkipCloseAnimation(m_internalWindow->property(s_skipClosePropertyName).toBool());

    // Create scene window, effect window, and update server-side shadow.
    setupCompositing();
    updateColorScheme();

    blockGeometryUpdates(true);
    commitGeometry(m_internalWindow->geometry());
    updateDecoration(true);
    moveResize(clientRectToFrameRect(m_internalWindow->geometry()));
    blockGeometryUpdates(false);

    m_internalWindow->installEventFilter(this);
}

InternalClient::~InternalClient()
{
}

bool InternalClient::isClient() const
{
    return true;
}

bool InternalClient::hitTest(const QPoint &point) const
{
    if (!AbstractClient::hitTest(point)) {
        return false;
    }

    const QRegion mask = m_internalWindow->mask();
    if (!mask.isEmpty() && !mask.contains(mapToLocal(point))) {
        return false;
    } else if (m_internalWindow->property("outputOnly").toBool()) {
        return false;
    }

    return true;
}

void InternalClient::pointerEnterEvent(const QPoint &globalPos)
{
    AbstractClient::pointerEnterEvent(globalPos);

    QEnterEvent enterEvent(pos(), pos(), globalPos);
    QCoreApplication::sendEvent(m_internalWindow, &enterEvent);
}

void InternalClient::pointerLeaveEvent()
{
    AbstractClient::pointerLeaveEvent();

    QEvent event(QEvent::Leave);
    QCoreApplication::sendEvent(m_internalWindow, &event);
}

bool InternalClient::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_internalWindow && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (pe->propertyName() == s_skipClosePropertyName) {
            setSkipCloseAnimation(m_internalWindow->property(s_skipClosePropertyName).toBool());
        }
        if (pe->propertyName() == s_shadowEnabledPropertyName) {
            updateShadow();
        }
        if (pe->propertyName() == "kwin_windowType") {
            m_windowType = m_internalWindow->property("kwin_windowType").value<NET::WindowType>();
            workspace()->updateClientArea();
        }
    }
    return false;
}

qreal InternalClient::bufferScale() const
{
    if (m_internalWindow) {
        return m_internalWindow->devicePixelRatio();
    }
    return 1;
}

QString InternalClient::captionNormal() const
{
    return m_captionNormal;
}

QString InternalClient::captionSuffix() const
{
    return m_captionSuffix;
}

QSize InternalClient::minSize() const
{
    return m_internalWindow->minimumSize();
}

QSize InternalClient::maxSize() const
{
    return m_internalWindow->maximumSize();
}

NET::WindowType InternalClient::windowType(bool direct, int supported_types) const
{
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return m_windowType;
}

void InternalClient::killWindow()
{
    // We don't kill our internal windows.
}

bool InternalClient::isPopupWindow() const
{
    if (AbstractClient::isPopupWindow()) {
        return true;
    }
    return m_internalWindowFlags.testFlag(Qt::Popup);
}

QByteArray InternalClient::windowRole() const
{
    return QByteArray();
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

bool InternalClient::isPlaceable() const
{
    return !m_internalWindowFlags.testFlag(Qt::BypassWindowManagerHint) && !m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalClient::noBorder() const
{
    return m_userNoBorder || m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalClient::userCanSetNoBorder() const
{
    return !m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
}

bool InternalClient::wantsInput() const
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

bool InternalClient::isOutline() const
{
    if (m_internalWindow) {
        return m_internalWindow->property("__kwin_outline").toBool();
    }
    return false;
}

bool InternalClient::isShown() const
{
    return readyForPainting();
}

bool InternalClient::isHiddenInternal() const
{
    return false;
}

void InternalClient::hideClient()
{
}

void InternalClient::showClient()
{
}

void InternalClient::resizeWithChecks(const QSize &size)
{
    if (!m_internalWindow) {
        return;
    }
    const QRect area = workspace()->clientArea(WorkArea, this);
    resize(size.boundedTo(area.size()));
}

void InternalClient::moveResizeInternal(const QRect &rect, MoveResizeMode mode)
{
    if (areGeometryUpdatesBlocked()) {
        setPendingMoveResizeMode(mode);
        return;
    }

    const QSize requestedClientSize = frameSizeToClientSize(rect.size());
    if (clientSize() == requestedClientSize) {
        commitGeometry(rect);
    } else {
        requestGeometry(rect);
    }
}

AbstractClient *InternalClient::findModal(bool allow_itself)
{
    Q_UNUSED(allow_itself)
    return nullptr;
}

bool InternalClient::takeFocus()
{
    return false;
}

void InternalClient::setNoBorder(bool set)
{
    if (!userCanSetNoBorder()) {
        return;
    }
    if (m_userNoBorder == set) {
        return;
    }
    m_userNoBorder = set;
    updateDecoration(true);
}

void InternalClient::updateDecoration(bool check_workspace_pos, bool force)
{
    if (!force && isDecorated() == !noBorder()) {
        return;
    }

    const QRect oldFrameGeometry = frameGeometry();
    const QRect oldClientGeometry = oldFrameGeometry - frameMargins();

    GeometryUpdatesBlocker blocker(this);

    if (force) {
        destroyDecoration();
    }

    if (!noBorder()) {
        createDecoration(oldClientGeometry);
    } else {
        destroyDecoration();
    }

    updateShadow();

    if (check_workspace_pos) {
        checkWorkspacePosition(oldFrameGeometry, oldClientGeometry);
    }
}

void InternalClient::invalidateDecoration()
{
    updateDecoration(true, true);
}

void InternalClient::destroyClient()
{
    markAsZombie();
    if (isInteractiveMoveResize()) {
        leaveInteractiveMoveResize();
        Q_EMIT clientFinishUserMovedResized(this);
    }

    Deleted *deleted = Deleted::create(this);
    Q_EMIT windowClosed(this, deleted);

    destroyDecoration();

    workspace()->removeInternalClient(this);

    deleted->unrefWindow();
    m_internalWindow = nullptr;

    delete this;
}

bool InternalClient::hasPopupGrab() const
{
    return !m_internalWindow->flags().testFlag(Qt::WindowTransparentForInput) &&
            m_internalWindow->flags().testFlag(Qt::Popup) &&
            !m_internalWindow->flags().testFlag(Qt::ToolTip);
}

void InternalClient::popupDone()
{
    m_internalWindow->hide();
}

void InternalClient::present(const QSharedPointer<QOpenGLFramebufferObject> fbo)
{
    Q_ASSERT(m_internalImage.isNull());

    const QSize bufferSize = fbo->size() / bufferScale();

    commitGeometry(QRect(pos(), clientSizeToFrameSize(bufferSize)));
    markAsMapped();

    m_internalFBO = fbo;

    setDepth(32);
    surfaceItem()->addDamage(surfaceItem()->rect());

    if (isInteractiveResize()) {
        performInteractiveMoveResize();
    }
}

void InternalClient::present(const QImage &image, const QRegion &damage)
{
    Q_ASSERT(m_internalFBO.isNull());

    const QSize bufferSize = image.size() / bufferScale();

    commitGeometry(QRect(pos(), clientSizeToFrameSize(bufferSize)));
    markAsMapped();

    m_internalImage = image;

    setDepth(32);
    surfaceItem()->addDamage(damage);

    if (isInteractiveResize()) {
        performInteractiveMoveResize();
    }
}

QWindow *InternalClient::internalWindow() const
{
    return m_internalWindow;
}

bool InternalClient::acceptsFocus() const
{
    return false;
}

bool InternalClient::belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const
{
    Q_UNUSED(checks)
    const InternalClient *otherInternal = qobject_cast<const InternalClient *>(other);
    if (!otherInternal) {
        return false;
    }
    if (otherInternal == this) {
        return true;
    }
    return otherInternal->internalWindow()->isAncestorOf(internalWindow()) ||
            internalWindow()->isAncestorOf(otherInternal->internalWindow());
}

void InternalClient::doInteractiveResizeSync()
{
    requestGeometry(moveResizeGeometry());
}

void InternalClient::updateCaption()
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
        Q_EMIT captionChanged();
    }
}

void InternalClient::requestGeometry(const QRect &rect)
{
    if (m_internalWindow) {
        m_internalWindow->setGeometry(frameRectToClientRect(rect));
    }
}

void InternalClient::commitGeometry(const QRect &rect)
{
    // The client geometry and the buffer geometry are the same.
    const QRect oldClientGeometry = m_clientGeometry;
    const QRect oldFrameGeometry = m_frameGeometry;

    m_clientGeometry = frameRectToClientRect(rect);
    m_frameGeometry = rect;
    m_bufferGeometry = m_clientGeometry;

    if (oldClientGeometry == m_clientGeometry && oldFrameGeometry == m_frameGeometry) {
        return;
    }

    syncGeometryToInternalWindow();

    if (oldClientGeometry != m_clientGeometry) {
        Q_EMIT bufferGeometryChanged(this, oldClientGeometry);
        Q_EMIT clientGeometryChanged(this, oldClientGeometry);
    }
    if (oldFrameGeometry != m_frameGeometry) {
        Q_EMIT frameGeometryChanged(this, oldFrameGeometry);
    }
    Q_EMIT geometryShapeChanged(this, oldFrameGeometry);
}

void InternalClient::setCaption(const QString &caption)
{
    if (m_captionNormal == caption) {
        return;
    }

    m_captionNormal = caption;

    const QString oldCaptionSuffix = m_captionSuffix;
    updateCaption();

    if (m_captionSuffix == oldCaptionSuffix) {
        Q_EMIT captionChanged();
    }
}

void InternalClient::markAsMapped()
{
    if (!ready_for_painting) {
        setReadyForPainting();
        workspace()->addInternalClient(this);
    }
}

void InternalClient::syncGeometryToInternalWindow()
{
    if (m_internalWindow->geometry() == frameRectToClientRect(frameGeometry())) {
        return;
    }

    QTimer::singleShot(0, this, [this] { requestGeometry(frameGeometry()); });
}

void InternalClient::updateInternalWindowGeometry()
{
    if (!isInteractiveMoveResize()) {
        const QRect rect = clientRectToFrameRect(m_internalWindow->geometry());
        setMoveResizeGeometry(rect);
        commitGeometry(rect);
    }
}

}
