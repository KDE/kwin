/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "decoratedclient.h"
#include "decorationbridge.h"
#include "decorationpalette.h"
#include "decorationrenderer.h"
#include "abstract_client.h"
#include "composite.h"
#include "cursor.h"
#include "options.h"
#include "platform.h"
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

DecoratedClientImpl::DecoratedClientImpl(AbstractClient *client, KDecoration2::DecoratedClient *decoratedClient, KDecoration2::Decoration *decoration)
    : QObject()
    , ApplicationMenuEnabledDecoratedClientPrivate(decoratedClient, decoration)
    , m_client(client)
    , m_clientSize(client->clientSize())
    , m_renderer(nullptr)
{
    createRenderer();
    client->setDecoratedClient(QPointer<DecoratedClientImpl>(this));
    connect(client, &AbstractClient::activeChanged, this,
        [decoratedClient, client]() {
            emit decoratedClient->activeChanged(client->isActive());
        }
    );
    connect(client, &AbstractClient::clientGeometryChanged, this,
        [decoratedClient, this]() {
            if (m_client->clientSize() == m_clientSize) {
                return;
            }
            const auto oldSize = m_clientSize;
            m_clientSize = m_client->clientSize();
            if (oldSize.width() != m_clientSize.width()) {
                emit decoratedClient->widthChanged(m_clientSize.width());
            }
            if (oldSize.height() != m_clientSize.height()) {
                emit decoratedClient->heightChanged(m_clientSize.height());
            }
            emit decoratedClient->sizeChanged(m_clientSize);
        }
    );
    connect(client, &AbstractClient::desktopChanged, this,
        [decoratedClient, client]() {
            emit decoratedClient->onAllDesktopsChanged(client->isOnAllDesktops());
        }
    );
    connect(client, &AbstractClient::captionChanged, this,
        [decoratedClient, client]() {
            emit decoratedClient->captionChanged(client->caption());
        }
    );
    connect(client, &AbstractClient::iconChanged, this,
        [decoratedClient, client]() {
            emit decoratedClient->iconChanged(client->icon());
        }
    );
    connect(client, &AbstractClient::shadeChanged, this,
            &Decoration::DecoratedClientImpl::signalShadeChange);
    connect(client, &AbstractClient::keepAboveChanged, decoratedClient, &KDecoration2::DecoratedClient::keepAboveChanged);
    connect(client, &AbstractClient::keepBelowChanged, decoratedClient, &KDecoration2::DecoratedClient::keepBelowChanged);
    connect(Compositor::self(), &Compositor::aboutToToggleCompositing, this, &DecoratedClientImpl::destroyRenderer);
    m_compositorToggledConnection = connect(Compositor::self(), &Compositor::compositingToggled, this,
        [this, decoration]() {
            createRenderer();
            decoration->update();
        }
    );
    connect(Compositor::self(), &Compositor::aboutToDestroy, this,
        [this] {
            disconnect(m_compositorToggledConnection);
            m_compositorToggledConnection = QMetaObject::Connection();
        }
    );
    connect(client, &AbstractClient::quickTileModeChanged, decoratedClient,
        [this, decoratedClient]() {
            emit decoratedClient->adjacentScreenEdgesChanged(adjacentScreenEdges());
        }
    );
    connect(client, &AbstractClient::closeableChanged, decoratedClient, &KDecoration2::DecoratedClient::closeableChanged);
    connect(client, &AbstractClient::shadeableChanged, decoratedClient, &KDecoration2::DecoratedClient::shadeableChanged);
    connect(client, &AbstractClient::minimizeableChanged, decoratedClient, &KDecoration2::DecoratedClient::minimizeableChanged);
    connect(client, &AbstractClient::maximizeableChanged, decoratedClient, &KDecoration2::DecoratedClient::maximizeableChanged);

    connect(client, &AbstractClient::paletteChanged, decoratedClient, &KDecoration2::DecoratedClient::paletteChanged);

    connect(client, &AbstractClient::hasApplicationMenuChanged, decoratedClient, &KDecoration2::DecoratedClient::hasApplicationMenuChanged);
    connect(client, &AbstractClient::applicationMenuActiveChanged, decoratedClient, &KDecoration2::DecoratedClient::applicationMenuActiveChanged);

    m_toolTipWakeUp.setSingleShot(true);
    connect(&m_toolTipWakeUp, &QTimer::timeout, this,
            [this]() {
                int fallAsleepDelay = QApplication::style()->styleHint(QStyle::SH_ToolTip_FallAsleepDelay);
                this->m_toolTipFallAsleep.setRemainingTime(fallAsleepDelay);

                QToolTip::showText(Cursors::self()->mouse()->pos(), this->m_toolTipText);
                m_toolTipShowing = true;
            }
    );
}

DecoratedClientImpl::~DecoratedClientImpl()
{
    if (m_toolTipShowing) {
        requestHideToolTip();
    }
}

void DecoratedClientImpl::signalShadeChange() {
    emit decoratedClient()->shadedChanged(m_client->isShade());
}

#define DELEGATE(type, name, clientName) \
    type DecoratedClientImpl::name() const \
    { \
        return m_client->clientName(); \
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

#define DELEGATE(type, name, clientName) \
    type DecoratedClientImpl::name() const \
    { \
        return m_client->clientName(); \
    }

DELEGATE(bool, isKeepAbove, keepAbove)
DELEGATE(bool, isKeepBelow, keepBelow)
DELEGATE(bool, isShaded, isShade)
DELEGATE(WId, windowId, windowId)
DELEGATE(WId, decorationId, frameId)

#undef DELEGATE

#define DELEGATE(name, op) \
    void DecoratedClientImpl::name() \
    { \
        Workspace::self()->performWindowOperation(m_client, Options::op); \
    }

DELEGATE(requestToggleShade, ShadeOp)
DELEGATE(requestToggleOnAllDesktops, OnAllDesktopsOp)
DELEGATE(requestToggleKeepAbove, KeepAboveOp)
DELEGATE(requestToggleKeepBelow, KeepBelowOp)

#undef DELEGATE

#define DELEGATE(name, clientName) \
    void DecoratedClientImpl::name() \
    { \
        m_client->clientName(); \
    }

DELEGATE(requestContextHelp, showContextHelp)
DELEGATE(requestMinimize, minimize)

#undef DELEGATE

void DecoratedClientImpl::requestClose()
{
    QMetaObject::invokeMethod(m_client, "closeWindow", Qt::QueuedConnection);
}

QColor DecoratedClientImpl::color(KDecoration2::ColorGroup group, KDecoration2::ColorRole role) const
{
    auto dp = m_client->decorationPalette();
    if (dp) {
        return dp->color(group, role);
    }

    return QColor();
}

void DecoratedClientImpl::requestShowToolTip(const QString &text)
{
    if (!DecorationBridge::self()->showToolTips()) {
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

void DecoratedClientImpl::requestShowWindowMenu()
{
    // TODO: add rect to requestShowWindowMenu
    Workspace::self()->showWindowMenu(QRect(Cursors::self()->mouse()->pos(), Cursors::self()->mouse()->pos()), m_client);
}

void DecoratedClientImpl::requestShowApplicationMenu(const QRect &rect, int actionId)
{
    Workspace::self()->showApplicationMenu(rect, m_client, actionId);
}

void DecoratedClientImpl::showApplicationMenu(int actionId)
{
    decoration()->showApplicationMenu(actionId);
}

void DecoratedClientImpl::requestToggleMaximization(Qt::MouseButtons buttons)
{
    QMetaObject::invokeMethod(this, "delayedRequestToggleMaximization", Qt::QueuedConnection, Q_ARG(Options::WindowOperation, options->operationMaxButtonClick(buttons)));
}

void DecoratedClientImpl::delayedRequestToggleMaximization(Options::WindowOperation operation)
{
    Workspace::self()->performWindowOperation(m_client, operation);
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
    return m_client->requestedMaximizeMode() & MaximizeVertical;
}

bool DecoratedClientImpl::isMaximized() const
{
    return isMaximizedHorizontally() && isMaximizedVertically();
}

bool DecoratedClientImpl::isMaximizedHorizontally() const
{
    return m_client->requestedMaximizeMode() & MaximizeHorizontal;
}

Qt::Edges DecoratedClientImpl::adjacentScreenEdges() const
{
    Qt::Edges edges;
    const QuickTileMode mode = m_client->quickTileMode();
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
    return m_client->hasApplicationMenu();
}

bool DecoratedClientImpl::isApplicationMenuActive() const
{
    return m_client->applicationMenuActive();
}

void DecoratedClientImpl::createRenderer()
{
    m_renderer = kwinApp()->platform()->createDecorationRenderer(this);
}

void DecoratedClientImpl::destroyRenderer()
{
    delete m_renderer;
    m_renderer = nullptr;
}

}
}
