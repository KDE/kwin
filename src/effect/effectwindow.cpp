/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/effectwindow.h"
#include "core/output.h"
#include "effect/effecthandler.h"
#include "group.h"
#include "internalwindow.h"
#include "scene/windowitem.h"
#include "virtualdesktops.h"
#include "waylandwindow.h"
#include "x11window.h"

namespace KWin
{

class Q_DECL_HIDDEN EffectWindow::Private
{
public:
    Private(EffectWindow *q, WindowItem *windowItem);

    EffectWindow *q;
    Window *m_window;
    WindowItem *m_windowItem; // This one is used only during paint pass.
    QHash<int, QVariant> dataMap;
    bool managed = false;
    bool m_waylandWindow;
    bool m_x11Window;
};

EffectWindow::Private::Private(EffectWindow *q, WindowItem *windowItem)
    : q(q)
    , m_window(windowItem->window())
    , m_windowItem(windowItem)
{
}

EffectWindow::EffectWindow(WindowItem *windowItem)
    : d(new Private(this, windowItem))
{
    // Deleted windows are not managed. So, when windowClosed signal is
    // emitted, effects can't distinguish managed windows from unmanaged
    // windows(e.g. combo box popups, popup menus, etc). Save value of the
    // managed property during construction of EffectWindow. At that time,
    // parent can be Client, XdgShellClient, or Unmanaged. So, later on, when
    // an instance of Deleted becomes parent of the EffectWindow, effects
    // can still figure out whether it is/was a managed window.
    d->managed = d->m_window->isClient();

    d->m_waylandWindow = qobject_cast<KWin::WaylandWindow *>(d->m_window) != nullptr;
    d->m_x11Window = qobject_cast<KWin::X11Window *>(d->m_window) != nullptr;

    connect(d->m_window, &Window::windowShown, this, [this]() {
        Q_EMIT windowShown(this);
    });
    connect(d->m_window, &Window::windowHidden, this, [this]() {
        Q_EMIT windowHidden(this);
    });
    connect(d->m_window, &Window::maximizedChanged, this, [this]() {
        const MaximizeMode mode = d->m_window->maximizeMode();
        Q_EMIT windowMaximizedStateChanged(this, mode & MaximizeHorizontal, mode & MaximizeVertical);
    });
    connect(d->m_window, &Window::maximizedAboutToChange, this, [this](MaximizeMode m) {
        Q_EMIT windowMaximizedStateAboutToChange(this, m & MaximizeHorizontal, m & MaximizeVertical);
    });
    connect(d->m_window, &Window::frameGeometryAboutToChange, this, [this]() {
        Q_EMIT windowFrameGeometryAboutToChange(this);
    });
    connect(d->m_window, &Window::interactiveMoveResizeStarted, this, [this]() {
        Q_EMIT windowStartUserMovedResized(this);
    });
    connect(d->m_window, &Window::interactiveMoveResizeStepped, this, [this](const QRectF &geometry) {
        Q_EMIT windowStepUserMovedResized(this, geometry);
    });
    connect(d->m_window, &Window::interactiveMoveResizeFinished, this, [this]() {
        Q_EMIT windowFinishUserMovedResized(this);
    });
    connect(d->m_window, &Window::opacityChanged, this, [this](Window *window, qreal oldOpacity) {
        Q_EMIT windowOpacityChanged(this, oldOpacity, window->opacity());
    });
    connect(d->m_window, &Window::minimizedChanged, this, [this]() {
        Q_EMIT minimizedChanged(this);
    });
    connect(d->m_window, &Window::modalChanged, this, [this]() {
        Q_EMIT windowModalityChanged(this);
    });
    connect(d->m_window, &Window::frameGeometryChanged, this, [this](const QRectF &oldGeometry) {
        Q_EMIT windowFrameGeometryChanged(this, oldGeometry);
    });
    connect(d->m_window, &Window::damaged, this, [this]() {
        Q_EMIT windowDamaged(this);
    });
    connect(d->m_window, &Window::unresponsiveChanged, this, [this](bool unresponsive) {
        Q_EMIT windowUnresponsiveChanged(this, unresponsive);
    });
    connect(d->m_window, &Window::keepAboveChanged, this, [this]() {
        Q_EMIT windowKeepAboveChanged(this);
    });
    connect(d->m_window, &Window::keepBelowChanged, this, [this]() {
        Q_EMIT windowKeepBelowChanged(this);
    });
    connect(d->m_window, &Window::fullScreenChanged, this, [this]() {
        Q_EMIT windowFullScreenChanged(this);
    });
    connect(d->m_window, &Window::visibleGeometryChanged, this, [this]() {
        Q_EMIT windowExpandedGeometryChanged(this);
    });
    connect(d->m_window, &Window::decorationChanged, this, [this]() {
        Q_EMIT windowDecorationChanged(this);
    });
    connect(d->m_window, &Window::desktopsChanged, this, [this]() {
        Q_EMIT windowDesktopsChanged(this);
    });
}

EffectWindow::~EffectWindow()
{
}

Window *EffectWindow::window() const
{
    return d->m_window;
}

WindowItem *EffectWindow::windowItem() const
{
    return d->m_windowItem;
}

bool EffectWindow::isOnActivity(const QString &activity) const
{
    const QStringList _activities = activities();
    return _activities.isEmpty() || _activities.contains(activity);
}

bool EffectWindow::isOnAllActivities() const
{
    return activities().isEmpty();
}

void EffectWindow::setMinimized(bool min)
{
    if (min) {
        minimize();
    } else {
        unminimize();
    }
}

bool EffectWindow::isOnCurrentActivity() const
{
    return isOnActivity(effects->currentActivity());
}

bool EffectWindow::isOnCurrentDesktop() const
{
    return isOnDesktop(effects->currentDesktop());
}

bool EffectWindow::isOnDesktop(VirtualDesktop *desktop) const
{
    const QList<VirtualDesktop *> ds = desktops();
    return ds.isEmpty() || ds.contains(desktop);
}

bool EffectWindow::isOnAllDesktops() const
{
    return desktops().isEmpty();
}

bool EffectWindow::hasDecoration() const
{
    return contentsRect() != QRect(0, 0, width(), height());
}

bool EffectWindow::isVisible() const
{
    return !isMinimized()
        && isOnCurrentDesktop()
        && isOnCurrentActivity();
}

void EffectWindow::refVisible(const EffectWindowVisibleRef *holder)
{
    d->m_windowItem->refVisible(holder->reason());
}

void EffectWindow::unrefVisible(const EffectWindowVisibleRef *holder)
{
    d->m_windowItem->unrefVisible(holder->reason());
}

void EffectWindow::addRepaint(const QRect &r)
{
    d->m_windowItem->scheduleRepaint(QRegion(r));
}

void EffectWindow::addRepaintFull()
{
    d->m_windowItem->scheduleRepaint(d->m_windowItem->boundingRect());
}

void EffectWindow::addLayerRepaint(const QRect &r)
{
    d->m_windowItem->scheduleRepaint(d->m_windowItem->mapFromGlobal(r));
}

const EffectWindowGroup *EffectWindow::group() const
{
    if (Group *group = d->m_window->group()) {
        return group->effectGroup();
    }
    return nullptr;
}

void EffectWindow::refWindow()
{
    if (d->m_window->isDeleted()) {
        return d->m_window->ref();
    }
    Q_UNREACHABLE(); // TODO
}

void EffectWindow::unrefWindow()
{
    if (d->m_window->isDeleted()) {
        return d->m_window->unref();
    }
    Q_UNREACHABLE(); // TODO
}

Output *EffectWindow::screen() const
{
    return d->m_window->output();
}

#define WINDOW_HELPER(rettype, prototype, toplevelPrototype) \
    rettype EffectWindow::prototype() const                  \
    {                                                        \
        return d->m_window->toplevelPrototype();             \
    }

WINDOW_HELPER(double, opacity, opacity)
WINDOW_HELPER(qreal, x, x)
WINDOW_HELPER(qreal, y, y)
WINDOW_HELPER(qreal, width, width)
WINDOW_HELPER(qreal, height, height)
WINDOW_HELPER(QPointF, pos, pos)
WINDOW_HELPER(QSizeF, size, size)
WINDOW_HELPER(QRectF, frameGeometry, frameGeometry)
WINDOW_HELPER(QRectF, bufferGeometry, bufferGeometry)
WINDOW_HELPER(QRectF, clientGeometry, clientGeometry)
WINDOW_HELPER(QRectF, expandedGeometry, visibleGeometry)
WINDOW_HELPER(QRectF, rect, rect)
WINDOW_HELPER(bool, isDesktop, isDesktop)
WINDOW_HELPER(bool, isDock, isDock)
WINDOW_HELPER(bool, isToolbar, isToolbar)
WINDOW_HELPER(bool, isMenu, isMenu)
WINDOW_HELPER(bool, isNormalWindow, isNormalWindow)
WINDOW_HELPER(bool, isDialog, isDialog)
WINDOW_HELPER(bool, isSplash, isSplash)
WINDOW_HELPER(bool, isUtility, isUtility)
WINDOW_HELPER(bool, isDropdownMenu, isDropdownMenu)
WINDOW_HELPER(bool, isPopupMenu, isPopupMenu)
WINDOW_HELPER(bool, isTooltip, isTooltip)
WINDOW_HELPER(bool, isNotification, isNotification)
WINDOW_HELPER(bool, isCriticalNotification, isCriticalNotification)
WINDOW_HELPER(bool, isAppletPopup, isAppletPopup)
WINDOW_HELPER(bool, isOnScreenDisplay, isOnScreenDisplay)
WINDOW_HELPER(bool, isComboBox, isComboBox)
WINDOW_HELPER(bool, isDNDIcon, isDNDIcon)
WINDOW_HELPER(bool, isDeleted, isDeleted)
WINDOW_HELPER(QString, windowRole, windowRole)
WINDOW_HELPER(QStringList, activities, activities)
WINDOW_HELPER(bool, skipsCloseAnimation, skipsCloseAnimation)
WINDOW_HELPER(SurfaceInterface *, surface, surface)
WINDOW_HELPER(bool, isPopupWindow, isPopupWindow)
WINDOW_HELPER(bool, isOutline, isOutline)
WINDOW_HELPER(bool, isLockScreen, isLockScreen)
WINDOW_HELPER(pid_t, pid, pid)
WINDOW_HELPER(QUuid, internalId, internalId)
WINDOW_HELPER(bool, isMinimized, isMinimized)
WINDOW_HELPER(bool, isHidden, isHidden)
WINDOW_HELPER(bool, isHiddenByShowDesktop, isHiddenByShowDesktop)
WINDOW_HELPER(bool, isModal, isModal)
WINDOW_HELPER(bool, isFullScreen, isFullScreen)
WINDOW_HELPER(bool, keepAbove, keepAbove)
WINDOW_HELPER(bool, keepBelow, keepBelow)
WINDOW_HELPER(QString, caption, caption)
WINDOW_HELPER(bool, isMovable, isMovable)
WINDOW_HELPER(bool, isMovableAcrossScreens, isMovableAcrossScreens)
WINDOW_HELPER(bool, isUserMove, isInteractiveMove)
WINDOW_HELPER(bool, isUserResize, isInteractiveResize)
WINDOW_HELPER(QRectF, iconGeometry, iconGeometry)
WINDOW_HELPER(bool, isSpecialWindow, isSpecialWindow)
WINDOW_HELPER(bool, acceptsFocus, wantsInput)
WINDOW_HELPER(QIcon, icon, icon)
WINDOW_HELPER(bool, isSkipSwitcher, skipSwitcher)
WINDOW_HELPER(bool, decorationHasAlpha, decorationHasAlpha)
WINDOW_HELPER(bool, isUnresponsive, unresponsive)
WINDOW_HELPER(QList<VirtualDesktop *>, desktops, desktops)
WINDOW_HELPER(bool, isInputMethod, isInputMethod)

#undef WINDOW_HELPER

qlonglong EffectWindow::windowId() const
{
    if (X11Window *x11Window = qobject_cast<X11Window *>(d->m_window)) {
        return x11Window->window();
    }
    return 0;
}

QString EffectWindow::windowClass() const
{
    return d->m_window->resourceName() + QLatin1Char(' ') + d->m_window->resourceClass();
}

QRectF EffectWindow::contentsRect() const
{
    return QRectF(d->m_window->clientPos(), d->m_window->clientSize());
}

NET::WindowType EffectWindow::windowType() const
{
    return d->m_window->windowType();
}

QSizeF EffectWindow::basicUnit() const
{
    if (auto window = qobject_cast<X11Window *>(d->m_window)) {
        return window->basicUnit();
    }
    return QSize(1, 1);
}

QRectF EffectWindow::decorationInnerRect() const
{
    return d->m_window->rect() - d->m_window->frameMargins();
}

KDecoration2::Decoration *EffectWindow::decoration() const
{
    return d->m_window->decoration();
}

QByteArray EffectWindow::readProperty(long atom, long type, int format) const
{
    auto x11Window = qobject_cast<X11Window *>(d->m_window);
    if (!x11Window) {
        return QByteArray();
    }
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    uint32_t len = 32768;
    for (;;) {
        Xcb::Property prop(false, x11Window->window(), atom, XCB_ATOM_ANY, 0, len);
        if (prop.isNull()) {
            // get property failed
            return QByteArray();
        }
        if (prop->bytes_after > 0) {
            len *= 2;
            continue;
        }
        return prop.toByteArray(format, type);
    }
}

void EffectWindow::deleteProperty(long int atom) const
{
    auto x11Window = qobject_cast<X11Window *>(d->m_window);
    if (!x11Window) {
        return;
    }
    if (!kwinApp()->x11Connection()) {
        return;
    }
    xcb_delete_property(kwinApp()->x11Connection(), x11Window->window(), atom);
}

EffectWindow *EffectWindow::findModal()
{
    Window *modal = d->m_window->findModal();
    if (modal) {
        return modal->effectWindow();
    }

    return nullptr;
}

EffectWindow *EffectWindow::transientFor()
{
    Window *transientFor = d->m_window->transientFor();
    if (transientFor) {
        return transientFor->effectWindow();
    }

    return nullptr;
}

QWindow *EffectWindow::internalWindow() const
{
    if (auto window = qobject_cast<InternalWindow *>(d->m_window)) {
        return window->handle();
    }
    return nullptr;
}

template<typename T>
QList<EffectWindow *> getMainWindows(T *c)
{
    const auto mainwindows = c->mainWindows();
    QList<EffectWindow *> ret;
    ret.reserve(mainwindows.size());
    std::transform(std::cbegin(mainwindows), std::cend(mainwindows),
                   std::back_inserter(ret),
                   [](auto window) {
                       return window->effectWindow();
                   });
    return ret;
}

QList<EffectWindow *> EffectWindow::mainWindows() const
{
    return getMainWindows(d->m_window);
}

void EffectWindow::setData(int role, const QVariant &data)
{
    if (!data.isNull()) {
        d->dataMap[role] = data;
    } else {
        d->dataMap.remove(role);
    }
    Q_EMIT effects->windowDataChanged(this, role);
}

QVariant EffectWindow::data(int role) const
{
    return d->dataMap.value(role);
}

void EffectWindow::elevate(bool elevate)
{
    effects->setElevatedWindow(this, elevate);
}

void EffectWindow::minimize()
{
    if (d->m_window->isClient()) {
        d->m_window->setMinimized(true);
    }
}

void EffectWindow::unminimize()
{
    if (d->m_window->isClient()) {
        d->m_window->setMinimized(false);
    }
}

void EffectWindow::closeWindow()
{
    if (d->m_window->isClient()) {
        d->m_window->closeWindow();
    }
}

bool EffectWindow::isManaged() const
{
    return d->managed;
}

bool EffectWindow::isWaylandClient() const
{
    return d->m_waylandWindow;
}

bool EffectWindow::isX11Client() const
{
    return d->m_x11Window;
}

//****************************************
// EffectWindowGroup
//****************************************

EffectWindowGroup::EffectWindowGroup(Group *group)
    : m_group(group)
{
}

EffectWindowGroup::~EffectWindowGroup()
{
}

QList<EffectWindow *> EffectWindowGroup::members() const
{
    const auto memberList = m_group->members();
    QList<EffectWindow *> ret;
    ret.reserve(memberList.size());
    std::transform(std::cbegin(memberList), std::cend(memberList), std::back_inserter(ret), [](auto window) {
        return window->effectWindow();
    });
    return ret;
}

} // namespace KWin

#include "moc_effectwindow.cpp"
