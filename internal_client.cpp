/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 Martin Flöser <mgraesslin@kde.org>
Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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
#include "decorations/decorationbridge.h"
#include "deleted.h"
#include "workspace.h"

#include <KDecoration2/Decoration>

#include <QOpenGLFramebufferObject>
#include <QWindow>

Q_DECLARE_METATYPE(NET::WindowType)

static const QByteArray s_skipClosePropertyName = QByteArrayLiteral("KWIN_SKIP_CLOSE_ANIMATION");
static const QByteArray s_shadowEnabledPropertyName = QByteArrayLiteral("kwin_shadow_enabled");

namespace KWin
{

InternalClient::InternalClient(QWindow *window)
    : m_internalWindow(window)
    , m_clientSize(window->size())
    , m_windowId(window->winId())
    , m_internalWindowFlags(window->flags())
{
    connect(m_internalWindow, &QWindow::xChanged, this, &InternalClient::updateInternalWindowGeometry);
    connect(m_internalWindow, &QWindow::yChanged, this, &InternalClient::updateInternalWindowGeometry);
    connect(m_internalWindow, &QWindow::widthChanged, this, &InternalClient::updateInternalWindowGeometry);
    connect(m_internalWindow, &QWindow::heightChanged, this, &InternalClient::updateInternalWindowGeometry);
    connect(m_internalWindow, &QWindow::windowTitleChanged, this, &InternalClient::setCaption);
    connect(m_internalWindow, &QWindow::opacityChanged, this, &InternalClient::setOpacity);
    connect(m_internalWindow, &QWindow::destroyed, this, &InternalClient::destroyClient);

    connect(this, &InternalClient::opacityChanged, this, &InternalClient::addRepaintFull);

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
    setFrameGeometry(clientRectToFrameRect(m_internalWindow->geometry()));
    setGeometryRestore(frameGeometry());
    blockGeometryUpdates(false);

    m_internalWindow->installEventFilter(this);
}

InternalClient::~InternalClient()
{
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

QRect InternalClient::bufferGeometry() const
{
    return frameGeometry() - frameMargins();
}

QStringList InternalClient::activities() const
{
    return QStringList();
}

void InternalClient::blockActivityUpdates(bool b)
{
    Q_UNUSED(b)

    // Internal clients do not support activities.
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

QPoint InternalClient::clientContentPos() const
{
    return -1 * clientPos();
}

QSize InternalClient::clientSize() const
{
    return m_clientSize;
}

void InternalClient::debug(QDebug &stream) const
{
    stream.nospace() << "\'InternalClient:" << m_internalWindow << "\'";
}

QRect InternalClient::transparentRect() const
{
    return QRect();
}

NET::WindowType InternalClient::windowType(bool direct, int supported_types) const
{
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return m_windowType;
}

double InternalClient::opacity() const
{
    return m_opacity;
}

void InternalClient::setOpacity(double opacity)
{
    if (m_opacity == opacity) {
        return;
    }

    const double oldOpacity = m_opacity;
    m_opacity = opacity;

    emit opacityChanged(this, oldOpacity);
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

bool InternalClient::isFullScreenable() const
{
    return false;
}

bool InternalClient::isFullScreen() const
{
    return false;
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

MaximizeMode InternalClient::maximizeMode() const
{
    return MaximizeRestore;
}

QRect InternalClient::geometryRestore() const
{
    return m_maximizeRestoreGeometry;
}

bool InternalClient::isShown(bool shaded_is_shown) const
{
    Q_UNUSED(shaded_is_shown)

    return readyForPainting();
}

bool InternalClient::isHiddenInternal() const
{
    return false;
}

void InternalClient::hideClient(bool hide)
{
    Q_UNUSED(hide)
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
    setFrameGeometry(QRect(x(), y(), w, h));
}

void InternalClient::setFrameGeometry(int x, int y, int w, int h, ForceGeometry_t force)
{
    const QRect rect(x, y, w, h);

    if (areGeometryUpdatesBlocked()) {
        m_frameGeometry = rect;
        if (pendingGeometryUpdate() == PendingGeometryForced) {
            // Maximum, nothing needed.
        } else if (force == ForceGeometrySet) {
            setPendingGeometryUpdate(PendingGeometryForced);
        } else {
            setPendingGeometryUpdate(PendingGeometryNormal);
        }
        return;
    }

    if (pendingGeometryUpdate() != PendingGeometryNone) {
        // Reset geometry to the one before blocking, so that we can compare properly.
        m_frameGeometry = frameGeometryBeforeUpdateBlocking();
    }

    if (m_frameGeometry == rect) {
        return;
    }

    const QRect newClientGeometry = frameRectToClientRect(rect);

    if (m_clientSize == newClientGeometry.size()) {
        commitGeometry(rect);
    } else {
        requestGeometry(rect);
    }
}

void InternalClient::setGeometryRestore(const QRect &rect)
{
    m_maximizeRestoreGeometry = rect;
}

bool InternalClient::supportsWindowRules() const
{
    return false;
}

AbstractClient *InternalClient::findModal(bool allow_itself)
{
    Q_UNUSED(allow_itself)
    return nullptr;
}

void InternalClient::setOnAllActivities(bool set)
{
    Q_UNUSED(set)

    // Internal clients do not support activities.
}

void InternalClient::takeFocus()
{
}

bool InternalClient::userCanSetFullScreen() const
{
    return false;
}

void InternalClient::setFullScreen(bool set, bool user)
{
    Q_UNUSED(set)
    Q_UNUSED(user)
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
        checkWorkspacePosition(oldFrameGeometry, -2, oldClientGeometry);
    }
}

void InternalClient::updateColorScheme()
{
    AbstractClient::updateColorScheme(QString());
}

void InternalClient::showOnScreenEdge()
{
}

void InternalClient::destroyClient()
{
    if (isMoveResize()) {
        leaveMoveResize();
    }

    Deleted *deleted = Deleted::create(this);
    emit windowClosed(this, deleted);

    destroyDecoration();

    workspace()->removeInternalClient(this);

    deleted->unrefWindow();
    m_internalWindow = nullptr;

    delete this;
}

void InternalClient::present(const QSharedPointer<QOpenGLFramebufferObject> fbo)
{
    Q_ASSERT(m_internalImage.isNull());

    const QSize bufferSize = fbo->size() / bufferScale();

    commitGeometry(QRect(pos(), sizeForClientSize(bufferSize)));
    markAsMapped();

    if (m_internalFBO != fbo) {
        discardWindowPixmap();
        m_internalFBO = fbo;
    }

    setDepth(32);
    addDamageFull();
    addRepaintFull();
}

void InternalClient::present(const QImage &image, const QRegion &damage)
{
    Q_ASSERT(m_internalFBO.isNull());

    const QSize bufferSize = image.size() / bufferScale();

    commitGeometry(QRect(pos(), sizeForClientSize(bufferSize)));
    markAsMapped();

    if (m_internalImage.size() != image.size()) {
        discardWindowPixmap();
    }

    m_internalImage = image;

    setDepth(32);
    addDamage(damage);
    addRepaint(damage.translated(borderLeft(), borderTop()));
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

    return qobject_cast<const InternalClient *>(other) != nullptr;
}

void InternalClient::changeMaximize(bool horizontal, bool vertical, bool adjust)
{
    Q_UNUSED(horizontal)
    Q_UNUSED(vertical)
    Q_UNUSED(adjust)

    // Internal clients are not maximizable.
}

void InternalClient::destroyDecoration()
{
    if (!isDecorated()) {
        return;
    }

    const QRect clientGeometry = frameRectToClientRect(frameGeometry());
    AbstractClient::destroyDecoration();
    setFrameGeometry(clientGeometry);
}

void InternalClient::doMove(int x, int y)
{
    Q_UNUSED(x)
    Q_UNUSED(y)

    syncGeometryToInternalWindow();
}

void InternalClient::doResizeSync()
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
        emit captionChanged();
    }
}

void InternalClient::createDecoration(const QRect &rect)
{
    KDecoration2::Decoration *decoration = Decoration::DecorationBridge::self()->createDecoration(this);
    if (decoration) {
        QMetaObject::invokeMethod(decoration, "update", Qt::QueuedConnection);
        connect(decoration, &KDecoration2::Decoration::shadowChanged, this, &Toplevel::updateShadow);
        connect(decoration, &KDecoration2::Decoration::bordersChanged, this,
            [this]() {
                GeometryUpdatesBlocker blocker(this);
                const QRect oldGeometry = frameGeometry();
                if (!isShade()) {
                    checkWorkspacePosition(oldGeometry);
                }
                emit geometryShapeChanged(this, oldGeometry);
            }
        );
    }

    const QRect oldFrameGeometry = frameGeometry();

    setDecoration(decoration);
    setFrameGeometry(clientRectToFrameRect(rect));

    emit geometryShapeChanged(this, oldFrameGeometry);
}

void InternalClient::requestGeometry(const QRect &rect)
{
    if (m_internalWindow) {
        m_internalWindow->setGeometry(frameRectToClientRect(rect));
    }
}

void InternalClient::commitGeometry(const QRect &rect)
{
    if (m_frameGeometry == rect && pendingGeometryUpdate() == PendingGeometryNone) {
        return;
    }

    m_frameGeometry = rect;

    m_clientSize = frameRectToClientRect(frameGeometry()).size();

    addWorkspaceRepaint(visibleRect());
    syncGeometryToInternalWindow();

    const QRect oldGeometry = frameGeometryBeforeUpdateBlocking();
    updateGeometryBeforeUpdateBlocking();
    emit geometryShapeChanged(this, oldGeometry);

    if (isResize()) {
        performMoveResize();
    }
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
        emit captionChanged();
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
    if (isMoveResize()) {
        return;
    }

    commitGeometry(clientRectToFrameRect(m_internalWindow->geometry()));
}

}
