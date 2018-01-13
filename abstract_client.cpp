/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "tabgroup.h"
#include "useractions.h"
#include "workspace.h"

#include "wayland_server.h"
#include <KWayland/Server/plasmawindowmanagement_interface.h>

#include <KDecoration2/Decoration>

#include <KDesktopFile>

#include <QMouseEvent>
#include <QStyleHints>

namespace KWin
{

QHash<QString, std::weak_ptr<Decoration::DecorationPalette>> AbstractClient::s_palettes;
std::shared_ptr<Decoration::DecorationPalette> AbstractClient::s_defaultPalette;

AbstractClient::AbstractClient()
    : Toplevel()
#ifdef KWIN_BUILD_TABBOX
    , m_tabBoxClient(QSharedPointer<TabBox::TabBoxClientImpl>(new TabBox::TabBoxClientImpl(this)))
#endif
    , m_colorScheme(QStringLiteral("kdeglobals"))
{
    connect(this, &AbstractClient::geometryShapeChanged, this, &AbstractClient::geometryChanged);
    auto signalMaximizeChanged = static_cast<void (AbstractClient::*)(KWin::AbstractClient*, MaximizeMode)>(&AbstractClient::clientMaximizedStateChanged);
    connect(this, signalMaximizeChanged, this, &AbstractClient::geometryChanged);
    connect(this, &AbstractClient::clientStepUserMovedResized,   this, &AbstractClient::geometryChanged);
    connect(this, &AbstractClient::clientStartUserMovedResized,  this, &AbstractClient::moveResizedChanged);
    connect(this, &AbstractClient::clientFinishUserMovedResized, this, &AbstractClient::moveResizedChanged);
    connect(this, &AbstractClient::clientStartUserMovedResized,  this, &AbstractClient::removeCheckScreenConnection);
    connect(this, &AbstractClient::clientFinishUserMovedResized, this, &AbstractClient::setupCheckScreenConnection);

    connect(this, &AbstractClient::paletteChanged, this, &AbstractClient::triggerDecorationRepaint);

    connect(Decoration::DecorationBridge::self(), &QObject::destroyed, this, &AbstractClient::destroyDecoration);

    // replace on-screen-display on size changes
    connect(this, &AbstractClient::geometryShapeChanged, this,
        [this] (Toplevel *c, const QRect &old) {
            Q_UNUSED(c)
            if (isOnScreenDisplay() && !geometry().isEmpty() && old.size() != geometry().size() && !isInitialPositionSet()) {
                GeometryUpdatesBlocker blocker(this);
                QRect area = workspace()->clientArea(PlacementArea, Screens::self()->current(), desktop());
                Placement::self()->place(this, area);
                setGeometryRestore(geometry());
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
    assert(m_blockGeometryUpdates == 0);
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

TabGroup *AbstractClient::tabGroup() const
{
    return nullptr;
}

bool AbstractClient::untab(const QRect &toGeometry, bool clientRemoved)
{
    Q_UNUSED(toGeometry)
    Q_UNUSED(clientRemoved)
    return false;
}

bool AbstractClient::isCurrentTab() const
{
    return true;
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

void AbstractClient::setIcon(const QIcon &icon)
{
    m_icon = icon;
    emit iconChanged();
}

void AbstractClient::setActive(bool act)
{
    if (m_active == act) {
        return;
    }
    m_active = act;
    const int ruledOpacity = m_active
                             ? rules()->checkOpacityActive(qRound(opacity() * 100.0))
                             : rules()->checkOpacityInactive(qRound(opacity() * 100.0));
    setOpacity(ruledOpacity / 100.0);
    workspace()->setActiveClient(act ? this : NULL);

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
        // force hint change if different
        if (info && bool(info->state() & NET::KeepAbove) != keepAbove())
            info->setState(keepAbove() ? NET::KeepAbove : NET::States(0), NET::KeepAbove);
        return;
    }
    m_keepAbove = b;
    if (info) {
        info->setState(keepAbove() ? NET::KeepAbove : NET::States(0), NET::KeepAbove);
    }
    workspace()->updateClientLayer(this);
    updateWindowRules(Rules::Above);

    doSetKeepAbove();
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
        // force hint change if different
        if (info && bool(info->state() & NET::KeepBelow) != keepBelow())
            info->setState(keepBelow() ? NET::KeepBelow : NET::States(0), NET::KeepBelow);
        return;
    }
    m_keepBelow = b;
    if (info) {
        info->setState(keepBelow() ? NET::KeepBelow : NET::States(0), NET::KeepBelow);
    }
    workspace()->updateClientLayer(this);
    updateWindowRules(Rules::Below);

    doSetKeepBelow();
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

bool AbstractClient::wantsTabFocus() const
{
    return (isNormalWindow() || isDialog()) && wantsInput();
}

bool AbstractClient::isSpecialWindow() const
{
    // TODO
    return isDesktop() || isDock() || isSplash() || isToolbar() || isNotification() || isOnScreenDisplay();
}

void AbstractClient::demandAttention(bool set)
{
    if (isActive())
        set = false;
    if (m_demandsAttention == set)
        return;
    m_demandsAttention = set;
    if (info) {
        info->setState(set ? NET::DemandsAttention : NET::States(0), NET::DemandsAttention);
    }
    workspace()->clientAttentionChanged(this, set);
    emit demandsAttentionChanged();
}

void AbstractClient::setDesktop(int desktop)
{
    const int numberOfDesktops = VirtualDesktopManager::self()->count();
    if (desktop != NET::OnAllDesktops)   // Do range check
        desktop = qMax(1, qMin(numberOfDesktops, desktop));
    desktop = qMin(numberOfDesktops, rules()->checkDesktop(desktop));
    if (m_desktop == desktop)
        return;

    int was_desk = m_desktop;
    const bool wasOnCurrentDesktop = isOnCurrentDesktop();
    m_desktop = desktop;

    if (info) {
        info->setDesktop(desktop);
    }
    if ((was_desk == NET::OnAllDesktops) != (desktop == NET::OnAllDesktops)) {
        // onAllDesktops changed
        workspace()->updateOnAllDesktopsOfTransients(this);
    }

    auto transients_stacking_order = workspace()->ensureStackingOrder(transients());
    for (auto it = transients_stacking_order.constBegin();
            it != transients_stacking_order.constEnd();
            ++it)
        (*it)->setDesktop(desktop);

    if (isModal())  // if a modal dialog is moved, move the mainwindow with it as otherwise
        // the (just moved) modal dialog will confusingly return to the mainwindow with
        // the next desktop change
    {
        foreach (AbstractClient * c2, mainClients())
        c2->setDesktop(desktop);
    }

    doSetDesktop(desktop, was_desk);

    FocusChain::self()->update(this, FocusChain::MakeFirst);
    updateWindowRules(Rules::Desktop);

    emit desktopChanged();
    if (wasOnCurrentDesktop != isOnCurrentDesktop())
        emit desktopPresenceChanged(this, was_desk);
}

void AbstractClient::doSetDesktop(int desktop, int was_desk)
{
    Q_UNUSED(desktop)
    Q_UNUSED(was_desk)
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
    Q_UNUSED(mode)
}

ShadeMode AbstractClient::shadeMode() const
{
    return ShadeNone;
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

    if (isShade() && info) // NETWM restriction - KWindowInfo::isMinimized() == Hidden && !Shaded
        info->setState(0, NET::Shaded);

    m_minimized = true;

    doMinimize();

    updateWindowRules(Rules::Minimize);
    FocusChain::self()->update(this, FocusChain::MakeFirstMinimized);
    // TODO: merge signal with s_minimized
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

    if (isShade() && info) // NETWM restriction - KWindowInfo::isMinimized() == Hidden && !Shaded
        info->setState(NET::Shaded, NET::Shaded);

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

void AbstractClient::updateColorScheme(QString path)
{
    if (path.isEmpty()) {
        path = QStringLiteral("kdeglobals");
    }

    if (!m_palette || m_colorScheme != path) {
        m_colorScheme = path;

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
    }
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
            resizeWithChecks(qMin(area.width(), width()), qMin(area.height(), height()));
    }
    int tx = x(), ty = y();
    if (geometry().right() > area.right() && width() <= area.width())
        tx = area.right() - width() + 1;
    if (geometry().bottom() > area.bottom() && height() <= area.height())
        ty = area.bottom() - height() + 1;
    if (!area.contains(geometry().topLeft())) {
        if (tx < area.x())
            tx = area.x();
        if (ty < area.y())
            ty = area.y();
    }
    if (tx != x() || ty != y())
        move(tx, ty);
}

QSize AbstractClient::maxSize() const
{
    return rules()->checkMaxSize(QSize(INT_MAX, INT_MAX));
}

QSize AbstractClient::minSize() const
{
    return rules()->checkMinSize(QSize(0, 0));
}

void AbstractClient::updateMoveResize(const QPointF &currentGlobalCursor)
{
    handleMoveResize(pos(), currentGlobalCursor.toPoint());
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
    using namespace KWayland::Server;
    auto w = waylandServer()->windowManagement()->createWindow(waylandServer()->windowManagement());
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
    w->setIcon(icon());
    auto updateAppId = [this, w] {
        w->setAppId(QString::fromUtf8(m_desktopFileName.isEmpty() ? resourceClass() : m_desktopFileName));
    };
    updateAppId();
    w->setSkipTaskbar(skipTaskbar());
    w->setPid(pid());
    w->setShadeable(isShadeable());
    w->setShaded(isShade());
    w->setResizable(isResizable());
    w->setMovable(isMovable());
    w->setVirtualDesktopChangeable(true); // FIXME Matches Client::actionSupported(), but both should be implemented.
    w->setParentWindow(transientFor() ? transientFor()->windowManagementInterface() : nullptr);
    w->setGeometry(geom);
    connect(this, &AbstractClient::skipTaskbarChanged, w,
        [w, this] {
            w->setSkipTaskbar(skipTaskbar());
        }
    );
    connect(this, &AbstractClient::captionChanged, w, [w, this] { w->setTitle(caption()); });
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
    connect(this, &AbstractClient::geometryChanged, w,
        [w, this] {
            w->setGeometry(geom);
        }
    );
    connect(w, &PlasmaWindowInterface::closeRequested, this, [this] { closeWindow(); });
    connect(w, &PlasmaWindowInterface::moveRequested, this,
        [this] {
            Cursor::setPos(geometry().center());
            performMouseCommand(Options::MouseMove, Cursor::pos());
        }
    );
    connect(w, &PlasmaWindowInterface::resizeRequested, this,
        [this] {
            Cursor::setPos(geometry().bottomRight());
            performMouseCommand(Options::MouseResize, Cursor::pos());
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
        if (options->isClickRaise()) {
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
            ToplevelList::const_iterator  it = workspace()->stackingOrder().constEnd(),
                                     begin = workspace()->stackingOrder().constBegin();
            while (mustReplay && --it != begin && *it != this) {
                AbstractClient *c = qobject_cast<AbstractClient*>(*it);
                if (!c || (c->keepAbove() && !keepAbove()) || (keepBelow() && !c->keepBelow()))
                    continue; // can never raise above "it"
                mustReplay = !(c->isOnCurrentDesktop() && c->isOnCurrentActivity() && c->geometry().intersects(geometry()));
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
    case Options::MousePreviousTab:
        if (tabGroup())
            tabGroup()->activatePrev();
    break;
    case Options::MouseNextTab:
        if (tabGroup())
            tabGroup()->activateNext();
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
    case Options::MouseDragTab:
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

QPoint AbstractClient::transientPlacementHint() const
{
    return QPoint();
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
    assert(!m_transients.contains(cl));
    assert(cl != this);
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

QSize AbstractClient::sizeForClientSize(const QSize &wsize, Sizemode mode, bool noframe) const
{
    Q_UNUSED(mode)
    Q_UNUSED(noframe)
    return wsize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
}

void AbstractClient::addRepaintDuringGeometryUpdates()
{
    const QRect deco_rect = visibleRect();
    addLayerRepaint(m_visibleRectBeforeGeometryUpdate);
    addLayerRepaint(deco_rect);   // trigger repaint of window's new location
    m_visibleRectBeforeGeometryUpdate = deco_rect;
}

void AbstractClient::updateGeometryBeforeUpdateBlocking()
{
    m_geometryBeforeUpdateBlocking = geom;
}

void AbstractClient::updateTabGroupStates(TabGroup::States)
{
}

void AbstractClient::doMove(int, int)
{
}

void AbstractClient::updateInitialMoveResizeGeometry()
{
    m_moveResize.initialGeometry = geometry();
    m_moveResize.geometry = m_moveResize.initialGeometry;
    m_moveResize.startScreen = screen();
}

void AbstractClient::updateCursor()
{
    Position m = moveResizePointerMode();
    if (!isResizable() || isShade())
        m = PositionCenter;
    Qt::CursorShape c = Qt::ArrowCursor;
    switch(m) {
    case PositionTopLeft:
    case PositionBottomRight:
        c = Qt::SizeFDiagCursor;
        break;
    case PositionBottomLeft:
    case PositionTopRight:
        c = Qt::SizeBDiagCursor;
        break;
    case PositionTop:
    case PositionBottom:
        c = Qt::SizeVerCursor;
        break;
    case PositionLeft:
    case PositionRight:
        c = Qt::SizeHorCursor;
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
    workspace()->setClientIsMoving(nullptr);
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
    QPoint pos = Cursor::pos();
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
        finishMoveResize(false);
        setMoveResizePointerButtonDown(false);
        updateCursor();
        break;
    case Qt::Key_Escape:
        finishMoveResize(true);
        setMoveResizePointerButtonDown(false);
        updateCursor();
        break;
    default:
        return;
    }
    Cursor::setPos(pos);
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
    else if (event->button() == Qt::MidButton)
        com = active ? options->commandActiveTitlebar2() : options->commandInactiveTitlebar2();
    else if (event->button() == Qt::RightButton)
        com = active ? options->commandActiveTitlebar3() : options->commandInactiveTitlebar3();
    if (event->button() == Qt::LeftButton
            && com != Options::MouseOperationsMenu // actions where it's not possible to get the matching
            && com != Options::MouseMinimize  // mouse release event
            && com != Options::MouseDragTab) {
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
               com == Options::MouseDragTab ||
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
    // TODO: shade hover
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
    // TODO: shade hover
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
        AbstractClient *client = waylandServer()->findAbstractClient(i.key());
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

bool AbstractClient::dockWantsInput() const
{
    return false;
}

void AbstractClient::setDesktopFileName(const QByteArray &name)
{
    if (name == m_desktopFileName) {
        return;
    }
    m_desktopFileName = name;
    emit desktopFileNameChanged();
}

QString AbstractClient::iconFromDesktopFile() const
{
    if (m_desktopFileName.isEmpty()) {
        return QString();
    }
    QString desktopFile = QString::fromUtf8(m_desktopFileName);
    if (!desktopFile.endsWith(QLatin1String(".desktop"))) {
        desktopFile.append(QLatin1String(".desktop"));
    }
    KDesktopFile df(desktopFile);
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

    if (old_hasApplicationMenu != new_hasApplicationMenu) {
        emit hasApplicationMenuChanged(new_hasApplicationMenu);
    }
}

void AbstractClient::updateApplicationMenuObjectPath(const QString &objectPath)
{
    const bool old_hasApplicationMenu = hasApplicationMenu();

    m_applicationMenuObjectPath = objectPath;

    const bool new_hasApplicationMenu = hasApplicationMenu();

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

}
