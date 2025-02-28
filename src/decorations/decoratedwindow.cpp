/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "decoratedwindow.h"
#include "cursor.h"
#include "decorationbridge.h"
#include "decorationpalette.h"
#include "tiles/tile.h"
#include "window.h"
#include "workspace.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>

#include <QDebug>
#include <QMenu>
#include <QStyle>
#include <QToolTip>

namespace KWin
{
namespace Decoration
{

DecoratedWindowImpl::DecoratedWindowImpl(Window *window, KDecoration3::DecoratedWindow *decoratedClient, KDecoration3::Decoration *decoration)
    : QObject()
    , DecoratedWindowPrivateV3(decoratedClient, decoration)
    , m_window(window)
    , m_clientSize(window->clientSize())
{
    window->setDecoratedWindow(this);
    connect(window, &Window::activeChanged, this, [decoratedClient, window]() {
        Q_EMIT decoratedClient->activeChanged(window->isActive());
    });
    connect(window, &Window::clientGeometryChanged, this, [decoratedClient, this]() {
        if (m_window->clientSize() == m_clientSize) {
            return;
        }
        const auto oldSize = m_clientSize;
        m_clientSize = m_window->clientSize();
        if (oldSize.width() != m_clientSize.width()) {
            Q_EMIT decoratedClient->widthChanged(m_clientSize.width());
        }
        if (oldSize.height() != m_clientSize.height()) {
            Q_EMIT decoratedClient->heightChanged(m_clientSize.height());
        }
        Q_EMIT decoratedClient->sizeChanged(m_clientSize);
    });
    connect(window, &Window::desktopsChanged, this, [decoratedClient, window]() {
        Q_EMIT decoratedClient->onAllDesktopsChanged(window->isOnAllDesktops());
    });
    connect(window, &Window::captionChanged, this, [decoratedClient, window]() {
        Q_EMIT decoratedClient->captionChanged(window->caption());
    });
    connect(window, &Window::iconChanged, this, [decoratedClient, window]() {
        Q_EMIT decoratedClient->iconChanged(window->icon());
    });
    connect(window, &Window::shadeChanged, this, &Decoration::DecoratedWindowImpl::signalShadeChange);
    connect(window, &Window::keepAboveChanged, decoratedClient, &KDecoration3::DecoratedWindow::keepAboveChanged);
    connect(window, &Window::keepBelowChanged, decoratedClient, &KDecoration3::DecoratedWindow::keepBelowChanged);
    connect(window, &Window::requestedTileChanged, decoratedClient, [this, decoratedClient]() {
        Q_EMIT decoratedClient->adjacentScreenEdgesChanged(adjacentScreenEdges());
    });
    connect(window, &Window::closeableChanged, decoratedClient, &KDecoration3::DecoratedWindow::closeableChanged);
    connect(window, &Window::shadeableChanged, decoratedClient, &KDecoration3::DecoratedWindow::shadeableChanged);
    connect(window, &Window::minimizeableChanged, decoratedClient, &KDecoration3::DecoratedWindow::minimizeableChanged);
    connect(window, &Window::maximizeableChanged, decoratedClient, &KDecoration3::DecoratedWindow::maximizeableChanged);

    connect(window, &Window::paletteChanged, decoratedClient, &KDecoration3::DecoratedWindow::paletteChanged);

    connect(window, &Window::hasApplicationMenuChanged, decoratedClient, &KDecoration3::DecoratedWindow::hasApplicationMenuChanged);
    connect(window, &Window::applicationMenuActiveChanged, decoratedClient, &KDecoration3::DecoratedWindow::applicationMenuActiveChanged);

    connect(window, &Window::targetScaleChanged, decoratedClient, &KDecoration3::DecoratedWindow::scaleChanged);
    connect(window, &Window::nextTargetScaleChanged, decoratedClient, &KDecoration3::DecoratedWindow::nextScaleChanged);
    connect(window, &Window::applicationMenuChanged, decoratedClient, &KDecoration3::DecoratedWindow::applicationMenuChanged);

    m_toolTipWakeUp.setSingleShot(true);
    connect(&m_toolTipWakeUp, &QTimer::timeout, this, [this]() {
        int fallAsleepDelay = QApplication::style()->styleHint(QStyle::SH_ToolTip_FallAsleepDelay);
        this->m_toolTipFallAsleep.setRemainingTime(fallAsleepDelay);

        QToolTip::showText(Cursors::self()->currentCursor()->pos().toPoint(), this->m_toolTipText);
        m_toolTipShowing = true;
    });
}

DecoratedWindowImpl::~DecoratedWindowImpl()
{
    if (m_toolTipShowing) {
        requestHideToolTip();
    }
}

void DecoratedWindowImpl::signalShadeChange()
{
    Q_EMIT decoratedWindow()->shadedChanged(m_window->isShade());
}

#define DELEGATE(type, name, clientName)   \
    type DecoratedWindowImpl::name() const \
    {                                      \
        return m_window->clientName();     \
    }

#define DELEGATE2(type, name) DELEGATE(type, name, name)

DELEGATE2(QString, caption)
DELEGATE2(bool, isActive)
DELEGATE2(bool, isCloseable)
DELEGATE(bool, isMaximizeable, isMaximizable)
DELEGATE(bool, isMinimizeable, isMinimizable)
DELEGATE2(bool, isModal)
DELEGATE(bool, isMoveable, isMovable)
DELEGATE(bool, isResizeable, isResizable)
DELEGATE2(bool, isShadeable)
DELEGATE2(bool, providesContextHelp)
DELEGATE2(bool, isOnAllDesktops)
DELEGATE2(QPalette, palette)
DELEGATE2(QIcon, icon)

#undef DELEGATE2
#undef DELEGATE

#define DELEGATE(type, name, clientName)   \
    type DecoratedWindowImpl::name() const \
    {                                      \
        return m_window->clientName();     \
    }

DELEGATE(bool, isKeepAbove, keepAbove)
DELEGATE(bool, isKeepBelow, keepBelow)
DELEGATE(bool, isShaded, isShade)

#undef DELEGATE

#define DELEGATE(name, op)                                                \
    void DecoratedWindowImpl::name()                                      \
    {                                                                     \
        if (m_window->isDeleted()) {                                      \
            return;                                                       \
        }                                                                 \
        Workspace::self()->performWindowOperation(m_window, Options::op); \
    }

DELEGATE(requestToggleShade, ShadeOp)
DELEGATE(requestToggleOnAllDesktops, OnAllDesktopsOp)
DELEGATE(requestToggleKeepAbove, KeepAboveOp)
DELEGATE(requestToggleKeepBelow, KeepBelowOp)

#undef DELEGATE

#define DELEGATE(name, clientName)   \
    void DecoratedWindowImpl::name() \
    {                                \
        if (m_window->isDeleted()) { \
            return;                  \
        }                            \
        m_window->clientName();      \
    }

DELEGATE(requestContextHelp, showContextHelp)

#undef DELEGATE

void DecoratedWindowImpl::requestMinimize()
{
    m_window->setMinimized(true);
}

void DecoratedWindowImpl::requestClose()
{
    if (m_window->isDeleted()) {
        return;
    }
    QMetaObject::invokeMethod(m_window, &Window::closeWindow, Qt::QueuedConnection);
}

QColor DecoratedWindowImpl::color(KDecoration3::ColorGroup group, KDecoration3::ColorRole role) const
{
    auto dp = m_window->decorationPalette();
    if (dp) {
        return dp->color(group, role);
    }

    return QColor();
}

void DecoratedWindowImpl::requestShowToolTip(const QString &text)
{
    if (m_window->isDeleted()) {
        return;
    }
    if (!workspace()->decorationBridge()->showToolTips()) {
        return;
    }

    m_toolTipText = text;

    int wakeUpDelay = QApplication::style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay);
    m_toolTipWakeUp.start(m_toolTipFallAsleep.hasExpired() ? wakeUpDelay : 20);
}

void DecoratedWindowImpl::requestHideToolTip()
{
    m_toolTipWakeUp.stop();
    QToolTip::hideText();
    m_toolTipShowing = false;
}

void DecoratedWindowImpl::requestShowWindowMenu(const QRect &rect)
{
    if (m_window->isDeleted()) {
        return;
    }
    Workspace::self()->showWindowMenu(QRectF(m_window->pos() + rect.topLeft(), m_window->pos() + rect.bottomRight()).toRect(), m_window);
}

void DecoratedWindowImpl::requestShowApplicationMenu(const QRect &rect, int actionId)
{
    if (m_window->isDeleted()) {
        return;
    }
    Workspace::self()->showApplicationMenu(rect, m_window, actionId);
}

void DecoratedWindowImpl::showApplicationMenu(int actionId)
{
    if (m_window->isDeleted()) {
        return;
    }
    decoration()->showApplicationMenu(actionId);
}

void DecoratedWindowImpl::requestToggleMaximization(Qt::MouseButtons buttons)
{
    if (m_window->isDeleted()) {
        return;
    }
    auto operation = options->operationMaxButtonClick(buttons);
    QMetaObject::invokeMethod(
        this, [this, operation] {
        delayedRequestToggleMaximization(operation);
    }, Qt::QueuedConnection);
}

void DecoratedWindowImpl::delayedRequestToggleMaximization(Options::WindowOperation operation)
{
    if (m_window->isDeleted()) {
        return;
    }
    Workspace::self()->performWindowOperation(m_window, operation);
}

qreal DecoratedWindowImpl::width() const
{
    return m_clientSize.width();
}

qreal DecoratedWindowImpl::height() const
{
    return m_clientSize.height();
}

QSizeF DecoratedWindowImpl::size() const
{
    return m_clientSize;
}

bool DecoratedWindowImpl::isMaximizedVertically() const
{
    return m_window->requestedMaximizeMode() & MaximizeVertical;
}

bool DecoratedWindowImpl::isMaximized() const
{
    return isMaximizedHorizontally() && isMaximizedVertically();
}

bool DecoratedWindowImpl::isMaximizedHorizontally() const
{
    return m_window->requestedMaximizeMode() & MaximizeHorizontal;
}

Qt::Edges DecoratedWindowImpl::adjacentScreenEdges() const
{
    if (Tile *tile = m_window->requestedTile()) {
        return tile->anchors();
    }
    return Qt::Edges();
}

bool DecoratedWindowImpl::hasApplicationMenu() const
{
    return m_window->hasApplicationMenu();
}

bool DecoratedWindowImpl::isApplicationMenuActive() const
{
    return m_window->applicationMenuActive();
}

QString DecoratedWindowImpl::windowClass() const
{
    return m_window->resourceName() + QLatin1Char(' ') + m_window->resourceClass();
}

qreal DecoratedWindowImpl::scale() const
{
    return m_window->targetScale();
}

qreal DecoratedWindowImpl::nextScale() const
{
    return m_window->nextTargetScale();
}

QString DecoratedWindowImpl::applicationMenuServiceName() const
{
    return m_window->applicationMenuServiceName();
}

QString DecoratedWindowImpl::applicationMenuObjectPath() const
{
    return m_window->applicationMenuObjectPath();
}

void DecoratedWindowImpl::popup(const KDecoration3::Positioner &positioner, QMenu *menu)
{
    const QRectF anchorRect = positioner.anchorRect().translated(m_window->pos());
    const QPointF position = qGuiApp->layoutDirection() == Qt::RightToLeft
        ? QPointF(anchorRect.right() - menu->width(), anchorRect.bottom())
        : QPointF(anchorRect.left(), anchorRect.bottom());

    if (menu->isVisible()) {
        menu->move(position.toPoint());
    } else {
        menu->popup(position.toPoint());
    }
}
}
}

#include "moc_decoratedwindow.cpp"
