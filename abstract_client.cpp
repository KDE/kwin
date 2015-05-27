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
#include "decorations/decorationpalette.h"
#include "focuschain.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "workspace.h"

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
}

AbstractClient::~AbstractClient() = default;

void AbstractClient::updateMouseGrab()
{
}

bool AbstractClient::belongToSameApplication(const AbstractClient *c1, const AbstractClient *c2, bool active_hack)
{
    return c1->belongsToSameApplication(c2, active_hack);
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

void AbstractClient::growHorizontal()
{
}

void AbstractClient::growVertical()
{
}

void AbstractClient::shrinkHorizontal()
{
}

void AbstractClient::shrinkVertical()
{
}

void AbstractClient::packTo(int left, int top)
{
    Q_UNUSED(left)
    Q_UNUSED(top)
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

    doSetActive();
    emit activeChanged();
    updateMouseGrab();
}

void AbstractClient::doSetActive()
{
}

void AbstractClient::updateLayer()
{
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

}
