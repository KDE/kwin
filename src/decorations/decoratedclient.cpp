/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "decoratedclient.h"
#include "cursor.h"
#include "decorationbridge.h"
#include "decorationpalette.h"
#include "window.h"
#include "workspace.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>

#include <QDebug>
#include <QStyle>
#include <QToolTip>

namespace KWin
{
namespace Decoration
{

DecoratedClientImpl::DecoratedClientImpl(Window *window, KDecoration2::DecoratedClient *decoratedClient, KDecoration2::Decoration *decoration)
    : QObject()
    , ApplicationMenuEnabledDecoratedClientPrivate(decoratedClient, decoration)
    , m_window(window)
    , m_clientSize(window->clientSize().toSize())
{
    window->setDecoratedClient(QPointer<DecoratedClientImpl>(this));
    connect(window, &Window::activeChanged, this, [decoratedClient, window]() {
        Q_EMIT decoratedClient->activeChanged(window->isActive());
    });
    connect(window, &Window::clientGeometryChanged, this, [decoratedClient, this]() {
        if (m_window->clientSize() == m_clientSize) {
            return;
        }
        const auto oldSize = m_clientSize;
        m_clientSize = m_window->clientSize().toSize();
        if (oldSize.width() != m_clientSize.width()) {
            Q_EMIT decoratedClient->widthChanged(m_clientSize.width());
        }
        if (oldSize.height() != m_clientSize.height()) {
            Q_EMIT decoratedClient->heightChanged(m_clientSize.height());
        }
        Q_EMIT decoratedClient->sizeChanged(m_clientSize);
    });
    connect(window, &Window::desktopChanged, this, [decoratedClient, window]() {
        Q_EMIT decoratedClient->onAllDesktopsChanged(window->isOnAllDesktops());
    });
    connect(window, &Window::captionChanged, this, [decoratedClient, window]() {
        Q_EMIT decoratedClient->captionChanged(window->caption());
    });
    connect(window, &Window::iconChanged, this, [decoratedClient, window]() {
        Q_EMIT decoratedClient->iconChanged(window->icon());
    });
    connect(window, &Window::shadeChanged, this, &Decoration::DecoratedClientImpl::signalShadeChange);
    connect(window, &Window::keepAboveChanged, decoratedClient, &KDecoration2::DecoratedClient::keepAboveChanged);
    connect(window, &Window::keepBelowChanged, decoratedClient, &KDecoration2::DecoratedClient::keepBelowChanged);
    connect(window, &Window::quickTileModeChanged, decoratedClient, [this, decoratedClient]() {
        Q_EMIT decoratedClient->adjacentScreenEdgesChanged(adjacentScreenEdges());
    });
    connect(window, &Window::closeableChanged, decoratedClient, &KDecoration2::DecoratedClient::closeableChanged);
    connect(window, &Window::shadeableChanged, decoratedClient, &KDecoration2::DecoratedClient::shadeableChanged);
    connect(window, &Window::minimizeableChanged, decoratedClient, &KDecoration2::DecoratedClient::minimizeableChanged);
    connect(window, &Window::maximizeableChanged, decoratedClient, &KDecoration2::DecoratedClient::maximizeableChanged);

    connect(window, &Window::paletteChanged, decoratedClient, &KDecoration2::DecoratedClient::paletteChanged);

    connect(window, &Window::hasApplicationMenuChanged, decoratedClient, &KDecoration2::DecoratedClient::hasApplicationMenuChanged);
    connect(window, &Window::applicationMenuActiveChanged, decoratedClient, &KDecoration2::DecoratedClient::applicationMenuActiveChanged);

    m_toolTipWakeUp.setSingleShot(true);
    connect(&m_toolTipWakeUp, &QTimer::timeout, this, [this]() {
        int fallAsleepDelay = QApplication::style()->styleHint(QStyle::SH_ToolTip_FallAsleepDelay);
        this->m_toolTipFallAsleep.setRemainingTime(fallAsleepDelay);

        QToolTip::showText(Cursors::self()->mouse()->pos(), this->m_toolTipText);
        m_toolTipShowing = true;
    });
}

DecoratedClientImpl::~DecoratedClientImpl()
{
    if (m_toolTipShowing) {
        requestHideToolTip();
    }
}

void DecoratedClientImpl::signalShadeChange()
{
    Q_EMIT decoratedClient()->shadedChanged(m_window->isShade());
}

#define DELEGATE(type, name, clientName)   \
    type DecoratedClientImpl::name() const \
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
DELEGATE2(int, desktop)
DELEGATE2(bool, isOnAllDesktops)
DELEGATE2(QPalette, palette)
DELEGATE2(QIcon, icon)

#undef DELEGATE2
#undef DELEGATE

#define DELEGATE(type, name, clientName)   \
    type DecoratedClientImpl::name() const \
    {                                      \
        return m_window->clientName();     \
    }

DELEGATE(bool, isKeepAbove, keepAbove)
DELEGATE(bool, isKeepBelow, keepBelow)
DELEGATE(bool, isShaded, isShade)
DELEGATE(WId, windowId, window)
DELEGATE(WId, decorationId, frameId)

#undef DELEGATE

#define DELEGATE(name, op)                                                \
    void DecoratedClientImpl::name()                                      \
    {                                                                     \
        Workspace::self()->performWindowOperation(m_window, Options::op); \
    }

DELEGATE(requestToggleShade, ShadeOp)
DELEGATE(requestToggleOnAllDesktops, OnAllDesktopsOp)
DELEGATE(requestToggleKeepAbove, KeepAboveOp)
DELEGATE(requestToggleKeepBelow, KeepBelowOp)

#undef DELEGATE

#define DELEGATE(name, clientName)   \
    void DecoratedClientImpl::name() \
    {                                \
        m_window->clientName();      \
    }

DELEGATE(requestContextHelp, showContextHelp)
DELEGATE(requestMinimize, minimize)

#undef DELEGATE

void DecoratedClientImpl::requestClose()
{
    QMetaObject::invokeMethod(m_window, &Window::closeWindow, Qt::QueuedConnection);
}

QColor DecoratedClientImpl::color(KDecoration2::ColorGroup group, KDecoration2::ColorRole role) const
{
    auto dp = m_window->decorationPalette();
    if (dp) {
        return dp->color(group, role);
    }

    return QColor();
}

void DecoratedClientImpl::requestShowToolTip(const QString &text)
{
    if (!workspace()->decorationBridge()->showToolTips()) {
        return;
    }

    m_toolTipText = text;

    int wakeUpDelay = QApplication::style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay);
    m_toolTipWakeUp.start(m_toolTipFallAsleep.hasExpired() ? wakeUpDelay : 20);
}

void DecoratedClientImpl::requestHideToolTip()
{
    m_toolTipWakeUp.stop();
    QToolTip::hideText();
    m_toolTipShowing = false;
}

void DecoratedClientImpl::requestShowWindowMenu(const QRect &rect)
{
    Workspace::self()->showWindowMenu(QRectF(m_window->pos() + rect.topLeft(), m_window->pos() + rect.bottomRight()).toRect(), m_window);
}

void DecoratedClientImpl::requestShowApplicationMenu(const QRect &rect, int actionId)
{
    Workspace::self()->showApplicationMenu(rect, m_window, actionId);
}

void DecoratedClientImpl::showApplicationMenu(int actionId)
{
    decoration()->showApplicationMenu(actionId);
}

void DecoratedClientImpl::requestToggleMaximization(Qt::MouseButtons buttons)
{
    auto operation = options->operationMaxButtonClick(buttons);
    QMetaObject::invokeMethod(
        this, [this, operation] {
            delayedRequestToggleMaximization(operation);
        },
        Qt::QueuedConnection);
}

void DecoratedClientImpl::delayedRequestToggleMaximization(Options::WindowOperation operation)
{
    Workspace::self()->performWindowOperation(m_window, operation);
}

int DecoratedClientImpl::width() const
{
    return m_clientSize.width();
}

int DecoratedClientImpl::height() const
{
    return m_clientSize.height();
}

QSize DecoratedClientImpl::size() const
{
    return m_clientSize;
}

bool DecoratedClientImpl::isMaximizedVertically() const
{
    return m_window->requestedMaximizeMode() & MaximizeVertical;
}

bool DecoratedClientImpl::isMaximized() const
{
    return isMaximizedHorizontally() && isMaximizedVertically();
}

bool DecoratedClientImpl::isMaximizedHorizontally() const
{
    return m_window->requestedMaximizeMode() & MaximizeHorizontal;
}

Qt::Edges DecoratedClientImpl::adjacentScreenEdges() const
{
    Qt::Edges edges;
    const QuickTileMode mode = m_window->quickTileMode();
    if (mode.testFlag(QuickTileFlag::Left)) {
        edges |= Qt::LeftEdge;
        if (!mode.testFlag(QuickTileFlag::Top) && !mode.testFlag(QuickTileFlag::Bottom)) {
            // using complete side
            edges |= Qt::TopEdge | Qt::BottomEdge;
        }
    }
    if (mode.testFlag(QuickTileFlag::Top)) {
        edges |= Qt::TopEdge;
    }
    if (mode.testFlag(QuickTileFlag::Right)) {
        edges |= Qt::RightEdge;
        if (!mode.testFlag(QuickTileFlag::Top) && !mode.testFlag(QuickTileFlag::Bottom)) {
            // using complete side
            edges |= Qt::TopEdge | Qt::BottomEdge;
        }
    }
    if (mode.testFlag(QuickTileFlag::Bottom)) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

bool DecoratedClientImpl::hasApplicationMenu() const
{
    return m_window->hasApplicationMenu();
}

bool DecoratedClientImpl::isApplicationMenuActive() const
{
    return m_window->applicationMenuActive();
}

QString DecoratedClientImpl::windowClass() const
{
    return m_window->resourceName() + QLatin1Char(' ') + m_window->resourceClass();
}

}
}
