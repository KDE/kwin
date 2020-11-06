/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstract_client.h"

#include "appmenu.h"
#include "decorations/decoratedclient.h"
#include "decorations/decorationpalette.h"
#include "decorations/decorationbridge.h"
#include "cursor.h"
#include "effects.h"
#include "focuschain.h"
#include "outline.h"
#include "screens.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "screenedge.h"
#include "useractions.h"
#include "workspace.h"

#include "wayland_server.h"
#include <KWaylandServer/plasmawindowmanagement_interface.h>

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
    connect(this, &AbstractClient::clientStartUserMovedResized,  this, &AbstractClient::removeCheckScreenConnection);
    connect(this, &AbstractClient::clientFinishUserMovedResized, this, &AbstractClient::setupCheckScreenConnection);

    connect(this, &AbstractClient::paletteChanged, this, &AbstractClient::triggerDecorationRepaint);

    connect(Decoration::DecorationBridge::self(), &QObject::destroyed, this, &AbstractClient::destroyDecoration);

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
            if (isOnScreenDisplay() && !frameGeometry().isEmpty() && old.size() != frameGeometry().size() && !isInitialPositionSet()) {
                GeometryUpdatesBlocker blocker(this);
                const QRect area = workspace()->clientArea(PlacementArea, Screens::self()->current(), desktop());
                Placement::self()->place(this, area);
                setGeometryRestore(frameGeometry());
            }
        }
    );

    connect(this, &AbstractClient::paddingChanged, this, [this]() {
        m_visibleRectBeforeGeometryUpdate = visibleRect();
    });

    connect(ApplicationMenu::self(), &ApplicationMenu::applicationMenuEnabledChanged, this, [this] {
        emit hasApplicationMenuChanged(hasApplicationMenu());
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
    emit skipSwitcherChanged();
}

void AbstractClient::setSkipPager(bool b)
{
    b = rules()->checkSkipPager(b);
    if (b == skipPager())
        return;
    m_skipPager = b;
    doSetSkipPager();
    updateWindowRules(Rules::SkipPager);
    emit skipPagerChanged();
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
    emit skipTaskbarChanged();
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
    emit iconChanged();
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
    workspace()->updateClientLayer(this);   // active windows may get different layer
    auto mainclients = mainClients();
    for (auto it = mainclients.constBegin();
            it != mainclients.constEnd();
            ++it)
        if ((*it)->isFullScreen())  // fullscreens go high even if their transient is active
            workspace()->updateClientLayer(*it);

    doSetActive();
    emit activeChanged();
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
    addWorkspaceRepaint(visibleRect());
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

void AbstractClient::placeIn(const QRect &area)
{
    // TODO: Get rid of this method eventually. We need to call setGeometryRestore() because
    // checkWorkspacePosition() operates on geometryRestore() and because of quick tiling.
    Placement::self()->place(this, area);
    setGeometryRestore(frameGeometry());
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
    workspace()->updateClientLayer(this);
    updateWindowRules(Rules::Above);

    emit keepAboveChanged(m_keepAbove);
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
    workspace()->updateClientLayer(this);
    updateWindowRules(Rules::Below);

    emit keepBelowChanged(m_keepBelow);
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
    return workspace()->topClientOnDesktop(VirtualDesktopManager::self()->current(), -1, true, false) == this;
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
    emit demandsAttentionChanged();
}

void AbstractClient::doSetDemandsAttention()
{
}

void AbstractClient::setDesktop(int desktop)
{
    const int numberOfDesktops = VirtualDesktopManager::self()->count();
    if (desktop != NET::OnAllDesktops)   // Do range check
        desktop = qMax(1, qMin(numberOfDesktops, desktop));
    desktop = qMin(numberOfDesktops, rules()->checkDesktop(desktop));

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
            for (auto desktop: m_desktops) {
                if (!currentDesktops.contains(desktop->id())) {
                    windowManagementInterface()->addPlasmaVirtualDesktop(desktop->id());
                } else {
                    currentDesktops.removeOne(desktop->id());
                }
            }
            for (auto desktopId: currentDesktops) {
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
        foreach (AbstractClient * c2, mainClients())
        c2->setDesktops(desktops);
    }

    doSetDesktop();

    FocusChain::self()->update(this, FocusChain::MakeFirst);
    updateWindowRules(Rules::Desktop);

    emit desktopChanged();
    if (wasOnCurrentDesktop != isOnCurrentDesktop())
        emit desktopPresenceChanged(this, was_desk);
    emit x11DesktopIdsChanged();
}

void AbstractClient::doSetDesktop()
{
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
    if ((b && isOnAllDesktops()) ||
            (!b && !isOnAllDesktops()))
        return;
    if (b)
        setDesktop(NET::OnAllDesktops);
    else
        setDesktop(VirtualDesktopManager::self()->current());
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
    if (mode == ShadeHover && isMove())
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
        emit shadeChanged();
        return; // No real change in shaded state
    }

    Q_ASSERT(isDecorated());
    GeometryUpdatesBlocker blocker(this);

    doSetShade(previousShadeMode);

    discardWindowPixmap();
    updateWindowRules(Rules::Shade);

    emit shadeChanged();
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
    if (m_shadeMode == ShadeHover && !isMoveResize() && !isMoveResizePointerButtonDown()) {
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

AbstractClient::Position AbstractClient::titlebarPosition() const
{
    // TODO: still needed, remove?
    return PositionTop;
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
    case AbstractClient::PositionTop:
        return (sectionUnderMouse == Qt::TopLeftSection || sectionUnderMouse == Qt::TopSection || sectionUnderMouse == Qt::TopRightSection);
    case AbstractClient::PositionLeft:
        return (sectionUnderMouse == Qt::TopLeftSection || sectionUnderMouse == Qt::LeftSection || sectionUnderMouse == Qt::BottomLeftSection);
    case AbstractClient::PositionRight:
        return (sectionUnderMouse == Qt::BottomRightSection || sectionUnderMouse == Qt::RightSection || sectionUnderMouse == Qt::TopRightSection);
    case AbstractClient::PositionBottom:
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
    // TODO: merge signal with s_minimized
    addWorkspaceRepaint(visibleRect());
    emit clientMinimized(this, !avoid_animation);
    emit minimizedChanged();
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
    emit clientUnminimized(this, !avoid_animation);
    emit minimizedChanged();
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

        emit paletteChanged(palette());
        emit colorSchemeChanged();
    }
}

void AbstractClient::updateColorScheme()
{
    setColorScheme(preferredColorScheme());
}

void AbstractClient::handlePaletteChange()
{
    emit paletteChanged(palette());
}

void AbstractClient::keepInArea(QRect area, bool partial)
{
    if (partial) {
        // increase the area so that can have only 100 pixels in the area
        area.setLeft(qMin(area.left() - width() + 100, area.left()));
        area.setTop(qMin(area.top() - height() + 100, area.top()));
        area.setRight(qMax(area.right() + width() - 100, area.right()));
        area.setBottom(qMax(area.bottom() + height() - 100, area.bottom()));
    }
    if (!partial) {
        // resize to fit into area
        if (area.width() < width() || area.height() < height())
            resizeWithChecks(size().boundedTo(area.size()));
    }
    int tx = x(), ty = y();
    if (frameGeometry().right() > area.right() && width() <= area.width())
        tx = area.right() - width() + 1;
    if (frameGeometry().bottom() > area.bottom() && height() <= area.height())
        ty = area.bottom() - height() + 1;
    if (!area.contains(frameGeometry().topLeft())) {
        if (tx < area.x())
            tx = area.x();
        if (ty < area.y())
            ty = area.y();
    }
    if (tx != x() || ty != y())
        move(tx, ty);
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
        if (m_blockGeometryUpdates == 0)
            m_pendingGeometryUpdate = PendingGeometryNone;
        ++m_blockGeometryUpdates;
    } else {
        if (--m_blockGeometryUpdates == 0) {
            if (m_pendingGeometryUpdate != PendingGeometryNone) {
                if (isShade())
                    setFrameGeometry(QRect(pos(), adjustedSize()), NormalGeometrySet);
                else
                    setFrameGeometry(frameGeometry(), NormalGeometrySet);
                m_pendingGeometryUpdate = PendingGeometryNone;
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
        emit clientMaximizedStateChanged(this, newMode);
        emit clientMaximizedStateChanged(this, vertically, horizontally);
    }
}

void AbstractClient::move(int x, int y, ForceGeometry_t force)
{
    // resuming geometry updates is handled only in setGeometry()
    Q_ASSERT(pendingGeometryUpdate() == PendingGeometryNone || areGeometryUpdatesBlocked());
    QPoint p(x, y);
    if (!areGeometryUpdatesBlocked() && p != rules()->checkPosition(p)) {
        qCDebug(KWIN_CORE) << "forced position fail:" << p << ":" << rules()->checkPosition(p);
    }
    if (force == NormalGeometrySet && m_frameGeometry.topLeft() == p)
        return;
    m_frameGeometry.moveTopLeft(p);
    if (areGeometryUpdatesBlocked()) {
        if (pendingGeometryUpdate() == PendingGeometryForced)
            {} // maximum, nothing needed
        else if (force == ForceGeometrySet)
            setPendingGeometryUpdate(PendingGeometryForced);
        else
            setPendingGeometryUpdate(PendingGeometryNormal);
        return;
    }
    const QRect oldBufferGeometry = bufferGeometryBeforeUpdateBlocking();
    const QRect oldClientGeometry = clientGeometryBeforeUpdateBlocking();
    const QRect oldFrameGeometry = frameGeometryBeforeUpdateBlocking();
    doMove(x, y);
    updateGeometryBeforeUpdateBlocking();
    updateWindowRules(Rules::Position);
    screens()->setCurrent(this);
    workspace()->updateStackingOrder();
    // client itself is not damaged
    emit bufferGeometryChanged(this, oldBufferGeometry);
    emit clientGeometryChanged(this, oldClientGeometry);
    emit frameGeometryChanged(this, oldFrameGeometry);
    addRepaintDuringGeometryUpdates();
}

bool AbstractClient::startMoveResize()
{
    Q_ASSERT(!isMoveResize());
    Q_ASSERT(QWidget::keyboardGrabber() == nullptr);
    Q_ASSERT(QWidget::mouseGrabber() == nullptr);
    stopDelayedMoveResize();
    if (QApplication::activePopupWidget() != nullptr)
        return false; // popups have grab
    if (isFullScreen() && (screens()->count() < 2 || !isMovableAcrossScreens()))
        return false;
    if (!doStartMoveResize()) {
        return false;
    }

    invalidateDecorationDoubleClickTimer();

    setMoveResize(true);
    workspace()->setMoveResizeClient(this);

    const Position mode = moveResizePointerMode();
    if (mode != PositionCenter) { // means "isResize()" but moveResizeMode = true is set below
        if (maximizeMode() == MaximizeFull) { // partial is cond. reset in finishMoveResize
            setGeometryRestore(frameGeometry()); // "restore" to current geometry
            setMaximize(false, false);
        }
    }

    if (quickTileMode() != QuickTileMode(QuickTileFlag::None) && mode != PositionCenter) { // Cannot use isResize() yet
        // Exit quick tile mode when the user attempts to resize a tiled window
        updateQuickTileMode(QuickTileFlag::None); // Do so without restoring original geometry
        setGeometryRestore(frameGeometry());
        doSetQuickTileMode();
        emit quickTileModeChanged();
    }

    updateHaveResizeEffect();
    updateInitialMoveResizeGeometry();
    checkUnrestrictedMoveResize();
    emit clientStartUserMovedResized(this);
    if (ScreenEdges::self()->isDesktopSwitchingMovingClients())
        ScreenEdges::self()->reserveDesktopSwitching(true, Qt::Vertical|Qt::Horizontal);
    return true;
}

void AbstractClient::finishMoveResize(bool cancel)
{
    GeometryUpdatesBlocker blocker(this);
    const bool wasResize = isResize(); // store across leaveMoveResize
    leaveMoveResize();

    doFinishMoveResize();

    if (cancel)
        setFrameGeometry(initialMoveResizeGeometry());
    else {
        const QRect &moveResizeGeom = moveResizeGeometry();
        if (wasResize) {
            const bool restoreH = maximizeMode() == MaximizeHorizontal &&
                                    moveResizeGeom.width() != initialMoveResizeGeometry().width();
            const bool restoreV = maximizeMode() == MaximizeVertical &&
                                    moveResizeGeom.height() != initialMoveResizeGeometry().height();
            if (restoreH || restoreV) {
                changeMaximize(restoreH, restoreV, false);
            }
        }
        setFrameGeometry(moveResizeGeom);
    }
    checkScreen(); // needs to be done because clientFinishUserMovedResized has not yet re-activated online alignment
    if (screen() != moveResizeStartScreen()) {
        workspace()->sendClientToScreen(this, screen()); // checks rule validity
        if (maximizeMode() != MaximizeRestore)
            checkWorkspacePosition();
    }

    if (isElectricBorderMaximizing()) {
        setQuickTileMode(electricBorderMode());
        setElectricBorderMaximizing(false);
    } else if (!cancel) {
        QRect geom_restore = geometryRestore();
        if (!(maximizeMode() & MaximizeHorizontal)) {
            geom_restore.setX(frameGeometry().x());
            geom_restore.setWidth(frameGeometry().width());
        }
        if (!(maximizeMode() & MaximizeVertical)) {
            geom_restore.setY(frameGeometry().y());
            geom_restore.setHeight(frameGeometry().height());
        }
        setGeometryRestore(geom_restore);
    }
// FRAME    update();

    emit clientFinishUserMovedResized(this);
}

// This function checks if it actually makes sense to perform a restricted move/resize.
// If e.g. the titlebar is already outside of the workarea, there's no point in performing
// a restricted move resize, because then e.g. resize would also move the window (#74555).
// NOTE: Most of it is duplicated from handleMoveResize().
void AbstractClient::checkUnrestrictedMoveResize()
{
    if (isUnrestrictedMoveResize())
        return;
    const QRect &moveResizeGeom = moveResizeGeometry();
    QRect desktopArea = workspace()->clientArea(WorkArea, moveResizeGeom.center(), desktop());
    int left_marge, right_marge, top_marge, bottom_marge, titlebar_marge;
    // restricted move/resize - keep at least part of the titlebar always visible
    // how much must remain visible when moved away in that direction
    left_marge = qMin(100 + borderRight(), moveResizeGeom.width());
    right_marge = qMin(100 + borderLeft(), moveResizeGeom.width());
    // width/height change with opaque resizing, use the initial ones
    titlebar_marge = initialMoveResizeGeometry().height();
    top_marge = borderBottom();
    bottom_marge = borderTop();
    if (isResize()) {
        if (moveResizeGeom.bottom() < desktopArea.top() + top_marge)
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.top() > desktopArea.bottom() - bottom_marge)
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.right() < desktopArea.left() + left_marge)
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.left() > desktopArea.right() - right_marge)
            setUnrestrictedMoveResize(true);
        if (!isUnrestrictedMoveResize() && moveResizeGeom.top() < desktopArea.top())   // titlebar mustn't go out
            setUnrestrictedMoveResize(true);
    }
    if (isMove()) {
        if (moveResizeGeom.bottom() < desktopArea.top() + titlebar_marge - 1)
            setUnrestrictedMoveResize(true);
        // no need to check top_marge, titlebar_marge already handles it
        if (moveResizeGeom.top() > desktopArea.bottom() - bottom_marge + 1) // titlebar mustn't go out
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.right() < desktopArea.left() + left_marge)
            setUnrestrictedMoveResize(true);
        if (moveResizeGeom.left() > desktopArea.right() - right_marge)
            setUnrestrictedMoveResize(true);
    }
}

// When the user pressed mouse on the titlebar, don't activate move immediately,
// since it may be just a click. Activate instead after a delay. Move used to be
// activated only after moving by several pixels, but that looks bad.
void AbstractClient::startDelayedMoveResize()
{
    Q_ASSERT(!m_moveResize.delayedTimer);
    m_moveResize.delayedTimer = new QTimer(this);
    m_moveResize.delayedTimer->setSingleShot(true);
    connect(m_moveResize.delayedTimer, &QTimer::timeout, this,
        [this]() {
            Q_ASSERT(isMoveResizePointerButtonDown());
            if (!startMoveResize()) {
                setMoveResizePointerButtonDown(false);
            }
            updateCursor();
            stopDelayedMoveResize();
        }
    );
    m_moveResize.delayedTimer->start(QApplication::startDragTime());
}

void AbstractClient::stopDelayedMoveResize()
{
    delete m_moveResize.delayedTimer;
    m_moveResize.delayedTimer = nullptr;
}

void AbstractClient::updateMoveResize(const QPointF &currentGlobalCursor)
{
    handleMoveResize(pos(), currentGlobalCursor.toPoint());
}

void AbstractClient::handleMoveResize(const QPoint &local, const QPoint &global)
{
    const QRect oldGeo = frameGeometry();
    handleMoveResize(local.x(), local.y(), global.x(), global.y());
    if (!isFullScreen() && isMove()) {
        if (quickTileMode() != QuickTileMode(QuickTileFlag::None) && oldGeo != frameGeometry()) {
            GeometryUpdatesBlocker blocker(this);
            setQuickTileMode(QuickTileFlag::None);
            const QRect &geom_restore = geometryRestore();
            setMoveOffset(QPoint(double(moveOffset().x()) / double(oldGeo.width()) * double(geom_restore.width()),
                                 double(moveOffset().y()) / double(oldGeo.height()) * double(geom_restore.height())));
            if (rules()->checkMaximize(MaximizeRestore) == MaximizeRestore)
                setMoveResizeGeometry(geom_restore);
            handleMoveResize(local.x(), local.y(), global.x(), global.y()); // fix position
        } else if (quickTileMode() == QuickTileMode(QuickTileFlag::None) && isResizable()) {
            checkQuickTilingMaximizationZones(global.x(), global.y());
        }
    }
}

void AbstractClient::handleMoveResize(int x, int y, int x_root, int y_root)
{
    if (isWaitingForMoveResizeSync())
        return; // we're still waiting for the client or the timeout

    const Position mode = moveResizePointerMode();
    if ((mode == PositionCenter && !isMovableAcrossScreens())
            || (mode != PositionCenter && (isShade() || !isResizable())))
        return;

    if (!isMoveResize()) {
        QPoint p(QPoint(x/* - padding_left*/, y/* - padding_top*/) - moveOffset());
        if (p.manhattanLength() >= QApplication::startDragDistance()) {
            if (!startMoveResize()) {
                setMoveResizePointerButtonDown(false);
                updateCursor();
                return;
            }
            updateCursor();
        } else
            return;
    }

    // ShadeHover or ShadeActive, ShadeNormal was already avoided above
    if (mode != PositionCenter && shadeMode() != ShadeNone)
        setShade(ShadeNone);

    QPoint globalPos(x_root, y_root);
    // these two points limit the geometry rectangle, i.e. if bottomleft resizing is done,
    // the bottomleft corner should be at is at (topleft.x(), bottomright().y())
    QPoint topleft = globalPos - moveOffset();
    QPoint bottomright = globalPos + invertedMoveOffset();
    QRect previousMoveResizeGeom = moveResizeGeometry();

    // TODO move whole group when moving its leader or when the leader is not mapped?

    auto titleBarRect = [this](bool &transposed, int &requiredPixels) -> QRect {
        const QRect &moveResizeGeom = moveResizeGeometry();
        QRect r(moveResizeGeom);
        r.moveTopLeft(QPoint(0,0));
        switch (titlebarPosition()) {
        default:
        case PositionTop:
            r.setHeight(borderTop());
            break;
        case PositionLeft:
            r.setWidth(borderLeft());
            transposed = true;
            break;
        case PositionBottom:
            r.setTop(r.bottom() - borderBottom());
            break;
        case PositionRight:
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
    if (isResize()) {
        QRect orig = initialMoveResizeGeometry();
        SizeMode sizeMode = SizeModeAny;
        auto calculateMoveResizeGeom = [this, &topleft, &bottomright, &orig, &sizeMode, &mode]() {
            switch(mode) {
            case PositionTopLeft:
                setMoveResizeGeometry(QRect(topleft, orig.bottomRight()));
                break;
            case PositionBottomRight:
                setMoveResizeGeometry(QRect(orig.topLeft(), bottomright));
                break;
            case PositionBottomLeft:
                setMoveResizeGeometry(QRect(QPoint(topleft.x(), orig.y()), QPoint(orig.right(), bottomright.y())));
                break;
            case PositionTopRight:
                setMoveResizeGeometry(QRect(QPoint(orig.x(), topleft.y()), QPoint(bottomright.x(), orig.bottom())));
                break;
            case PositionTop:
                setMoveResizeGeometry(QRect(QPoint(orig.left(), topleft.y()), orig.bottomRight()));
                sizeMode = SizeModeFixedH; // try not to affect height
                break;
            case PositionBottom:
                setMoveResizeGeometry(QRect(orig.topLeft(), QPoint(orig.right(), bottomright.y())));
                sizeMode = SizeModeFixedH;
                break;
            case PositionLeft:
                setMoveResizeGeometry(QRect(QPoint(topleft.x(), orig.top()), orig.bottomRight()));
                sizeMode = SizeModeFixedW;
                break;
            case PositionRight:
                setMoveResizeGeometry(QRect(orig.topLeft(), QPoint(bottomright.x(), orig.bottom())));
                sizeMode = SizeModeFixedW;
                break;
            case PositionCenter:
            default:
                abort();
                break;
            }
        };

        // first resize (without checking constrains), then snap, then check bounds, then check constrains
        calculateMoveResizeGeom();
        // adjust new size to snap to other windows/borders
        setMoveResizeGeometry(workspace()->adjustClientSize(this, moveResizeGeometry(), mode));

        if (!isUnrestrictedMoveResize()) {
            // Make sure the titlebar isn't behind a restricted area. We don't need to restrict
            // the other directions. If not visible enough, move the window to the closest valid
            // point. We bruteforce this by slowly moving the window back to its previous position
            QRegion availableArea(workspace()->clientArea(FullArea, -1, 0));   // On the screen
            availableArea -= workspace()->restrictedMoveArea(desktop());   // Strut areas
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
                case PositionTop:
                    fixChangedState(topChanged, btmChanged, leftChanged, rightChanged);
                    break;
                case PositionLeft:
                    fixChangedState(leftChanged, rightChanged, topChanged, btmChanged);
                    break;
                case PositionBottom:
                    fixChangedState(btmChanged, topChanged, leftChanged, rightChanged);
                    break;
                case PositionRight:
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
    } else if (isMove()) {
        Q_ASSERT(mode == PositionCenter);
        if (!isMovable()) { // isMovableAcrossScreens() must have been true to get here
            // Special moving of maximized windows on Xinerama screens
            int screen = screens()->number(globalPos);
            if (isFullScreen())
                setMoveResizeGeometry(workspace()->clientArea(FullScreenArea, screen, 0));
            else {
                QRect moveResizeGeom = workspace()->clientArea(MaximizeArea, screen, 0);
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
                                       isUnrestrictedMoveResize()));
            setMoveResizeGeometry(moveResizeGeom);

            if (!isUnrestrictedMoveResize()) {
                const QRegion strut = workspace()->restrictedMoveArea(desktop());   // Strut areas
                QRegion availableArea(workspace()->clientArea(FullArea, -1, 0));   // On the screen
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
    } else
        abort();

    if (!update)
        return;

    if (isResize() && !haveResizeEffect()) {
        doResizeSync();
    } else
        performMoveResize();

    if (isMove()) {
        ScreenEdges::self()->check(globalPos, QDateTime::fromMSecsSinceEpoch(xTime(), Qt::UTC));
    }
}

void AbstractClient::performMoveResize()
{
    const QRect &moveResizeGeom = moveResizeGeometry();
    if (isMove() || (isResize() && !haveResizeEffect())) {
        setFrameGeometry(moveResizeGeom);
    }
    doPerformMoveResize();
    positionGeometryTip();
    emit clientStepUserMovedResized(this, moveResizeGeom);
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
    auto w = waylandServer()->windowManagement()->createWindow(waylandServer()->windowManagement(), internalId());
    w->setTitle(caption());
    w->setVirtualDesktop(isOnAllDesktops() ? 0 : desktop() - 1);
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
    connect(w, &PlasmaWindowInterface::virtualDesktopRequested, this,
        [this] (quint32 desktop) {
            workspace()->sendClientToDesktop(this, desktop + 1, true);
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

    for (const auto vd : m_desktops) {
        w->addPlasmaVirtualDesktop(vd->id());
    }

    //this is only for the legacy
    connect(this, &AbstractClient::desktopChanged, w,
        [w, this] {
            if (isOnAllDesktops()) {
                w->setOnAllDesktops(true);
                return;
            }
            w->setVirtualDesktop(desktop() - 1);
            w->setOnAllDesktops(false);
        }
    );

    //Plasma Virtual desktop management
    //show/hide when the window enters/exits from desktop
    connect(w, &PlasmaWindowInterface::enterPlasmaVirtualDesktopRequested, this,
        [this] (const QString &desktopId) {
            VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForId(desktopId.toUtf8());
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
            VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForId(desktopId.toUtf8());
            if (vd) {
                leaveDesktop(vd);
            }
        }
    );

    m_windowManagementInterface = w;
}

void AbstractClient::destroyWindowManagementInterface()
{
    if (m_windowManagementInterface) {
        m_windowManagementInterface->unmap();
        m_windowManagementInterface = nullptr;
    }
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
            AbstractClient *next = workspace()->clientUnderMouse(screen());
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
        screens()->setCurrent(globalPos);
        replay = replay || mustReplay;
        break;
    }
    case Options::MouseActivateAndLower:
        workspace()->requestFocus(this);
        workspace()->lowerClient(this);
        screens()->setCurrent(globalPos);
        replay = replay || !rules()->checkAcceptFocus(acceptsFocus());
        break;
    case Options::MouseActivate:
        replay = isActive(); // for clickraise mode
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        screens()->setCurrent(globalPos);
        replay = replay || !rules()->checkAcceptFocus(acceptsFocus());
        break;
    case Options::MouseActivateRaiseAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus | Workspace::ActivityRaise);
        screens()->setCurrent(globalPos);
        replay = true;
        break;
    case Options::MouseActivateAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        screens()->setCurrent(globalPos);
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
        screens()->setCurrent(globalPos);
        // fallthrough
    case Options::MouseMove:
    case Options::MouseUnrestrictedMove: {
        if (!isMovableAcrossScreens())
            break;
        if (isMoveResize())
            finishMoveResize(false);
        setMoveResizePointerMode(PositionCenter);
        setMoveResizePointerButtonDown(true);
        setMoveOffset(QPoint(globalPos.x() - x(), globalPos.y() - y()));  // map from global
        setInvertedMoveOffset(rect().bottomRight() - moveOffset());
        setUnrestrictedMoveResize((cmd == Options::MouseActivateRaiseAndUnrestrictedMove
                                  || cmd == Options::MouseUnrestrictedMove));
        if (!startMoveResize())
            setMoveResizePointerButtonDown(false);
        updateCursor();
        break;
    }
    case Options::MouseResize:
    case Options::MouseUnrestrictedResize: {
        if (!isResizable() || isShade())
            break;
        if (isMoveResize())
            finishMoveResize(false);
        setMoveResizePointerButtonDown(true);
        const QPoint moveOffset = QPoint(globalPos.x() - x(), globalPos.y() - y());  // map from global
        setMoveOffset(moveOffset);
        int x = moveOffset.x(), y = moveOffset.y();
        bool left = x < width() / 3;
        bool right = x >= 2 * width() / 3;
        bool top = y < height() / 3;
        bool bot = y >= 2 * height() / 3;
        Position mode;
        if (top)
            mode = left ? PositionTopLeft : (right ? PositionTopRight : PositionTop);
        else if (bot)
            mode = left ? PositionBottomLeft : (right ? PositionBottomRight : PositionBottom);
        else
            mode = (x < width() / 2) ? PositionLeft : PositionRight;
        setMoveResizePointerMode(mode);
        setInvertedMoveOffset(rect().bottomRight() - moveOffset);
        setUnrestrictedMoveResize((cmd == Options::MouseUnrestrictedResize));
        if (!startMoveResize())
            setMoveResizePointerButtonDown(false);
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
    emit transientChanged();
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
    foreach (const auto *cl, result) {
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
    emit modalChanged();
    // Changing modality for a mapped window is weird (?)
    // _NET_WM_STATE_MODAL should possibly rather be _NET_WM_WINDOW_TYPE_MODAL_DIALOG
}

bool AbstractClient::isModal() const
{
    return m_modal;
}

void AbstractClient::addTransient(AbstractClient *cl)
{
    Q_ASSERT(!m_transients.contains(cl));
    Q_ASSERT(cl != this);
    m_transients.append(cl);
}

void AbstractClient::removeTransient(AbstractClient *cl)
{
    m_transients.removeAll(cl);
    if (cl->transientFor() == this) {
        cl->setTransientFor(nullptr);
    }
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
    return ac && (ac == this || ac->screen() != screen()|| ac->allMainClients().contains(const_cast<AbstractClient*>(this)));
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

void AbstractClient::addRepaintDuringGeometryUpdates()
{
    const QRect deco_rect = visibleRect();
    addLayerRepaint(m_visibleRectBeforeGeometryUpdate);
    addLayerRepaint(deco_rect);   // trigger repaint of window's new location
    m_visibleRectBeforeGeometryUpdate = deco_rect;
}

QRect AbstractClient::bufferGeometryBeforeUpdateBlocking() const
{
    return m_bufferGeometryBeforeUpdateBlocking;
}

QRect AbstractClient::frameGeometryBeforeUpdateBlocking() const
{
    return m_frameGeometryBeforeUpdateBlocking;
}

QRect AbstractClient::clientGeometryBeforeUpdateBlocking() const
{
    return m_clientGeometryBeforeUpdateBlocking;
}

void AbstractClient::updateGeometryBeforeUpdateBlocking()
{
    m_bufferGeometryBeforeUpdateBlocking = bufferGeometry();
    m_frameGeometryBeforeUpdateBlocking = frameGeometry();
    m_clientGeometryBeforeUpdateBlocking = clientGeometry();
}

void AbstractClient::doMove(int, int)
{
}

void AbstractClient::updateInitialMoveResizeGeometry()
{
    m_moveResize.initialGeometry = frameGeometry();
    m_moveResize.geometry = m_moveResize.initialGeometry;
    m_moveResize.startScreen = screen();
}

void AbstractClient::updateCursor()
{
    Position m = moveResizePointerMode();
    if (!isResizable() || isShade())
        m = PositionCenter;
    CursorShape c = Qt::ArrowCursor;
    switch(m) {
    case PositionTopLeft:
        c = KWin::ExtendedCursor::SizeNorthWest;
        break;
    case PositionBottomRight:
        c = KWin::ExtendedCursor::SizeSouthEast;
        break;
    case PositionBottomLeft:
        c = KWin::ExtendedCursor::SizeSouthWest;
        break;
    case PositionTopRight:
        c = KWin::ExtendedCursor::SizeNorthEast;
        break;
    case PositionTop:
        c = KWin::ExtendedCursor::SizeNorth;
        break;
    case PositionBottom:
        c = KWin::ExtendedCursor::SizeSouth;
        break;
    case PositionLeft:
        c = KWin::ExtendedCursor::SizeWest;
        break;
    case PositionRight:
        c = KWin::ExtendedCursor::SizeEast;
        break;
    default:
        if (isMoveResize())
            c = Qt::SizeAllCursor;
        else
            c = Qt::ArrowCursor;
        break;
    }
    if (c == m_moveResize.cursor)
        return;
    m_moveResize.cursor = c;
    emit moveResizeCursorChanged(c);
}

void AbstractClient::leaveMoveResize()
{
    workspace()->setMoveResizeClient(nullptr);
    setMoveResize(false);
    if (ScreenEdges::self()->isDesktopSwitchingMovingClients())
        ScreenEdges::self()->reserveDesktopSwitching(false, Qt::Vertical|Qt::Horizontal);
    if (isElectricBorderMaximizing()) {
        outline()->hide();
        elevate(false);
    }
}

bool AbstractClient::s_haveResizeEffect = false;

void AbstractClient::updateHaveResizeEffect()
{
    s_haveResizeEffect = effects && static_cast<EffectsHandlerImpl*>(effects)->provides(Effect::Resize);
}

bool AbstractClient::doStartMoveResize()
{
    return true;
}

void AbstractClient::doFinishMoveResize()
{
}

void AbstractClient::positionGeometryTip()
{
}

void AbstractClient::doPerformMoveResize()
{
}

bool AbstractClient::isWaitingForMoveResizeSync() const
{
    return false;
}

void AbstractClient::doResizeSync()
{
}

void AbstractClient::checkQuickTilingMaximizationZones(int xroot, int yroot)
{
    QuickTileMode mode = QuickTileFlag::None;
    bool innerBorder = false;
    for (int i=0; i < screens()->count(); ++i) {

        if (!screens()->geometry(i).contains(QPoint(xroot, yroot)))
            continue;

        auto isInScreen = [i](const QPoint &pt) {
            for (int j = 0; j < screens()->count(); ++j) {
                if (j == i)
                    continue;
                if (screens()->geometry(j).contains(pt)) {
                    return true;
                }
            }
            return false;
        };

        QRect area = workspace()->clientArea(MaximizeArea, QPoint(xroot, yroot), desktop());
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
                connect(m_electricMaximizingDelay, &QTimer::timeout, [this]() {
                    if (isMove())
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
    if (!isMove() && !isResize())
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
        setMoveResizePointerButtonDown(false);
        finishMoveResize(false);
        updateCursor();
        break;
    case Qt::Key_Escape:
        setMoveResizePointerButtonDown(false);
        finishMoveResize(true);
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

void AbstractClient::dontMoveResize()
{
    setMoveResizePointerButtonDown(false);
    stopDelayedMoveResize();
    if (isMoveResize())
        finishMoveResize(false);
}

AbstractClient::Position AbstractClient::mousePosition() const
{
    if (isDecorated()) {
        switch (decoration()->sectionUnderMouse()) {
            case Qt::BottomLeftSection:
                return PositionBottomLeft;
            case Qt::BottomRightSection:
                return PositionBottomRight;
            case Qt::BottomSection:
                return PositionBottom;
            case Qt::LeftSection:
                return PositionLeft;
            case Qt::RightSection:
                return PositionRight;
            case Qt::TopSection:
                return PositionTop;
            case Qt::TopLeftSection:
                return PositionTopLeft;
            case Qt::TopRightSection:
                return PositionTopRight;
            default:
                return PositionCenter;
        }
    }
    return PositionCenter;
}

void AbstractClient::endMoveResize()
{
    setMoveResizePointerButtonDown(false);
    stopDelayedMoveResize();
    if (isMoveResize()) {
        finishMoveResize(false);
        setMoveResizePointerMode(mousePosition());
    }
    updateCursor();
}

void AbstractClient::createDecoration(const QRect &oldGeometry)
{
    KDecoration2::Decoration *decoration = Decoration::DecorationBridge::self()->createDecoration(this);
    if (decoration) {
        QMetaObject::invokeMethod(decoration, "update", Qt::QueuedConnection);
        connect(decoration, &KDecoration2::Decoration::shadowChanged, this, &Toplevel::updateShadow);
        connect(decoration, &KDecoration2::Decoration::bordersChanged, this, [this]() {
            GeometryUpdatesBlocker blocker(this);
            const QRect oldGeometry = frameGeometry();
            if (!isShade()) {
                checkWorkspacePosition(oldGeometry);
            }
            emit geometryShapeChanged(this, oldGeometry);
        });
    }
    setDecoration(decoration);
    setFrameGeometry(QRect(oldGeometry.topLeft(), clientSizeToFrameSize(clientSize())));

    emit geometryShapeChanged(this, oldGeometry);
}

void AbstractClient::destroyDecoration()
{
    delete m_decoration.decoration;
    m_decoration.decoration = nullptr;
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
    if (isMoveResizePointerButtonDown()) {
        handleMoveResize(localPos.x(), localPos.y(), globalPos.x(), globalPos.y());
        return;
    }
    // TODO: handle modifiers
    Position newmode = mousePosition();
    if (newmode != moveResizePointerMode()) {
        setMoveResizePointerMode(newmode);
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
                dontMoveResize();
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
        setMoveResizePointerMode(mousePosition());
        setMoveResizePointerButtonDown(true);
        setMoveOffset(event->pos());
        setInvertedMoveOffset(rect().bottomRight() - moveOffset());
        setUnrestrictedMoveResize(false);
        startDelayedMoveResize();
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
        setMoveResizePointerButtonDown(false);
        stopDelayedMoveResize();
        if (isMoveResize()) {
            finishMoveResize(false);
            setMoveResizePointerMode(mousePosition());
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

void AbstractClient::enterEvent(const QPoint &globalPos)
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
            workspace()->topClientOnDesktop(VirtualDesktopManager::self()->current(),
                                            options->isSeparateScreenFocus() ? screen() : -1) != this) {
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

void AbstractClient::leaveEvent()
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

    for (auto i = windowManagementInterface()->minimizedGeometries().constBegin(), end = windowManagementInterface()->minimizedGeometries().constEnd(); i != end; ++i) {
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

QRect AbstractClient::virtualKeyboardGeometry() const
{
    return m_virtualKeyboardGeometry;
}

void AbstractClient::setVirtualKeyboardGeometry(const QRect &geo)
{
    // No keyboard anymore
    if (geo.isEmpty() && !m_keyboardGeometryRestore.isEmpty()) {
        setFrameGeometry(m_keyboardGeometryRestore);
        m_keyboardGeometryRestore = QRect();
    } else if (geo.isEmpty()) {
        return;
    // The keyboard has just been opened (rather than resized) save client geometry for a restore
    } else if (m_keyboardGeometryRestore.isEmpty()) {
        m_keyboardGeometryRestore = frameGeometry();
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
    QRect newWindowGeometry = m_keyboardGeometryRestore;
    newWindowGeometry.moveBottom(geo.top());
    newWindowGeometry.setTop(qMax(newWindowGeometry.top(), availableArea.top()));

    setFrameGeometry(newWindowGeometry);
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
    emit desktopFileNameChanged();
}

QString AbstractClient::iconFromDesktopFile() const
{
    if (m_desktopFileName.isEmpty()) {
        return {};
    }

    const QString desktopFileName = QString::fromUtf8(m_desktopFileName);
    QString desktopFilePath;

    if (QDir::isAbsolutePath(desktopFileName)) {
        desktopFilePath = desktopFileName;
    }

    if (desktopFilePath.isEmpty()) {
        desktopFilePath = QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                                 desktopFileName);
    }
    if (desktopFilePath.isEmpty()) {
        desktopFilePath = QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                                 desktopFileName + QLatin1String(".desktop"));
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

    emit applicationMenuChanged();
    if (old_hasApplicationMenu != new_hasApplicationMenu) {
        emit hasApplicationMenuChanged(new_hasApplicationMenu);
    }
}

void AbstractClient::updateApplicationMenuObjectPath(const QString &objectPath)
{
    const bool old_hasApplicationMenu = hasApplicationMenu();

    m_applicationMenuObjectPath = objectPath;

    const bool new_hasApplicationMenu = hasApplicationMenu();

    emit applicationMenuChanged();
    if (old_hasApplicationMenu != new_hasApplicationMenu) {
        emit hasApplicationMenuChanged(new_hasApplicationMenu);
    }
}

void AbstractClient::setApplicationMenuActive(bool applicationMenuActive)
{
    if (m_applicationMenuActive != applicationMenuActive) {
        m_applicationMenuActive = applicationMenuActive;
        emit applicationMenuActiveChanged(applicationMenuActive);
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
        emit unresponsiveChanged(m_unresponsive);
        emit captionChanged();
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

void AbstractClient::setOnActivities(QStringList newActivitiesList)
{
    Q_UNUSED(newActivitiesList)
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

bool AbstractClient::isInternal() const
{
    return false;
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
        outline()->show(electricBorderMaximizeGeometry(Cursors::self()->mouse()->pos(), desktop()), moveResizeGeometry());
    else
        outline()->hide();
    elevate(maximizing);
}

QRect AbstractClient::electricBorderMaximizeGeometry(QPoint pos, int desktop)
{
    if (electricBorderMode() == QuickTileMode(QuickTileFlag::Maximize)) {
        if (maximizeMode() == MaximizeFull)
            return geometryRestore();
        else
            return workspace()->clientArea(MaximizeArea, pos, desktop);
    }

    QRect ret = workspace()->clientArea(MaximizeArea, pos, desktop);
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
            QRect clientArea = workspace()->clientArea(MaximizeArea, this);
            if (frameGeometry().top() != clientArea.top()) {
                QRect r(frameGeometry());
                r.moveTop(clientArea.top());
                setFrameGeometry(r);
            }
            setGeometryRestore(prev_geom_restore);
        }
        doSetQuickTileMode();
        emit quickTileModeChanged();
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
            // decorations may turn off some borders when tiled
            const ForceGeometry_t geom_mode = isDecorated() ? ForceGeometrySet : NormalGeometrySet;
            m_quickTileMode = int(QuickTileFlag::None); // Temporary, so the maximize code doesn't get all confused

            setMaximize(false, false);

            setFrameGeometry(electricBorderMaximizeGeometry(keyboard ? frameGeometry().center() : Cursors::self()->mouse()->pos(), desktop()), geom_mode);
            // Store the mode change
            m_quickTileMode = mode;
        } else {
            m_quickTileMode = mode;
            setMaximize(false, false);
        }

        doSetQuickTileMode();
        emit quickTileModeChanged();

        return;
    }

    if (mode != QuickTileMode(QuickTileFlag::None)) {
        QPoint whichScreen = keyboard ? frameGeometry().center() : Cursors::self()->mouse()->pos();

        // If trying to tile to the side that the window is already tiled to move the window to the next
        // screen if it exists, otherwise toggle the mode (set QuickTileFlag::None)
        if (quickTileMode() == mode) {
            const int numScreens = screens()->count();
            const int curScreen = screen();
            int nextScreen = curScreen;
            QVarLengthArray<QRect> screens(numScreens);
            for (int i = 0; i < numScreens; ++i)   // Cache
                screens[i] = Screens::self()->geometry(i);
            for (int i = 0; i < numScreens; ++i) {

                if (i == curScreen)
                    continue;

                if (screens[i].bottom() <= screens[curScreen].top() || screens[i].top() >= screens[curScreen].bottom())
                    continue; // not in horizontal line

                const int x = screens[i].center().x();
                if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Left)) {
                    if (x >= screens[curScreen].center().x() || (curScreen != nextScreen && x <= screens[nextScreen].center().x()))
                        continue; // not left of current or more left then found next
                } else if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Right)) {
                    if (x <= screens[curScreen].center().x() || (curScreen != nextScreen && x >= screens[nextScreen].center().x()))
                        continue; // not right of current or more right then found next
                }

                nextScreen = i;
            }

            if (nextScreen == curScreen) {
                mode = QuickTileFlag::None; // No other screens, toggle tiling
            } else {
                // Move to other screen
                setFrameGeometry(geometryRestore().translated(screens[nextScreen].topLeft() - screens[curScreen].topLeft()));
                whichScreen = screens[nextScreen].center();

                // Swap sides
                if (mode & QuickTileFlag::Horizontal) {
                    mode = (~mode & QuickTileFlag::Horizontal) | (mode & QuickTileFlag::Vertical);
                }
            }
            setElectricBorderMode(mode); // used by ::electricBorderMaximizeGeometry(.)
        } else if (quickTileMode() == QuickTileMode(QuickTileFlag::None)) {
            // Not coming out of an existing tile, not shifting monitors, we're setting a brand new tile.
            // Store geometry first, so we can go out of this tile later.
            setGeometryRestore(frameGeometry());
        }

        if (mode != QuickTileMode(QuickTileFlag::None)) {
            m_quickTileMode = mode;
            // decorations may turn off some borders when tiled
            const ForceGeometry_t geom_mode = isDecorated() ? ForceGeometrySet : NormalGeometrySet;
            // Temporary, so the maximize code doesn't get all confused
            m_quickTileMode = int(QuickTileFlag::None);
            setFrameGeometry(electricBorderMaximizeGeometry(whichScreen, desktop()), geom_mode);
        }

        // Store the mode change
        m_quickTileMode = mode;
    }

    if (mode == QuickTileMode(QuickTileFlag::None)) {
        m_quickTileMode = int(QuickTileFlag::None);
        // Untiling, so just restore geometry, and we're done.
        if (!geometryRestore().isValid()) // invalid if we started maximized and wait for placement
            setGeometryRestore(frameGeometry());
        // decorations may turn off some borders when tiled
        const ForceGeometry_t geom_mode = isDecorated() ? ForceGeometrySet : NormalGeometrySet;
        setFrameGeometry(geometryRestore(), geom_mode);
        checkWorkspacePosition(); // Just in case it's a different screen
    }
    doSetQuickTileMode();
    emit quickTileModeChanged();
}

void AbstractClient::doSetQuickTileMode()
{
}

void AbstractClient::sendToScreen(int newScreen)
{
    newScreen = rules()->checkScreen(newScreen);
    if (isActive()) {
        screens()->setCurrent(newScreen);
        // might impact the layer of a fullscreen window
        foreach (AbstractClient *cc, workspace()->allClientList()) {
            if (cc->isFullScreen() && cc->screen() == newScreen) {
                cc->updateLayer();
            }
        }
    }
    if (screen() == newScreen)   // Don't use isOnScreen(), that's true even when only partially
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
    QRect screenArea = workspace()->clientArea(MaximizeArea, newScreen, desktop());

    // the window can have its center so that the position correction moves the new center onto
    // the old screen, what will tile it where it is. Ie. the screen is not changed
    // this happens esp. with electric border quicktiling
    if (qtMode != QuickTileMode(QuickTileFlag::None))
        keepInArea(oldScreenArea);

    QRect oldGeom = frameGeometry();
    QRect newGeom = oldGeom;
    // move the window to have the same relative position to the center of the screen
    // (i.e. one near the middle of the right edge will also end up near the middle of the right edge)
    QPoint center = newGeom.center() - oldScreenArea.center();
    center.setX(center.x() * screenArea.width() / oldScreenArea.width());
    center.setY(center.y() * screenArea.height() / oldScreenArea.height());
    center += screenArea.center();
    newGeom.moveCenter(center);
    setFrameGeometry(newGeom);

    // If the window was inside the old screen area, explicitly make sure its inside also the new screen area.
    // Calling checkWorkspacePosition() should ensure that, but when moving to a small screen the window could
    // be big enough to overlap outside of the new screen area, making struts from other screens come into effect,
    // which could alter the resulting geometry.
    if (oldScreenArea.contains(oldGeom)) {
        keepInArea(screenArea);
    }

    // align geom_restore - checkWorkspacePosition operates on it
    setGeometryRestore(frameGeometry());

    checkWorkspacePosition(oldGeom);

    // re-align geom_restore to constrained geometry
    setGeometryRestore(frameGeometry());

    // finally reset special states
    // NOTICE that MaximizeRestore/QuickTileFlag::None checks are required.
    // eg. setting QuickTileFlag::None would break maximization
    if (maxMode != MaximizeRestore)
        maximize(maxMode);
    if (qtMode != QuickTileMode(QuickTileFlag::None) && qtMode != quickTileMode())
        setQuickTileMode(qtMode, true);

    auto tso = workspace()->ensureStackingOrder(transients());
    for (auto it = tso.constBegin(), end = tso.constEnd(); it != end; ++it)
        (*it)->sendToScreen(newScreen);
}

void AbstractClient::checkWorkspacePosition(QRect oldGeometry, int oldDesktop, QRect oldClientGeometry)
{
    if (isDock() || isDesktop() || !isPlaceable()) {
        return;
    }
    enum { Left = 0, Top, Right, Bottom };
    const int border[4] = { borderLeft(), borderTop(), borderRight(), borderBottom() };
    if( !oldGeometry.isValid())
        oldGeometry = frameGeometry();
    if( oldDesktop == -2 )
        oldDesktop = desktop();
    if (!oldClientGeometry.isValid())
        oldClientGeometry = oldGeometry.adjusted(border[Left], border[Top], -border[Right], -border[Bottom]);
    if (isFullScreen()) {
        QRect area = workspace()->clientArea(FullScreenArea, this);
        if (frameGeometry() != area)
            setFrameGeometry(area);
        return;
    }

    if (maximizeMode() != MaximizeRestore) {
        GeometryUpdatesBlocker block(this);
        changeMaximize(false, false, true);   // adjust size
        const QRect screenArea = workspace()->clientArea(ScreenArea, this);
        QRect geom = frameGeometry();
        checkOffscreenPosition(&geom, screenArea);
        setFrameGeometry(geom);
        return;
    }

    if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        setFrameGeometry(electricBorderMaximizeGeometry(frameGeometry().center(), desktop()));
        return;
    }

    // this can be true only if this window was mapped before KWin
    // was started - in such case, don't adjust position to workarea,
    // because the window already had its position, and if a window
    // with a strut altering the workarea would be managed in initialization
    // after this one, this window would be moved
    if (!workspace() || workspace()->initializing())
        return;

    // If the window was touching an edge before but not now move it so it is again.
    // Old and new maximums have different starting values so windows on the screen
    // edge will move when a new strut is placed on the edge.
    QRect oldScreenArea;
    if( workspace()->inUpdateClientArea()) {
        // we need to find the screen area as it was before the change
        oldScreenArea = QRect( 0, 0, workspace()->oldDisplayWidth(), workspace()->oldDisplayHeight());
        int distance = INT_MAX;
        foreach(const QRect &r, workspace()->previousScreenSizes()) {
            int d = r.contains( oldGeometry.center()) ? 0 : ( r.center() - oldGeometry.center()).manhattanLength();
            if( d < distance ) {
                distance = d;
                oldScreenArea = r;
            }
        }
    } else {
        oldScreenArea = workspace()->clientArea(ScreenArea, oldGeometry.center(), oldDesktop);
    }
    const QRect oldGeomTall = QRect(oldGeometry.x(), oldScreenArea.y(), oldGeometry.width(), oldScreenArea.height());   // Full screen height
    const QRect oldGeomWide = QRect(oldScreenArea.x(), oldGeometry.y(), oldScreenArea.width(), oldGeometry.height());   // Full screen width
    int oldTopMax = oldScreenArea.y();
    int oldRightMax = oldScreenArea.x() + oldScreenArea.width();
    int oldBottomMax = oldScreenArea.y() + oldScreenArea.height();
    int oldLeftMax = oldScreenArea.x();
    const QRect screenArea = workspace()->clientArea(ScreenArea, geometryRestore().center(), desktop());
    int topMax = screenArea.y();
    int rightMax = screenArea.x() + screenArea.width();
    int bottomMax = screenArea.y() + screenArea.height();
    int leftMax = screenArea.x();
    QRect newGeom = geometryRestore(); // geometry();
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
    for (const QRect &r : workspace()->restrictedMoveArea(desktop(), StrutAreaTop)) {
        QRect rect = r & newGeomTall;
        if (!rect.isEmpty())
            topMax = qMax(topMax, rect.y() + rect.height());
    }
    for (const QRect &r : workspace()->restrictedMoveArea(desktop(), StrutAreaRight)) {
        QRect rect = r & newGeomWide;
        if (!rect.isEmpty())
            rightMax = qMin(rightMax, rect.x());
    }
    for (const QRect &r : workspace()->restrictedMoveArea(desktop(), StrutAreaBottom)) {
        QRect rect = r & newGeomTall;
        if (!rect.isEmpty())
            bottomMax = qMin(bottomMax, rect.y());
    }
    for (const QRect &r : workspace()->restrictedMoveArea(desktop(), StrutAreaLeft)) {
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

    if (newGeom != frameGeometry())
        setFrameGeometry(newGeom);
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
 * Returns the appropriate frame size for the current client size.
 *
 * This is equivalent to clientSizeToFrameSize(constrainClientSize(clientSize())).
 */
QSize AbstractClient::adjustedSize() const
{
    return clientSizeToFrameSize(constrainClientSize(clientSize()));
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

void AbstractClient::updateDecoration(bool check_workspace_pos, bool force)
{
    Q_UNUSED(check_workspace_pos)
    Q_UNUSED(force)
    qCWarning(KWIN_CORE, "%s doesn't support server side decorations", metaObject()->className());
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

}
