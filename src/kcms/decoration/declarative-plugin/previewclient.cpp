/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "previewclient.h"
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QModelIndex>

namespace KDecoration3
{
namespace Preview
{

PreviewClient::PreviewClient(DecoratedWindow *c, Decoration *decoration)
    : QObject(decoration)
    , DecoratedWindowPrivateV3(c, decoration)
    , m_icon(QIcon::fromTheme(QStringLiteral("start-here-kde")))
    , m_iconName(m_icon.name())
    , m_palette(QStringLiteral("kdeglobals"))
    , m_active(true)
    , m_closeable(true)
    , m_keepBelow(false)
    , m_keepAbove(false)
    , m_maximizable(true)
    , m_maximizedHorizontally(false)
    , m_maximizedVertically(false)
    , m_minimizable(true)
    , m_modal(false)
    , m_movable(true)
    , m_resizable(true)
    , m_shadeable(true)
    , m_shaded(false)
    , m_providesContextHelp(false)
    , m_onAllDesktops(false)
    , m_width(0)
    , m_height(0)
    , m_bordersTopEdge(false)
    , m_bordersLeftEdge(false)
    , m_bordersRightEdge(false)
    , m_bordersBottomEdge(false)
{
    connect(this, &PreviewClient::captionChanged, c, &DecoratedWindow::captionChanged);
    connect(this, &PreviewClient::activeChanged, c, &DecoratedWindow::activeChanged);
    connect(this, &PreviewClient::closeableChanged, c, &DecoratedWindow::closeableChanged);
    connect(this, &PreviewClient::keepAboveChanged, c, &DecoratedWindow::keepAboveChanged);
    connect(this, &PreviewClient::keepBelowChanged, c, &DecoratedWindow::keepBelowChanged);
    connect(this, &PreviewClient::maximizableChanged, c, &DecoratedWindow::maximizeableChanged);
    connect(this, &PreviewClient::maximizedChanged, c, &DecoratedWindow::maximizedChanged);
    connect(this, &PreviewClient::maximizedVerticallyChanged, c, &DecoratedWindow::maximizedVerticallyChanged);
    connect(this, &PreviewClient::maximizedHorizontallyChanged, c, &DecoratedWindow::maximizedHorizontallyChanged);
    connect(this, &PreviewClient::minimizableChanged, c, &DecoratedWindow::minimizeableChanged);
    connect(this, &PreviewClient::movableChanged, c, &DecoratedWindow::moveableChanged);
    connect(this, &PreviewClient::onAllDesktopsChanged, c, &DecoratedWindow::onAllDesktopsChanged);
    connect(this, &PreviewClient::resizableChanged, c, &DecoratedWindow::resizeableChanged);
    connect(this, &PreviewClient::shadeableChanged, c, &DecoratedWindow::shadeableChanged);
    connect(this, &PreviewClient::shadedChanged, c, &DecoratedWindow::shadedChanged);
    connect(this, &PreviewClient::providesContextHelpChanged, c, &DecoratedWindow::providesContextHelpChanged);
    connect(this, &PreviewClient::widthChanged, c, &DecoratedWindow::widthChanged);
    connect(this, &PreviewClient::heightChanged, c, &DecoratedWindow::heightChanged);
    connect(this, &PreviewClient::iconChanged, c, &DecoratedWindow::iconChanged);
    connect(this, &PreviewClient::paletteChanged, c, &DecoratedWindow::paletteChanged);
    connect(this, &PreviewClient::maximizedVerticallyChanged, this, [this]() {
        Q_EMIT maximizedChanged(isMaximized());
    });
    connect(this, &PreviewClient::maximizedHorizontallyChanged, this, [this]() {
        Q_EMIT maximizedChanged(isMaximized());
    });
    connect(this, &PreviewClient::iconNameChanged, this, [this]() {
        m_icon = QIcon::fromTheme(m_iconName);
        Q_EMIT iconChanged(m_icon);
    });
    connect(&m_palette, &KWin::Decoration::DecorationPalette::changed, this, [this]() {
        Q_EMIT paletteChanged(m_palette.palette());
    });
    auto emitEdgesChanged = [this, c]() {
        Q_EMIT c->adjacentScreenEdgesChanged(adjacentScreenEdges());
    };
    connect(this, &PreviewClient::bordersTopEdgeChanged, this, emitEdgesChanged);
    connect(this, &PreviewClient::bordersLeftEdgeChanged, this, emitEdgesChanged);
    connect(this, &PreviewClient::bordersRightEdgeChanged, this, emitEdgesChanged);
    connect(this, &PreviewClient::bordersBottomEdgeChanged, this, emitEdgesChanged);
    auto emitSizeChanged = [c]() {
        Q_EMIT c->sizeChanged(c->size());
    };
    connect(this, &PreviewClient::widthChanged, this, emitSizeChanged);
    connect(this, &PreviewClient::heightChanged, this, emitSizeChanged);

    qApp->installEventFilter(this);
}

PreviewClient::~PreviewClient() = default;

void PreviewClient::setIcon(const QIcon &pixmap)
{
    m_icon = pixmap;
    Q_EMIT iconChanged(m_icon);
}

qreal PreviewClient::width() const
{
    return m_width;
}

qreal PreviewClient::height() const
{
    return m_height;
}

QSizeF PreviewClient::size() const
{
    return QSize(m_width, m_height);
}

QString PreviewClient::caption() const
{
    return m_caption;
}

QIcon PreviewClient::icon() const
{
    return m_icon;
}

QString PreviewClient::iconName() const
{
    return m_iconName;
}

bool PreviewClient::isActive() const
{
    return m_active;
}

bool PreviewClient::isCloseable() const
{
    return m_closeable;
}

bool PreviewClient::isKeepAbove() const
{
    return m_keepAbove;
}

bool PreviewClient::isKeepBelow() const
{
    return m_keepBelow;
}

bool PreviewClient::isMaximizeable() const
{
    return m_maximizable;
}

bool PreviewClient::isMaximized() const
{
    return isMaximizedHorizontally() && isMaximizedVertically();
}

bool PreviewClient::isMaximizedHorizontally() const
{
    return m_maximizedHorizontally;
}

bool PreviewClient::isMaximizedVertically() const
{
    return m_maximizedVertically;
}

bool PreviewClient::isMinimizeable() const
{
    return m_minimizable;
}

bool PreviewClient::isModal() const
{
    return m_modal;
}

bool PreviewClient::isMoveable() const
{
    return m_movable;
}

bool PreviewClient::isOnAllDesktops() const
{
    return m_onAllDesktops;
}

bool PreviewClient::isResizeable() const
{
    return m_resizable;
}

bool PreviewClient::isShadeable() const
{
    return m_shadeable;
}

bool PreviewClient::isShaded() const
{
    return m_shaded;
}

bool PreviewClient::providesContextHelp() const
{
    return m_providesContextHelp;
}

QPalette PreviewClient::palette() const
{
    return m_palette.palette();
}

QColor PreviewClient::color(ColorGroup group, ColorRole role) const
{
    return m_palette.color(group, role);
}

Qt::Edges PreviewClient::adjacentScreenEdges() const
{
    Qt::Edges edges;
    if (m_bordersBottomEdge) {
        edges |= Qt::BottomEdge;
    }
    if (m_bordersLeftEdge) {
        edges |= Qt::LeftEdge;
    }
    if (m_bordersRightEdge) {
        edges |= Qt::RightEdge;
    }
    if (m_bordersTopEdge) {
        edges |= Qt::TopEdge;
    }
    return edges;
}

QString PreviewClient::windowClass() const
{
    return QString();
}

qreal PreviewClient::scale() const
{
    return 1;
}

qreal PreviewClient::nextScale() const
{
    return 1;
}

QString PreviewClient::applicationMenuServiceName() const
{
    return QString();
}

QString PreviewClient::applicationMenuObjectPath() const
{
    return QString();
}

bool PreviewClient::hasApplicationMenu() const
{
    return true;
}

bool PreviewClient::isApplicationMenuActive() const
{
    return false;
}

bool PreviewClient::bordersBottomEdge() const
{
    return m_bordersBottomEdge;
}

bool PreviewClient::bordersLeftEdge() const
{
    return m_bordersLeftEdge;
}

bool PreviewClient::bordersRightEdge() const
{
    return m_bordersRightEdge;
}

bool PreviewClient::bordersTopEdge() const
{
    return m_bordersTopEdge;
}

void PreviewClient::setBordersBottomEdge(bool enabled)
{
    if (m_bordersBottomEdge == enabled) {
        return;
    }
    m_bordersBottomEdge = enabled;
    Q_EMIT bordersBottomEdgeChanged(enabled);
}

void PreviewClient::setBordersLeftEdge(bool enabled)
{
    if (m_bordersLeftEdge == enabled) {
        return;
    }
    m_bordersLeftEdge = enabled;
    Q_EMIT bordersLeftEdgeChanged(enabled);
}

void PreviewClient::setBordersRightEdge(bool enabled)
{
    if (m_bordersRightEdge == enabled) {
        return;
    }
    m_bordersRightEdge = enabled;
    Q_EMIT bordersRightEdgeChanged(enabled);
}

void PreviewClient::setBordersTopEdge(bool enabled)
{
    if (m_bordersTopEdge == enabled) {
        return;
    }
    m_bordersTopEdge = enabled;
    Q_EMIT bordersTopEdgeChanged(enabled);
}

void PreviewClient::requestShowToolTip(const QString &text)
{
}

void PreviewClient::requestHideToolTip()
{
}

void PreviewClient::requestClose()
{
    Q_EMIT closeRequested();
}

void PreviewClient::requestContextHelp()
{
}

void PreviewClient::requestToggleMaximization(Qt::MouseButtons buttons)
{
    if (buttons.testFlag(Qt::LeftButton)) {
        const bool set = !isMaximized();
        setMaximizedHorizontally(set);
        setMaximizedVertically(set);
    } else if (buttons.testFlag(Qt::RightButton)) {
        setMaximizedHorizontally(!isMaximizedHorizontally());
    } else if (buttons.testFlag(Qt::MiddleButton)) {
        setMaximizedVertically(!isMaximizedVertically());
    }
}

void PreviewClient::requestMinimize()
{
    Q_EMIT minimizeRequested();
}

void PreviewClient::requestToggleKeepAbove()
{
    setKeepAbove(!isKeepAbove());
}

void PreviewClient::requestToggleKeepBelow()
{
    setKeepBelow(!isKeepBelow());
}

void PreviewClient::requestShowWindowMenu(const QRect &rect)
{
    Q_EMIT showWindowMenuRequested();
}

void PreviewClient::requestShowApplicationMenu(const QRect &rect, int actionId)
{
}

void PreviewClient::showApplicationMenu(int actionId)
{
}

void PreviewClient::requestToggleOnAllDesktops()
{
    m_onAllDesktops = !m_onAllDesktops;
    Q_EMIT onAllDesktopsChanged(m_onAllDesktops);
}

void PreviewClient::requestToggleShade()
{
    setShaded(!isShaded());
}

#define SETTER(type, name, variable)            \
    void PreviewClient::name(type variable)     \
    {                                           \
        if (m_##variable == variable) {         \
            return;                             \
        }                                       \
        m_##variable = variable;                \
        Q_EMIT variable##Changed(m_##variable); \
    }

#define SETTER2(name, variable) SETTER(bool, name, variable)

SETTER(const QString &, setCaption, caption)
SETTER(const QString &, setIconName, iconName)
SETTER(int, setWidth, width)
SETTER(int, setHeight, height)

SETTER2(setActive, active)
SETTER2(setCloseable, closeable)
SETTER2(setMaximizable, maximizable)
SETTER2(setKeepBelow, keepBelow)
SETTER2(setKeepAbove, keepAbove)
SETTER2(setMaximizedHorizontally, maximizedHorizontally)
SETTER2(setMaximizedVertically, maximizedVertically)
SETTER2(setMinimizable, minimizable)
SETTER2(setModal, modal)
SETTER2(setMovable, movable)
SETTER2(setResizable, resizable)
SETTER2(setShadeable, shadeable)
SETTER2(setShaded, shaded)
SETTER2(setProvidesContextHelp, providesContextHelp)

#undef SETTER2
#undef SETTER

void PreviewClient::popup(const KDecoration3::Positioner &positioner, QMenu *menu)
{
}

} // namespace Preview
} // namespace KDecoration3

#include "moc_previewclient.cpp"
