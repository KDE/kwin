/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstract_client.h"

#include "abstract_output.h"
#include "abstract_wayland_output.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "appmenu.h"
#include "decorations/decoratedclient.h"
#include "decorations/decorationpalette.h"
#include "decorations/decorationbridge.h"
#include "focuschain.h"
#include "outline.h"
#include "platform.h"
#include "screens.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "screenedge.h"
#include "useractions.h"
#include "virtualdesktops.h"
#include "workspace.h"

#include "wayland_server.h"
#include <KWaylandServer/plasmawindowmanagement_interface.h>
#include <KWaylandServer/output_interface.h>

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>

#include <KDesktopFile>

#include <QDir>
#include <QMouseEvent>
#include <QStyleHints>

namespace KWin
{

static inline int sign(int v)
{
    return (v > 0) - (v < 0);
}

QHash<QString, std::weak_ptr<Decoration::DecorationPalette>> AbstractClient::s_palettes;
std::shared_ptr<Decoration::DecorationPalette> AbstractClient::s_defaultPalette;

AbstractClient::AbstractClient()
    : Toplevel()
#ifdef KWIN_BUILD_TABBOX
    , m_tabBoxClient(QSharedPointer<TabBox::TabBoxClientImpl>(new TabBox::TabBoxClientImpl(this)))
#endif
    , m_colorScheme(QStringLiteral("kdeglobals"))
{
    connect(this, &AbstractClient::clientStartUserMovedResized,  this, &AbstractClient::moveResizedChanged);
    connect(this, &AbstractClient::clientFinishUserMovedResized, this, &AbstractClient::moveResizedChanged);
    connect(this, &AbstractClient::clientStartUserMovedResized,  this, &AbstractClient::removeCheckOutputConnection);
    connect(this, &AbstractClient::clientFinishUserMovedResized, this, &AbstractClient::setupCheckOutputConnection);

    connect(this, &AbstractClient::windowShown, this, &AbstractClient::hiddenChanged);
    connect(this, &AbstractClient::windowHidden, this, &AbstractClient::hiddenChanged);

    connect(this, &AbstractClient::paletteChanged, this, &AbstractClient::triggerDecorationRepaint);

    // If the user manually moved the window, don't restore it after the keyboard closes
    connect(this, &AbstractClient::clientFinishUserMovedResized, this, [this] () {
        m_keyboardGeometryRestore = QRect();
    });
    connect(this, qOverload<AbstractClient *, bool, bool>(&AbstractClient::clientMaximizedStateChanged), this, [this] () {
        m_keyboardGeometryRestore = QRect();
    });
    connect(this, &AbstractClient::fullScreenChanged, this, [this] () {
        m_keyboardGeometryRestore = QRect();
    });

    // replace on-screen-display on size changes
    connect(this, &AbstractClient::frameGeometryChanged, this,
        [this] (Toplevel *c, const QRect &old) {
            Q_UNUSED(c)
            if (isOnScreenDisplay() && !frameGeometry().isEmpty() && old.size() != frameGeometry().size() && isPlaceable()) {
                GeometryUpdatesBlocker blocker(this);
                Placement::self()->place(this, workspace()->clientArea(PlacementArea, this, workspace()->activeOutput()));
            }
        }
    );

    connect(ApplicationMenu::self(), &ApplicationMenu::applicationMenuEnabledChanged, this, [this] {
        Q_EMIT hasApplicationMenuChanged(hasApplicationMenu());
    });
}

AbstractClient::~AbstractClient()
{
    Q_ASSERT(m_blockGeometryUpdates == 0);
    Q_ASSERT(m_decoration.decoration == nullptr);
}

void AbstractClient::updateMouseGrab()
{
}

bool AbstractClient::belongToSameApplication(const AbstractClient *c1, const AbstractClient *c2, SameApplicationChecks checks)
{
    return c1->belongsToSameApplication(c2, checks);
}

bool AbstractClient::isTransient() const
{
    return false;
}

void AbstractClient::setClientShown(bool shown)
{
    Q_UNUSED(shown)
}

xcb_timestamp_t AbstractClient::userTime() const
{
    return XCB_TIME_CURRENT_TIME;
}

void AbstractClient::setSkipSwitcher(bool set)
{
    set = rules()->checkSkipSwitcher(set);
    if (set == skipSwitcher())
        return;
    m_skipSwitcher = set;
    doSetSkipSwitcher();
    updateWindowRules(Rules::SkipSwitcher);
    Q_EMIT skipSwitcherChanged();
}

void AbstractClient::setSkipPager(bool b)
{
    b = rules()->checkSkipPager(b);
    if (b == skipPager())
        return;
    m_skipPager = b;
    doSetSkipPager();
    updateWindowRules(Rules::SkipPager);
    Q_EMIT skipPagerChanged();
}

void AbstractClient::doSetSkipPager()
{
}

void AbstractClient::setSkipTaskbar(bool b)
{
    int was_wants_tab_focus = wantsTabFocus();
    if (b == skipTaskbar())
        return;
    m_skipTaskbar = b;
    doSetSkipTaskbar();
    updateWindowRules(Rules::SkipTaskbar);
    if (was_wants_tab_focus != wantsTabFocus()) {
        FocusChain::self()->update(this, isActive() ? FocusChain::MakeFirst : FocusChain::Update);
    }
    Q_EMIT skipTaskbarChanged();
}

void AbstractClient::setOriginalSkipTaskbar(bool b)
{
    m_originalSkipTaskbar = rules()->checkSkipTaskbar(b);
    setSkipTaskbar(m_originalSkipTaskbar);
}

void AbstractClient::doSetSkipTaskbar()
{

}

void AbstractClient::doSetSkipSwitcher()
{

}

void AbstractClient::setIcon(const QIcon &icon)
{
    m_icon = icon;
    Q_EMIT iconChanged();
}

void AbstractClient::setActive(bool act)
{
    if (isZombie()) {
        return;
    }
    if (m_active == act) {
        return;
    }
    m_active = act;
    const int ruledOpacity = m_active
                             ? rules()->checkOpacityActive(qRound(opacity() * 100.0))
                             : rules()->checkOpacityInactive(qRound(opacity() * 100.0));
    setOpacity(ruledOpacity / 100.0);
    workspace()->setActiveClient(act ? this : nullptr);

    if (!m_active)
        cancelAutoRaise();

    if (!m_active && shadeMode() == ShadeActivated)
        setShade(ShadeNormal);

    StackingUpdatesBlocker blocker(workspace());
    updateLayer(); // active windows may get different layer
    auto mainclients = mainClients();
    for (auto it = mainclients.constBegin();
            it != mainclients.constEnd();
            ++it)
        if ((*it)->isFullScreen())  // fullscreens go high even if their transient is active
            (*it)->updateLayer();

    doSetActive();
    Q_EMIT activeChanged();
    updateMouseGrab();
}

void AbstractClient::doSetActive()
{
}

bool AbstractClient::isZombie() const
{
    return m_zombie;
}

void AbstractClient::markAsZombie()
{
    Q_ASSERT(!m_zombie);
    m_zombie = true;
}

Layer AbstractClient::layer() const
{
    if (m_layer == UnknownLayer)
        const_cast< AbstractClient* >(this)->m_layer = belongsToLayer();
    return m_layer;
}

void AbstractClient::updateLayer()
{
    if (layer() == belongsToLayer())
        return;
    StackingUpdatesBlocker blocker(workspace());
    invalidateLayer(); // invalidate, will be updated when doing restacking
    for (auto it = transients().constBegin(),
                                  end = transients().constEnd(); it != end; ++it)
        (*it)->updateLayer();
}

void AbstractClient::invalidateLayer()
{
    m_layer = UnknownLayer;
}

Layer AbstractClient::belongsToLayer() const
{
    // NOTICE while showingDesktop, desktops move to the AboveLayer
    // (interchangeable w/ eg. yakuake etc. which will at first remain visible)
    // and the docks move into the NotificationLayer (which is between Above- and
    // ActiveLayer, so that active fullscreen windows will still cover everything)
    // Since the desktop is also activated, nothing should be in the ActiveLayer, though
    if (isInternal())
        return UnmanagedLayer;
    if (isLockScreen())
        return UnmanagedLayer;
    if (isInputMethod())
        return UnmanagedLayer;
    if (isDesktop())
        return workspace()->showingDesktop() ? AboveLayer : DesktopLayer;
    if (isSplash())          // no damn annoying splashscreens
        return NormalLayer; // getting in the way of everything else
    if (isDock()) {
        if (workspace()->showingDesktop())
            return NotificationLayer;
        return layerForDock();
    }
    if (isPopupWindow())
        return PopupLayer;
    if (isOnScreenDisplay())
        return OnScreenDisplayLayer;
    if (isNotification())
        return NotificationLayer;
    if (isCriticalNotification())
        return CriticalNotificationLayer;
    if (workspace()->showingDesktop() && belongsToDesktop()) {
        return AboveLayer;
    }
    if (keepBelow())
        return BelowLayer;
    if (isActiveFullScreen())
        return ActiveLayer;
    if (keepAbove())
        return AboveLayer;

    return NormalLayer;
}

bool AbstractClient::belongsToDesktop() const
{
    return false;
}

Layer AbstractClient::layerForDock() const
{
    // slight hack for the 'allow window to cover panel' Kicker setting
    // don't move keepbelow docks below normal window, but only to the same
    // layer, so that both may be raised to cover the other
    if (keepBelow())
        return NormalLayer;
    if (keepAbove()) // slight hack for the autohiding panels
        return AboveLayer;
    return DockLayer;
}

void AbstractClient::setKeepAbove(bool b)
{
    b = rules()->checkKeepAbove(b);
    if (b && !rules()->checkKeepBelow(false))
        setKeepBelow(false);
    if (b == keepAbove()) {
        return;
    }
    m_keepAbove = b;
    doSetKeepAbove();
    updateLayer();
    updateWindowRules(Rules::Above);

    Q_EMIT keepAboveChanged(m_keepAbove);
}

void AbstractClient::doSetKeepAbove()
{
}

void AbstractClient::setKeepBelow(bool b)
{
    b = rules()->checkKeepBelow(b);
    if (b && !rules()->checkKeepAbove(false))
        setKeepAbove(false);
    if (b == keepBelow()) {
        return;
    }
    m_keepBelow = b;
    doSetKeepBelow();
    updateLayer();
    updateWindowRules(Rules::Below);

    Q_EMIT keepBelowChanged(m_keepBelow);
}

void AbstractClient::doSetKeepBelow()
{
}

void AbstractClient::startAutoRaise()
{
    delete m_autoRaiseTimer;
    m_autoRaiseTimer = new QTimer(this);
    connect(m_autoRaiseTimer, &QTimer::timeout, this, &AbstractClient::autoRaise);
    m_autoRaiseTimer->setSingleShot(true);
    m_autoRaiseTimer->start(options->autoRaiseInterval());
}

void AbstractClient::cancelAutoRaise()
{
    delete m_autoRaiseTimer;
    m_autoRaiseTimer = nullptr;
}

void AbstractClient::autoRaise()
{
    workspace()->raiseClient(this);
    cancelAutoRaise();
}

bool AbstractClient::isMostRecentlyRaised() const
{
    // The last toplevel in the unconstrained stacking order is the most recently raised one.
    return workspace()->topClientOnDesktop(VirtualDesktopManager::self()->currentDesktop(), nullptr, true, false) == this;
}

bool AbstractClient::wantsTabFocus() const
{
    return (isNormalWindow() || isDialog()) && wantsInput();
}

bool AbstractClient::isSpecialWindow() const
{
    // TODO
    return isDesktop() || isDock() || isSplash() || isToolbar() || isNotification() || isOnScreenDisplay() || isCriticalNotification();
}

void AbstractClient::demandAttention(bool set)
{
    if (isActive())
        set = false;
    if (m_demandsAttention == set)
        return;
    m_demandsAttention = set;
    doSetDemandsAttention();
    workspace()->clientAttentionChanged(this, set);
    Q_EMIT demandsAttentionChanged();
}

void AbstractClient::doSetDemandsAttention()
{
}

void AbstractClient::setDesktop(int desktop)
{
    const int numberOfDesktops = VirtualDesktopManager::self()->count();
    if (desktop != NET::OnAllDesktops)   // Do range check
        desktop = qMax(1, qMin(numberOfDesktops, desktop));

    QVector<VirtualDesktop *> desktops;
    if (desktop != NET::OnAllDesktops) {
       desktops << VirtualDesktopManager::self()->desktopForX11Id(desktop);
    }
    setDesktops(desktops);
}

void AbstractClient::setDesktops(QVector<VirtualDesktop*> desktops)
{
    //on x11 we can have only one desktop at a time
    if (kwinApp()->operationMode() == Application::OperationModeX11 && desktops.size() > 1) {
        desktops = QVector<VirtualDesktop*>({desktops.last()});
    }

    desktops = rules()->checkDesktops(desktops);
    if (desktops == m_desktops) {
        return;
    }

    int was_desk = AbstractClient::desktop();
    const bool wasOnCurrentDesktop = isOnCurrentDesktop() && was_desk >= 0;

    m_desktops = desktops;

    if (windowManagementInterface()) {
        if (m_desktops.isEmpty()) {
            windowManagementInterface()->setOnAllDesktops(true);
        } else {
            windowManagementInterface()->setOnAllDesktops(false);
            auto currentDesktops = windowManagementInterface()->plasmaVirtualDesktops();
            for (auto desktop: qAsConst(m_desktops)) {
                if (!currentDesktops.contains(desktop->id())) {
                    windowManagementInterface()->addPlasmaVirtualDesktop(desktop->id());
                } else {
                    currentDesktops.removeOne(desktop->id());
                }
            }
            for (const auto &desktopId: qAsConst(currentDesktops)) {
                windowManagementInterface()->removePlasmaVirtualDesktop(desktopId);
            }
        }
    }
    if (info) {
        info->setDesktop(desktop());
    }

    if ((was_desk == NET::OnAllDesktops) != (desktop() == NET::OnAllDesktops)) {
        // onAllDesktops changed
        workspace()->updateOnAllDesktopsOfTransients(this);
    }

    auto transients_stacking_order = workspace()->ensureStackingOrder(transients());
    for (auto it = transients_stacking_order.constBegin();
            it != transients_stacking_order.constEnd();
            ++it)
        (*it)->setDesktops(desktops);

    if (isModal())  // if a modal dialog is moved, move the mainwindow with it as otherwise
        // the (just moved) modal dialog will confusingly return to the mainwindow with
        // the next desktop change
    {
        const auto clients = mainClients();
        for (AbstractClient *c2 : clients) {
            c2->setDesktops(desktops);
        }
    }

    doSetDesktop();

    FocusChain::self()->update(this, FocusChain::MakeFirst);
    updateWindowRules(Rules::Desktops);

    Q_EMIT desktopChanged();
    if (wasOnCurrentDesktop != isOnCurrentDesktop())
        Q_EMIT desktopPresenceChanged(this, was_desk);
    Q_EMIT x11DesktopIdsChanged();
}

void AbstractClient::doSetDesktop()
{
}

void AbstractClient::doSetOnActivities(const QStringList &activityList)
{
    Q_UNUSED(activityList);
}

void AbstractClient::enterDesktop(VirtualDesktop *virtualDesktop)
{
    if (m_desktops.contains(virtualDesktop)) {
        return;
    }
    auto desktops = m_desktops;
    desktops.append(virtualDesktop);
    setDesktops(desktops);
}

void AbstractClient::leaveDesktop(VirtualDesktop *virtualDesktop)
{
    QVector<VirtualDesktop*> currentDesktops;
    if (m_desktops.isEmpty()) {
        currentDesktops = VirtualDesktopManager::self()->desktops();
    } else {
        currentDesktops = m_desktops;
    }

    if (!currentDesktops.contains(virtualDesktop)) {
        return;
    }
    auto desktops = currentDesktops;
    desktops.removeOne(virtualDesktop);
    setDesktops(desktops);
}

void AbstractClient::setOnAllDesktops(bool b)
{
    if (b == isOnAllDesktops()) {
        return;
    }
    if (b) {
        setDesktops({});
    } else {
        setDesktops({VirtualDesktopManager::self()->currentDesktop()});
    }
}

int AbstractClient::desktop() const
{
    return m_desktops.isEmpty() ? (int)NET::OnAllDesktops : m_desktops.last()->x11DesktopNumber();
}

QVector<uint> AbstractClient::x11DesktopIds() const
{
    const auto desks = desktops();
    QVector<uint> x11Ids;
    x11Ids.reserve(desks.count());
    std::transform(desks.constBegin(), desks.constEnd(),
        std::back_inserter(x11Ids),
        [] (const VirtualDesktop *vd) {
            return vd->x11DesktopNumber();
        }
    );
    return x11Ids;
}

QStringList AbstractClient::desktopIds() const
{
    const auto desks = desktops();
    QStringList ids;
    ids.reserve(desks.count());
    std::transform(desks.constBegin(), desks.constEnd(),
                   std::back_inserter(ids),
                   [] (const VirtualDesktop *vd) {
                       return vd->id();
                   }
    );
    return ids;
};

ShadeMode AbstractClient::shadeMode() const
{
    return m_shadeMode;
}

bool AbstractClient::isShadeable() const
{
    return false;
}

void AbstractClient::setShade(bool set)
{
    set ? setShade(ShadeNormal) : setShade(ShadeNone);
}

void AbstractClient::setShade(ShadeMode mode)
{
    if (!isShadeable())
        return;
    if (mode == ShadeHover && isInteractiveMove())
        return; // causes geometry breaks and is probably nasty
    if (isSpecialWindow() || noBorder())
        mode = ShadeNone;

    mode = rules()->checkShade(mode);
    if (m_shadeMode == mode)
        return;

    const bool wasShade = isShade();
    const ShadeMode previousShadeMode = shadeMode();
    m_shadeMode = mode;

    if (wasShade == isShade()) {
        // Decoration may want to update after e.g. hover-shade changes
        Q_EMIT shadeChanged();
        return; // No real change in shaded state
    }

    Q_ASSERT(isDecorated());
    GeometryUpdatesBlocker blocker(this);

    doSetShade(previousShadeMode);
    updateWindowRules(Rules::Shade);

    Q_EMIT shadeChanged();
}

void AbstractClient::doSetShade(ShadeMode previousShadeMode)
{
    Q_UNUSED(previousShadeMode)
}

void AbstractClient::shadeHover()
{
    setShade(ShadeHover);
    cancelShadeHoverTimer();
}

void AbstractClient::shadeUnhover()
{
    setShade(ShadeNormal);
    cancelShadeHoverTimer();
}

void AbstractClient::startShadeHoverTimer()
{
    if (!isShade())
        return;
    m_shadeHoverTimer = new QTimer(this);
    connect(m_shadeHoverTimer, &QTimer::timeout, this, &AbstractClient::shadeHover);
    m_shadeHoverTimer->setSingleShot(true);
    m_shadeHoverTimer->start(options->shadeHoverInterval());
}

void AbstractClient::startShadeUnhoverTimer()
{
    if (m_shadeMode == ShadeHover && !isInteractiveMoveResize() && !isInteractiveMoveResizePointerButtonDown()) {
        m_shadeHoverTimer = new QTimer(this);
        connect(m_shadeHoverTimer, &QTimer::timeout, this, &AbstractClient::shadeUnhover);
        m_shadeHoverTimer->setSingleShot(true);
        m_shadeHoverTimer->start(options->shadeHoverInterval());
    }
}

void AbstractClient::cancelShadeHoverTimer()
{
    delete m_shadeHoverTimer;
    m_shadeHoverTimer = nullptr;
}

void AbstractClient::toggleShade()
{
    // If the mode is ShadeHover or ShadeActive, cancel shade too.
    setShade(shadeMode() == ShadeNone ? ShadeNormal : ShadeNone);
}

Qt::Edge AbstractClient::titlebarPosition() const
{
    // TODO: still needed, remove?
    return Qt::TopEdge;
}

bool AbstractClient::titlebarPositionUnderMouse() const
{
    if (!isDecorated()) {
        return false;
    }
    const auto sectionUnderMouse = decoration()->sectionUnderMouse();
    if (sectionUnderMouse == Qt::TitleBarArea) {
        return true;
    }
    // check other sections based on titlebarPosition
    switch (titlebarPosition()) {
    case Qt::TopEdge:
        return (sectionUnderMouse == Qt::TopLeftSection || sectionUnderMouse == Qt::TopSection || sectionUnderMouse == Qt::TopRightSection);
    case Qt::LeftEdge:
        return (sectionUnderMouse == Qt::TopLeftSection || sectionUnderMouse == Qt::LeftSection || sectionUnderMouse == Qt::BottomLeftSection);
    case Qt::RightEdge:
        return (sectionUnderMouse == Qt::BottomRightSection || sectionUnderMouse == Qt::RightSection || sectionUnderMouse == Qt::TopRightSection);
    case Qt::BottomEdge:
        return (sectionUnderMouse == Qt::BottomLeftSection || sectionUnderMouse == Qt::BottomSection || sectionUnderMouse == Qt::BottomRightSection);
    default:
        // nothing
        return false;
    }
}

void AbstractClient::setMinimized(bool set)
{
    set ? minimize() : unminimize();
}

void AbstractClient::minimize(bool avoid_animation)
{
    if (!isMinimizable() || isMinimized())
        return;

    m_minimized = true;
    doMinimize();

    updateWindowRules(Rules::Minimize);

    if (options->moveMinimizedWindowsToEndOfTabBoxFocusChain()) {
        FocusChain::self()->update(this, FocusChain::MakeFirstMinimized);
    }

    // TODO: merge signal with s_minimized
    addWorkspaceRepaint(visibleGeometry());
    Q_EMIT clientMinimized(this, !avoid_animation);
    Q_EMIT minimizedChanged();
}

void AbstractClient::unminimize(bool avoid_animation)
{
    if (!isMinimized())
        return;

    if (rules()->checkMinimize(false)) {
        return;
    }

    m_minimized = false;
    doMinimize();

    updateWindowRules(Rules::Minimize);
    Q_EMIT clientUnminimized(this, !avoid_animation);
    Q_EMIT minimizedChanged();
}

void AbstractClient::doMinimize()
{
}

QPalette AbstractClient::palette() const
{
    if (!m_palette) {
        return QPalette();
    }
    return m_palette->palette();
}

const Decoration::DecorationPalette *AbstractClient::decorationPalette() const
{
    return m_palette.get();
}

QString AbstractClient::preferredColorScheme() const
{
    return rules()->checkDecoColor(QString());
}

QString AbstractClient::colorScheme() const
{
    return m_colorScheme;
}

void AbstractClient::setColorScheme(const QString &colorScheme)
{
    QString requestedColorScheme = colorScheme;
    if (requestedColorScheme.isEmpty()) {
        requestedColorScheme = QStringLiteral("kdeglobals");
    }

    if (!m_palette || m_colorScheme != requestedColorScheme) {
        m_colorScheme = requestedColorScheme;

        if (m_palette) {
            disconnect(m_palette.get(), &Decoration::DecorationPalette::changed, this, &AbstractClient::handlePaletteChange);
        }

        auto it = s_palettes.find(m_colorScheme);

        if (it == s_palettes.end() || it->expired()) {
            m_palette = std::make_shared<Decoration::DecorationPalette>(m_colorScheme);
            if (m_palette->isValid()) {
                s_palettes[m_colorScheme] = m_palette;
            } else {
                if (!s_defaultPalette) {
                    s_defaultPalette = std::make_shared<Decoration::DecorationPalette>(QStringLiteral("kdeglobals"));
                    s_palettes[QStringLiteral("kdeglobals")] = s_defaultPalette;
                }

                m_palette = s_defaultPalette;
            }

            if (m_colorScheme == QStringLiteral("kdeglobals")) {
                s_defaultPalette = m_palette;
            }
        } else {
            m_palette = it->lock();
        }

        connect(m_palette.get(), &Decoration::DecorationPalette::changed, this, &AbstractClient::handlePaletteChange);

        Q_EMIT paletteChanged(palette());
        Q_EMIT colorSchemeChanged();
    }
}

void AbstractClient::updateColorScheme()
{
    setColorScheme(preferredColorScheme());
}

void AbstractClient::handlePaletteChange()
{
    Q_EMIT paletteChanged(palette());
}

void AbstractClient::keepInArea(QRect area, bool partial)
{
    if (partial) {
        // increase the area so that can have only 100 pixels in the area
        const QRect geometry = moveResizeGeometry();
        area.setLeft(std::min(area.left() - geometry.width() + 100, area.left()));
        area.setTop(std::min(area.top() - geometry.height() + 100, area.top()));
        area.setRight(std::max(area.right() + geometry.width() - 100, area.right()));
        area.setBottom(std::max(area.bottom() + geometry.height() - 100, area.bottom()));
    }
    if (!partial) {
        // resize to fit into area
        const QRect geometry = moveResizeGeometry();
        if (area.width() < geometry.width() || area.height() < geometry.height()) {
            resizeWithChecks(geometry.size().boundedTo(area.size()));
        }
    }

    QRect geometry = moveResizeGeometry();
    if (geometry.right() > area.right() && geometry.width() <= area.width()) {
        geometry.moveRight(area.right());
    }
    if (geometry.bottom() > area.bottom() && geometry.height() <= area.height()) {
        geometry.moveBottom(area.bottom());
    }

    if (!area.contains(geometry.topLeft())) {
        if (geometry.left() < area.left()) {
            geometry.moveLeft(area.left());
        }
        if (geometry.top() < area.top()) {
            geometry.moveTop(area.top());
        }
    }

    if (moveResizeGeometry().topLeft() != geometry.topLeft()) {
        move(geometry.topLeft());
    }
}

/**
 * Returns the maximum client size, not the maximum frame size.
 */
QSize AbstractClient::maxSize() const
{
    return rules()->checkMaxSize(QSize(INT_MAX, INT_MAX));
}

/**
 * Returns the minimum client size, not the minimum frame size.
 */
QSize AbstractClient::minSize() const
{
    return rules()->checkMinSize(QSize(0, 0));
}

void AbstractClient::blockGeometryUpdates(bool block)
{
    if (block) {
        if (m_blockGeometryUpdates == 0) {
            m_pendingMoveResizeMode = MoveResizeMode::None;
        }
        ++m_blockGeometryUpdates;
    } else {
        if (--m_blockGeometryUpdates == 0) {
            if (m_pendingMoveResizeMode != MoveResizeMode::None) {
                moveResizeInternal(moveResizeGeometry(), m_pendingMoveResizeMode);
                m_pendingMoveResizeMode = MoveResizeMode::None;
            }
        }
    }
}

void AbstractClient::maximize(MaximizeMode m)
{
    setMaximize(m & MaximizeVertical, m & MaximizeHorizontal);
}

void AbstractClient::setMaximize(bool vertically, bool horizontally)
{
    // changeMaximize() flips the state, so change from set->flip
    const MaximizeMode oldMode = requestedMaximizeMode();
    changeMaximize(
        oldMode & MaximizeHorizontal ? !horizontally : horizontally,
        oldMode & MaximizeVertical ? !vertically : vertically,
        false);
    const MaximizeMode newMode = maximizeMode();
    if (oldMode != newMode) {
        Q_EMIT clientMaximizedStateChanged(this, newMode);
        Q_EMIT clientMaximizedStateChanged(this, vertically, horizontally);
    }
}

bool AbstractClient::startInteractiveMoveResize()
{
    Q_ASSERT(!isInteractiveMoveResize());
    Q_ASSERT(QWidget::keyboardGrabber() == nullptr);
    Q_ASSERT(QWidget::mouseGrabber() == nullptr);
    stopDelayedInteractiveMoveResize();
    if (QApplication::activePopupWidget() != nullptr)
        return false; // popups have grab
    if (isFullScreen() && (screens()->count() < 2 || !isMovableAcrossScreens()))
        return false;
    if (!doStartInteractiveMoveResize()) {
        return false;
    }

    invalidateDecorationDoubleClickTimer();

    setInteractiveMoveResize(true);
    workspace()->setMoveResizeClient(this);

    if (maximizeMode() != MaximizeRestore) {
        switch (interactiveMoveResizeGravity()) {
        case Gravity::Left:
        case Gravity::Right:
            // Quit maximized horizontally state if the window is resized horizontally.
            if (maximizeMode() & MaximizeHorizontal) {
                QRect originalGeometry = geometryRestore();
                originalGeometry.setX(moveResizeGeometry().x());
                originalGeometry.setWidth(moveResizeGeometry().width());
                setGeometryRestore(originalGeometry);
                maximize(maximizeMode() ^ MaximizeHorizontal);
            }
            break;
        case Gravity::Top:
        case Gravity::Bottom:
            // Quit maximized vertically state if the window is resized vertically.
            if (maximizeMode() & MaximizeVertical) {
                QRect originalGeometry = geometryRestore();
                originalGeometry.setY(moveResizeGeometry().y());
                originalGeometry.setHeight(moveResizeGeometry().height());
                setGeometryRestore(originalGeometry);
                maximize(maximizeMode() ^ MaximizeVertical);
            }
            break;
        case Gravity::TopLeft:
        case Gravity::BottomLeft:
        case Gravity::TopRight:
        case Gravity::BottomRight:
            // Quit the maximized mode if the window is resized by dragging one of its corners.
            setGeometryRestore(moveResizeGeometry());
            maximize(MaximizeRestore);
            break;
        default:
            break;
        }
    }

    if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        if (interactiveMoveResizeGravity() != Gravity::None) { // Cannot use isResize() yet
            // Exit quick tile mode when the user attempts to resize a tiled window
            updateQuickTileMode(QuickTileFlag::None); // Do so without restoring original geometry
            setGeometryRestore(moveResizeGeometry());
            doSetQuickTileMode();
            Q_EMIT quickTileModeChanged();
        }
    }

    updateInitialMoveResizeGeometry();
    checkUnrestrictedInteractiveMoveResize();
    Q_EMIT clientStartUserMovedResized(this);
    if (ScreenEdges::self()->isDesktopSwitchingMovingClients())
        ScreenEdges::self()->reserveDesktopSwitching(true, Qt::Vertical|Qt::Horizontal);
    return true;
}

void AbstractClient::finishInteractiveMoveResize(bool cancel)
{
    GeometryUpdatesBlocker blocker(this);
    leaveInteractiveMoveResize();

    doFinishInteractiveMoveResize();

    if (cancel) {
        moveResize(initialInteractiveMoveResizeGeometry());
    }
    checkOutput(); // needs to be done because clientFinishUserMovedResized has not yet re-activated online alignment
    if (output() != interactiveMoveResizeStartOutput()) {
        if (isFullScreen() || isElectricBorderMaximizing()) {
            updateGeometryRestoresForFullscreen(output());
        }
        workspace()->sendClientToOutput(this, output()); // checks rule validity
        if (maximizeMode() != MaximizeRestore) {
            checkWorkspacePosition();
        }
    }

    if (isElectricBorderMaximizing()) {
        setQuickTileMode(electricBorderMode());
        setElectricBorderMaximizing(false);
    } else if (!cancel && !isFullScreen()) {
        QRect geom_restore = geometryRestore();
        if (!(maximizeMode() & MaximizeHorizontal)) {
            geom_restore.setX(moveResizeGeometry().x());
            geom_restore.setWidth(moveResizeGeometry().width());
        }
        if (!(maximizeMode() & MaximizeVertical)) {
            geom_restore.setY(moveResizeGeometry().y());
            geom_restore.setHeight(moveResizeGeometry().height());
        }
        setGeometryRestore(geom_restore);
    }
// FRAME    update();

    Q_EMIT clientFinishUserMovedResized(this);
}

// This function checks if it actually makes sense to perform a restricted move/resize.
// If e.g. the titlebar is already outside of the workarea, there's no point in performing
// a restricted move resize, because then e.g. resize would also move the window (#74555).
// NOTE: Most of it is duplicated from handleMoveResize().
void AbstractClient::checkUnrestrictedInteractiveMoveResize()
{
    if (isUnrestrictedInteractiveMoveResize())
        return;
    const QRect &moveResizeGeom = moveResizeGeometry();
    QRect desktopArea = workspace()->clientArea(WorkArea, this, moveResizeGeom.center());
    int left_marge, right_marge, top_marge, bottom_marge, titlebar_marge;
    // restricted move/resize - keep at least part of the titlebar always visible
    // how much must remain visible when moved away in that direction
    left_marge = qMin(100 + borderRight(), moveResizeGeom.width());
    right_marge = qMin(100 + borderLeft(), moveResizeGeom.width());
    // width/height change with opaque resizing, use the initial ones
    titlebar_marge = initialInteractiveMoveResizeGeometry().height();
    top_marge = borderBottom();
    bottom_marge = borderTop();
    if (isInteractiveResize()) {
        if (moveResizeGeom.bottom() < desktopArea.top() + top_marge)
            setUnrestrictedInteractiveMoveResize(true);
        if (moveResizeGeom.top() > desktopArea.bottom() - bottom_marge)
            setUnrestrictedInteractiveMoveResize(true);
        if (moveResizeGeom.right() < desktopArea.left() + left_marge)
            setUnrestrictedInteractiveMoveResize(true);
        if (moveResizeGeom.left() > desktopArea.right() - right_marge)
            setUnrestrictedInteractiveMoveResize(true);
        if (!isUnrestrictedInteractiveMoveResize() && moveResizeGeom.top() < desktopArea.top())   // titlebar mustn't go out
            setUnrestrictedInteractiveMoveResize(true);
    }
    if (isInteractiveMove()) {
        if (moveResizeGeom.bottom() < desktopArea.top() + titlebar_marge - 1)
            setUnrestrictedInteractiveMoveResize(true);
        // no need to check top_marge, titlebar_marge already handles it
        if (moveResizeGeom.top() > desktopArea.bottom() - bottom_marge + 1) // titlebar mustn't go out
            setUnrestrictedInteractiveMoveResize(true);
        if (moveResizeGeom.right() < desktopArea.left() + left_marge)
            setUnrestrictedInteractiveMoveResize(true);
        if (moveResizeGeom.left() > desktopArea.right() - right_marge)
            setUnrestrictedInteractiveMoveResize(true);
    }
}

// When the user pressed mouse on the titlebar, don't activate move immediately,
// since it may be just a click. Activate instead after a delay. Move used to be
// activated only after moving by several pixels, but that looks bad.
void AbstractClient::startDelayedInteractiveMoveResize()
{
    Q_ASSERT(!m_interactiveMoveResize.delayedTimer);
    m_interactiveMoveResize.delayedTimer = new QTimer(this);
    m_interactiveMoveResize.delayedTimer->setSingleShot(true);
    connect(m_interactiveMoveResize.delayedTimer, &QTimer::timeout, this,
        [this]() {
            Q_ASSERT(isInteractiveMoveResizePointerButtonDown());
            if (!startInteractiveMoveResize()) {
                setInteractiveMoveResizePointerButtonDown(false);
            }
            updateCursor();
            stopDelayedInteractiveMoveResize();
        }
    );
    m_interactiveMoveResize.delayedTimer->start(QApplication::startDragTime());
}

void AbstractClient::stopDelayedInteractiveMoveResize()
{
    delete m_interactiveMoveResize.delayedTimer;
    m_interactiveMoveResize.delayedTimer = nullptr;
}

void AbstractClient::updateInteractiveMoveResize(const QPointF &currentGlobalCursor)
{
    handleInteractiveMoveResize(pos(), currentGlobalCursor.toPoint());
}

void AbstractClient::handleInteractiveMoveResize(const QPoint &local, const QPoint &global)
{
    const QRect oldGeo = moveResizeGeometry();
    handleInteractiveMoveResize(local.x(), local.y(), global.x(), global.y());
    if (!isFullScreen() && isInteractiveMove()) {
        if (quickTileMode() != QuickTileMode(QuickTileFlag::None) && oldGeo != moveResizeGeometry()) {
            GeometryUpdatesBlocker blocker(this);
            setQuickTileMode(QuickTileFlag::None);
            const QRect &geom_restore = geometryRestore();
            setInteractiveMoveOffset(QPoint(double(interactiveMoveOffset().x()) / double(oldGeo.width()) * double(geom_restore.width()),
                                 double(interactiveMoveOffset().y()) / double(oldGeo.height()) * double(geom_restore.height())));
            if (rules()->checkMaximize(MaximizeRestore) == MaximizeRestore)
                setMoveResizeGeometry(geom_restore);
            handleInteractiveMoveResize(local.x(), local.y(), global.x(), global.y()); // fix position
        } else if (quickTileMode() == QuickTileMode(QuickTileFlag::None) && isResizable()) {
            checkQuickTilingMaximizationZones(global.x(), global.y());
        }
    }
}

void AbstractClient::handleInteractiveMoveResize(int x, int y, int x_root, int y_root)
{
    if (isWaitingForInteractiveMoveResizeSync())
        return; // we're still waiting for the client or the timeout

    const Gravity gravity = interactiveMoveResizeGravity();
    if ((gravity == Gravity::None && !isMovableAcrossScreens())
            || (gravity != Gravity::None && (isShade() || !isResizable()))) {
        return;
    }

    if (!isInteractiveMoveResize()) {
        QPoint p(QPoint(x/* - padding_left*/, y/* - padding_top*/) - interactiveMoveOffset());
        if (p.manhattanLength() >= QApplication::startDragDistance()) {
            if (!startInteractiveMoveResize()) {
                setInteractiveMoveResizePointerButtonDown(false);
                updateCursor();
                return;
            }
            updateCursor();
        } else
            return;
    }

    // ShadeHover or ShadeActive, ShadeNormal was already avoided above
    if (gravity != Gravity::None && shadeMode() != ShadeNone) {
        setShade(ShadeNone);
    }

    QPoint globalPos(x_root, y_root);
    // these two points limit the geometry rectangle, i.e. if bottomleft resizing is done,
    // the bottomleft corner should be at is at (topleft.x(), bottomright().y())
    QPoint topleft = globalPos - interactiveMoveOffset();
    QPoint bottomright = globalPos + invertedInteractiveMoveOffset();
    QRect previousMoveResizeGeom = moveResizeGeometry();

    // TODO move whole group when moving its leader or when the leader is not mapped?

    auto titleBarRect = [this](bool &transposed, int &requiredPixels) -> QRect {
        const QRect &moveResizeGeom = moveResizeGeometry();
        QRect r(moveResizeGeom);
        r.moveTopLeft(QPoint(0,0));
        switch (titlebarPosition()) {
        default:
        case Qt::TopEdge:
            r.setHeight(borderTop());
            break;
        case Qt::LeftEdge:
            r.setWidth(borderLeft());
            transposed = true;
            break;
        case Qt::BottomEdge:
            r.setTop(r.bottom() - borderBottom());
            break;
        case Qt::RightEdge:
            r.setLeft(r.right() - borderRight());
            transposed = true;
            break;
        }
        // When doing a restricted move we must always keep 100px of the titlebar
        // visible to allow the user to be able to move it again.
        requiredPixels = qMin(100 * (transposed ? r.width() : r.height()),
                              moveResizeGeom.width() * moveResizeGeom.height());
        return r;
    };

    bool update = false;
    if (isInteractiveResize()) {
        QRect orig = initialInteractiveMoveResizeGeometry();
        SizeMode sizeMode = SizeModeAny;
        auto calculateMoveResizeGeom = [this, &topleft, &bottomright, &orig, &sizeMode, &gravity]() {
            switch (gravity) {
            case Gravity::TopLeft:
                setMoveResizeGeometry(QRect(topleft, orig.bottomRight()));
                break;
            case Gravity::BottomRight:
                setMoveResizeGeometry(QRect(orig.topLeft(), bottomright));
                break;
            case Gravity::BottomLeft:
                setMoveResizeGeometry(QRect(QPoint(topleft.x(), orig.y()), QPoint(orig.right(), bottomright.y())));
                break;
            case Gravity::TopRight:
                setMoveResizeGeometry(QRect(QPoint(orig.x(), topleft.y()), QPoint(bottomright.x(), orig.bottom())));
                break;
            case Gravity::Top:
                setMoveResizeGeometry(QRect(QPoint(orig.left(), topleft.y()), orig.bottomRight()));
                sizeMode = SizeModeFixedH; // try not to affect height
                break;
            case Gravity::Bottom:
                setMoveResizeGeometry(QRect(orig.topLeft(), QPoint(orig.right(), bottomright.y())));
                sizeMode = SizeModeFixedH;
                break;
            case Gravity::Left:
                setMoveResizeGeometry(QRect(QPoint(topleft.x(), orig.top()), orig.bottomRight()));
                sizeMode = SizeModeFixedW;
                break;
            case Gravity::Right:
                setMoveResizeGeometry(QRect(orig.topLeft(), QPoint(bottomright.x(), orig.bottom())));
                sizeMode = SizeModeFixedW;
                break;
            case Gravity::None:
                Q_UNREACHABLE();
                break;
            }
        };

        // first resize (without checking constrains), then snap, then check bounds, then check constrains
        calculateMoveResizeGeom();
        // adjust new size to snap to other windows/borders
        setMoveResizeGeometry(workspace()->adjustClientSize(this, moveResizeGeometry(), gravity));

        if (!isUnrestrictedInteractiveMoveResize()) {
            // Make sure the titlebar isn't behind a restricted area. We don't need to restrict
            // the other directions. If not visible enough, move the window to the closest valid
            // point. We bruteforce this by slowly moving the window back to its previous position
            QRegion availableArea(workspace()->clientArea(FullArea, this, workspace()->activeOutput()));
            availableArea -= workspace()->restrictedMoveArea(VirtualDesktopManager::self()->currentDesktop());
            bool transposed = false;
            int requiredPixels;
            QRect bTitleRect = titleBarRect(transposed, requiredPixels);
            int lastVisiblePixels = -1;
            QRect lastTry = moveResizeGeometry();
            bool titleFailed = false;
            for (;;) {
                const QRect titleRect(bTitleRect.translated(moveResizeGeometry().topLeft()));
                int visiblePixels = 0;
                int realVisiblePixels = 0;
                for (const QRect &rect : availableArea) {
                    const QRect r = rect & titleRect;
                    realVisiblePixels += r.width() * r.height();
                    if ((transposed && r.width() == titleRect.width()) || // Only the full size regions...
                        (!transposed && r.height() == titleRect.height())) // ...prevents long slim areas
                        visiblePixels += r.width() * r.height();
                }

                if (visiblePixels >= requiredPixels)
                    break; // We have reached a valid position

                if (realVisiblePixels <= lastVisiblePixels) {
                    if (titleFailed && realVisiblePixels < lastVisiblePixels)
                        break; // we won't become better
                    else {
                        if (!titleFailed)
                            setMoveResizeGeometry(lastTry);
                        titleFailed = true;
                    }
                }
                lastVisiblePixels = realVisiblePixels;
                QRect moveResizeGeom = moveResizeGeometry();
                lastTry = moveResizeGeom;

                // Not visible enough, move the window to the closest valid point. We bruteforce
                // this by slowly moving the window back to its previous position.
                // The geometry changes at up to two edges, the one with the title (if) shall take
                // precedence. The opposing edge has no impact on visiblePixels and only one of
                // the adjacent can alter at a time, ie. it's enough to ignore adjacent edges
                // if the title edge altered
                bool leftChanged  = previousMoveResizeGeom.left()   != moveResizeGeom.left();
                bool rightChanged = previousMoveResizeGeom.right()  != moveResizeGeom.right();
                bool topChanged   = previousMoveResizeGeom.top()    != moveResizeGeom.top();
                bool btmChanged   = previousMoveResizeGeom.bottom() != moveResizeGeom.bottom();
                auto fixChangedState = [titleFailed](bool &major, bool &counter, bool &ad1, bool &ad2) {
                    counter = false;
                    if (titleFailed)
                        major = false;
                    if (major)
                        ad1 = ad2 = false;
                };
                switch (titlebarPosition()) {
                default:
                case Qt::TopEdge:
                    fixChangedState(topChanged, btmChanged, leftChanged, rightChanged);
                    break;
                case Qt::LeftEdge:
                    fixChangedState(leftChanged, rightChanged, topChanged, btmChanged);
                    break;
                case Qt::BottomEdge:
                    fixChangedState(btmChanged, topChanged, leftChanged, rightChanged);
                    break;
                case Qt::RightEdge:
                    fixChangedState(rightChanged, leftChanged, topChanged, btmChanged);
                    break;
                }
                if (topChanged)
                    moveResizeGeom.setTop(moveResizeGeom.y() + sign(previousMoveResizeGeom.y() - moveResizeGeom.y()));
                else if (leftChanged)
                    moveResizeGeom.setLeft(moveResizeGeom.x() + sign(previousMoveResizeGeom.x() - moveResizeGeom.x()));
                else if (btmChanged)
                    moveResizeGeom.setBottom(moveResizeGeom.bottom() + sign(previousMoveResizeGeom.bottom() - moveResizeGeom.bottom()));
                else if (rightChanged)
                    moveResizeGeom.setRight(moveResizeGeom.right() + sign(previousMoveResizeGeom.right() - moveResizeGeom.right()));
                else
                    break; // no position changed - that's certainly not good
                setMoveResizeGeometry(moveResizeGeom);
            }
        }

        // Always obey size hints, even when in "unrestricted" mode
        QSize size = constrainFrameSize(moveResizeGeometry().size(), sizeMode);
        // the new topleft and bottomright corners (after checking size constrains), if they'll be needed
        topleft = QPoint(moveResizeGeometry().right() - size.width() + 1, moveResizeGeometry().bottom() - size.height() + 1);
        bottomright = QPoint(moveResizeGeometry().left() + size.width() - 1, moveResizeGeometry().top() + size.height() - 1);
        orig = moveResizeGeometry();

        // if aspect ratios are specified, both dimensions may change.
        // Therefore grow to the right/bottom if needed.
        // TODO it should probably obey gravity rather than always using right/bottom ?
        if (sizeMode == SizeModeFixedH)
            orig.setRight(bottomright.x());
        else if (sizeMode == SizeModeFixedW)
            orig.setBottom(bottomright.y());

        calculateMoveResizeGeom();

        if (moveResizeGeometry().size() != previousMoveResizeGeom.size())
            update = true;
    } else if (isInteractiveMove()) {
        Q_ASSERT(gravity == Gravity::None);
        if (!isMovable()) { // isMovableAcrossScreens() must have been true to get here
            // Special moving of maximized windows on Xinerama screens
            AbstractOutput *output = kwinApp()->platform()->outputAt(globalPos);
            if (isFullScreen())
                setMoveResizeGeometry(workspace()->clientArea(FullScreenArea, this, output));
            else {
                QRect moveResizeGeom = workspace()->clientArea(MaximizeArea, this, output);
                QSize adjSize = constrainFrameSize(moveResizeGeom.size(), SizeModeMax);
                if (adjSize != moveResizeGeom.size()) {
                    QRect r(moveResizeGeom);
                    moveResizeGeom.setSize(adjSize);
                    moveResizeGeom.moveCenter(r.center());
                }
                setMoveResizeGeometry(moveResizeGeom);
            }
        } else {
            // first move, then snap, then check bounds
            QRect moveResizeGeom = moveResizeGeometry();
            moveResizeGeom.moveTopLeft(topleft);
            moveResizeGeom.moveTopLeft(workspace()->adjustClientPosition(this, moveResizeGeom.topLeft(),
                                       isUnrestrictedInteractiveMoveResize()));
            setMoveResizeGeometry(moveResizeGeom);

            if (!isUnrestrictedInteractiveMoveResize()) {
                const QRegion strut = workspace()->restrictedMoveArea(VirtualDesktopManager::self()->currentDesktop());
                QRegion availableArea(workspace()->clientArea(FullArea, this, workspace()->activeOutput()));
                availableArea -= strut;   // Strut areas
                bool transposed = false;
                int requiredPixels;
                QRect bTitleRect = titleBarRect(transposed, requiredPixels);
                for (;;) {
                    QRect moveResizeGeom = moveResizeGeometry();
                    const QRect titleRect(bTitleRect.translated(moveResizeGeom.topLeft()));
                    int visiblePixels = 0;
                    for (const QRect &rect : availableArea) {
                        const QRect r = rect & titleRect;
                        if ((transposed && r.width() == titleRect.width()) || // Only the full size regions...
                            (!transposed && r.height() == titleRect.height())) // ...prevents long slim areas
                            visiblePixels += r.width() * r.height();
                    }
                    if (visiblePixels >= requiredPixels)
                        break; // We have reached a valid position

                    // (esp.) if there're more screens with different struts (panels) it the titlebar
                    // will be movable outside the movearea (covering one of the panels) until it
                    // crosses the panel "too much" (not enough visiblePixels) and then stucks because
                    // it's usually only pushed by 1px to either direction
                    // so we first check whether we intersect suc strut and move the window below it
                    // immediately (it's still possible to hit the visiblePixels >= titlebarArea break
                    // by moving the window slightly downwards, but it won't stuck)
                    // see bug #274466
                    // and bug #301805 for why we can't just match the titlearea against the screen
                    if (screens()->count() > 1) { // optimization
                        // TODO: could be useful on partial screen struts (half-width panels etc.)
                        int newTitleTop = -1;
                        for (const QRect &r : strut) {
                            if (r.top() == 0 && r.width() > r.height() && // "top panel"
                                r.intersects(moveResizeGeom) && moveResizeGeom.top() < r.bottom()) {
                                newTitleTop = r.bottom() + 1;
                                break;
                            }
                        }
                        if (newTitleTop > -1) {
                            moveResizeGeom.moveTop(newTitleTop); // invalid position, possibly on screen change
                            setMoveResizeGeometry(moveResizeGeom);
                            break;
                        }
                    }

                    int dx = sign(previousMoveResizeGeom.x() - moveResizeGeom.x()),
                        dy = sign(previousMoveResizeGeom.y() - moveResizeGeom.y());
                    if (visiblePixels && dx) // means there's no full width cap -> favor horizontally
                        dy = 0;
                    else if (dy)
                        dx = 0;

                    // Move it back
                    moveResizeGeom.translate(dx, dy);
                    setMoveResizeGeometry(moveResizeGeom);

                    if (moveResizeGeom == previousMoveResizeGeom) {
                        break; // Prevent lockup
                    }
                }
            }
        }
        if (moveResizeGeometry().topLeft() != previousMoveResizeGeom.topLeft())
            update = true;
    } else {
        Q_UNREACHABLE();
    }

    if (!update)
        return;

    if (isInteractiveMove()) {
        move(moveResizeGeometry().topLeft());
    } else {
        doInteractiveResizeSync();
    }

    Q_EMIT clientStepUserMovedResized(this, moveResizeGeometry());
}

StrutRect AbstractClient::strutRect(StrutArea area) const
{
    Q_UNUSED(area)
    return StrutRect();
}

StrutRects AbstractClient::strutRects() const
{
    StrutRects region;
    region += strutRect(StrutAreaTop);
    region += strutRect(StrutAreaRight);
    region += strutRect(StrutAreaBottom);
    region += strutRect(StrutAreaLeft);
    return region;
}

bool AbstractClient::hasStrut() const
{
    return false;
}

void AbstractClient::setupWindowManagementInterface()
{
    if (m_windowManagementInterface) {
        // already setup
        return;
    }
    if (!waylandServer() || !surface()) {
        return;
    }
    if (!waylandServer()->windowManagement()) {
        return;
    }
    using namespace KWaylandServer;
    auto w = waylandServer()->windowManagement()->createWindow(this, internalId());
    w->setTitle(caption());
    w->setActive(isActive());
    w->setFullscreen(isFullScreen());
    w->setKeepAbove(keepAbove());
    w->setKeepBelow(keepBelow());
    w->setMaximized(maximizeMode() == KWin::MaximizeFull);
    w->setMinimized(isMinimized());
    w->setOnAllDesktops(isOnAllDesktops());
    w->setDemandsAttention(isDemandingAttention());
    w->setCloseable(isCloseable());
    w->setMaximizeable(isMaximizable());
    w->setMinimizeable(isMinimizable());
    w->setFullscreenable(isFullScreenable());
    w->setApplicationMenuPaths(applicationMenuServiceName(), applicationMenuObjectPath());
    w->setIcon(icon());
    auto updateAppId = [this, w] {
        w->setAppId(QString::fromUtf8(m_desktopFileName.isEmpty() ? resourceClass() : m_desktopFileName));
    };
    updateAppId();
    w->setSkipTaskbar(skipTaskbar());
    w->setSkipSwitcher(skipSwitcher());
    w->setPid(pid());
    w->setShadeable(isShadeable());
    w->setShaded(isShade());
    w->setResizable(isResizable());
    w->setMovable(isMovable());
    w->setVirtualDesktopChangeable(true); // FIXME Matches X11Client::actionSupported(), but both should be implemented.
    w->setParentWindow(transientFor() ? transientFor()->windowManagementInterface() : nullptr);
    w->setGeometry(frameGeometry());
    connect(this, &AbstractClient::skipTaskbarChanged, w,
        [w, this] {
            w->setSkipTaskbar(skipTaskbar());
        }
    );
    connect(this, &AbstractClient::skipSwitcherChanged, w,
         [w, this] {
            w->setSkipSwitcher(skipSwitcher());
        }
    );
    connect(this, &AbstractClient::captionChanged, w, [w, this] { w->setTitle(caption()); });

    connect(this, &AbstractClient::activeChanged, w, [w, this] { w->setActive(isActive()); });
    connect(this, &AbstractClient::fullScreenChanged, w, [w, this] { w->setFullscreen(isFullScreen()); });
    connect(this, &AbstractClient::keepAboveChanged, w, &PlasmaWindowInterface::setKeepAbove);
    connect(this, &AbstractClient::keepBelowChanged, w, &PlasmaWindowInterface::setKeepBelow);
    connect(this, &AbstractClient::minimizedChanged, w, [w, this] { w->setMinimized(isMinimized()); });
    connect(this, static_cast<void (AbstractClient::*)(AbstractClient*,MaximizeMode)>(&AbstractClient::clientMaximizedStateChanged), w,
        [w] (KWin::AbstractClient *c, MaximizeMode mode) {
            Q_UNUSED(c);
            w->setMaximized(mode == KWin::MaximizeFull);
        }
    );
    connect(this, &AbstractClient::demandsAttentionChanged, w, [w, this] { w->setDemandsAttention(isDemandingAttention()); });
    connect(this, &AbstractClient::iconChanged, w,
        [w, this] {
            w->setIcon(icon());
        }
    );
    connect(this, &AbstractClient::windowClassChanged, w, updateAppId);
    connect(this, &AbstractClient::desktopFileNameChanged, w, updateAppId);
    connect(this, &AbstractClient::shadeChanged, w, [w, this] { w->setShaded(isShade()); });
    connect(this, &AbstractClient::transientChanged, w,
        [w, this] {
            w->setParentWindow(transientFor() ? transientFor()->windowManagementInterface() : nullptr);
        }
    );
    connect(this, &AbstractClient::frameGeometryChanged, w,
        [w, this] {
            w->setGeometry(frameGeometry());
        }
    );
    connect(this, &AbstractClient::applicationMenuChanged, w,
        [w, this] {
            w->setApplicationMenuPaths(applicationMenuServiceName(), applicationMenuObjectPath());
        }
    );
    connect(w, &PlasmaWindowInterface::closeRequested, this, [this] { closeWindow(); });
    connect(w, &PlasmaWindowInterface::moveRequested, this,
        [this] {
            Cursors::self()->mouse()->setPos(frameGeometry().center());
            performMouseCommand(Options::MouseMove, Cursors::self()->mouse()->pos());
        }
    );
    connect(w, &PlasmaWindowInterface::resizeRequested, this,
        [this] {
            Cursors::self()->mouse()->setPos(frameGeometry().bottomRight());
            performMouseCommand(Options::MouseResize, Cursors::self()->mouse()->pos());
        }
    );
    connect(w, &PlasmaWindowInterface::fullscreenRequested, this,
        [this] (bool set) {
            setFullScreen(set, false);
        }
    );
    connect(w, &PlasmaWindowInterface::minimizedRequested, this,
        [this] (bool set) {
            if (set) {
                minimize();
            } else {
                unminimize();
            }
        }
    );
    connect(w, &PlasmaWindowInterface::maximizedRequested, this,
        [this] (bool set) {
            maximize(set ? MaximizeFull : MaximizeRestore);
        }
    );
    connect(w, &PlasmaWindowInterface::keepAboveRequested, this,
        [this] (bool set) {
            setKeepAbove(set);
        }
    );
    connect(w, &PlasmaWindowInterface::keepBelowRequested, this,
        [this] (bool set) {
            setKeepBelow(set);
        }
    );
    connect(w, &PlasmaWindowInterface::demandsAttentionRequested, this,
        [this] (bool set) {
            demandAttention(set);
        }
    );
    connect(w, &PlasmaWindowInterface::activeRequested, this,
        [this] (bool set) {
            if (set) {
                workspace()->activateClient(this, true);
            }
        }
    );
    connect(w, &PlasmaWindowInterface::shadedRequested, this,
        [this] (bool set) {
            setShade(set);
        }
    );

    for (const auto vd : qAsConst(m_desktops)) {
        w->addPlasmaVirtualDesktop(vd->id());
    }

    //Plasma Virtual desktop management
    //show/hide when the window enters/exits from desktop
    connect(w, &PlasmaWindowInterface::enterPlasmaVirtualDesktopRequested, this,
        [this] (const QString &desktopId) {
            VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForId(desktopId);
            if (vd) {
                enterDesktop(vd);
            }
        }
    );
    connect(w, &PlasmaWindowInterface::enterNewPlasmaVirtualDesktopRequested, this,
        [this] () {
            VirtualDesktopManager::self()->setCount(VirtualDesktopManager::self()->count() + 1);
            enterDesktop(VirtualDesktopManager::self()->desktops().last());
        }
    );
    connect(w, &PlasmaWindowInterface::leavePlasmaVirtualDesktopRequested, this,
        [this] (const QString &desktopId) {
            VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForId(desktopId);
            if (vd) {
                leaveDesktop(vd);
            }
        }
    );

    for (const auto &activity : qAsConst(m_activityList)) {
        w->addPlasmaActivity(activity);
    }

    // Notify clients on activities changes
    connect(this, &AbstractClient::activitiesChanged, w, [w, this] {
        const auto newActivities = m_activityList.toSet();
        const auto oldActivities = w->plasmaActivities().toSet();

        const auto activitiesToAdd = newActivities - oldActivities;
        for (const auto &activity : activitiesToAdd) {
            w->addPlasmaActivity(activity);
        }

        const auto activitiesToRemove = oldActivities - newActivities;
        for (const auto &activity : activitiesToRemove) {
            w->removePlasmaActivity(activity);
        }
    });

    //Plasma Activities management
    //show/hide when the window enters/exits activity
    connect(w, &PlasmaWindowInterface::enterPlasmaActivityRequested, this,
        [this] (const QString &activityId) {
            setOnActivity(activityId, true);
        }
    );
    connect(w, &PlasmaWindowInterface::leavePlasmaActivityRequested, this,
        [this] (const QString &activityId) {
            setOnActivity(activityId, false);
        }
    );
    connect(w, &PlasmaWindowInterface::sendToOutput, this,
        [this] (KWaylandServer::OutputInterface *output) {
            sendToOutput(waylandServer()->findOutput(output));
        }
    );

    m_windowManagementInterface = w;
}

Options::MouseCommand AbstractClient::getMouseCommand(Qt::MouseButton button, bool *handled) const
{
    *handled = false;
    if (button == Qt::NoButton) {
        return Options::MouseNothing;
    }
    if (isActive()) {
        if (options->isClickRaise() && !isMostRecentlyRaised()) {
            *handled = true;
            return Options::MouseActivateRaiseAndPassClick;
        }
    } else {
        *handled = true;
        switch (button) {
        case Qt::LeftButton:
            return options->commandWindow1();
        case Qt::MiddleButton:
            return options->commandWindow2();
        case Qt::RightButton:
            return options->commandWindow3();
        default:
            // all other buttons pass Activate & Pass Client
            return Options::MouseActivateAndPassClick;
        }
    }
    return Options::MouseNothing;
}

Options::MouseCommand AbstractClient::getWheelCommand(Qt::Orientation orientation, bool *handled) const
{
    *handled = false;
    if (orientation != Qt::Vertical) {
        return Options::MouseNothing;
    }
    if (!isActive()) {
        *handled = true;
        return options->commandWindowWheel();
    }
    return Options::MouseNothing;
}

bool AbstractClient::performMouseCommand(Options::MouseCommand cmd, const QPoint &globalPos)
{
    bool replay = false;
    switch(cmd) {
    case Options::MouseRaise:
        workspace()->raiseClient(this);
        break;
    case Options::MouseLower: {
        workspace()->lowerClient(this);
        // used to be activateNextClient(this), then topClientOnDesktop
        // since this is a mouseOp it's however safe to use the client under the mouse instead
        if (isActive() && options->focusPolicyIsReasonable()) {
            AbstractClient *next = workspace()->clientUnderMouse(output());
            if (next && next != this)
                workspace()->requestFocus(next, false);
        }
        break;
    }
    case Options::MouseOperationsMenu:
        if (isActive() && options->isClickRaise())
            autoRaise();
        workspace()->showWindowMenu(QRect(globalPos, globalPos), this);
        break;
    case Options::MouseToggleRaiseAndLower:
        workspace()->raiseOrLowerClient(this);
        break;
    case Options::MouseActivateAndRaise: {
        replay = isActive(); // for clickraise mode
        bool mustReplay = !rules()->checkAcceptFocus(acceptsFocus());
        if (mustReplay) {
            auto it = workspace()->stackingOrder().constEnd(),
                                     begin = workspace()->stackingOrder().constBegin();
            while (mustReplay && --it != begin && *it != this) {
                AbstractClient *c = qobject_cast<AbstractClient*>(*it);
                if (!c || (c->keepAbove() && !keepAbove()) || (keepBelow() && !c->keepBelow()))
                    continue; // can never raise above "it"
                mustReplay = !(c->isOnCurrentDesktop() && c->isOnCurrentActivity() && c->frameGeometry().intersects(frameGeometry()));
            }
        }
        workspace()->takeActivity(this, Workspace::ActivityFocus | Workspace::ActivityRaise);
        workspace()->setActiveOutput(globalPos);
        replay = replay || mustReplay;
        break;
    }
    case Options::MouseActivateAndLower:
        workspace()->requestFocus(this);
        workspace()->lowerClient(this);
        workspace()->setActiveOutput(globalPos);
        replay = replay || !rules()->checkAcceptFocus(acceptsFocus());
        break;
    case Options::MouseActivate:
        replay = isActive(); // for clickraise mode
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        workspace()->setActiveOutput(globalPos);
        replay = replay || !rules()->checkAcceptFocus(acceptsFocus());
        break;
    case Options::MouseActivateRaiseAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus | Workspace::ActivityRaise);
        workspace()->setActiveOutput(globalPos);
        replay = true;
        break;
    case Options::MouseActivateAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        workspace()->setActiveOutput(globalPos);
        replay = true;
        break;
    case Options::MouseMaximize:
        maximize(MaximizeFull);
        break;
    case Options::MouseRestore:
        maximize(MaximizeRestore);
        break;
    case Options::MouseMinimize:
        minimize();
        break;
    case Options::MouseAbove: {
        StackingUpdatesBlocker blocker(workspace());
        if (keepBelow())
            setKeepBelow(false);
        else
            setKeepAbove(true);
        break;
    }
    case Options::MouseBelow: {
        StackingUpdatesBlocker blocker(workspace());
        if (keepAbove())
            setKeepAbove(false);
        else
            setKeepBelow(true);
        break;
    }
    case Options::MousePreviousDesktop:
        workspace()->windowToPreviousDesktop(this);
        break;
    case Options::MouseNextDesktop:
        workspace()->windowToNextDesktop(this);
        break;
    case Options::MouseOpacityMore:
        if (!isDesktop())   // No point in changing the opacity of the desktop
            setOpacity(qMin(opacity() + 0.1, 1.0));
        break;
    case Options::MouseOpacityLess:
        if (!isDesktop())   // No point in changing the opacity of the desktop
            setOpacity(qMax(opacity() - 0.1, 0.1));
        break;
    case Options::MouseClose:
        closeWindow();
        break;
    case Options::MouseActivateRaiseAndMove:
    case Options::MouseActivateRaiseAndUnrestrictedMove:
        workspace()->raiseClient(this);
        workspace()->requestFocus(this);
        workspace()->setActiveOutput(globalPos);
        // fallthrough
    case Options::MouseMove:
    case Options::MouseUnrestrictedMove: {
        if (!isMovableAcrossScreens())
            break;
        if (isInteractiveMoveResize())
            finishInteractiveMoveResize(false);
        setInteractiveMoveResizeGravity(Gravity::None);
        setInteractiveMoveResizePointerButtonDown(true);
        setInteractiveMoveOffset(QPoint(globalPos.x() - x(), globalPos.y() - y()));  // map from global
        setInvertedInteractiveMoveOffset(rect().bottomRight() - interactiveMoveOffset());
        setUnrestrictedInteractiveMoveResize((cmd == Options::MouseActivateRaiseAndUnrestrictedMove
                                  || cmd == Options::MouseUnrestrictedMove));
        if (!startInteractiveMoveResize())
            setInteractiveMoveResizePointerButtonDown(false);
        updateCursor();
        break;
    }
    case Options::MouseResize:
    case Options::MouseUnrestrictedResize: {
        if (!isResizable() || isShade())
            break;
        if (isInteractiveMoveResize())
            finishInteractiveMoveResize(false);
        setInteractiveMoveResizePointerButtonDown(true);
        const QPoint moveOffset = QPoint(globalPos.x() - x(), globalPos.y() - y());  // map from global
        setInteractiveMoveOffset(moveOffset);
        int x = moveOffset.x(), y = moveOffset.y();
        bool left = x < width() / 3;
        bool right = x >= 2 * width() / 3;
        bool top = y < height() / 3;
        bool bot = y >= 2 * height() / 3;
        Gravity gravity;
        if (top) {
            gravity = left ? Gravity::TopLeft : (right ? Gravity::TopRight : Gravity::Top);
        } else if (bot) {
            gravity = left ? Gravity::BottomLeft : (right ? Gravity::BottomRight : Gravity::Bottom);
        } else {
            gravity = (x < width() / 2) ? Gravity::Left : Gravity::Right;
        }
        setInteractiveMoveResizeGravity(gravity);
        setInvertedInteractiveMoveOffset(rect().bottomRight() - moveOffset);
        setUnrestrictedInteractiveMoveResize((cmd == Options::MouseUnrestrictedResize));
        if (!startInteractiveMoveResize())
            setInteractiveMoveResizePointerButtonDown(false);
        updateCursor();
        break;
    }
    case Options::MouseShade:
        toggleShade();
        cancelShadeHoverTimer();
        break;
    case Options::MouseSetShade:
        setShade(ShadeNormal);
        cancelShadeHoverTimer();
        break;
    case Options::MouseUnsetShade:
        setShade(ShadeNone);
        cancelShadeHoverTimer();
        break;
    case Options::MouseNothing:
    default:
        replay = true;
        break;
    }
    return replay;
}

void AbstractClient::setTransientFor(AbstractClient *transientFor)
{
    if (transientFor == this) {
        // cannot be transient for one self
        return;
    }
    if (m_transientFor == transientFor) {
        return;
    }
    m_transientFor = transientFor;
    Q_EMIT transientChanged();
}

const AbstractClient *AbstractClient::transientFor() const
{
    return m_transientFor;
}

AbstractClient *AbstractClient::transientFor()
{
    return m_transientFor;
}

bool AbstractClient::hasTransientPlacementHint() const
{
    return false;
}

QRect AbstractClient::transientPlacement(const QRect &bounds) const
{
    Q_UNUSED(bounds);
    Q_UNREACHABLE();
    return QRect();
}

bool AbstractClient::hasTransient(const AbstractClient *c, bool indirect) const
{
    Q_UNUSED(indirect);
    return c->transientFor() == this;
}

QList< AbstractClient* > AbstractClient::mainClients() const
{
    if (const AbstractClient *t = transientFor()) {
        return QList<AbstractClient*>{const_cast< AbstractClient* >(t)};
    }
    return QList<AbstractClient*>();
}

QList<AbstractClient*> AbstractClient::allMainClients() const
{
    auto result = mainClients();
    for (const auto *cl : result) {
        result += cl->allMainClients();
    }
    return result;
}

void AbstractClient::setModal(bool m)
{
    // Qt-3.2 can have even modal normal windows :(
    if (m_modal == m)
        return;
    m_modal = m;
    Q_EMIT modalChanged();
    // Changing modality for a mapped window is weird (?)
    // _NET_WM_STATE_MODAL should possibly rather be _NET_WM_WINDOW_TYPE_MODAL_DIALOG
}

bool AbstractClient::isModal() const
{
    return m_modal;
}

// check whether a transient should be actually kept above its mainwindow
// there may be some special cases where this rule shouldn't be enfored
static bool shouldKeepTransientAbove(const AbstractClient *parent, const AbstractClient *transient)
{
    // #93832 - don't keep splashscreens above dialogs
    if (transient->isSplash() && parent->isDialog()) {
        return false;
    }
    // This is rather a hack for #76026. Don't keep non-modal dialogs above
    // the mainwindow, but only if they're group transient (since only such dialogs
    // have taskbar entry in Kicker). A proper way of doing this (both kwin and kicker)
    // needs to be found.
    if (transient->isDialog() && !transient->isModal() && transient->groupTransient()) {
        return false;
    }
    // #63223 - don't keep transients above docks, because the dock is kept high,
    // and e.g. dialogs for them would be too high too
    // ignore this if the transient has a placement hint which indicates it should go above it's parent
    if (parent->isDock() && !transient->hasTransientPlacementHint()) {
        return false;
    }
    return true;
}

void AbstractClient::addTransient(AbstractClient *cl)
{
    Q_ASSERT(!m_transients.contains(cl));
    Q_ASSERT(cl != this);
    m_transients.append(cl);
    if (shouldKeepTransientAbove(this, cl)) {
        workspace()->constrain(this, cl);
    }
}

void AbstractClient::removeTransient(AbstractClient *cl)
{
    m_transients.removeAll(cl);
    if (cl->transientFor() == this) {
        cl->setTransientFor(nullptr);
    }
    workspace()->unconstrain(this, cl);
}

void AbstractClient::removeTransientFromList(AbstractClient *cl)
{
    m_transients.removeAll(cl);
}

bool AbstractClient::isActiveFullScreen() const
{
    if (!isFullScreen())
        return false;

    const auto ac = workspace()->mostRecentlyActivatedClient(); // instead of activeClient() - avoids flicker
    // according to NETWM spec implementation notes suggests
    // "focused windows having state _NET_WM_STATE_FULLSCREEN" to be on the highest layer.
    // we'll also take the screen into account
    return ac && (ac == this || ac->output() != output()|| ac->allMainClients().contains(const_cast<AbstractClient*>(this)));
}

#define BORDER(which) \
    int AbstractClient::border##which() const \
    { \
        return isDecorated() ? decoration()->border##which() : 0; \
    }

BORDER(Bottom)
BORDER(Left)
BORDER(Right)
BORDER(Top)
#undef BORDER

void AbstractClient::updateInitialMoveResizeGeometry()
{
    m_interactiveMoveResize.initialGeometry = frameGeometry();
    m_interactiveMoveResize.startOutput = output();
}

void AbstractClient::updateCursor()
{
    Gravity gravity = interactiveMoveResizeGravity();
    if (!isResizable() || isShade())
        gravity = Gravity::None;
    CursorShape c = Qt::ArrowCursor;
    switch (gravity) {
    case Gravity::TopLeft:
        c = KWin::ExtendedCursor::SizeNorthWest;
        break;
    case Gravity::BottomRight:
        c = KWin::ExtendedCursor::SizeSouthEast;
        break;
    case Gravity::BottomLeft:
        c = KWin::ExtendedCursor::SizeSouthWest;
        break;
    case Gravity::TopRight:
        c = KWin::ExtendedCursor::SizeNorthEast;
        break;
    case Gravity::Top:
        c = KWin::ExtendedCursor::SizeNorth;
        break;
    case Gravity::Bottom:
        c = KWin::ExtendedCursor::SizeSouth;
        break;
    case Gravity::Left:
        c = KWin::ExtendedCursor::SizeWest;
        break;
    case Gravity::Right:
        c = KWin::ExtendedCursor::SizeEast;
        break;
    default:
        if (isInteractiveMoveResize())
            c = Qt::SizeAllCursor;
        else
            c = Qt::ArrowCursor;
        break;
    }
    if (c == m_interactiveMoveResize.cursor)
        return;
    m_interactiveMoveResize.cursor = c;
    Q_EMIT moveResizeCursorChanged(c);
}

void AbstractClient::leaveInteractiveMoveResize()
{
    workspace()->setMoveResizeClient(nullptr);
    setInteractiveMoveResize(false);
    if (ScreenEdges::self()->isDesktopSwitchingMovingClients())
        ScreenEdges::self()->reserveDesktopSwitching(false, Qt::Vertical|Qt::Horizontal);
    if (isElectricBorderMaximizing()) {
        outline()->hide();
        elevate(false);
    }
}

bool AbstractClient::doStartInteractiveMoveResize()
{
    return true;
}

void AbstractClient::doFinishInteractiveMoveResize()
{
}

bool AbstractClient::isWaitingForInteractiveMoveResizeSync() const
{
    return false;
}

void AbstractClient::doInteractiveResizeSync()
{
}

void AbstractClient::checkQuickTilingMaximizationZones(int xroot, int yroot)
{
    QuickTileMode mode = QuickTileFlag::None;
    bool innerBorder = false;

    const auto outputs = kwinApp()->platform()->enabledOutputs();
    for (const AbstractOutput *output : outputs) {
        if (!output->geometry().contains(QPoint(xroot, yroot))) {
            continue;
        }

        auto isInScreen = [&output, &outputs](const QPoint &pt) {
            for (const AbstractOutput *other : outputs) {
                if (other == output) {
                    continue;
                }
                if (other->geometry().contains(pt)) {
                    return true;
                }
            }
            return false;
        };

        QRect area = workspace()->clientArea(MaximizeArea, this, QPoint(xroot, yroot));
        if (options->electricBorderTiling()) {
            if (xroot <= area.x() + 20) {
                mode |= QuickTileFlag::Left;
                innerBorder = isInScreen(QPoint(area.x() - 1, yroot));
            } else if (xroot >= area.x() + area.width() - 20) {
                mode |= QuickTileFlag::Right;
                innerBorder = isInScreen(QPoint(area.right() + 1, yroot));
            }
        }

        if (mode != QuickTileMode(QuickTileFlag::None)) {
            if (yroot <= area.y() + area.height() * options->electricBorderCornerRatio())
                mode |= QuickTileFlag::Top;
            else if (yroot >= area.y() + area.height() - area.height()  * options->electricBorderCornerRatio())
                mode |= QuickTileFlag::Bottom;
        } else if (options->electricBorderMaximize() && yroot <= area.y() + 5 && isMaximizable()) {
            mode = QuickTileFlag::Maximize;
            innerBorder = isInScreen(QPoint(xroot, area.y() - 1));
        }
        break; // no point in checking other screens to contain this... "point"...
    }
    if (mode != electricBorderMode()) {
        setElectricBorderMode(mode);
        if (innerBorder) {
            if (!m_electricMaximizingDelay) {
                m_electricMaximizingDelay = new QTimer(this);
                m_electricMaximizingDelay->setInterval(250);
                m_electricMaximizingDelay->setSingleShot(true);
                connect(m_electricMaximizingDelay, &QTimer::timeout, this, [this]() {
                    if (isInteractiveMove())
                        setElectricBorderMaximizing(electricBorderMode() != QuickTileMode(QuickTileFlag::None));
                });
            }
            m_electricMaximizingDelay->start();
        } else {
            setElectricBorderMaximizing(mode != QuickTileMode(QuickTileFlag::None));
        }
    }
}

void AbstractClient::keyPressEvent(uint key_code)
{
    if (!isInteractiveMove() && !isInteractiveResize())
        return;
    bool is_control = key_code & Qt::CTRL;
    bool is_alt = key_code & Qt::ALT;
    key_code = key_code & ~Qt::KeyboardModifierMask;
    int delta = is_control ? 1 : is_alt ? 32 : 8;
    QPoint pos = Cursors::self()->mouse()->pos();
    switch(key_code) {
    case Qt::Key_Left:
        pos.rx() -= delta;
        break;
    case Qt::Key_Right:
        pos.rx() += delta;
        break;
    case Qt::Key_Up:
        pos.ry() -= delta;
        break;
    case Qt::Key_Down:
        pos.ry() += delta;
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        setInteractiveMoveResizePointerButtonDown(false);
        finishInteractiveMoveResize(false);
        updateCursor();
        break;
    case Qt::Key_Escape:
        setInteractiveMoveResizePointerButtonDown(false);
        finishInteractiveMoveResize(true);
        updateCursor();
        break;
    default:
        return;
    }
    Cursors::self()->mouse()->setPos(pos);
}

QSize AbstractClient::resizeIncrements() const
{
    return QSize(1, 1);
}

void AbstractClient::dontInteractiveMoveResize()
{
    setInteractiveMoveResizePointerButtonDown(false);
    stopDelayedInteractiveMoveResize();
    if (isInteractiveMoveResize())
        finishInteractiveMoveResize(false);
}

Gravity AbstractClient::mouseGravity() const
{
    if (isDecorated()) {
        switch (decoration()->sectionUnderMouse()) {
            case Qt::BottomLeftSection:
                return Gravity::BottomLeft;
            case Qt::BottomRightSection:
                return Gravity::BottomRight;
            case Qt::BottomSection:
                return Gravity::Bottom;
            case Qt::LeftSection:
                return Gravity::Left;
            case Qt::RightSection:
                return Gravity::Right;
            case Qt::TopSection:
                return Gravity::Top;
            case Qt::TopLeftSection:
                return Gravity::TopLeft;
            case Qt::TopRightSection:
                return Gravity::TopRight;
            default:
                return Gravity::None;
        }
    }
    return Gravity::None;
}

void AbstractClient::endInteractiveMoveResize()
{
    setInteractiveMoveResizePointerButtonDown(false);
    stopDelayedInteractiveMoveResize();
    if (isInteractiveMoveResize()) {
        finishInteractiveMoveResize(false);
        setInteractiveMoveResizeGravity(mouseGravity());
    }
    updateCursor();
}

void AbstractClient::setDecoration(QSharedPointer<KDecoration2::Decoration> decoration)
{
    if (m_decoration.decoration.data() == decoration) {
        return;
    }
    if (decoration) {
        QMetaObject::invokeMethod(decoration.data(), QOverload<>::of(&KDecoration2::Decoration::update), Qt::QueuedConnection);
        connect(decoration.data(), &KDecoration2::Decoration::shadowChanged, this, &Toplevel::updateShadow);
        connect(decoration.data(), &KDecoration2::Decoration::bordersChanged,
                this, &AbstractClient::updateDecorationInputShape);
        connect(decoration.data(), &KDecoration2::Decoration::resizeOnlyBordersChanged,
                this, &AbstractClient::updateDecorationInputShape);
        connect(decoration.data(), &KDecoration2::Decoration::bordersChanged, this, [this]() {
            GeometryUpdatesBlocker blocker(this);
            const QRect oldGeometry = frameGeometry();
            resize(implicitSize());
            if (!isShade()) {
                checkWorkspacePosition(oldGeometry);
            }
            Q_EMIT geometryShapeChanged(this, oldGeometry);
        });
        connect(decoratedClient()->decoratedClient(), &KDecoration2::DecoratedClient::sizeChanged,
                this, &AbstractClient::updateDecorationInputShape);
    }
    m_decoration.decoration = decoration;
    updateDecorationInputShape();
    Q_EMIT decorationChanged();
}

void AbstractClient::updateDecorationInputShape()
{
    if (!isDecorated()) {
        m_decoration.inputRegion = QRegion();
        return;
    }

    const QMargins borders = decoration()->borders();
    const QMargins resizeBorders = decoration()->resizeOnlyBorders();

    const QRect innerRect = QRect(QPoint(borderLeft(), borderTop()), decoratedClient()->size());
    const QRect outerRect = innerRect + borders + resizeBorders;

    m_decoration.inputRegion = QRegion(outerRect) - innerRect;
}

bool AbstractClient::decorationHasAlpha() const
{
    if (!isDecorated() || decoration()->isOpaque()) {
        // either no decoration or decoration has alpha disabled
        return false;
    }
    return true;
}

void AbstractClient::triggerDecorationRepaint()
{
    if (isDecorated()) {
        decoration()->update();
    }
}

void AbstractClient::layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const
{
    if (!isDecorated()) {
        return;
    }
    QRect r = decoration()->rect();

    top = QRect(r.x(), r.y(), r.width(), borderTop());
    bottom = QRect(r.x(), r.y() + r.height() - borderBottom(),
                   r.width(), borderBottom());
    left = QRect(r.x(), r.y() + top.height(),
                 borderLeft(), r.height() - top.height() - bottom.height());
    right = QRect(r.x() + r.width() - borderRight(), r.y() + top.height(),
                  borderRight(), r.height() - top.height() - bottom.height());
}

void AbstractClient::processDecorationMove(const QPoint &localPos, const QPoint &globalPos)
{
    if (isInteractiveMoveResizePointerButtonDown()) {
        handleInteractiveMoveResize(localPos.x(), localPos.y(), globalPos.x(), globalPos.y());
        return;
    }
    // TODO: handle modifiers
    Gravity newGravity = mouseGravity();
    if (newGravity != interactiveMoveResizeGravity()) {
        setInteractiveMoveResizeGravity(newGravity);
        updateCursor();
    }
}

bool AbstractClient::processDecorationButtonPress(QMouseEvent *event, bool ignoreMenu)
{
    Options::MouseCommand com = Options::MouseNothing;
    bool active = isActive();
    if (!wantsInput())    // we cannot be active, use it anyway
        active = true;

    // check whether it is a double click
    if (event->button() == Qt::LeftButton && titlebarPositionUnderMouse()) {
        if (m_decoration.doubleClickTimer.isValid()) {
            const qint64 interval = m_decoration.doubleClickTimer.elapsed();
            m_decoration.doubleClickTimer.invalidate();
            if (interval > QGuiApplication::styleHints()->mouseDoubleClickInterval()) {
                m_decoration.doubleClickTimer.start(); // expired -> new first click and pot. init
            } else {
                Workspace::self()->performWindowOperation(this, options->operationTitlebarDblClick());
                dontInteractiveMoveResize();
                return false;
            }
        }
         else {
            m_decoration.doubleClickTimer.start(); // new first click and pot. init, could be invalidated by release - see below
        }
    }

    if (event->button() == Qt::LeftButton)
        com = active ? options->commandActiveTitlebar1() : options->commandInactiveTitlebar1();
    else if (event->button() == Qt::MiddleButton)
        com = active ? options->commandActiveTitlebar2() : options->commandInactiveTitlebar2();
    else if (event->button() == Qt::RightButton)
        com = active ? options->commandActiveTitlebar3() : options->commandInactiveTitlebar3();
    if (event->button() == Qt::LeftButton
            && com != Options::MouseOperationsMenu // actions where it's not possible to get the matching
            && com != Options::MouseMinimize)  // mouse release event
    {
        setInteractiveMoveResizeGravity(mouseGravity());
        setInteractiveMoveResizePointerButtonDown(true);
        setInteractiveMoveOffset(event->pos());
        setInvertedInteractiveMoveOffset(rect().bottomRight() - interactiveMoveOffset());
        setUnrestrictedInteractiveMoveResize(false);
        startDelayedInteractiveMoveResize();
        updateCursor();
    }
    // In the new API the decoration may process the menu action to display an inactive tab's menu.
    // If the event is unhandled then the core will create one for the active window in the group.
    if (!ignoreMenu || com != Options::MouseOperationsMenu)
        performMouseCommand(com, event->globalPos());
    return !( // Return events that should be passed to the decoration in the new API
               com == Options::MouseRaise ||
               com == Options::MouseOperationsMenu ||
               com == Options::MouseActivateAndRaise ||
               com == Options::MouseActivate ||
               com == Options::MouseActivateRaiseAndPassClick ||
               com == Options::MouseActivateAndPassClick ||
               com == Options::MouseNothing);
}

void AbstractClient::processDecorationButtonRelease(QMouseEvent *event)
{
    if (isDecorated()) {
        if (event->isAccepted() || !titlebarPositionUnderMouse()) {
            invalidateDecorationDoubleClickTimer(); // click was for the deco and shall not init a doubleclick
        }
    }

    if (event->buttons() == Qt::NoButton) {
        setInteractiveMoveResizePointerButtonDown(false);
        stopDelayedInteractiveMoveResize();
        if (isInteractiveMoveResize()) {
            finishInteractiveMoveResize(false);
            setInteractiveMoveResizeGravity(mouseGravity());
        }
        updateCursor();
    }
}


void AbstractClient::startDecorationDoubleClickTimer()
{
    m_decoration.doubleClickTimer.start();
}

void AbstractClient::invalidateDecorationDoubleClickTimer()
{
    m_decoration.doubleClickTimer.invalidate();
}

bool AbstractClient::providesContextHelp() const
{
    return false;
}

void AbstractClient::showContextHelp()
{
}

QPointer<Decoration::DecoratedClientImpl> AbstractClient::decoratedClient() const
{
    return m_decoration.client;
}

void AbstractClient::setDecoratedClient(QPointer< Decoration::DecoratedClientImpl > client)
{
    m_decoration.client = client;
}

void AbstractClient::pointerEnterEvent(const QPoint &globalPos)
{
    if (options->isShadeHover()) {
        cancelShadeHoverTimer();
        startShadeHoverTimer();
    }

    if (options->focusPolicy() == Options::ClickToFocus || workspace()->userActionsMenu()->isShown())
        return;

    if (options->isAutoRaise() && !isDesktop() &&
            !isDock() && workspace()->focusChangeEnabled() &&
            globalPos != workspace()->focusMousePosition() &&
            workspace()->topClientOnDesktop(VirtualDesktopManager::self()->currentDesktop(),
                                            options->isSeparateScreenFocus() ? output() : nullptr) != this) {
        startAutoRaise();
    }

    if (isDesktop() || isDock())
        return;
    // for FocusFollowsMouse, change focus only if the mouse has actually been moved, not if the focus
    // change came because of window changes (e.g. closing a window) - #92290
    if (options->focusPolicy() != Options::FocusFollowsMouse
            || globalPos != workspace()->focusMousePosition()) {
        workspace()->requestDelayFocus(this);
    }
}

void AbstractClient::pointerLeaveEvent()
{
    cancelAutoRaise();
    workspace()->cancelDelayFocus();
    cancelShadeHoverTimer();
    startShadeUnhoverTimer();
    // TODO: send hover leave to deco
    // TODO: handle Options::FocusStrictlyUnderMouse
}

QRect AbstractClient::iconGeometry() const
{
    if (!windowManagementInterface() || !waylandServer()) {
        // window management interface is only available if the surface is mapped
        return QRect();
    }

    int minDistance = INT_MAX;
    AbstractClient *candidatePanel = nullptr;
    QRect candidateGeom;

    const auto minGeometries = windowManagementInterface()->minimizedGeometries();
    for (auto i = minGeometries.constBegin(), end = minGeometries.constEnd(); i != end; ++i) {
        AbstractClient *client = waylandServer()->findClient(i.key());
        if (!client) {
            continue;
        }
        const int distance = QPoint(client->pos() - pos()).manhattanLength();
        if (distance < minDistance) {
            minDistance = distance;
            candidatePanel = client;
            candidateGeom = i.value();
        }
    }
    if (!candidatePanel) {
        return QRect();
    }
    return candidateGeom.translated(candidatePanel->pos());
}

QRect AbstractClient::inputGeometry() const
{
    if (isDecorated()) {
        return Toplevel::inputGeometry() + decoration()->resizeOnlyBorders();
    }
    return Toplevel::inputGeometry();
}

bool AbstractClient::hitTest(const QPoint &point) const
{
    if (isDecorated()) {
        if (m_decoration.inputRegion.contains(mapToFrame(point))) {
            return true;
        }
    }
    return Toplevel::hitTest(point);
}

QRect AbstractClient::virtualKeyboardGeometry() const
{
    return m_virtualKeyboardGeometry;
}

void AbstractClient::setVirtualKeyboardGeometry(const QRect &geo)
{
    // No keyboard anymore
    if (geo.isEmpty() && !m_keyboardGeometryRestore.isEmpty()) {
        const QRect availableArea = workspace()->clientArea(MaximizeArea, this);
        QRect newWindowGeometry = (maximizeMode() & MaximizeHorizontal) ? availableArea : m_keyboardGeometryRestore;
        moveResize(newWindowGeometry);
        m_keyboardGeometryRestore = QRect();
    } else if (geo.isEmpty()) {
        return;
    // The keyboard has just been opened (rather than resized) save client geometry for a restore
    } else if (m_keyboardGeometryRestore.isEmpty()) {
        m_keyboardGeometryRestore = moveResizeGeometry();
    }

    m_virtualKeyboardGeometry = geo;

    // Don't resize Desktop and fullscreen windows
    if (isFullScreen() || isDesktop()) {
        return;
    }

    if (!geo.intersects(m_keyboardGeometryRestore)) {
        return;
    }

    const QRect availableArea = workspace()->clientArea(MaximizeArea, this);
    QRect newWindowGeometry = (maximizeMode() & MaximizeHorizontal) ? availableArea : m_keyboardGeometryRestore;
    newWindowGeometry.moveBottom(geo.top());
    newWindowGeometry.setTop(qMax(newWindowGeometry.top(), availableArea.top()));

    moveResize(newWindowGeometry);
}

QRect AbstractClient::keyboardGeometryRestore() const
{
    return m_keyboardGeometryRestore;
}

void AbstractClient::setKeyboardGeometryRestore(const QRect &geom)
{
    m_keyboardGeometryRestore = geom;
}

bool AbstractClient::dockWantsInput() const
{
    return false;
}

void AbstractClient::setDesktopFileName(QByteArray name)
{
    name = rules()->checkDesktopFile(name).toUtf8();
    if (name == m_desktopFileName) {
        return;
    }
    m_desktopFileName = name;
    updateWindowRules(Rules::DesktopFile);
    Q_EMIT desktopFileNameChanged();
}

QString AbstractClient::iconFromDesktopFile() const
{
    return iconFromDesktopFile(QFile::decodeName(m_desktopFileName));
}

QString AbstractClient::iconFromDesktopFile(const QString &desktopFileName)
{
    if (desktopFileName.isEmpty()) {
        return {};
    }

    const QString desktopFileNameWithPrefix = desktopFileName + QLatin1String(".desktop");
    QString desktopFilePath;

    if (QDir::isAbsolutePath(desktopFileName)) {
        if (QFile::exists(desktopFileNameWithPrefix)) {
            desktopFilePath = desktopFileNameWithPrefix;
        } else {
            desktopFilePath = desktopFileName;
        }
    }

    if (desktopFilePath.isEmpty()) {
        desktopFilePath = QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                                 desktopFileNameWithPrefix);
    }
    if (desktopFilePath.isEmpty()) {
        desktopFilePath = QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                                 desktopFileName);
    }
    if (desktopFilePath.isEmpty()) {
        return {};
    }

    KDesktopFile df(desktopFilePath);
    return df.readIcon();
}

bool AbstractClient::hasApplicationMenu() const
{
    return ApplicationMenu::self()->applicationMenuEnabled() && !m_applicationMenuServiceName.isEmpty() && !m_applicationMenuObjectPath.isEmpty();
}

void AbstractClient::updateApplicationMenuServiceName(const QString &serviceName)
{
    const bool old_hasApplicationMenu = hasApplicationMenu();

    m_applicationMenuServiceName = serviceName;

    const bool new_hasApplicationMenu = hasApplicationMenu();

    Q_EMIT applicationMenuChanged();
    if (old_hasApplicationMenu != new_hasApplicationMenu) {
        Q_EMIT hasApplicationMenuChanged(new_hasApplicationMenu);
    }
}

void AbstractClient::updateApplicationMenuObjectPath(const QString &objectPath)
{
    const bool old_hasApplicationMenu = hasApplicationMenu();

    m_applicationMenuObjectPath = objectPath;

    const bool new_hasApplicationMenu = hasApplicationMenu();

    Q_EMIT applicationMenuChanged();
    if (old_hasApplicationMenu != new_hasApplicationMenu) {
        Q_EMIT hasApplicationMenuChanged(new_hasApplicationMenu);
    }
}

void AbstractClient::setApplicationMenuActive(bool applicationMenuActive)
{
    if (m_applicationMenuActive != applicationMenuActive) {
        m_applicationMenuActive = applicationMenuActive;
        Q_EMIT applicationMenuActiveChanged(applicationMenuActive);
    }
}

void AbstractClient::showApplicationMenu(int actionId)
{
    if (isDecorated()) {
        decoration()->showApplicationMenu(actionId);
    } else {
        // we don't know where the application menu button will be, show it in the top left corner instead
        Workspace::self()->showApplicationMenu(QRect(), this, actionId);
    }
}

bool AbstractClient::unresponsive() const
{
    return m_unresponsive;
}

void AbstractClient::setUnresponsive(bool unresponsive)
{
    if (m_unresponsive != unresponsive) {
        m_unresponsive = unresponsive;
        Q_EMIT unresponsiveChanged(m_unresponsive);
        Q_EMIT captionChanged();
    }
}

QString AbstractClient::shortcutCaptionSuffix() const
{
    if (shortcut().isEmpty()) {
        return QString();
    }
    return QLatin1String(" {") + shortcut().toString() + QLatin1Char('}');
}

AbstractClient *AbstractClient::findClientWithSameCaption() const
{
    auto fetchNameInternalPredicate = [this](const AbstractClient *cl) {
        return (!cl->isSpecialWindow() || cl->isToolbar()) && cl != this && cl->captionNormal() == captionNormal() && cl->captionSuffix() == captionSuffix();
    };
    return workspace()->findAbstractClient(fetchNameInternalPredicate);
}

QString AbstractClient::caption() const
{
    QString cap = captionNormal() + captionSuffix();
    if (unresponsive()) {
        cap += QLatin1String(" ");
        cap += i18nc("Application is not responding, appended to window title", "(Not Responding)");
    }
    return cap;
}

void AbstractClient::removeRule(Rules* rule)
{
    m_rules.remove(rule);
}

void AbstractClient::discardTemporaryRules()
{
    m_rules.discardTemporary();
}

void AbstractClient::evaluateWindowRules()
{
    setupWindowRules(true);
    applyWindowRules();
}

/**
 * Returns the list of activities the client window is on.
 * if it's on all activities, the list will be empty.
 * Don't use this, use isOnActivity() and friends (from class Toplevel)
 */
QStringList AbstractClient::activities() const
{
    return m_activityList;
}

/**
 * Sets whether the client is on @p activity.
 * If you remove it from its last activity, then it's on all activities.
 *
 * Note: If it was on all activities and you try to remove it from one, nothing will happen;
 * I don't think that's an important enough use case to handle here.
 */
void AbstractClient::setOnActivity(const QString &activity, bool enable)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return;
    }
    QStringList newActivitiesList = activities();
    if (newActivitiesList.contains(activity) == enable) {
        //nothing to do
        return;
    }
    if (enable) {
        QStringList allActivities = Activities::self()->all();
        if (!allActivities.contains(activity)) {
            //bogus ID
            return;
        }
        newActivitiesList.append(activity);
    } else {
        newActivitiesList.removeOne(activity);
    }
    setOnActivities(newActivitiesList);
#else
    Q_UNUSED(activity)
    Q_UNUSED(enable)
#endif
}

/**
 * set exactly which activities this client is on
 */
void AbstractClient::setOnActivities(const QStringList &newActivitiesList)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return;
    }
    const auto allActivities = Activities::self()->all();
    const auto activityList = [&] {
        auto result = rules()->checkActivity(newActivitiesList);

        const auto it = std::remove_if(result.begin(), result.end(), [=](const QString &activity) {
            return !allActivities.contains(activity);
        });
        result.erase(it, result.end());
        return result;
    }();

    const auto allActivityExplicitlyRequested = activityList.isEmpty() || activityList.contains(Activities::nullUuid());
    const auto allActivitiesCovered = activityList.size() > 1 && activityList.size() == allActivities.size();

    if (allActivityExplicitlyRequested || allActivitiesCovered) {
        if (!m_activityList.isEmpty()) {
            m_activityList.clear();
            doSetOnActivities(m_activityList);
        }
    } else {
        if (m_activityList != activityList) {
            m_activityList = activityList;
            doSetOnActivities(m_activityList);
        }
    }

    updateActivities(false);
#else
    Q_UNUSED(newActivitiesList)
#endif
}

/**
 * if @p all is true, sets on all activities.
 * if it's false, sets it to only be on the current activity
 */
void AbstractClient::setOnAllActivities(bool all)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (all == isOnAllActivities()) {
        return;
    }
    if (all) {
        setOnActivities(QStringList());
    } else {
        setOnActivity(Activities::self()->current(), true);
    }
#else
    Q_UNUSED(all)
#endif
}

/**
 * update after activities changed
 */
void AbstractClient::updateActivities(bool includeTransients)
{
    if (m_activityUpdatesBlocked) {
        m_blockedActivityUpdatesRequireTransients |= includeTransients;
        return;
    }
    Q_EMIT activitiesChanged(this);
    m_blockedActivityUpdatesRequireTransients = false; // reset
    FocusChain::self()->update(this, FocusChain::MakeFirst);
    updateWindowRules(Rules::Activity);
}

void AbstractClient::blockActivityUpdates(bool b)
{
    if (b) {
        ++m_activityUpdatesBlocked;
    } else {
        Q_ASSERT(m_activityUpdatesBlocked);
        --m_activityUpdatesBlocked;
        if (!m_activityUpdatesBlocked) {
            updateActivities(m_blockedActivityUpdatesRequireTransients);
        }
    }
}

void AbstractClient::checkNoBorder()
{
    setNoBorder(false);
}

bool AbstractClient::groupTransient() const
{
    return false;
}

const Group *AbstractClient::group() const
{
    return nullptr;
}

Group *AbstractClient::group()
{
    return nullptr;
}

bool AbstractClient::supportsWindowRules() const
{
    return false;
}

QMargins AbstractClient::frameMargins() const
{
    return QMargins(borderLeft(), borderTop(), borderRight(), borderBottom());
}

QPoint AbstractClient::framePosToClientPos(const QPoint &point) const
{
    return point + QPoint(borderLeft(), borderTop());
}

QPoint AbstractClient::clientPosToFramePos(const QPoint &point) const
{
    return point - QPoint(borderLeft(), borderTop());
}

QSize AbstractClient::frameSizeToClientSize(const QSize &size) const
{
    const int width = size.width() - borderLeft() - borderRight();
    const int height = size.height() - borderTop() - borderBottom();
    return QSize(width, height);
}

QSize AbstractClient::clientSizeToFrameSize(const QSize &size) const
{
    const int width = size.width() + borderLeft() + borderRight();
    const int height = size.height() + borderTop() + borderBottom();
    return QSize(width, height);
}

QRect AbstractClient::frameRectToClientRect(const QRect &rect) const
{
    const QPoint position = framePosToClientPos(rect.topLeft());
    const QSize size = frameSizeToClientSize(rect.size());
    return QRect(position, size);
}

QRect AbstractClient::clientRectToFrameRect(const QRect &rect) const
{
    const QPoint position = clientPosToFramePos(rect.topLeft());
    const QSize size = clientSizeToFrameSize(rect.size());
    return QRect(position, size);
}

QRect AbstractClient::moveResizeGeometry() const
{
    return m_moveResizeGeometry;
}

void AbstractClient::setMoveResizeGeometry(const QRect &geo)
{
    m_moveResizeGeometry = geo;
}

void AbstractClient::move(const QPoint &point)
{
    m_moveResizeGeometry.moveTopLeft(point);
    moveResizeInternal(m_moveResizeGeometry, MoveResizeMode::Move);
}

void AbstractClient::resize(const QSize &size)
{
    m_moveResizeGeometry.setSize(size);
    moveResizeInternal(m_moveResizeGeometry, MoveResizeMode::Resize);
}

void AbstractClient::moveResize(const QRect &rect)
{
    m_moveResizeGeometry = rect;
    moveResizeInternal(m_moveResizeGeometry, MoveResizeMode::MoveResize);
}

void AbstractClient::setElectricBorderMode(QuickTileMode mode)
{
    if (mode != QuickTileMode(QuickTileFlag::Maximize)) {
        // sanitize the mode, ie. simplify "invalid" combinations
        if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Horizontal))
            mode &= ~QuickTileMode(QuickTileFlag::Horizontal);
        if ((mode & QuickTileFlag::Vertical) == QuickTileMode(QuickTileFlag::Vertical))
            mode &= ~QuickTileMode(QuickTileFlag::Vertical);
    }
    m_electricMode = mode;
}

void AbstractClient::setElectricBorderMaximizing(bool maximizing)
{
    m_electricMaximizing = maximizing;
    if (maximizing)
        outline()->show(electricBorderMaximizeGeometry(Cursors::self()->mouse()->pos()), moveResizeGeometry());
    else
        outline()->hide();
    elevate(maximizing);
}

QRect AbstractClient::electricBorderMaximizeGeometry(const QPoint &pos) const
{
    if (electricBorderMode() == QuickTileMode(QuickTileFlag::Maximize)) {
        if (maximizeMode() == MaximizeFull)
            return geometryRestore();
        else
            return workspace()->clientArea(MaximizeArea, this, pos);
    }

    QRect ret = workspace()->clientArea(MaximizeArea, this, pos);
    if (electricBorderMode() & QuickTileFlag::Left)
        ret.setRight(ret.left()+ret.width()/2 - 1);
    else if (electricBorderMode() & QuickTileFlag::Right)
        ret.setLeft(ret.right()-(ret.width()-ret.width()/2) + 1);
    if (electricBorderMode() & QuickTileFlag::Top)
        ret.setBottom(ret.top()+ret.height()/2 - 1);
    else if (electricBorderMode() & QuickTileFlag::Bottom)
        ret.setTop(ret.bottom()-(ret.height()-ret.height()/2) + 1);

    return ret;
}

void AbstractClient::setQuickTileMode(QuickTileMode mode, bool keyboard)
{
    // Only allow quick tile on a regular window.
    if (!isResizable()) {
        return;
    }

    workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event

    GeometryUpdatesBlocker blocker(this);

    if (mode == QuickTileMode(QuickTileFlag::Maximize)) {
        m_quickTileMode = int(QuickTileFlag::None);
        if (maximizeMode() == MaximizeFull) {
            setMaximize(false, false);
        } else {
            QRect prev_geom_restore = geometryRestore(); // setMaximize() would set moveResizeGeom as geom_restore
            m_quickTileMode = int(QuickTileFlag::Maximize);
            setMaximize(true, true);
            setGeometryRestore(prev_geom_restore);
        }
        doSetQuickTileMode();
        Q_EMIT quickTileModeChanged();
        return;
    }

    // sanitize the mode, ie. simplify "invalid" combinations
    if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Horizontal))
        mode &= ~QuickTileMode(QuickTileFlag::Horizontal);
    if ((mode & QuickTileFlag::Vertical) == QuickTileMode(QuickTileFlag::Vertical))
        mode &= ~QuickTileMode(QuickTileFlag::Vertical);

    setElectricBorderMode(mode); // used by ::electricBorderMaximizeGeometry(.)

    // restore from maximized so that it is possible to tile maximized windows with one hit or by dragging
    if (maximizeMode() != MaximizeRestore) {

        if (mode != QuickTileMode(QuickTileFlag::None)) {
            m_quickTileMode = int(QuickTileFlag::None); // Temporary, so the maximize code doesn't get all confused

            setMaximize(false, false);

            moveResize(electricBorderMaximizeGeometry(keyboard ? moveResizeGeometry().center() : Cursors::self()->mouse()->pos()));
            // Store the mode change
            m_quickTileMode = mode;
        } else {
            m_quickTileMode = mode;
            setMaximize(false, false);
        }

        doSetQuickTileMode();
        Q_EMIT quickTileModeChanged();

        return;
    }

    if (mode != QuickTileMode(QuickTileFlag::None)) {
        QPoint whichScreen = keyboard ? moveResizeGeometry().center() : Cursors::self()->mouse()->pos();

        // If trying to tile to the side that the window is already tiled to move the window to the next
        // screen if it exists, otherwise toggle the mode (set QuickTileFlag::None)
        if (quickTileMode() == mode) {
            const QVector<AbstractOutput *> outputs = kwinApp()->platform()->enabledOutputs();
            const AbstractOutput *currentOutput = output();
            const AbstractOutput *nextOutput = currentOutput;

            for (const AbstractOutput *output : outputs) {
                if (output == currentOutput) {
                    continue;
                }

                if (output->geometry().bottom() <= currentOutput->geometry().top()
                    || output->geometry().top() >= currentOutput->geometry().bottom()) {
                    continue; // not in horizontal line
                }

                const int x = output->geometry().center().x();
                if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Left)) {
                    if (x >= currentOutput->geometry().center().x()
                        || (currentOutput != nextOutput && x <= nextOutput->geometry().center().x())) {
                        continue; // not left of current or more left then found next
                    }
                } else if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Right)) {
                    if (x <= currentOutput->geometry().center().x()
                        || (currentOutput != nextOutput && x >= nextOutput->geometry().center().x())) {
                        continue; // not right of current or more right then found next
                    }
                }

                nextOutput = output;
            }

            if (nextOutput == currentOutput) {
                mode = QuickTileFlag::None; // No other screens, toggle tiling
            } else {
                // Move to other screen
                moveResize(geometryRestore().translated(nextOutput->geometry().topLeft() - currentOutput->geometry().topLeft()));
                whichScreen = nextOutput->geometry().center();

                // Swap sides
                if (mode & QuickTileFlag::Horizontal) {
                    mode = (~mode & QuickTileFlag::Horizontal) | (mode & QuickTileFlag::Vertical);
                }
            }
            setElectricBorderMode(mode); // used by ::electricBorderMaximizeGeometry(.)
        } else if (quickTileMode() == QuickTileMode(QuickTileFlag::None)) {
            // Not coming out of an existing tile, not shifting monitors, we're setting a brand new tile.
            // Store geometry first, so we can go out of this tile later.
            setGeometryRestore(moveResizeGeometry());
        }

        if (mode != QuickTileMode(QuickTileFlag::None)) {
            m_quickTileMode = mode;
            // Temporary, so the maximize code doesn't get all confused
            m_quickTileMode = int(QuickTileFlag::None);
            moveResize(electricBorderMaximizeGeometry(whichScreen));
        }

        // Store the mode change
        m_quickTileMode = mode;
    }

    if (mode == QuickTileMode(QuickTileFlag::None)) {
        m_quickTileMode = int(QuickTileFlag::None);
        // Untiling, so just restore geometry, and we're done.
        if (!geometryRestore().isValid()) // invalid if we started maximized and wait for placement
            setGeometryRestore(moveResizeGeometry());
        moveResize(geometryRestore());
        checkWorkspacePosition(); // Just in case it's a different screen
    }
    doSetQuickTileMode();
    Q_EMIT quickTileModeChanged();
}

void AbstractClient::doSetQuickTileMode()
{
}

void AbstractClient::sendToOutput(AbstractOutput *newOutput)
{
    newOutput = rules()->checkOutput(newOutput);
    if (isActive()) {
        workspace()->setActiveOutput(newOutput);
        // might impact the layer of a fullscreen window
        const auto clients = workspace()->allClientList();
        for (AbstractClient *cc : clients) {
            if (cc->isFullScreen() && cc->output() == newOutput) {
                cc->updateLayer();
            }
        }
    }
    if (output() == newOutput && !isFullScreen())   // Don't use isOnScreen(), that's true even when only partially
        return;

    GeometryUpdatesBlocker blocker(this);

    // operating on the maximized / quicktiled window would leave the old geom_restore behind,
    // so we clear the state first
    MaximizeMode maxMode = maximizeMode();
    QuickTileMode qtMode = quickTileMode();
    if (maxMode != MaximizeRestore)
        maximize(MaximizeRestore);
    if (qtMode != QuickTileMode(QuickTileFlag::None))
        setQuickTileMode(QuickTileFlag::None, true);

    QRect oldScreenArea = workspace()->clientArea(MaximizeArea, this);
    QRect screenArea = workspace()->clientArea(MaximizeArea, this, newOutput);

    // the window can have its center so that the position correction moves the new center onto
    // the old screen, what will tile it where it is. Ie. the screen is not changed
    // this happens esp. with electric border quicktiling
    if (qtMode != QuickTileMode(QuickTileFlag::None))
        keepInArea(oldScreenArea);

    QRect oldGeom = moveResizeGeometry();
    QRect newGeom = oldGeom;
    // move the window to have the same relative position to the center of the screen
    // (i.e. one near the middle of the right edge will also end up near the middle of the right edge)
    QPoint center = newGeom.center() - oldScreenArea.center();
    center.setX(center.x() * screenArea.width() / oldScreenArea.width());
    center.setY(center.y() * screenArea.height() / oldScreenArea.height());
    center += screenArea.center();
    newGeom.moveCenter(center);
    moveResize(newGeom);

    // If the window was inside the old screen area, explicitly make sure its inside also the new screen area.
    // Calling checkWorkspacePosition() should ensure that, but when moving to a small screen the window could
    // be big enough to overlap outside of the new screen area, making struts from other screens come into effect,
    // which could alter the resulting geometry.
    if (oldScreenArea.contains(oldGeom)) {
        keepInArea(screenArea);
    }

    if (isFullScreen()) {
        updateGeometryRestoresForFullscreen(newOutput);
        checkWorkspacePosition(oldGeom);
    } else {
        // align geom_restore - checkWorkspacePosition operates on it
        setGeometryRestore(moveResizeGeometry());

        checkWorkspacePosition(oldGeom);

        // re-align geom_restore to constrained geometry
        setGeometryRestore(moveResizeGeometry());
    }
    // finally reset special states
    // NOTICE that MaximizeRestore/QuickTileFlag::None checks are required.
    // eg. setting QuickTileFlag::None would break maximization
    if (maxMode != MaximizeRestore)
        maximize(maxMode);
    if (qtMode != QuickTileMode(QuickTileFlag::None) && qtMode != quickTileMode())
        setQuickTileMode(qtMode, true);

    auto tso = workspace()->ensureStackingOrder(transients());
    for (auto it = tso.constBegin(), end = tso.constEnd(); it != end; ++it)
        (*it)->sendToOutput(newOutput);
}

void AbstractClient::updateGeometryRestoresForFullscreen(AbstractOutput *output)
{
    QRect screenArea = workspace()->clientArea(MaximizeArea, this, output);
    QRect newFullScreenGeometryRestore = screenArea;
    if (!(maximizeMode() & MaximizeVertical)) {
        newFullScreenGeometryRestore.setHeight(geometryRestore().height());
    }
    if (!(maximizeMode() & MaximizeHorizontal)) {
        newFullScreenGeometryRestore.setWidth(geometryRestore().width());
    }
    newFullScreenGeometryRestore.setSize(newFullScreenGeometryRestore.size().boundedTo(screenArea.size()));
    QSize move = (screenArea.size() - newFullScreenGeometryRestore.size()) / 2;
    newFullScreenGeometryRestore.translate(move.width(), move.height());

    QRect newGeometryRestore = QRect(screenArea.topLeft(), geometryRestore().size().boundedTo(screenArea.size()));
    move = (screenArea.size() - newGeometryRestore.size()) / 2;
    newGeometryRestore.translate(move.width(), move.height());

    setFullscreenGeometryRestore(newFullScreenGeometryRestore);
    setGeometryRestore(newGeometryRestore);
}

void AbstractClient::checkWorkspacePosition(QRect oldGeometry, QRect oldClientGeometry, const VirtualDesktop *oldDesktop)
{
    if (isDock() || isDesktop() || !isPlaceable()) {
        return;
    }
    enum { Left = 0, Top, Right, Bottom };
    const int border[4] = { borderLeft(), borderTop(), borderRight(), borderBottom() };

    QRect newGeom = moveResizeGeometry();
    if( !oldGeometry.isValid())
        oldGeometry = newGeom;
    if (!oldClientGeometry.isValid())
        oldClientGeometry = oldGeometry.adjusted(border[Left], border[Top], -border[Right], -border[Bottom]);
    if (isFullScreen()) {
        moveResize(workspace()->clientArea(FullScreenArea, this, fullscreenGeometryRestore().center()));
        updateGeometryRestoresForFullscreen(kwinApp()->platform()->outputAt(newGeom.center()));
        return;
    }

    if (maximizeMode() != MaximizeRestore) {
        changeMaximize(false, false, true);   // adjust size
        QRect geom = moveResizeGeometry();
        const QRect screenArea = workspace()->clientArea(ScreenArea, this, geom.center());
        checkOffscreenPosition(&geom, screenArea);
        moveResize(geom);
        return;
    }

    if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        moveResize(electricBorderMaximizeGeometry(moveResizeGeometry().center()));
        return;
    }

    // this can be true only if this window was mapped before KWin
    // was started - in such case, don't adjust position to workarea,
    // because the window already had its position, and if a window
    // with a strut altering the workarea would be managed in initialization
    // after this one, this window would be moved
    if (!workspace() || workspace()->initializing())
        return;

    VirtualDesktop *desktop = !isOnCurrentDesktop() ? desktops().constLast() : VirtualDesktopManager::self()->currentDesktop();
    if (!oldDesktop) {
        oldDesktop = desktop;
    }

    // If the window was touching an edge before but not now move it so it is again.
    // Old and new maximums have different starting values so windows on the screen
    // edge will move when a new strut is placed on the edge.
    QRect oldScreenArea;
    QRect screenArea;
    if (workspace()->inUpdateClientArea()) {
        // check if the window is on an about to be destroyed output
        AbstractOutput *newOutput = output();
        if (!kwinApp()->platform()->enabledOutputs().contains(newOutput)) {
            newOutput = kwinApp()->platform()->outputAt(newGeom.center());
        }
        // we need to find the screen area as it was before the change
        oldScreenArea = workspace()->previousScreenSizes().value(output());
        if (oldScreenArea.isNull()) {
            oldScreenArea = newOutput->geometry();
        }
        screenArea = newOutput->geometry();
        newGeom.translate(screenArea.topLeft() - oldScreenArea.topLeft());
    } else {
        oldScreenArea = workspace()->clientArea(ScreenArea, kwinApp()->platform()->outputAt(oldGeometry.center()), oldDesktop);
        screenArea = workspace()->clientArea(ScreenArea, this, newGeom.center());
    }
    const QRect oldGeomTall = QRect(oldGeometry.x(), oldScreenArea.y(), oldGeometry.width(), oldScreenArea.height());   // Full screen height
    const QRect oldGeomWide = QRect(oldScreenArea.x(), oldGeometry.y(), oldScreenArea.width(), oldGeometry.height());   // Full screen width
    int oldTopMax = oldScreenArea.y();
    int oldRightMax = oldScreenArea.x() + oldScreenArea.width();
    int oldBottomMax = oldScreenArea.y() + oldScreenArea.height();
    int oldLeftMax = oldScreenArea.x();
    int topMax = screenArea.y();
    int rightMax = screenArea.x() + screenArea.width();
    int bottomMax = screenArea.y() + screenArea.height();
    int leftMax = screenArea.x();
    QRect newClientGeom = newGeom.adjusted(border[Left], border[Top], -border[Right], -border[Bottom]);
    const QRect newGeomTall = QRect(newGeom.x(), screenArea.y(), newGeom.width(), screenArea.height());   // Full screen height
    const QRect newGeomWide = QRect(screenArea.x(), newGeom.y(), screenArea.width(), newGeom.height());   // Full screen width
    // Get the max strut point for each side where the window is (E.g. Highest point for
    // the bottom struts bounded by the window's left and right sides).

    // These 4 compute old bounds ...
    auto moveAreaFunc = workspace()->inUpdateClientArea() ?
                                &Workspace::previousRestrictedMoveArea : //... the restricted areas changed
                                &Workspace::restrictedMoveArea; //... when e.g. active desktop or screen changes

    for (const QRect &r : (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaTop)) {
        QRect rect = r & oldGeomTall;
        if (!rect.isEmpty())
            oldTopMax = qMax(oldTopMax, rect.y() + rect.height());
    }
    for (const QRect &r : (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaRight)) {
        QRect rect = r & oldGeomWide;
        if (!rect.isEmpty())
            oldRightMax = qMin(oldRightMax, rect.x());
    }
    for (const QRect &r : (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaBottom)) {
        QRect rect = r & oldGeomTall;
        if (!rect.isEmpty())
            oldBottomMax = qMin(oldBottomMax, rect.y());
    }
    for (const QRect &r : (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaLeft)) {
        QRect rect = r & oldGeomWide;
        if (!rect.isEmpty())
            oldLeftMax = qMax(oldLeftMax, rect.x() + rect.width());
    }

    // These 4 compute new bounds
    for (const QRect &r : workspace()->restrictedMoveArea(desktop, StrutAreaTop)) {
        QRect rect = r & newGeomTall;
        if (!rect.isEmpty())
            topMax = qMax(topMax, rect.y() + rect.height());
    }
    for (const QRect &r : workspace()->restrictedMoveArea(desktop, StrutAreaRight)) {
        QRect rect = r & newGeomWide;
        if (!rect.isEmpty())
            rightMax = qMin(rightMax, rect.x());
    }
    for (const QRect &r : workspace()->restrictedMoveArea(desktop, StrutAreaBottom)) {
        QRect rect = r & newGeomTall;
        if (!rect.isEmpty())
            bottomMax = qMin(bottomMax, rect.y());
    }
    for (const QRect &r : workspace()->restrictedMoveArea(desktop, StrutAreaLeft)) {
        QRect rect = r & newGeomWide;
        if (!rect.isEmpty())
            leftMax = qMax(leftMax, rect.x() + rect.width());
    }


    // Check if the sides were inside or touching but are no longer
    bool keep[4] = {false, false, false, false};
    bool save[4] = {false, false, false, false};
    int padding[4] = {0, 0, 0, 0};
    if (oldGeometry.x() >= oldLeftMax)
        save[Left] = newGeom.x() < leftMax;
    if (oldGeometry.x() == oldLeftMax)
        keep[Left] = newGeom.x() != leftMax;
    else if (oldClientGeometry.x() == oldLeftMax && newClientGeom.x() != leftMax) {
        padding[0] = border[Left];
        keep[Left] = true;
    }
    if (oldGeometry.y() >= oldTopMax)
        save[Top] = newGeom.y() < topMax;
    if (oldGeometry.y() == oldTopMax)
        keep[Top] = newGeom.y() != topMax;
    else if (oldClientGeometry.y() == oldTopMax && newClientGeom.y() != topMax) {
        padding[1] = border[Left];
        keep[Top] = true;
    }
    if (oldGeometry.right() <= oldRightMax - 1)
        save[Right] = newGeom.right() > rightMax - 1;
    if (oldGeometry.right() == oldRightMax - 1)
        keep[Right] = newGeom.right() != rightMax - 1;
    else if (oldClientGeometry.right() == oldRightMax - 1 && newClientGeom.right() != rightMax - 1) {
        padding[2] = border[Right];
        keep[Right] = true;
    }
    if (oldGeometry.bottom() <= oldBottomMax - 1)
        save[Bottom] = newGeom.bottom() > bottomMax - 1;
    if (oldGeometry.bottom() == oldBottomMax - 1)
        keep[Bottom] = newGeom.bottom() != bottomMax - 1;
    else if (oldClientGeometry.bottom() == oldBottomMax - 1 && newClientGeom.bottom() != bottomMax - 1) {
        padding[3] = border[Bottom];
        keep[Bottom] = true;
    }

    // if randomly touches opposing edges, do not favor either
    if (keep[Left] && keep[Right]) {
        keep[Left] = keep[Right] = false;
        padding[0] = padding[2] = 0;
    }
    if (keep[Top] && keep[Bottom]) {
        keep[Top] = keep[Bottom] = false;
        padding[1] = padding[3] = 0;
    }

    if (save[Left] || keep[Left])
        newGeom.moveLeft(qMax(leftMax, screenArea.x()) - padding[0]);
    if (padding[0] && screens()->intersecting(newGeom) > 1)
        newGeom.moveLeft(newGeom.left() + padding[0]);
    if (save[Top] || keep[Top])
        newGeom.moveTop(qMax(topMax, screenArea.y()) - padding[1]);
    if (padding[1] && screens()->intersecting(newGeom) > 1)
        newGeom.moveTop(newGeom.top() + padding[1]);
    if (save[Right] || keep[Right])
        newGeom.moveRight(qMin(rightMax - 1, screenArea.right()) + padding[2]);
    if (padding[2] && screens()->intersecting(newGeom) > 1)
        newGeom.moveRight(newGeom.right() - padding[2]);
    if (oldGeometry.x() >= oldLeftMax && newGeom.x() < leftMax)
        newGeom.setLeft(qMax(leftMax, screenArea.x()));
    else if (oldClientGeometry.x() >= oldLeftMax && newGeom.x() + border[Left] < leftMax) {
        newGeom.setLeft(qMax(leftMax, screenArea.x()) - border[Left]);
        if (screens()->intersecting(newGeom) > 1)
            newGeom.setLeft(newGeom.left() + border[Left]);
    }
    if (save[Bottom] || keep[Bottom])
        newGeom.moveBottom(qMin(bottomMax - 1, screenArea.bottom()) + padding[3]);
    if (padding[3] && screens()->intersecting(newGeom) > 1)
        newGeom.moveBottom(newGeom.bottom() - padding[3]);
    if (oldGeometry.y() >= oldTopMax && newGeom.y() < topMax)
        newGeom.setTop(qMax(topMax, screenArea.y()));
    else if (oldClientGeometry.y() >= oldTopMax && newGeom.y() + border[Top] < topMax) {
        newGeom.setTop(qMax(topMax, screenArea.y()) - border[Top]);
        if (screens()->intersecting(newGeom) > 1)
            newGeom.setTop(newGeom.top() + border[Top]);
    }

    checkOffscreenPosition(&newGeom, screenArea);
    // Obey size hints. TODO: We really should make sure it stays in the right place
    if (!isShade())
        newGeom.setSize(constrainFrameSize(newGeom.size()));

    moveResize(newGeom);
}

void AbstractClient::checkOffscreenPosition(QRect* geom, const QRect& screenArea)
{
    if (geom->left() > screenArea.right()) {
        geom->moveLeft(screenArea.right() - screenArea.width()/4);
    } else if (geom->right() < screenArea.left()) {
        geom->moveRight(screenArea.left() + screenArea.width()/4);
    }
    if (geom->top() > screenArea.bottom()) {
        geom->moveTop(screenArea.bottom() - screenArea.height()/4);
    } else if (geom->bottom() < screenArea.top()) {
        geom->moveBottom(screenArea.top() + screenArea.width()/4);
    }
}

/**
 * Returns the natural size of the window, if the window is not shaded it's the same
 * as size().
 */
QSize AbstractClient::implicitSize() const
{
    return clientSizeToFrameSize(clientSize());
}

/**
 * Constrains the client size @p size according to a set of the window's size hints.
 *
 * Default implementation applies only minimum and maximum size constraints.
 */
QSize AbstractClient::constrainClientSize(const QSize &size, SizeMode mode) const
{
    Q_UNUSED(mode)

    int width = size.width();
    int height = size.height();

    // When user is resizing the window, the move resize geometry may have negative width or
    // height. In which case, we need to set negative dimensions to reasonable values.
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    const QSize minimumSize = minSize();
    const QSize maximumSize = maxSize();

    width = qBound(minimumSize.width(), width, maximumSize.width());
    height = qBound(minimumSize.height(), height, maximumSize.height());

    return QSize(width, height);
}

/**
 * Constrains the frame size @p size according to a set of the window's size hints.
 */
QSize AbstractClient::constrainFrameSize(const QSize &size, SizeMode mode) const
{
    const QSize unconstrainedClientSize = frameSizeToClientSize(size);
    const QSize constrainedClientSize = constrainClientSize(unconstrainedClientSize, mode);
    return clientSizeToFrameSize(constrainedClientSize);
}

/**
 * Returns @c true if the AbstractClient can be shown in full screen mode; otherwise @c false.
 *
 * Default implementation returns @c false.
 */
bool AbstractClient::isFullScreenable() const
{
    return false;
}

/**
 * Returns @c true if the AbstractClient is currently being shown in full screen mode; otherwise @c false.
 *
 * A client in full screen mode occupies the entire screen with no window frame around it.
 *
 * Default implementation returns @c false.
 */
bool AbstractClient::isFullScreen() const
{
    return false;
}

bool AbstractClient::isRequestedFullScreen() const
{
    return isFullScreen();
}

/**
 * Returns whether requests initiated by the user to enter or leave full screen mode are honored.
 *
 * Default implementation returns @c false.
 */
bool AbstractClient::userCanSetFullScreen() const
{
    return false;
}

/**
 * Asks the AbstractClient to enter or leave full screen mode.
 *
 * Default implementation does nothing.
 *
 * @param set @c true if the AbstractClient has to be shown in full screen mode, otherwise @c false
 * @param user @c true if the request is initiated by the user, otherwise @c false
 */
void AbstractClient::setFullScreen(bool set, bool user)
{
    Q_UNUSED(set)
    Q_UNUSED(user)
    qCWarning(KWIN_CORE, "%s doesn't support setting fullscreen state", metaObject()->className());
}

/**
 * Returns @c true if the AbstractClient can be minimized; otherwise @c false.
 *
 * Default implementation returns @c false.
 */
bool AbstractClient::isMinimizable() const
{
    return false;
}

/**
 * Returns @c true if the AbstractClient can be maximized; otherwise @c false.
 *
 * Default implementation returns @c false.
 */
bool AbstractClient::isMaximizable() const
{
    return false;
}

/**
 * Returns the currently applied maximize mode.
 *
 * Default implementation returns MaximizeRestore.
 */
MaximizeMode AbstractClient::maximizeMode() const
{
    return MaximizeRestore;
}

/**
 * Returns the last requested maximize mode.
 *
 * On X11, this method always matches maximizeMode(). On Wayland, it is asynchronous.
 *
 * Default implementation matches maximizeMode().
 */
MaximizeMode AbstractClient::requestedMaximizeMode() const
{
    return maximizeMode();
}

/**
 * Returns the geometry of the AbstractClient before it was maximized or quick tiled.
 */
QRect AbstractClient::geometryRestore() const
{
    return m_maximizeGeometryRestore;
}

/**
 * Sets the geometry of the AbstractClient before it was maximized or quick tiled to @p rect.
 */
void AbstractClient::setGeometryRestore(const QRect &rect)
{
    m_maximizeGeometryRestore = rect;
}

/**
 * Toggles the maximized state along specified dimensions @p horizontal and @p vertical.
 *
 * If @p adjust is @c true, only frame geometry will be updated to match requestedMaximizeMode().
 *
 * Default implementation does nothing.
 */
void AbstractClient::changeMaximize(bool horizontal, bool vertical, bool adjust)
{
    Q_UNUSED(horizontal)
    Q_UNUSED(vertical)
    Q_UNUSED(adjust)
    qCWarning(KWIN_CORE, "%s doesn't support setting maximized state", metaObject()->className());
}

void AbstractClient::invalidateDecoration()
{
}

bool AbstractClient::noBorder() const
{
    return true;
}

bool AbstractClient::userCanSetNoBorder() const
{
    return false;
}

void AbstractClient::setNoBorder(bool set)
{
    Q_UNUSED(set)
    qCWarning(KWIN_CORE, "%s doesn't support setting decorations", metaObject()->className());
}

void AbstractClient::showOnScreenEdge()
{
    qCWarning(KWIN_CORE, "%s doesn't support screen edge activation", metaObject()->className());
}

bool AbstractClient::isPlaceable() const
{
    return true;
}

QRect AbstractClient::fullscreenGeometryRestore() const
{
    return m_fullscreenGeometryRestore;
}

void AbstractClient::setFullscreenGeometryRestore(const QRect &geom)
{
    m_fullscreenGeometryRestore = geom;
}

void AbstractClient::cleanTabBox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    if (tabBox && tabBox->isDisplayed() && tabBox->currentClient() == this) {
        tabBox->nextPrev(true);
    }
#endif
}

bool AbstractClient::wantsShadowToBeRendered() const
{
    return !isFullScreen() && maximizeMode() != MaximizeFull;
}

}
