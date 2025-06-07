/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "window.h"
#include "effect/globals.h"
#include "utils/common.h"

#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "appmenu.h"
#include "client_machine.h"
#include "compositor.h"
#include "core/output.h"
#include "decorations/decoratedwindow.h"
#include "decorations/decorationpalette.h"
#include "focuschain.h"
#include "input.h"
#include "outline.h"
#include "placement.h"
#include "pointer_input.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "screenedge.h"
#include "shadow.h"
#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "tiles/tilemanager.h"
#include "useractions.h"
#include "virtualdesktops.h"
#include "wayland/output.h"
#include "wayland/plasmawindowmanagement.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <KDesktopFile>

#include <QDebug>
#include <QDir>
#include <QJSEngine>
#include <QMouseEvent>
#include <QStyleHints>

namespace KWin
{

QHash<QString, std::weak_ptr<Decoration::DecorationPalette>> Window::s_palettes;
std::shared_ptr<Decoration::DecorationPalette> Window::s_defaultPalette;

Window::Window()
    : ready_for_painting(false)
    , m_internalId(QUuid::createUuid())
    , m_clientMachine(new ClientMachine(this))
    , m_skipCloseAnimation(false)
    , m_colorScheme(QStringLiteral("kdeglobals"))
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);

    connect(this, &Window::bufferGeometryChanged, this, &Window::inputTransformationChanged);

    connect(this, &Window::interactiveMoveResizeStarted, this, &Window::moveResizedChanged);
    connect(this, &Window::interactiveMoveResizeFinished, this, &Window::moveResizedChanged);

    connect(this, &Window::paletteChanged, this, &Window::triggerDecorationRepaint);

    // If the user manually moved the window, don't restore it after the keyboard closes
    connect(this, &Window::interactiveMoveResizeFinished, this, [this]() {
        m_keyboardGeometryRestore = QRectF();
    });
    connect(this, &Window::maximizedChanged, this, [this]() {
        m_keyboardGeometryRestore = QRectF();
    });
    connect(this, &Window::fullScreenChanged, this, [this]() {
        m_keyboardGeometryRestore = QRectF();
    });

    // replace on-screen-display on size changes
    connect(this, &Window::frameGeometryChanged, this, [this](const QRectF &old) {
        if (isOnScreenDisplay() && !frameGeometry().isEmpty() && old.size() != frameGeometry().size() && isPlaceable()) {
            if (const auto placement = workspace()->placement()->place(this, workspace()->clientArea(PlacementArea, this, workspace()->activeOutput()))) {
                place(*placement);
            }
        }
    });

    connect(Workspace::self()->applicationMenu(), &ApplicationMenu::applicationMenuEnabledChanged, this, [this] {
        Q_EMIT hasApplicationMenuChanged(hasApplicationMenu());
    });
    connect(&m_offscreenFramecallbackTimer, &QTimer::timeout, this, &Window::maybeSendFrameCallback);
}

Window::~Window()
{
}

void Window::ref()
{
    ++m_refCount;
}

void Window::unref()
{
    Q_ASSERT(m_refCount > 0);
    --m_refCount;
    if (m_refCount) {
        return;
    }
    if (m_deleted) {
        workspace()->removeDeleted(this);
    }
    delete this;
}

QDebug operator<<(QDebug debug, const Window *window)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    if (window) {
        debug << window->metaObject()->className() << '(' << static_cast<const void *>(window);
        if (const SurfaceInterface *surface = window->surface()) {
            debug << ", surface=" << surface;
        }
        if (window->isClient()) {
            if (!window->isPopupWindow()) {
                debug << ", caption=" << window->caption();
            }
            if (window->transientFor()) {
                debug << ", transientFor=" << window->transientFor();
            }
        }
        if (debug.verbosity() > 2) {
            debug << ", frameGeometry=" << window->frameGeometry();
            debug << ", resourceName=" << window->resourceName();
            debug << ", resourceClass=" << window->resourceClass();
        }
        debug << ')';
    } else {
        debug << "Window(0x0)";
    }
    return debug;
}

QRectF Window::visibleGeometry() const
{
    if (const WindowItem *item = windowItem()) {
        return item->mapToScene(item->boundingRect());
    }
    return QRectF();
}

/**
 * Returns client machine for this window,
 * taken either from its window or from the leader window.
 */
QString Window::wmClientMachine(bool use_localhost) const
{
    if (!m_clientMachine) {
        // this should never happen
        return QString();
    }
    if (use_localhost && m_clientMachine->isLocal()) {
        // special name for the local machine (localhost)
        return ClientMachine::localhost();
    }
    return m_clientMachine->hostName();
}

void Window::setResourceClass(const QString &name, const QString &className)
{
    resource_name = name;
    resource_class = className;
    Q_EMIT windowClassChanged();
}

qreal Window::opacity() const
{
    return m_opacity;
}

void Window::setOpacity(qreal opacity)
{
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (m_opacity == opacity) {
        return;
    }
    const qreal oldOpacity = m_opacity;
    m_opacity = opacity;
    Q_EMIT opacityChanged(this, oldOpacity);
}

bool Window::setupCompositing()
{
    WorkspaceScene *scene = Compositor::self()->scene();
    if (!scene) {
        return false;
    }

    m_windowItem = createItem(scene->containerItem());

    connect(windowItem(), &WindowItem::positionChanged, this, &Window::visibleGeometryChanged);
    connect(windowItem(), &WindowItem::boundingRectChanged, this, &Window::visibleGeometryChanged);

    return true;
}

void Window::finishCompositing()
{
    m_windowItem.reset();
}

void Window::setReadyForPainting()
{
    if (!ready_for_painting) {
        ready_for_painting = true;
        Q_EMIT readyForPaintingChanged();
    }
}

Output *Window::output() const
{
    return m_output;
}

void Window::setOutput(Output *output)
{
    if (m_output != output) {
        m_output = output;
        Q_EMIT outputChanged();
    }
}

bool Window::isOnActiveOutput() const
{
    return isOnOutput(workspace()->activeOutput());
}

bool Window::isOnOutput(Output *output) const
{
    return output->geometry().intersects(frameGeometry().toRect());
}

Shadow *Window::shadow() const
{
    return m_shadow.get();
}

void Window::updateShadow()
{
    if (m_shadow) {
        if (!m_shadow->updateShadow()) {
            m_shadow.reset();
        }
        Q_EMIT shadowChanged();
    } else {
        m_shadow = Shadow::createShadow(this);
        if (m_shadow) {
            Q_EMIT shadowChanged();
        }
    }
}

EffectWindow *Window::effectWindow()
{
    return m_windowItem ? m_windowItem->effectWindow() : nullptr;
}

const EffectWindow *Window::effectWindow() const
{
    return m_windowItem ? m_windowItem->effectWindow() : nullptr;
}

SurfaceItem *Window::surfaceItem() const
{
    if (m_windowItem) {
        return m_windowItem->surfaceItem();
    }
    return nullptr;
}

bool Window::wantsShadowToBeRendered() const
{
    return !isFullScreen() && maximizeMode() != MaximizeFull;
}

bool Window::isClient() const
{
    return false;
}

bool Window::isUnmanaged() const
{
    return false;
}

void Window::elevate(bool elevate)
{
    if (m_windowItem) {
        if (elevate) {
            m_windowItem->elevate();
        } else {
            m_windowItem->deelevate();
        }
    }
}

pid_t Window::pid() const
{
    return -1;
}

bool Window::skipsCloseAnimation() const
{
    return m_skipCloseAnimation;
}

void Window::setSkipCloseAnimation(bool set)
{
    if (set == m_skipCloseAnimation) {
        return;
    }
    m_skipCloseAnimation = set;
    Q_EMIT skipCloseAnimationChanged();
}

SurfaceInterface *Window::surface() const
{
    return m_surface;
}

void Window::setSurface(SurfaceInterface *surface)
{
    if (m_surface == surface) {
        return;
    }
    m_surface = surface;
    Q_EMIT surfaceChanged();
}

int Window::stackingOrder() const
{
    return m_stackingOrder;
}

void Window::setStackingOrder(int order)
{
    if (m_stackingOrder != order) {
        m_stackingOrder = order;
        Q_EMIT stackingOrderChanged();
    }
}

QString Window::windowRole() const
{
    return QString();
}

QMatrix4x4 Window::inputTransformation() const
{
    QMatrix4x4 m;
    m.translate(-m_bufferGeometry.x(), -m_bufferGeometry.y());
    return m;
}

bool Window::hitTest(const QPointF &point) const
{
    if (isDecorated()) {
        if (m_decoration.inputRegion.contains(flooredPoint(mapToFrame(point)))) {
            return true;
        }
    }
    if (m_surface && m_surface->isMapped()) {
        return m_surface->inputSurfaceAt(mapToLocal(point));
    }
    return exclusiveContains(m_bufferGeometry, point);
}

QPointF Window::mapToFrame(const QPointF &point) const
{
    return point - frameGeometry().topLeft();
}

QPointF Window::mapToLocal(const QPointF &point) const
{
    return point - bufferGeometry().topLeft();
}

QPointF Window::mapFromLocal(const QPointF &point) const
{
    return point + bufferGeometry().topLeft();
}

bool Window::isLocalhost() const
{
    if (!m_clientMachine) {
        return true;
    }
    return m_clientMachine->isLocal();
}

QMargins Window::frameMargins() const
{
    return QMargins(borderLeft(), borderTop(), borderRight(), borderBottom());
}

bool Window::belongToSameApplication(const Window *c1, const Window *c2, SameApplicationChecks checks)
{
    return c1->belongsToSameApplication(c2, checks);
}

#if KWIN_BUILD_X11
xcb_timestamp_t Window::userTime() const
{
    return XCB_TIME_CURRENT_TIME;
}
#endif

void Window::setSkipSwitcher(bool set)
{
    set = rules()->checkSkipSwitcher(set);
    if (set == skipSwitcher()) {
        return;
    }
    m_skipSwitcher = set;
    doSetSkipSwitcher();
    updateWindowRules(Rules::SkipSwitcher);
    Q_EMIT skipSwitcherChanged();
}

void Window::setSkipPager(bool b)
{
    b = rules()->checkSkipPager(b);
    if (b == skipPager()) {
        return;
    }
    m_skipPager = b;
    doSetSkipPager();
    updateWindowRules(Rules::SkipPager);
    Q_EMIT skipPagerChanged();
}

void Window::doSetSkipPager()
{
}

void Window::setSkipTaskbar(bool b)
{
    if (b == skipTaskbar()) {
        return;
    }
    m_skipTaskbar = b;
    doSetSkipTaskbar();
    updateWindowRules(Rules::SkipTaskbar);
    Q_EMIT skipTaskbarChanged();
}

void Window::setOriginalSkipTaskbar(bool b)
{
    m_originalSkipTaskbar = rules()->checkSkipTaskbar(b);
    setSkipTaskbar(m_originalSkipTaskbar);
}

void Window::doSetSkipTaskbar()
{
}

void Window::doSetSkipSwitcher()
{
}

void Window::setIcon(const QIcon &icon)
{
    m_icon = icon;
    Q_EMIT iconChanged();
}

void Window::setActive(bool act)
{
    if (isDeleted()) {
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
    workspace()->setActiveWindow(act ? this : nullptr);

    if (!m_active) {
        cancelAutoRaise();
    }

    if (!m_active && shadeMode() == ShadeActivated) {
        setShade(ShadeNormal);
    }

    StackingUpdatesBlocker blocker(workspace());
    updateLayer(); // active windows may get different layer
    auto mainwindows = mainWindows();
    for (auto it = mainwindows.constBegin(); it != mainwindows.constEnd(); ++it) {
        if ((*it)->isFullScreen()) { // fullscreens go high even if their transient is active
            (*it)->updateLayer();
        }
    }

    doSetActive();
    Q_EMIT activeChanged();
}

void Window::doSetActive()
{
}

bool Window::isDeleted() const
{
    return m_deleted;
}

void Window::markAsDeleted()
{
    Q_ASSERT(!m_deleted);
    m_deleted = true;
    workspace()->addDeleted(this);
}

Layer Window::layer() const
{
    if (m_layer == UnknownLayer) {
        const_cast<Window *>(this)->m_layer = rules()->checkLayer(belongsToLayer());
    }
    return m_layer;
}

void Window::updateLayer()
{
    if (isDeleted()) {
        return;
    }
    if (layer() == rules()->checkLayer(belongsToLayer())) {
        return;
    }
    StackingUpdatesBlocker blocker(workspace());
    m_layer = UnknownLayer; // invalidate, will be updated when doing restacking
}

Layer Window::belongsToLayer() const
{
    if (isOutline()) {
        return NormalLayer;
    }
    if (isUnmanaged() || isInternal()) {
        return OverlayLayer;
    }
    if (isLockScreen() && !waylandServer()) {
        return OverlayLayer;
    }
    if (isInputMethod()) {
        return OverlayLayer;
    }
    if (isLockScreenOverlay() && waylandServer() && waylandServer()->isScreenLocked()) {
        return OverlayLayer;
    }
    if (isDesktop()) {
        return DesktopLayer;
    }
    if (isSplash()) { // no damn annoying splashscreens
        return NormalLayer; // getting in the way of everything else
    }
    if (isDock() || isAppletPopup()) {
        return AboveLayer;
    }
    if (isPopupWindow()) {
        return PopupLayer;
    }
    if (isOnScreenDisplay()) {
        return OnScreenDisplayLayer;
    }
    if (isNotification()) {
        return NotificationLayer;
    }
    if (isCriticalNotification()) {
        return CriticalNotificationLayer;
    }
    if (keepBelow()) {
        return BelowLayer;
    }
    if (isActiveFullScreen()) {
        return ActiveLayer;
    }
    if (keepAbove()) {
        return AboveLayer;
    }

    return NormalLayer;
}

bool Window::belongsToDesktop() const
{
    return false;
}

void Window::setKeepAbove(bool b)
{
    b = rules()->checkKeepAbove(b);
    if (b && !rules()->checkKeepBelow(false)) {
        setKeepBelow(false);
    }
    if (b == keepAbove()) {
        return;
    }
    m_keepAbove = b;
    doSetKeepAbove();
    updateLayer();
    updateWindowRules(Rules::Above);

    Q_EMIT keepAboveChanged(m_keepAbove);
}

void Window::doSetKeepAbove()
{
}

void Window::setKeepBelow(bool b)
{
    b = rules()->checkKeepBelow(b);
    if (b && !rules()->checkKeepAbove(false)) {
        setKeepAbove(false);
    }
    if (b == keepBelow()) {
        return;
    }
    m_keepBelow = b;
    doSetKeepBelow();
    updateLayer();
    updateWindowRules(Rules::Below);

    Q_EMIT keepBelowChanged(m_keepBelow);
}

void Window::doSetKeepBelow()
{
}

void Window::startAutoRaise()
{
    delete m_autoRaiseTimer;
    m_autoRaiseTimer = new QTimer(this);
    connect(m_autoRaiseTimer, &QTimer::timeout, this, &Window::autoRaise);
    m_autoRaiseTimer->setSingleShot(true);
    m_autoRaiseTimer->start(options->autoRaiseInterval());
}

void Window::cancelAutoRaise()
{
    delete m_autoRaiseTimer;
    m_autoRaiseTimer = nullptr;
}

void Window::autoRaise()
{
    workspace()->raiseWindow(this);
    cancelAutoRaise();
}

bool Window::isMostRecentlyRaised() const
{
    // The last window in the unconstrained stacking order is the most recently raised one.
    return workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop(), nullptr, true, false) == this;
}

bool Window::wantsTabFocus() const
{
    return (isNormalWindow() || isDialog() || isAppletPopup()) && wantsInput();
}

bool Window::isSpecialWindow() const
{
    // TODO
    return isDesktop() || isDock() || isSplash() || isToolbar() || isNotification() || isOnScreenDisplay() || isCriticalNotification() || isAppletPopup();
}

void Window::demandAttention(bool set)
{
    if (isActive()) {
        set = false;
    }
    if (m_demandsAttention == set) {
        return;
    }
    m_demandsAttention = set;
    doSetDemandsAttention();
    workspace()->windowAttentionChanged(this, set);
    Q_EMIT demandsAttentionChanged();
}

void Window::doSetDemandsAttention()
{
}

void Window::setDesktops(QList<VirtualDesktop *> desktops)
{
    desktops = rules()->checkDesktops(desktops);
    if (desktops == m_desktops) {
        return;
    }

    m_desktops = desktops;

    if (windowManagementInterface()) {
        if (m_desktops.isEmpty()) {
            windowManagementInterface()->setOnAllDesktops(true);
        } else {
            windowManagementInterface()->setOnAllDesktops(false);
            auto currentDesktops = windowManagementInterface()->plasmaVirtualDesktops();
            for (auto desktop : std::as_const(m_desktops)) {
                if (!currentDesktops.contains(desktop->id())) {
                    windowManagementInterface()->addPlasmaVirtualDesktop(desktop->id());
                } else {
                    currentDesktops.removeOne(desktop->id());
                }
            }
            for (const auto &desktopId : std::as_const(currentDesktops)) {
                windowManagementInterface()->removePlasmaVirtualDesktop(desktopId);
            }
        }
    }

    auto transients_stacking_order = workspace()->ensureStackingOrder(transients());
    for (auto it = transients_stacking_order.constBegin(); it != transients_stacking_order.constEnd(); ++it) {
        (*it)->setDesktops(desktops);
    }

    if (isModal()) // if a modal dialog is moved, move the mainwindow with it as otherwise
                   // the (just moved) modal dialog will confusingly return to the mainwindow with
                   // the next desktop change
    {
        const auto windows = mainWindows();
        for (Window *other : windows) {
            other->setDesktops(desktops);
        }
    }

    doSetDesktop();

    Workspace::self()->focusChain()->update(this, FocusChain::MakeFirst);
    updateWindowRules(Rules::Desktops);

    Q_EMIT desktopsChanged();
}

void Window::doSetDesktop()
{
}

void Window::enterDesktop(VirtualDesktop *virtualDesktop)
{
    if (m_desktops.contains(virtualDesktop)) {
        return;
    }
    auto desktops = m_desktops;
    desktops.append(virtualDesktop);
    setDesktops(desktops);
}

void Window::leaveDesktop(VirtualDesktop *virtualDesktop)
{
    QList<VirtualDesktop *> currentDesktops;
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

void Window::setOnAllDesktops(bool b)
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

QList<VirtualDesktop *> Window::desktops() const
{
    return m_desktops;
}

QStringList Window::desktopIds() const
{
    const auto desks = desktops();
    QStringList ids;
    ids.reserve(desks.count());
    std::transform(desks.constBegin(), desks.constEnd(),
                   std::back_inserter(ids),
                   [](const VirtualDesktop *vd) {
                       return vd->id();
                   });
    return ids;
}

bool Window::isOnDesktop(VirtualDesktop *desktop) const
{
    return isOnAllDesktops() || desktops().contains(desktop);
}

bool Window::isOnCurrentDesktop() const
{
    return isOnDesktop(VirtualDesktopManager::self()->currentDesktop());
}

ShadeMode Window::shadeMode() const
{
    return m_shadeMode;
}

bool Window::isShadeable() const
{
    return false;
}

void Window::setShade(bool set)
{
    set ? setShade(ShadeNormal) : setShade(ShadeNone);
}

void Window::setShade(ShadeMode mode)
{
    if (!isShadeable()) {
        return;
    }
    if (mode == ShadeHover && isInteractiveMove()) {
        return; // causes geometry breaks and is probably nasty
    }
    if (isSpecialWindow() || !isDecorated()) {
        mode = ShadeNone;
    }

    mode = rules()->checkShade(mode);
    if (m_shadeMode == mode) {
        return;
    }

    const bool wasShade = isShade();
    const ShadeMode previousShadeMode = shadeMode();
    m_shadeMode = mode;

    if (wasShade == isShade()) {
        // Decoration may want to update after e.g. hover-shade changes
        Q_EMIT shadeChanged();
        return; // No real change in shaded state
    }

    Q_ASSERT(isDecorated());

    doSetShade(previousShadeMode);
    updateWindowRules(Rules::Shade);

    Q_EMIT shadeChanged();
}

void Window::doSetShade(ShadeMode previousShadeMode)
{
}

void Window::shadeHover()
{
    setShade(ShadeHover);
    cancelShadeHoverTimer();
}

void Window::shadeUnhover()
{
    setShade(ShadeNormal);
    cancelShadeHoverTimer();
}

void Window::startShadeHoverTimer()
{
    if (!isShade()) {
        return;
    }
    m_shadeHoverTimer = new QTimer(this);
    connect(m_shadeHoverTimer, &QTimer::timeout, this, &Window::shadeHover);
    m_shadeHoverTimer->setSingleShot(true);
    m_shadeHoverTimer->start(options->shadeHoverInterval());
}

void Window::startShadeUnhoverTimer()
{
    if (m_shadeMode == ShadeHover && !isInteractiveMoveResize() && !isInteractiveMoveResizePointerButtonDown()) {
        m_shadeHoverTimer = new QTimer(this);
        connect(m_shadeHoverTimer, &QTimer::timeout, this, &Window::shadeUnhover);
        m_shadeHoverTimer->setSingleShot(true);
        m_shadeHoverTimer->start(options->shadeHoverInterval());
    }
}

void Window::cancelShadeHoverTimer()
{
    delete m_shadeHoverTimer;
    m_shadeHoverTimer = nullptr;
}

void Window::toggleShade()
{
    // If the mode is ShadeHover or ShadeActive, cancel shade too.
    setShade(shadeMode() == ShadeNone ? ShadeNormal : ShadeNone);
}

Qt::Edge Window::titlebarPosition() const
{
    // TODO: still needed, remove?
    return Qt::TopEdge;
}

bool Window::titlebarPositionUnderMouse() const
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

void Window::setMinimized(bool set)
{
    const bool effectiveSet = rules()->checkMinimize(set);
    if (m_minimized == effectiveSet) {
        return;
    }

    if (effectiveSet && !isMinimizable()) {
        return;
    }

    m_minimized = effectiveSet;
    doMinimize();

    updateWindowRules(Rules::Minimize);
    Q_EMIT minimizedChanged();
}

void Window::doMinimize()
{
}

QPalette Window::palette()
{
    ensurePalette();
    return m_palette->palette();
}

const Decoration::DecorationPalette *Window::decorationPalette()
{
    ensurePalette();
    return m_palette.get();
}

QString Window::preferredColorScheme() const
{
    return rules()->checkDecoColor(QString());
}

QString Window::colorScheme() const
{
    return m_colorScheme;
}

void Window::setColorScheme(const QString &colorScheme)
{
    QString requestedColorScheme = colorScheme;
    if (requestedColorScheme.isEmpty()) {
        requestedColorScheme = QStringLiteral("kdeglobals");
    }

    if (m_colorScheme == requestedColorScheme) {
        return;
    }

    m_colorScheme = requestedColorScheme;

    if (m_palette) {
        disconnect(m_palette.get(), &Decoration::DecorationPalette::changed, this, &Window::handlePaletteChange);
        m_palette.reset();

        // If there already was a palette, re-create it right away
        // so the signals for repainting the decoration are emitted.
        ensurePalette();
    }

    Q_EMIT colorSchemeChanged();
}

void Window::updateColorScheme()
{
    setColorScheme(preferredColorScheme());
}

void Window::ensurePalette()
{
    if (m_palette) {
        return;
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

        if (m_colorScheme == QLatin1StringView("kdeglobals")) {
            s_defaultPalette = m_palette;
        }
    } else {
        m_palette = it->lock();
    }

    connect(m_palette.get(), &Decoration::DecorationPalette::changed, this, &Window::handlePaletteChange);

    handlePaletteChange();
}

void Window::handlePaletteChange()
{
    Q_EMIT paletteChanged(palette());
}

QRectF Window::keepInArea(QRectF geometry, QRectF area, bool partial) const
{
    if (partial) {
        // increase the area so that can have only 100 pixels in the area
        const QRectF geometry = moveResizeGeometry();
        area.setLeft(std::min(area.left() - geometry.width() + 100, area.left()));
        area.setTop(std::min(area.top() - geometry.height() + 100, area.top()));
        area.setRight(std::max(area.right() + geometry.width() - 100, area.right()));
        area.setBottom(std::max(area.bottom() + geometry.height() - 100, area.bottom()));
    }
    if (!partial) {
        // resize to fit into area
        if (area.width() < geometry.width() || area.height() < geometry.height()) {
            geometry = resizeWithChecks(geometry, geometry.size().boundedTo(area.size()));
        }
    }

    if (geometry.right() > area.right() && geometry.width() <= area.width()) {
        geometry.moveRight(area.right());
    }
    if (geometry.bottom() > area.bottom() && geometry.height() <= area.height()) {
        geometry.moveBottom(area.bottom());
    }

    if (geometry.left() < area.left()) {
        geometry.moveLeft(area.left());
    }
    if (geometry.top() < area.top()) {
        geometry.moveTop(area.top());
    }
    return geometry;
}

/**
 * Returns the maximum client size, not the maximum frame size.
 */
QSizeF Window::maxSize() const
{
    return rules()->checkMaxSize(QSize(INT_MAX, INT_MAX));
}

/**
 * Returns the minimum client size, not the minimum frame size.
 */
QSizeF Window::minSize() const
{
    return rules()->checkMinSize(QSize(0, 0));
}

void Window::maximize(MaximizeMode mode, const QRectF &restore)
{
    qCWarning(KWIN_CORE, "%s doesn't support setting maximized state", metaObject()->className());
}

void Window::setMaximize(bool vertically, bool horizontally, const QRectF &restore)
{
    MaximizeMode mode = MaximizeRestore;
    if (vertically) {
        mode = MaximizeMode(mode | MaximizeVertical);
    }
    if (horizontally) {
        mode = MaximizeMode(mode | MaximizeHorizontal);
    }

    maximize(mode, restore);
}

bool Window::startInteractiveMoveResize()
{
    if (isDeleted()) {
        return false;
    }

    Q_ASSERT(!isInteractiveMoveResize());
    stopDelayedInteractiveMoveResize();

    if (isRequestedFullScreen() && (workspace()->outputs().count() < 2 || !isMovableAcrossScreens())) {
        return false;
    }
    if ((interactiveMoveResizeGravity() == Gravity::None && !isMovableAcrossScreens())
        || (interactiveMoveResizeGravity() != Gravity::None && (isShade() || !isResizable()))) {
        return false;
    }
    if (!doStartInteractiveMoveResize()) {
        return false;
    }

    invalidateDecorationDoubleClickTimer();

    setInteractiveMoveResize(true);
    workspace()->setMoveResizeWindow(this);
    workspace()->raiseWindow(this);

    m_interactiveMoveResize.initialGeometry = moveResizeGeometry();
    m_interactiveMoveResize.startOutput = moveResizeOutput();
    m_interactiveMoveResize.initialMaximizeMode = requestedMaximizeMode();
    m_interactiveMoveResize.initialQuickTileMode = requestedQuickTileMode();
    m_interactiveMoveResize.initialGeometryRestore = geometryRestore();

    updateElectricGeometryRestore();
    checkUnrestrictedInteractiveMoveResize();
    Q_EMIT interactiveMoveResizeStarted();
    if (workspace()->screenEdges()->isDesktopSwitchingMovingClients()) {
        workspace()->screenEdges()->reserveDesktopSwitching(true, Qt::Vertical | Qt::Horizontal);
    }
    return true;
}

void Window::finishInteractiveMoveResize(bool cancel)
{
    const bool wasMove = isInteractiveMove();
    leaveInteractiveMoveResize();

    doFinishInteractiveMoveResize();

    if (cancel) {
        moveResize(initialInteractiveMoveResizeGeometry());
        if (m_interactiveMoveResize.initialMaximizeMode != MaximizeMode::MaximizeRestore) {
            setMaximize(m_interactiveMoveResize.initialMaximizeMode & MaximizeMode::MaximizeVertical, m_interactiveMoveResize.initialMaximizeMode & MaximizeMode::MaximizeHorizontal, m_interactiveMoveResize.initialGeometryRestore);
        } else if (m_interactiveMoveResize.initialQuickTileMode) {
            setQuickTileMode(m_interactiveMoveResize.initialQuickTileMode, m_interactiveMoveResize.initialGeometry.center());
            setGeometryRestore(m_interactiveMoveResize.initialGeometryRestore);
        }
    } else if (moveResizeOutput() != interactiveMoveResizeStartOutput()) {
        sendToOutput(moveResizeOutput()); // checks rule validity
        const QRectF oldScreenArea = workspace()->clientArea(MaximizeArea, this, interactiveMoveResizeStartOutput());
        const QRectF screenArea = workspace()->clientArea(MaximizeArea, this, moveResizeOutput());
        m_electricGeometryRestore = moveToArea(m_electricGeometryRestore, oldScreenArea, screenArea);
        if (isRequestedFullScreen() || requestedMaximizeMode() != MaximizeRestore) {
            checkWorkspacePosition();
        }
    }

    if (isElectricBorderMaximizing()) {
        if (auto quickTileMode = std::get_if<QuickTileMode>(&m_electricMode.value())) {
            setQuickTileMode(*quickTileMode, m_interactiveMoveResize.anchor);
        } else if (auto maximizeMode = std::get_if<MaximizeMode>(&m_electricMode.value())) {
            maximize(*maximizeMode, m_electricGeometryRestore);
        }
        setElectricBorderMaximizing(false);
    } else if (wasMove && (m_interactiveMoveResize.modifiers & Qt::ShiftModifier)) {
        setQuickTileMode(QuickTileFlag::Custom, m_interactiveMoveResize.anchor);
    }
    setElectricBorderMode(std::nullopt);
    workspace()->outline()->hide();

    m_interactiveMoveResize.counter++;
    Q_EMIT interactiveMoveResizeFinished();
}

// This function checks if it actually makes sense to perform a restricted move/resize.
// If e.g. the titlebar is already outside of the workarea, there's no point in performing
// a restricted move resize, because then e.g. resize would also move the window (#74555).
// NOTE: Most of it is duplicated from handleMoveResize().
void Window::checkUnrestrictedInteractiveMoveResize()
{
    if (isUnrestrictedInteractiveMoveResize()) {
        return;
    }
    const QRectF &moveResizeGeom = moveResizeGeometry();
    QRectF desktopArea = workspace()->clientArea(WorkArea, this, moveResizeGeom.center());
    int left_marge, right_marge, top_marge, bottom_marge;
    // restricted move/resize - keep at least part of the titlebar always visible
    // how much must remain visible when moved away in that direction
    left_marge = std::min(100. + borderRight(), moveResizeGeom.width());
    right_marge = std::min(100. + borderLeft(), moveResizeGeom.width());
    top_marge = borderBottom();
    bottom_marge = borderTop();
    if (isInteractiveResize()) {
        if (moveResizeGeom.bottom() < desktopArea.top() + top_marge) {
            setUnrestrictedInteractiveMoveResize(true);
        }
        if (moveResizeGeom.top() > desktopArea.bottom() - bottom_marge) {
            setUnrestrictedInteractiveMoveResize(true);
        }
        if (moveResizeGeom.right() < desktopArea.left() + left_marge) {
            setUnrestrictedInteractiveMoveResize(true);
        }
        if (moveResizeGeom.left() > desktopArea.right() - right_marge) {
            setUnrestrictedInteractiveMoveResize(true);
        }
        if (!isUnrestrictedInteractiveMoveResize() && moveResizeGeom.top() < desktopArea.top()) { // titlebar mustn't go out
            setUnrestrictedInteractiveMoveResize(true);
        }
    }
}

// When the user pressed mouse on the titlebar, don't activate move immediately,
// since it may be just a click. Activate instead after a delay. Move used to be
// activated only after moving by several pixels, but that looks bad.
void Window::startDelayedInteractiveMoveResize()
{
    if (isDeleted()) {
        return;
    }
    Q_ASSERT(!m_interactiveMoveResize.delayedTimer);
    m_interactiveMoveResize.delayedTimer = new QTimer(this);
    m_interactiveMoveResize.delayedTimer->setSingleShot(true);
    connect(m_interactiveMoveResize.delayedTimer, &QTimer::timeout, this, [this]() {
        Q_ASSERT(isInteractiveMoveResizePointerButtonDown());
        if (!startInteractiveMoveResize()) {
            setInteractiveMoveResizePointerButtonDown(false);
        }
        updateCursor();
        stopDelayedInteractiveMoveResize();
    });
    m_interactiveMoveResize.delayedTimer->start(QApplication::startDragTime());
}

void Window::stopDelayedInteractiveMoveResize()
{
    delete m_interactiveMoveResize.delayedTimer;
    m_interactiveMoveResize.delayedTimer = nullptr;
}

void Window::updateInteractiveMoveResize(const QPointF &global, Qt::KeyboardModifiers modifiers)
{
    setInteractiveMoveResizeAnchor(global);
    setInteractiveMoveResizeModifiers(modifiers);

    // ShadeHover or ShadeActive, ShadeNormal was already avoided above
    const Gravity gravity = interactiveMoveResizeGravity();
    if (gravity != Gravity::None && shadeMode() != ShadeNone) {
        setShade(ShadeNone);
    }

    const QRectF currentMoveResizeGeom = moveResizeGeometry();
    QRectF nextMoveResizeGeom = currentMoveResizeGeom;

    if (isInteractiveResize()) {
        if (isWaitingForInteractiveResizeSync()) {
            return; // we're still waiting for the client or the timeout
        }

        if (m_tile && m_tile->supportsResizeGravity(gravity)) {
            m_tile->resizeFromGravity(gravity, global.x(), global.y());
            return;
        }

        nextMoveResizeGeom = nextInteractiveResizeGeometry(global);
        if (nextMoveResizeGeom != currentMoveResizeGeom) {
            if (m_tile && !m_tile->supportsResizeGravity(gravity)) {
                setGeometryRestore(nextMoveResizeGeom);
                setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
                return;
            }

            if (requestedMaximizeMode() != MaximizeRestore) {
                switch (interactiveMoveResizeGravity()) {
                case Gravity::Left:
                case Gravity::Right:
                    // Quit maximized horizontally state if the window is resized horizontally.
                    if (requestedMaximizeMode() & MaximizeHorizontal) {
                        QRectF originalGeometry = geometryRestore();
                        originalGeometry.setX(nextMoveResizeGeom.x());
                        originalGeometry.setWidth(nextMoveResizeGeom.width());
                        maximize(requestedMaximizeMode() ^ MaximizeHorizontal, originalGeometry);
                        return;
                    }
                    break;
                case Gravity::Top:
                case Gravity::Bottom:
                    // Quit maximized vertically state if the window is resized vertically.
                    if (requestedMaximizeMode() & MaximizeVertical) {
                        QRectF originalGeometry = geometryRestore();
                        originalGeometry.setY(nextMoveResizeGeom.y());
                        originalGeometry.setHeight(nextMoveResizeGeom.height());
                        maximize(requestedMaximizeMode() ^ MaximizeVertical, originalGeometry);
                        return;
                    }
                    break;
                case Gravity::TopLeft:
                case Gravity::BottomLeft:
                case Gravity::TopRight:
                case Gravity::BottomRight:
                    // Quit the maximized mode if the window is resized by dragging one of its corners.
                    maximize(MaximizeRestore, nextMoveResizeGeom);
                    return;
                default:
                    Q_UNREACHABLE();
                }
            }

            doInteractiveResizeSync(nextMoveResizeGeom);
            Q_EMIT interactiveMoveResizeStepped(nextMoveResizeGeom);
        }
    } else if (isInteractiveMove()) {
        if (isRequestedFullScreen()) {
            nextMoveResizeGeom = workspace()->clientArea(FullScreenArea, this, global);
        } else {
            nextMoveResizeGeom = nextInteractiveMoveGeometry(global);
        }

        if (nextMoveResizeGeom != currentMoveResizeGeom) {
            if (!isRequestedFullScreen()) {
                if (maximizeMode() != MaximizeRestore) {
                    if (maximizeMode() & MaximizeHorizontal) {
                        if (nextMoveResizeGeom.x() != currentMoveResizeGeom.x() || nextMoveResizeGeom.width() != currentMoveResizeGeom.width()) {
                            maximize(MaximizeRestore);
                            return;
                        }
                    }
                    if (maximizeMode() & MaximizeVertical) {
                        if (nextMoveResizeGeom.y() != currentMoveResizeGeom.y() || nextMoveResizeGeom.height() != currentMoveResizeGeom.height()) {
                            maximize(MaximizeRestore);
                            return;
                        }
                    }
                } else if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
                    setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
                    return;
                }
            }

            move(nextMoveResizeGeom.topLeft());
            Q_EMIT interactiveMoveResizeStepped(nextMoveResizeGeom);
        }

        if (!isRequestedFullScreen()) {
            if (modifiers & Qt::ShiftModifier) {
                resetQuickTilingMaximizationZones();
                const auto &r = quickTileGeometry(QuickTileFlag::Custom, global);
                if (r.isEmpty()) {
                    workspace()->outline()->hide();
                } else {
                    if (!workspace()->outline()->isActive() || workspace()->outline()->geometry() != r.toRect()) {
                        workspace()->outline()->show(r.toRect(), moveResizeGeometry().toRect());
                    }
                }
            } else {
                if (quickTileMode() == QuickTileMode(QuickTileFlag::None) && isResizable()) {
                    checkQuickTilingMaximizationZones(global.x(), global.y());
                }
                if (!m_electricMaximizing) {
                    // Only if we are in an electric maximizing gesture we should keep the outline,
                    // otherwise we must make sure it's hidden
                    workspace()->outline()->hide();
                }
            }
        }
    }
}

qreal Window::titlebarThickness() const
{
    switch (titlebarPosition()) {
    case Qt::LeftEdge:
        return borderLeft();
    case Qt::BottomEdge:
        return borderBottom();
    case Qt::RightEdge:
        return borderRight();
    case Qt::TopEdge:
    default:
        return borderTop();
    }
}

/**
 * Computes possible locations for the visible titlebar subrect of height at least @p minHeight
 * and width at least @p minWidth. @p geometry is the geometry of the window,
 * and @p gravity specifies the move/resize mode.
 *
 * When @p gravity is None, TopLeft, Left, BottomLeft, or Top, returns the *top left corner* of the visible subrect.
 * When @p gravity is Right, TopRight, or BottomRight, returns the *top right corner* of the visible subrect.
 * @p gravity being Bottom is not supported, since in this case the titlebar does not move
 *
 * See doc/moveresizerestriction for more details on algorithm.
 */
static QRegion interactiveMoveResizeVisibleSubrectRegion(const QRectF &geometry, Gravity gravity, int minWidth, int minHeight)
{
    const auto outputs = workspace()->outputs();
    const auto struts = workspace()->restrictedMoveArea(VirtualDesktopManager::self()->currentDesktop());
    const auto availableArea = workspace()->clientArea(FullArea, workspace()->activeOutput(), VirtualDesktopManager::self()->currentDesktop());

    QRegion offscreenArea(availableArea.toAlignedRect());
    for (const Output *output : outputs) {
        offscreenArea -= output->geometry();
    }

    // QRectF is used because QRect::right() returns left() + width() - 1; QRectF::right() returns left() + width()
    QRectF initialRect = availableArea;
    switch (gravity) {
    case Gravity::None:
    case Gravity::Top:
        // resizing from the top is handled like moving the window to avoid zero width rectangles when window width is equal to minWidth
        initialRect.adjust(0, 0, -minWidth, -minHeight);
        break;
    case Gravity::Left:
    case Gravity::TopLeft:
    case Gravity::BottomLeft:
        initialRect.setRight(std::min(initialRect.right(), geometry.right()));
        initialRect.setBottom(std::min(initialRect.bottom(), geometry.bottom()));
        initialRect.adjust(0, 0, -minWidth, -minHeight);
        break;
    case Gravity::Right:
    case Gravity::TopRight:
    case Gravity::BottomRight:
        initialRect.setLeft(std::max(initialRect.left(), geometry.left()));
        initialRect.setBottom(std::min(initialRect.bottom(), geometry.bottom()));
        initialRect.adjust(minWidth, 0, 0, -minHeight);
        break;
    default:
        Q_UNREACHABLE();
    }
    QRegion availableRegion(initialRect.toAlignedRect());

    switch (gravity) {
    case Gravity::None:
    case Gravity::Top:
    case Gravity::Left:
    case Gravity::TopLeft:
    case Gravity::BottomLeft:
        for (const QRect &rect : struts) {
            availableRegion -= rect.adjusted(-minWidth, -minHeight, 0, 0);
        }
        for (const QRect &rect : offscreenArea) {
            availableRegion -= rect.adjusted(-minWidth, -minHeight, 0, 0);
        }
        break;
    case Gravity::Right:
    case Gravity::TopRight:
    case Gravity::BottomRight:
        for (const QRect &rect : struts) {
            availableRegion -= rect.adjusted(0, -minHeight, minWidth, 0);
        }
        for (const QRect &rect : offscreenArea) {
            availableRegion -= rect.adjusted(0, -minHeight, minWidth, 0);
        }
        break;
    default:
        Q_UNREACHABLE();
    }

    return availableRegion;
}

/**
 * Find closest point to move top left corner of window such that a titlebar subrect of size at least @p minVisibleWidth by @p minVisibleHeight is visible.
 *
 * See doc/moveresizerestriction for more details on algorithm.
 */
static std::optional<QPointF> confineInteractiveMove(const QRectF &geometry, int minVisibleWidth, int minVisibleHeight)
{
    std::optional<QPointF> candidate;
    qreal bestScore;

    minVisibleWidth = std::min(std::floor(geometry.width()), qreal(minVisibleWidth));

    const QRegion visibleSubrectRegion = interactiveMoveResizeVisibleSubrectRegion(geometry, Gravity::None, minVisibleWidth, minVisibleHeight);
    const QPointF anchor = geometry.topLeft();
    for (QRect rect : visibleSubrectRegion) {
        // convert visibleSubrect top left to window top left
        // Allow the window to be moved "geometry.width() - minVisibleWidth" pixels offscreen to the left
        rect.setLeft(rect.left() - geometry.width() + minVisibleWidth);
        const QPointF closest(std::clamp<qreal>(anchor.x(), rect.x(), rect.x() + rect.width()),
                              std::clamp<qreal>(anchor.y(), rect.y(), rect.y() + rect.height()));
        const qreal score = QLineF(anchor, closest).length();

        if (!candidate || score < bestScore) {
            candidate = closest;
            bestScore = score;
        }
    }

    return candidate;
}

/**
 * Find closest point to move anchor corner of window such that a titlebar subrect of size at least @p minVisibleWidth by @p minVisibleHeight is visible.
 *
 * When @p gravity is None, TopLeft, Left, BottomLeft, or Top, anchor is *top left corner*
 * When @p gravity is Right, TopRight, or BottomRight, anchor is *top right corner*
 * When @p gravity is Bottom, anchor is *bottom left corner*
 *
 * See doc/moveresizerestriction for more details on algorithm.
 */
static std::optional<QPointF> confineInteractiveResize(const QRectF &geometry, Gravity gravity, int minVisibleWidth, int minVisibleHeight)
{
    if (gravity == Gravity::Bottom) {
        QPointF candidate = geometry.bottomLeft();
        if (geometry.height() < minVisibleHeight) {
            candidate.setY(geometry.top() + minVisibleHeight);
        }
        return candidate;
    }

    if (gravity == Gravity::Top) {
        // only in this case is the width of the window fixed during resize
        minVisibleWidth = std::min(std::floor(geometry.width()), qreal(minVisibleWidth));
    }

    const QRegion visibleSubrectRegion = interactiveMoveResizeVisibleSubrectRegion(geometry, gravity, minVisibleWidth, minVisibleHeight);
    QPointF anchor;
    switch (gravity) {
    case Gravity::Top:
    case Gravity::Left:
    case Gravity::TopLeft:
    case Gravity::BottomLeft:
        anchor = geometry.topLeft();
        break;
    case Gravity::Right:
    case Gravity::TopRight:
    case Gravity::BottomRight:
        anchor = geometry.topRight();
        break;
    default:
        Q_UNREACHABLE();
    }

    const auto clampPoint = [](const QRectF &rect, const QPointF &point) {
        return QPointF(std::clamp(point.x(), rect.x(), rect.x() + rect.width()),
                       std::clamp(point.y(), rect.y(), rect.y() + rect.height()));
    };

    // calculate distance, considering movement constraints in the different modes of resizing
    const auto calcDistance = [gravity](const QPointF &source, const QPointF &dest) {
        switch (gravity) {
        case Gravity::Top:
            // when resizing top edge, no horizontal movement is allowed
            if (!qFuzzyCompare(dest.x(), source.x())) {
                return std::numeric_limits<qreal>::max();
            }
            break;
        case Gravity::Left:
        case Gravity::Right:
        case Gravity::BottomLeft:
        case Gravity::BottomRight:
            // for these cases, no vertical movement is allowed
            if (!qFuzzyCompare(dest.y(), source.y())) {
                return std::numeric_limits<qreal>::max();
            }
            break;
        default:
            break;
        }
        return QLineF(source, dest).length();
    };

    std::optional<QPointF> candidate;
    qreal bestScore;
    const auto availableArea = workspace()->clientArea(FullArea, workspace()->activeOutput(), VirtualDesktopManager::self()->currentDesktop());

    switch (gravity) {
    case Gravity::Top:
        // resizing from the top is handled like moving the window to avoid zero width rectangles when window width is equal to minVisibleWidth
        for (QRect rect : visibleSubrectRegion) {
            // convert top-left of visible titlebar subrect to top-left of the window
            rect.setLeft(rect.left() - geometry.width() + minVisibleWidth);

            const QPointF closest = clampPoint(rect, anchor);
            const qreal score = calcDistance(anchor, closest);
            if (!candidate || score < bestScore) {
                candidate = closest;
                bestScore = score;
            }
        }
        break;
    case Gravity::Left:
    case Gravity::TopLeft:
    case Gravity::BottomLeft:
        for (QRect rect : visibleSubrectRegion) {
            // convert top-left of visible titlebar subrect to top-left of the window
            rect.setLeft(availableArea.left());

            const QPointF closest = clampPoint(rect, anchor);
            const qreal score = calcDistance(anchor, closest);
            if (!candidate || score < bestScore) {
                candidate = closest;
                bestScore = score;
            }
        }
        break;
    case Gravity::Right:
    case Gravity::TopRight:
    case Gravity::BottomRight:
        for (QRectF rect : visibleSubrectRegion) {
            // convert top-right of visible titlebar subrect to top-right of the window
            rect.setRight(availableArea.left() + availableArea.width());

            const QPointF closest = clampPoint(rect, anchor);
            const qreal score = calcDistance(anchor, closest);
            if (!candidate || score < bestScore) {
                candidate = closest;
                bestScore = score;
            }
        }
        break;
    default:
        Q_UNREACHABLE();
    }
    return candidate;
}

QRectF Window::nextInteractiveResizeGeometry(const QPointF &global) const
{
    const QRectF currentMoveResizeGeom = moveResizeGeometry();
    QRectF nextMoveResizeGeom = moveResizeGeometry();

    const Gravity gravity = interactiveMoveResizeGravity();
    if (gravity == Gravity::None || isShade() || !isResizable()) {
        return nextMoveResizeGeom;
    }

    // these two points limit the geometry rectangle, i.e. if bottomleft resizing is done,
    // the bottomleft corner should be at is at (topleft.x(), bottomright().y())
    QRectF orig = initialInteractiveMoveResizeGeometry();
    QPointF topleft = global - QPointF(interactiveMoveOffset().x() * orig.width(), interactiveMoveOffset().y() * orig.height());
    QPointF bottomright = global + QPointF((1.0 - interactiveMoveOffset().x()) * orig.width(), (1.0 - interactiveMoveOffset().y()) * orig.height());

    // TODO move whole group when moving its leader or when the leader is not mapped?

    SizeMode sizeMode = SizeModeAny;
    auto calculateMoveResizeGeom = [&topleft, &bottomright, &orig, &nextMoveResizeGeom, &sizeMode, &gravity]() {
        switch (gravity) {
        case Gravity::TopLeft:
            nextMoveResizeGeom = QRectF(topleft, orig.bottomRight());
            break;
        case Gravity::BottomRight:
            nextMoveResizeGeom = QRectF(orig.topLeft(), bottomright);
            break;
        case Gravity::BottomLeft:
            nextMoveResizeGeom = QRectF(QPointF(topleft.x(), orig.y()), QPointF(orig.right(), bottomright.y()));
            break;
        case Gravity::TopRight:
            nextMoveResizeGeom = QRectF(QPointF(orig.x(), topleft.y()), QPointF(bottomright.x(), orig.bottom()));
            break;
        case Gravity::Top:
            nextMoveResizeGeom = QRectF(QPointF(orig.left(), topleft.y()), orig.bottomRight());
            sizeMode = SizeModeFixedH; // try not to affect height
            break;
        case Gravity::Bottom:
            nextMoveResizeGeom = QRectF(orig.topLeft(), QPointF(orig.right(), bottomright.y()));
            sizeMode = SizeModeFixedH;
            break;
        case Gravity::Left:
            nextMoveResizeGeom = QRectF(QPointF(topleft.x(), orig.top()), orig.bottomRight());
            sizeMode = SizeModeFixedW;
            break;
        case Gravity::Right:
            nextMoveResizeGeom = QRectF(orig.topLeft(), QPointF(bottomright.x(), orig.bottom()));
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
    nextMoveResizeGeom = workspace()->adjustWindowSize(this, nextMoveResizeGeom, gravity);

    if (!isUnrestrictedInteractiveMoveResize()) {
        if (const auto anchor = confineInteractiveResize(nextMoveResizeGeom, gravity, 100, titlebarThickness())) {
            switch (gravity) {
            case Gravity::TopLeft:
                nextMoveResizeGeom.setTopLeft(*anchor);
                break;
            case Gravity::Top:
                nextMoveResizeGeom.setTop(anchor->y());
                break;
            case Gravity::TopRight:
                nextMoveResizeGeom.setTopRight(*anchor);
                break;
            case Gravity::Right:
            case Gravity::BottomRight:
                nextMoveResizeGeom.setRight(anchor->x());
                break;
            case Gravity::Left:
            case Gravity::BottomLeft:
                nextMoveResizeGeom.setLeft(anchor->x());
                break;
            case Gravity::Bottom:
                nextMoveResizeGeom.setBottom(anchor->y());
                break;
            default:
                Q_UNREACHABLE();
            }
        } else {
            nextMoveResizeGeom = currentMoveResizeGeom;
        }
    }

    // Always obey size hints, even when in "unrestricted" mode
    QSizeF size = constrainFrameSize(nextMoveResizeGeom.size(), sizeMode);
    // the new topleft and bottomright corners (after checking size constrains), if they'll be needed
    topleft = QPointF(nextMoveResizeGeom.right() - size.width(), nextMoveResizeGeom.bottom() - size.height());
    bottomright = QPointF(nextMoveResizeGeom.left() + size.width(), nextMoveResizeGeom.top() + size.height());
    orig = nextMoveResizeGeom;

    // if aspect ratios are specified, both dimensions may change.
    // Therefore grow to the right/bottom if needed.
    // TODO it should probably obey gravity rather than always using right/bottom ?
    if (sizeMode == SizeModeFixedH) {
        orig.setRight(bottomright.x());
    } else if (sizeMode == SizeModeFixedW) {
        orig.setBottom(bottomright.y());
    }

    calculateMoveResizeGeom();

    return nextMoveResizeGeom;
}

QRectF Window::nextInteractiveMoveGeometry(const QPointF &global) const
{
    const QRectF currentMoveResizeGeom = frameGeometry();
    if (!isMovable()) {
        return currentMoveResizeGeom;
    }

    QRectF nextMoveResizeGeom = currentMoveResizeGeom;
    nextMoveResizeGeom.moveTopLeft(QPointF(global.x() - interactiveMoveOffset().x() * currentMoveResizeGeom.width(),
                                           global.y() - interactiveMoveOffset().y() * currentMoveResizeGeom.height()));
    nextMoveResizeGeom.moveTopLeft(workspace()->adjustWindowPosition(this, nextMoveResizeGeom.topLeft(), isUnrestrictedInteractiveMoveResize()));

    if (!isUnrestrictedInteractiveMoveResize()) {
        if (const auto anchor = confineInteractiveMove(nextMoveResizeGeom, 100, titlebarThickness())) {
            nextMoveResizeGeom.moveTopLeft(anchor.value());
        }
    }

    return nextMoveResizeGeom;
}

StrutRect Window::strutRect(StrutArea area) const
{
    return StrutRect();
}

StrutRects Window::strutRects() const
{
    StrutRects region;
    if (const StrutRect strut = strutRect(StrutAreaTop); strut.isValid()) {
        region += strut;
    }
    if (const StrutRect strut = strutRect(StrutAreaRight); strut.isValid()) {
        region += strut;
    }
    if (const StrutRect strut = strutRect(StrutAreaBottom); strut.isValid()) {
        region += strut;
    }
    if (const StrutRect strut = strutRect(StrutAreaLeft); strut.isValid()) {
        region += strut;
    }
    return region;
}

bool Window::hasStrut() const
{
    return false;
}

void Window::setupWindowManagementInterface()
{
    if (m_windowManagementInterface) {
        // already setup
        return;
    }
    if (!waylandServer()->windowManagement()) {
        return;
    }
    auto w = waylandServer()->windowManagement()->createWindow(this, internalId());
    w->setTitle(caption());
    w->setActive(isActive());
    w->setFullscreen(isFullScreen());
    w->setKeepAbove(keepAbove());
    w->setKeepBelow(keepBelow());
    w->setMaximized(maximizeMode() == KWin::MaximizeFull);
    w->setMinimized(isMinimized());
    w->setDemandsAttention(isDemandingAttention());
    w->setCloseable(isCloseable());
    w->setMaximizeable(isMaximizable());
    w->setMinimizeable(isMinimizable());
    w->setFullscreenable(isFullScreenable());
    w->setApplicationMenuPaths(applicationMenuServiceName(), applicationMenuObjectPath());
    w->setIcon(icon());
    auto updateAppId = [this, w] {
        w->setResourceName(resourceName());
        w->setAppId(m_desktopFileName.isEmpty() ? resourceClass() : m_desktopFileName);
    };
    updateAppId();
    w->setSkipTaskbar(skipTaskbar());
    w->setSkipSwitcher(skipSwitcher());
    w->setPid(pid());
    w->setShadeable(isShadeable());
    w->setShaded(isShade());
    w->setResizable(isResizable());
    w->setMovable(isMovable());
    w->setVirtualDesktopChangeable(true); // FIXME Matches X11Window::actionSupported(), but both should be implemented.
    w->setNoBorder(noBorder());
    w->setCanSetNoBorder(userCanSetNoBorder());
    w->setParentWindow(transientFor() ? transientFor()->windowManagementInterface() : nullptr);
    w->setGeometry(frameGeometry().toRect());
    w->setClientGeometry(clientGeometry().toRect());
    connect(this, &Window::skipTaskbarChanged, w, [w, this]() {
        w->setSkipTaskbar(skipTaskbar());
    });
    connect(this, &Window::skipSwitcherChanged, w, [w, this]() {
        w->setSkipSwitcher(skipSwitcher());
    });
    connect(this, &Window::captionChanged, w, [w, this] {
        w->setTitle(caption());
    });

    connect(this, &Window::activeChanged, w, [w, this] {
        w->setActive(isActive());
    });
    connect(this, &Window::fullScreenChanged, w, [w, this] {
        w->setFullscreen(isFullScreen());
    });
    connect(this, &Window::keepAboveChanged, w, &PlasmaWindowInterface::setKeepAbove);
    connect(this, &Window::keepBelowChanged, w, &PlasmaWindowInterface::setKeepBelow);
    connect(this, &Window::minimizedChanged, w, [w, this] {
        w->setMinimized(isMinimized());
    });
    connect(this, &Window::maximizedChanged, w, [w, this]() {
        w->setMaximized(maximizeMode() == MaximizeFull);
    });
    connect(this, &Window::demandsAttentionChanged, w, [w, this] {
        w->setDemandsAttention(isDemandingAttention());
    });
    connect(this, &Window::iconChanged, w, [w, this]() {
        w->setIcon(icon());
    });
    connect(this, &Window::windowClassChanged, w, updateAppId);
    connect(this, &Window::desktopFileNameChanged, w, updateAppId);
    connect(this, &Window::shadeChanged, w, [w, this] {
        w->setShaded(isShade());
    });
    connect(this, &Window::noBorderChanged, w, [w, this] {
        w->setNoBorder(noBorder());
    });
    connect(this, &Window::transientChanged, w, [w, this]() {
        w->setParentWindow(transientFor() ? transientFor()->windowManagementInterface() : nullptr);
    });
    connect(this, &Window::frameGeometryChanged, w, [w, this]() {
        w->setGeometry(frameGeometry().toRect());
    });
    connect(this, &Window::clientGeometryChanged, w, [w, this]() {
        w->setClientGeometry(clientGeometry().toRect());
    });
    connect(this, &Window::applicationMenuChanged, w, [w, this]() {
        w->setApplicationMenuPaths(applicationMenuServiceName(), applicationMenuObjectPath());
    });
    connect(w, &PlasmaWindowInterface::closeRequested, this, [this] {
        closeWindow();
    });
    connect(w, &PlasmaWindowInterface::moveRequested, this, [this]() {
        input()->pointer()->warp(frameGeometry().center());
        performMousePressCommand(Options::MouseMove, Cursors::self()->mouse()->pos());
    });
    connect(w, &PlasmaWindowInterface::resizeRequested, this, [this]() {
        input()->pointer()->warp(frameGeometry().bottomRight());
        performMousePressCommand(Options::MouseResize, Cursors::self()->mouse()->pos());
    });
    connect(w, &PlasmaWindowInterface::fullscreenRequested, this, [this](bool set) {
        setFullScreen(set);
    });
    connect(w, &PlasmaWindowInterface::minimizedRequested, this, [this](bool set) {
        setMinimized(set);
    });
    connect(w, &PlasmaWindowInterface::maximizedRequested, this, [this](bool set) {
        maximize(set ? MaximizeFull : MaximizeRestore);
    });
    connect(w, &PlasmaWindowInterface::keepAboveRequested, this, [this](bool set) {
        setKeepAbove(set);
    });
    connect(w, &PlasmaWindowInterface::keepBelowRequested, this, [this](bool set) {
        setKeepBelow(set);
    });
    connect(w, &PlasmaWindowInterface::demandsAttentionRequested, this, [this](bool set) {
        demandAttention(set);
    });
    connect(w, &PlasmaWindowInterface::activeRequested, this, [this](bool set) {
        if (set) {
            workspace()->activateWindow(this, true);
        }
    });
    connect(w, &PlasmaWindowInterface::shadedRequested, this, [this](bool set) {
        setShade(set);
    });
    connect(w, &PlasmaWindowInterface::noBorderRequested, this, [this](bool set) {
        setNoBorder(set);
    });

    for (const auto vd : std::as_const(m_desktops)) {
        w->addPlasmaVirtualDesktop(vd->id());
    }
    // We need to set `OnAllDesktops` after the actual VD list has been added.
    // Otherwise it will unconditionally add the current desktop to the interface
    // which may not be the case, for example, when using rules
    w->setOnAllDesktops(isOnAllDesktops());

    // Plasma Virtual desktop management
    // show/hide when the window enters/exits from desktop
    connect(w, &PlasmaWindowInterface::enterPlasmaVirtualDesktopRequested, this, [this](const QString &desktopId) {
        VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForId(desktopId);
        if (vd) {
            Workspace::self()->addWindowToDesktop(this, vd);
        }
    });
    connect(w, &PlasmaWindowInterface::enterNewPlasmaVirtualDesktopRequested, this, [this]() {
        VirtualDesktopManager::self()->setCount(VirtualDesktopManager::self()->count() + 1);
        auto vd = VirtualDesktopManager::self()->desktops().last();
        Workspace::self()->addWindowToDesktop(this, vd);
    });
    connect(w, &PlasmaWindowInterface::leavePlasmaVirtualDesktopRequested, this, [this](const QString &desktopId) {
        VirtualDesktop *vd = VirtualDesktopManager::self()->desktopForId(desktopId);
        if (vd) {
            Workspace::self()->removeWindowFromDesktop(this, vd);
        }
    });

    for (const auto &activity : std::as_const(m_activityList)) {
        w->addPlasmaActivity(activity);
    }

    connect(this, &Window::activitiesChanged, w, [w, this] {
        const auto newActivities = QSet<QString>(m_activityList.begin(), m_activityList.end());
        const auto oldActivitiesList = w->plasmaActivities();
        const auto oldActivities = QSet<QString>(oldActivitiesList.begin(), oldActivitiesList.end());

        const auto activitiesToAdd = newActivities - oldActivities;
        for (const auto &activity : activitiesToAdd) {
            w->addPlasmaActivity(activity);
        }

        const auto activitiesToRemove = oldActivities - newActivities;
        for (const auto &activity : activitiesToRemove) {
            w->removePlasmaActivity(activity);
        }
    });

    // Plasma Activities management
    // show/hide when the window enters/exits activity
    connect(w, &PlasmaWindowInterface::enterPlasmaActivityRequested, this, [this](const QString &activityId) {
        setOnActivity(activityId, true);
    });
    connect(w, &PlasmaWindowInterface::leavePlasmaActivityRequested, this, [this](const QString &activityId) {
        setOnActivity(activityId, false);
    });
    connect(w, &PlasmaWindowInterface::sendToOutput, this, [this](OutputInterface *output) {
        sendToOutput(output->handle());
    });

    m_windowManagementInterface = w;
}

void Window::destroyWindowManagementInterface()
{
    delete m_windowManagementInterface;
    m_windowManagementInterface = nullptr;
}

std::optional<Options::MouseCommand> Window::getMousePressCommand(Qt::MouseButton button) const
{
    if (button == Qt::NoButton) {
        return std::nullopt;
    }
    if (isActive()) {
        if (options->isClickRaise() && !isMostRecentlyRaised()) {
            return Options::MouseActivateRaiseAndPassClick;
        }
    } else {
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
    return std::nullopt;
}

std::optional<Options::MouseCommand> Window::getMouseReleaseCommand(Qt::MouseButton button) const
{
    switch (button) {
    case Qt::LeftButton:
        return options->commandWindow1();
    case Qt::MiddleButton:
        return options->commandWindow2();
    case Qt::RightButton:
        return options->commandWindow3();
    default:
        return std::nullopt;
    }
}

std::optional<Options::MouseCommand> Window::getWheelCommand(Qt::Orientation orientation) const
{
    if (orientation != Qt::Vertical || isActive()) {
        return std::nullopt;
    }
    return options->commandWindowWheel();
}

bool Window::mousePressCommandConsumesEvent(Options::MouseCommand command) const
{
    switch (command) {
    case Options::MouseRaise:
    case Options::MouseLower:
    case Options::MouseOperationsMenu:
    case Options::MouseToggleRaiseAndLower:
    case Options::MouseMaximize:
    case Options::MouseRestore:
    case Options::MouseMinimize:
    case Options::MouseAbove:
    case Options::MouseBelow:
    case Options::MousePreviousDesktop:
    case Options::MouseNextDesktop:
    case Options::MouseOpacityMore:
    case Options::MouseOpacityLess:
    case Options::MouseClose:
    case Options::MouseResize:
    case Options::MouseUnrestrictedResize:
    case Options::MouseShade:
    case Options::MouseSetShade:
    case Options::MouseUnsetShade:
        return true;
    case Options::MouseActivateRaiseAndPassClick:
    case Options::MouseActivateRaiseOnReleaseAndPassClick:
    case Options::MouseActivateAndPassClick:
    case Options::MouseNothing:
        return false;
    case Options::MouseActivateAndRaise:
        if (isActive()) {
            // for clickraise mode
            return false;
        }
        if (!rules()->checkAcceptFocus(acceptsFocus())) {
            const auto stackingOrder = workspace()->stackingOrder();
            auto it = stackingOrder.end();
            while (--it != stackingOrder.begin() && *it != this) {
                auto window = *it;
                if (!window->isClient() || (window->keepAbove() && !keepAbove()) || (keepBelow() && !window->keepBelow())) {
                    continue; // can never raise above "window"
                }
                if (window->isOnCurrentDesktop() && window->isOnCurrentActivity() && window->frameGeometry().intersects(frameGeometry())) {
                    return true;
                }
            }
            return false;
        }
        return true;
    case Options::MouseActivateAndLower:
        return rules()->checkAcceptFocus(acceptsFocus());
    case Options::MouseActivate:
        return !isActive() || rules()->checkAcceptFocus(acceptsFocus());
    case Options::MouseActivateRaiseAndMove:
    case Options::MouseActivateRaiseAndUnrestrictedMove:
    case Options::MouseMove:
    case Options::MouseUnrestrictedMove:
        return isMovableAcrossScreens();
    }
    return false;
}

bool Window::performMousePressCommand(Options::MouseCommand cmd, const QPointF &globalPos)
{
    // NOTE that this has to be checked before running the command
    // as raising the window may change the return value
    const bool consumes = mousePressCommandConsumesEvent(cmd);
    switch (cmd) {
    case Options::MouseRaise:
        workspace()->raiseWindow(this);
        break;
    case Options::MouseLower: {
        workspace()->lowerWindow(this);
        // used to be activateNextWindow(this), then topWindowOnDesktop
        // since this is a mouseOp it's however safe to use the window under the mouse instead
        if (isActive() && options->focusPolicyIsReasonable()) {
            Window *next = workspace()->windowUnderMouse(output());
            if (next && next != this) {
                workspace()->requestFocus(next, false);
            }
        }
        break;
    }
    case Options::MouseOperationsMenu:
        if (isActive() && options->isClickRaise()) {
            autoRaise();
        }
        workspace()->showWindowMenu(QRect(globalPos.toPoint(), globalPos.toPoint()), this);
        break;
    case Options::MouseToggleRaiseAndLower:
        workspace()->raiseOrLowerWindow(this);
        break;
    case Options::MouseActivateAndRaise: {
        workspace()->takeActivity(this, Workspace::ActivityFocus | Workspace::ActivityRaise);
        workspace()->setActiveOutput(globalPos);
        break;
    }
    case Options::MouseActivateAndLower:
        workspace()->requestFocus(this);
        workspace()->lowerWindow(this);
        workspace()->setActiveOutput(globalPos);
        break;
    case Options::MouseActivate:
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        workspace()->setActiveOutput(globalPos);
        break;
    case Options::MouseActivateRaiseAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus | Workspace::ActivityRaise);
        workspace()->setActiveOutput(globalPos);
        break;
    case Options::MouseActivateRaiseOnReleaseAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        workspace()->setActiveOutput(globalPos);
        break;
    case Options::MouseActivateAndPassClick:
        workspace()->takeActivity(this, Workspace::ActivityFocus);
        workspace()->setActiveOutput(globalPos);
        break;
    case Options::MouseMaximize:
        maximize(MaximizeFull);
        break;
    case Options::MouseRestore:
        maximize(MaximizeRestore);
        break;
    case Options::MouseMinimize:
        setMinimized(true);
        break;
    case Options::MouseAbove: {
        StackingUpdatesBlocker blocker(workspace());
        if (keepBelow()) {
            setKeepBelow(false);
        } else {
            setKeepAbove(true);
        }
        break;
    }
    case Options::MouseBelow: {
        StackingUpdatesBlocker blocker(workspace());
        if (keepAbove()) {
            setKeepAbove(false);
        } else {
            setKeepBelow(true);
        }
        break;
    }
    case Options::MousePreviousDesktop:
        workspace()->windowToPreviousDesktop(this);
        break;
    case Options::MouseNextDesktop:
        workspace()->windowToNextDesktop(this);
        break;
    case Options::MouseOpacityMore:
        if (!isDesktop()) { // No point in changing the opacity of the desktop
            setOpacity(std::min(opacity() + 0.1, 1.0));
        }
        break;
    case Options::MouseOpacityLess:
        if (!isDesktop()) { // No point in changing the opacity of the desktop
            setOpacity(std::max(opacity() - 0.1, 0.1));
        }
        break;
    case Options::MouseClose:
        closeWindow();
        break;
    case Options::MouseActivateRaiseAndMove:
    case Options::MouseActivateRaiseAndUnrestrictedMove:
        workspace()->raiseWindow(this);
        workspace()->requestFocus(this);
        workspace()->setActiveOutput(globalPos);
        // fallthrough
    case Options::MouseMove:
    case Options::MouseUnrestrictedMove: {
        if (!isMovableAcrossScreens()) {
            break;
        }
        if (isInteractiveMoveResize()) {
            finishInteractiveMoveResize(false);
        }
        setInteractiveMoveResizeGravity(Gravity::None);
        setInteractiveMoveResizePointerButtonDown(true);
        setInteractiveMoveResizeAnchor(globalPos);
        setInteractiveMoveResizeModifiers(Qt::KeyboardModifiers());
        setInteractiveMoveOffset(QPointF(qreal(globalPos.x() - x()) / width(), qreal(globalPos.y() - y()) / height())); // map from global
        setUnrestrictedInteractiveMoveResize((cmd == Options::MouseActivateRaiseAndUnrestrictedMove
                                              || cmd == Options::MouseUnrestrictedMove));
        if (!startInteractiveMoveResize()) {
            setInteractiveMoveResizePointerButtonDown(false);
        }
        updateCursor();
        break;
    }
    case Options::MouseResize:
    case Options::MouseUnrestrictedResize: {
        if (!isResizable() || isShade()) {
            break;
        }
        if (isInteractiveMoveResize()) {
            finishInteractiveMoveResize(false);
        }
        setInteractiveMoveResizePointerButtonDown(true);
        setInteractiveMoveResizeAnchor(globalPos);
        setInteractiveMoveResizeModifiers(Qt::KeyboardModifiers());
        const QPointF moveOffset = QPointF(globalPos.x() - x(), globalPos.y() - y()); // map from global
        setInteractiveMoveOffset(QPointF(moveOffset.x() / width(), moveOffset.y() / height()));
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
        setUnrestrictedInteractiveMoveResize((cmd == Options::MouseUnrestrictedResize));
        if (!startInteractiveMoveResize()) {
            setInteractiveMoveResizePointerButtonDown(false);
        }
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
        break;
    }
    return consumes;
}

void Window::performMouseReleaseCommand(Options::MouseCommand command, const QPointF &globalPos)
{
    if (command != Options::MouseActivateRaiseOnReleaseAndPassClick) {
        return;
    }
    if (isActive()) {
        workspace()->takeActivity(this, Workspace::ActivityRaise);
    }
    workspace()->setActiveOutput(globalPos);
}

void Window::setTransientFor(Window *transientFor)
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

const Window *Window::transientFor() const
{
    return m_transientFor;
}

Window *Window::transientFor()
{
    return m_transientFor;
}

bool Window::hasTransient(const Window *c, bool indirect) const
{
    return c->transientFor() == this;
}

QList<Window *> Window::mainWindows() const
{
    if (const Window *t = transientFor()) {
        return QList<Window *>{const_cast<Window *>(t)};
    }
    return QList<Window *>();
}

QList<Window *> Window::allMainWindows() const
{
    auto result = mainWindows();
    const auto copy = result; // we need a copy to ensure we don't assign into the container we loop over
    for (const auto *window : copy) {
        result += window->allMainWindows();
    }
    return result;
}

void Window::setModal(bool m)
{
    // Qt-3.2 can have even modal normal windows :(
    if (m_modal == m) {
        return;
    }
    m_modal = m;
    doSetModal();
    Q_EMIT modalChanged();
    // Changing modality for a mapped window is weird (?)
    // _NET_WM_STATE_MODAL should possibly rather be _NET_WM_WINDOW_TYPE_MODAL_DIALOG
}

bool Window::isModal() const
{
    return m_modal;
}

Window *Window::findModal() const
{
    for (Window *transient : m_transients) {
        if (transient->isDeleted()) {
            continue;
        }
        if (transient->isModal()) {
            return transient;
        }
        if (Window *modal = transient->findModal()) {
            return modal;
        }
    }
    return nullptr;
}

bool Window::isTransient() const
{
    return false;
}

// check whether a transient should be actually kept above its mainwindow
// there may be some special cases where this rule shouldn't be enfored
static bool shouldKeepTransientAbove(const Window *parent, const Window *transient)
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
    return true;
}

void Window::addTransient(Window *cl)
{
    Q_ASSERT(!m_transients.contains(cl));
    Q_ASSERT(cl != this);
    m_transients.append(cl);
    if (shouldKeepTransientAbove(this, cl)) {
        workspace()->constrain(this, cl);
    }
}

void Window::removeTransient(Window *cl)
{
    m_transients.removeAll(cl);
    if (cl->transientFor() == this) {
        cl->setTransientFor(nullptr);
    }
    workspace()->unconstrain(this, cl);
}

void Window::removeTransientFromList(Window *cl)
{
    m_transients.removeAll(cl);
}

bool Window::isActiveFullScreen() const
{
    if (!isFullScreen()) {
        return false;
    }

    const auto ac = workspace()->mostRecentlyActivatedWindow(); // instead of activeWindow() - avoids flicker
    // according to NETWM spec implementation notes suggests
    // "focused windows having state _NET_WM_STATE_FULLSCREEN" to be on the highest layer.
    // we'll also take the screen into account
    return ac && (ac == this || !ac->isOnOutput(output()) || ac->allMainWindows().contains(const_cast<Window *>(this)));
}

qreal Window::borderBottom() const
{
    return isDecorated() ? decoration()->borderBottom() : 0;
}

qreal Window::borderLeft() const
{
    return isDecorated() ? decoration()->borderLeft() : 0;
}

qreal Window::borderRight() const
{
    return isDecorated() ? decoration()->borderRight() : 0;
}

qreal Window::borderTop() const
{
    return isDecorated() ? decoration()->borderTop() : 0;
}

void Window::updateCursor()
{
    if (isDeleted()) {
        return;
    }
    Gravity gravity = interactiveMoveResizeGravity();
    if (!isResizable() || isShade()) {
        gravity = Gravity::None;
    }
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
        if (isInteractiveMoveResize()) {
            c = Qt::ClosedHandCursor;
        } else {
            c = Qt::ArrowCursor;
        }
        break;
    }
    if (c == m_interactiveMoveResize.cursor) {
        return;
    }
    m_interactiveMoveResize.cursor = c;
    Q_EMIT moveResizeCursorChanged(c);
}

void Window::leaveInteractiveMoveResize()
{
    workspace()->setMoveResizeWindow(nullptr);
    setInteractiveMoveResize(false);
    if (workspace()->screenEdges()->isDesktopSwitchingMovingClients()) {
        workspace()->screenEdges()->reserveDesktopSwitching(false, Qt::Vertical | Qt::Horizontal);
    }
    if (isElectricBorderMaximizing()) {
        workspace()->outline()->hide();
    }
}

bool Window::doStartInteractiveMoveResize()
{
    return true;
}

void Window::doFinishInteractiveMoveResize()
{
}

bool Window::isWaitingForInteractiveResizeSync() const
{
    return false;
}

void Window::doInteractiveResizeSync(const QRectF &rect)
{
    moveResize(rect);
}

void Window::checkQuickTilingMaximizationZones(int xroot, int yroot)
{
    std::optional<ElectricBorderMode> mode = std::nullopt;
    bool innerBorder = false;

    const auto outputs = workspace()->outputs();
    for (const Output *output : outputs) {
        if (!output->geometry().contains(QPoint(xroot, yroot))) {
            continue;
        }

        auto isInScreen = [&output, &outputs](const QPoint &pt) {
            for (const Output *other : outputs) {
                if (other == output) {
                    continue;
                }
                if (other->geometry().contains(pt)) {
                    return true;
                }
            }
            return false;
        };

        QRectF area = workspace()->clientArea(MaximizeArea, this, QPointF(xroot, yroot));
        QuickTileMode tile = QuickTileFlag::None;
        if (options->electricBorderTiling()) {
            if (xroot <= area.x() + 20) {
                tile |= QuickTileFlag::Left;
                innerBorder = isInScreen(QPoint(area.x() - 1, yroot));
            } else if (xroot >= area.x() + area.width() - 20) {
                tile |= QuickTileFlag::Right;
                innerBorder = isInScreen(QPoint(area.right() + 1, yroot));
            }
        }

        if (tile != QuickTileMode(QuickTileFlag::None)) {
            if (yroot <= area.y() + area.height() * options->electricBorderCornerRatio()) {
                tile |= QuickTileFlag::Top;
            } else if (yroot >= area.y() + area.height() - area.height() * options->electricBorderCornerRatio()) {
                tile |= QuickTileFlag::Bottom;
            }
            mode = tile;
        } else if (options->electricBorderMaximize() && yroot <= area.y() + 5 && isMaximizable()) {
            mode = MaximizeFull;
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
                    if (isInteractiveMove()) {
                        setElectricBorderMaximizing(electricBorderMode().has_value());
                    }
                });
            }
            m_electricMaximizingDelay->start();
        } else {
            setElectricBorderMaximizing(mode.has_value());
        }
    }
}

void Window::resetQuickTilingMaximizationZones()
{
    if (electricBorderMode().has_value()) {
        if (m_electricMaximizingDelay) {
            m_electricMaximizingDelay->stop();
        }
        setElectricBorderMaximizing(false);
        setElectricBorderMode(std::nullopt);
    }
}

void Window::keyPressEvent(QKeyCombination key_combination)
{
    if (!isInteractiveMove() && !isInteractiveResize()) {
        return;
    }
    bool is_control = key_combination.keyboardModifiers() & Qt::CTRL;
    bool is_alt = key_combination.keyboardModifiers() & Qt::ALT;
    int delta = is_control ? 1 : is_alt ? 32
                                        : 8;
    QPointF pos = interactiveMoveResizeAnchor();
    switch (key_combination.key()) {
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
    input()->pointer()->warp(pos);
}

QSizeF Window::resizeIncrements() const
{
    return QSizeF(1, 1);
}

void Window::dontInteractiveMoveResize()
{
    setInteractiveMoveResizePointerButtonDown(false);
    stopDelayedInteractiveMoveResize();
    if (isInteractiveMoveResize()) {
        finishInteractiveMoveResize(false);
    }
}

Gravity Window::mouseGravity() const
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

void Window::endInteractiveMoveResize()
{
    setInteractiveMoveResizePointerButtonDown(false);
    stopDelayedInteractiveMoveResize();
    if (isInteractiveMoveResize()) {
        finishInteractiveMoveResize(false);
        setInteractiveMoveResizeGravity(mouseGravity());
    }
    updateCursor();
}

void Window::cancelInteractiveMoveResize()
{
    setInteractiveMoveResizePointerButtonDown(false);
    stopDelayedInteractiveMoveResize();
    if (isInteractiveMoveResize()) {
        finishInteractiveMoveResize(true);
        setInteractiveMoveResizeGravity(mouseGravity());
    }
    updateCursor();
}

void Window::setDecoration(std::shared_ptr<KDecoration3::Decoration> decoration)
{
    if (m_decoration.decoration == decoration) {
        return;
    }
    if (decoration) {
        QMetaObject::invokeMethod(decoration.get(), QOverload<>::of(&KDecoration3::Decoration::update), Qt::QueuedConnection);
        connect(decoration.get(), &KDecoration3::Decoration::shadowChanged, this, [this]() {
            if (!isDeleted()) {
                updateShadow();
            }
        });
        connect(decoration.get(), &KDecoration3::Decoration::bordersChanged, this, [this]() {
            if (!isDeleted()) {
                updateDecorationInputShape();
            }
        });
        connect(decoration.get(), &KDecoration3::Decoration::resizeOnlyBordersChanged, this, [this]() {
            if (!isDeleted()) {
                updateDecorationInputShape();
            }
        });
        connect(decoratedWindow()->decoratedWindow(), &KDecoration3::DecoratedWindow::sizeChanged, this, [this]() {
            if (!isDeleted()) {
                updateDecorationInputShape();
            }
        });
    }
    m_decoration.decoration = decoration;
    updateDecorationInputShape();
    Q_EMIT decorationChanged();
}

void Window::updateDecorationInputShape()
{
    if (!isDecorated()) {
        m_decoration.inputRegion = QRegion();
        return;
    }

    const QMarginsF borders = decoration()->borders();
    const QMarginsF resizeBorders = decoration()->resizeOnlyBorders();

    const QRectF innerRect = QRectF(QPointF(borderLeft(), borderTop()), decoratedWindow()->size());
    const QRectF outerRect = innerRect + borders + resizeBorders;

    m_decoration.inputRegion = QRegion(outerRect.toAlignedRect()) - innerRect.toAlignedRect();
}

bool Window::decorationHasAlpha() const
{
    if (!isDecorated() || decoration()->isOpaque()) {
        // either no decoration or decoration has alpha disabled
        return false;
    }
    return true;
}

void Window::triggerDecorationRepaint()
{
    if (isDecorated()) {
        decoration()->update();
    }
}

void Window::layoutDecorationRects(QRectF &left, QRectF &top, QRectF &right, QRectF &bottom) const
{
    if (!isDecorated()) {
        return;
    }
    QRectF r = decoration()->rect();

    top = QRectF(r.x(), r.y(), r.width(), borderTop());
    bottom = QRectF(r.x(), r.y() + r.height() - borderBottom(),
                    r.width(), borderBottom());
    left = QRectF(r.x(), r.y() + top.height(),
                  borderLeft(), r.height() - top.height() - bottom.height());
    right = QRectF(r.x() + r.width() - borderRight(), r.y() + top.height(),
                   borderRight(), r.height() - top.height() - bottom.height());
}

void Window::processDecorationMove(const QPointF &localPos, const QPointF &globalPos)
{
    if (isInteractiveMoveResizePointerButtonDown()) {
        if (!isInteractiveMoveResize()) {
            const QPointF offset(interactiveMoveOffset().x() * width(), interactiveMoveOffset().y() * height());
            const QPointF delta(localPos - offset);
            if (delta.manhattanLength() >= QApplication::startDragDistance()) {
                if (startInteractiveMoveResize()) {
                    updateInteractiveMoveResize(globalPos, input()->keyboardModifiers());
                } else {
                    setInteractiveMoveResizePointerButtonDown(false);
                }
                updateCursor();
            }
        }
        return;
    }

    // TODO: handle modifiers
    Gravity newGravity = mouseGravity();
    if (newGravity != interactiveMoveResizeGravity()) {
        setInteractiveMoveResizeGravity(newGravity);
        updateCursor();
    }
}

bool Window::processDecorationButtonPress(const QPointF &localPos, const QPointF &globalPos, Qt::MouseButton button, bool ignoreMenu)
{
    Options::MouseCommand com = Options::MouseNothing;
    bool active = isActive();
    if (!wantsInput()) { // we cannot be active, use it anyway
        active = true;
    }

    // check whether it is a double click
    if (button == Qt::LeftButton) {
        if (m_decoration.doubleClickTimer.isValid()) {
            const qint64 interval = m_decoration.doubleClickTimer.elapsed();
            m_decoration.doubleClickTimer.invalidate();
            if (interval > QGuiApplication::styleHints()->mouseDoubleClickInterval()) {
                m_decoration.doubleClickTimer.start(); // expired -> new first click and pot. init
            } else {
                Options::WindowOperation operation;
                switch (decoration()->sectionUnderMouse()) {
                case Qt::TitleBarArea:
                    operation = options->operationTitlebarDblClick();
                    break;
                case Qt::LeftSection:
                case Qt::RightSection:
                    operation = options->doubleClickBorderToMaximize() ? Options::HMaximizeOp : Options::NoOp;
                    break;
                case Qt::TopSection:
                case Qt::BottomSection:
                    operation = options->doubleClickBorderToMaximize() ? Options::VMaximizeOp : Options::NoOp;
                    break;
                case Qt::TopLeftSection:
                case Qt::TopRightSection:
                case Qt::BottomLeftSection:
                case Qt::BottomRightSection:
                    operation = options->doubleClickBorderToMaximize() ? Options::MaximizeOp : Options::NoOp;
                    break;
                default:
                    operation = Options::NoOp;
                    break;
                }
                workspace()->performWindowOperation(this, operation);
                dontInteractiveMoveResize();
                return false;
            }
        } else {
            m_decoration.doubleClickTimer.start(); // new first click and pot. init, could be invalidated by release - see below
        }
    }

    if (button == Qt::LeftButton) {
        com = active ? options->commandActiveTitlebar1() : options->commandInactiveTitlebar1();
    } else if (button == Qt::MiddleButton) {
        com = active ? options->commandActiveTitlebar2() : options->commandInactiveTitlebar2();
    } else if (button == Qt::RightButton) {
        com = active ? options->commandActiveTitlebar3() : options->commandInactiveTitlebar3();
    }
    if (button == Qt::LeftButton
        && com != Options::MouseOperationsMenu // actions where it's not possible to get the matching
        && com != Options::MouseMinimize) // mouse release event
    {
        setInteractiveMoveResizeGravity(mouseGravity());
        setInteractiveMoveResizePointerButtonDown(true);
        setInteractiveMoveResizeAnchor(globalPos);
        setInteractiveMoveResizeModifiers(Qt::KeyboardModifiers());
        setInteractiveMoveOffset(QPointF(qreal(localPos.x()) / width(), qreal(localPos.y()) / height()));
        setUnrestrictedInteractiveMoveResize(false);
        startDelayedInteractiveMoveResize();
        updateCursor();
    }
    // In the new API the decoration may process the menu action to display an inactive tab's menu.
    // If the event is unhandled then the core will create one for the active window in the group.
    if (!ignoreMenu || com != Options::MouseOperationsMenu) {
        performMousePressCommand(com, globalPos);
    }
    return !( // Return events that should be passed to the decoration in the new API
        com == Options::MouseRaise || com == Options::MouseOperationsMenu || com == Options::MouseActivateAndRaise || com == Options::MouseActivate || com == Options::MouseActivateRaiseAndPassClick || com == Options::MouseActivateAndPassClick || com == Options::MouseNothing);
}

void Window::processDecorationButtonRelease(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        setInteractiveMoveResizePointerButtonDown(false);
        stopDelayedInteractiveMoveResize();
        if (isInteractiveMoveResize()) {
            finishInteractiveMoveResize(false);
            setInteractiveMoveResizeGravity(mouseGravity());
        }
        updateCursor();
    }
}

void Window::startDecorationDoubleClickTimer()
{
    m_decoration.doubleClickTimer.start();
}

void Window::invalidateDecorationDoubleClickTimer()
{
    m_decoration.doubleClickTimer.invalidate();
}

bool Window::providesContextHelp() const
{
    return false;
}

void Window::showContextHelp()
{
}

KDecoration3::Decoration *Window::nextDecoration() const
{
    return decoration();
}

Decoration::DecoratedWindowImpl *Window::decoratedWindow() const
{
    return m_decoration.client;
}

void Window::setDecoratedWindow(Decoration::DecoratedWindowImpl *client)
{
    m_decoration.client = client;
}

void Window::pointerEnterEvent(const QPointF &globalPos)
{
    if (options->isShadeHover()) {
        cancelShadeHoverTimer();
        startShadeHoverTimer();
    }

    if (options->focusPolicy() == Options::ClickToFocus || workspace()->userActionsMenu()->isShown()) {
        return;
    }

    if (options->isAutoRaise() && !isDesktop() && !isDock() && workspace()->focusChangeEnabled() && globalPos != workspace()->focusMousePosition() && workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop(), options->isSeparateScreenFocus() ? output() : nullptr) != this) {
        startAutoRaise();
    }

    if (isDesktop() || isDock()) {
        return;
    }
    // for FocusFollowsMouse, change focus only if the mouse has actually been moved, not if the focus
    // change came because of window changes (e.g. closing a window) - #92290
    if (options->focusPolicy() != Options::FocusFollowsMouse
        || globalPos != workspace()->focusMousePosition()) {
        workspace()->requestDelayFocus(this);
    }
}

void Window::pointerLeaveEvent()
{
    cancelAutoRaise();
    workspace()->cancelDelayFocus();
    cancelShadeHoverTimer();
    startShadeUnhoverTimer();
    // TODO: send hover leave to deco
    // TODO: handle Options::FocusStrictlyUnderMouse
}

QRectF Window::iconGeometry() const
{
    if (!windowManagementInterface()) {
        return QRectF();
    }

    int minDistance = INT_MAX;
    Window *candidatePanel = nullptr;
    QRectF candidateGeom;

    const auto minGeometries = windowManagementInterface()->minimizedGeometries();
    for (auto i = minGeometries.constBegin(), end = minGeometries.constEnd(); i != end; ++i) {
        Window *panel = waylandServer()->findWindow(i.key());
        if (!panel) {
            continue;
        }
        const int distance = QPointF(panel->pos() - pos()).manhattanLength();
        if (distance < minDistance) {
            minDistance = distance;
            candidatePanel = panel;
            candidateGeom = i.value();
        }
    }
    if (!candidatePanel) {
        // Check all mainwindows of this window.
        const auto windows = mainWindows();
        for (Window *mainWindow : windows) {
            const auto geom = mainWindow->iconGeometry();
            if (geom.isValid()) {
                return geom;
            }
        }
        return QRectF();
    }
    return candidateGeom.translated(candidatePanel->pos());
}

QRectF Window::virtualKeyboardGeometry() const
{
    return m_virtualKeyboardGeometry;
}

void Window::setVirtualKeyboardGeometry(const QRectF &geo)
{
    // No keyboard anymore
    if (geo.isEmpty() && !m_keyboardGeometryRestore.isEmpty()) {
        const QRectF availableArea = workspace()->clientArea(MaximizeArea, this);
        QRectF newWindowGeometry = (requestedMaximizeMode() & MaximizeHorizontal) ? availableArea : m_keyboardGeometryRestore;
        moveResize(newWindowGeometry);
        m_keyboardGeometryRestore = QRectF();
    } else if (geo.isEmpty()) {
        return;
        // The keyboard has just been opened (rather than resized) save window geometry for a restore
    } else if (m_keyboardGeometryRestore.isEmpty()) {
        m_keyboardGeometryRestore = moveResizeGeometry();
    }

    m_virtualKeyboardGeometry = geo;

    // Don't resize Desktop and fullscreen windows
    if (isRequestedFullScreen() || isDesktop()) {
        return;
    }

    if (!geo.intersects(m_keyboardGeometryRestore)) {
        return;
    }

    const QRectF availableArea = workspace()->clientArea(MaximizeArea, this);
    QRectF newWindowGeometry = (requestedMaximizeMode() & MaximizeHorizontal) ? availableArea : m_keyboardGeometryRestore;
    newWindowGeometry.setHeight(std::min(newWindowGeometry.height(), geo.top() - availableArea.top()));
    newWindowGeometry.moveTop(std::max(geo.top() - newWindowGeometry.height(), availableArea.top()));
    newWindowGeometry = newWindowGeometry.intersected(availableArea);
    moveResize(newWindowGeometry);
}

QRectF Window::keyboardGeometryRestore() const
{
    return m_keyboardGeometryRestore;
}

void Window::setKeyboardGeometryRestore(const QRectF &geom)
{
    m_keyboardGeometryRestore = geom;
}

void Window::setDesktopFileName(const QString &name)
{
    const QString effectiveName = rules()->checkDesktopFile(name);
    if (effectiveName == m_desktopFileName) {
        return;
    }
    m_desktopFileName = effectiveName;
    updateWindowRules(Rules::DesktopFile);
    Q_EMIT desktopFileNameChanged();
}

QString Window::iconFromDesktopFile(const QString &desktopFileName)
{
    const QString absolutePath = findDesktopFile(desktopFileName);
    if (absolutePath.isEmpty()) {
        return {};
    }

    KDesktopFile df(absolutePath);
    return df.readIcon();
}

QString Window::iconFromDesktopFile() const
{
    return iconFromDesktopFile(m_desktopFileName);
}

QString Window::findDesktopFile(const QString &desktopFileName)
{
    if (desktopFileName.isEmpty()) {
        return {};
    }

    const QLatin1StringView suffix(".desktop");
    const QString desktopFileNameWithPrefix = desktopFileName + suffix;

    if (QDir::isAbsolutePath(desktopFileName)) {
        if (QFile::exists(desktopFileNameWithPrefix)) {
            return desktopFileNameWithPrefix;
        }

        if (desktopFileName.endsWith(suffix)) {
            if (QFile::exists(desktopFileName)) {
                return desktopFileName;
            }
        }

        return QString();
    }

    const QString filePath = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopFileNameWithPrefix);
    if (!filePath.isEmpty()) {
        return filePath;
    }

    if (desktopFileName.endsWith(suffix)) {
        return QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopFileName);
    }

    return QString();
}

bool Window::hasApplicationMenu() const
{
    return Workspace::self()->applicationMenu()->applicationMenuEnabled() && !m_applicationMenuServiceName.isEmpty() && !m_applicationMenuObjectPath.isEmpty();
}

void Window::updateApplicationMenuServiceName(const QString &serviceName)
{
    const bool old_hasApplicationMenu = hasApplicationMenu();

    m_applicationMenuServiceName = serviceName;

    const bool new_hasApplicationMenu = hasApplicationMenu();

    Q_EMIT applicationMenuChanged();
    if (old_hasApplicationMenu != new_hasApplicationMenu) {
        Q_EMIT hasApplicationMenuChanged(new_hasApplicationMenu);
    }
}

void Window::updateApplicationMenuObjectPath(const QString &objectPath)
{
    const bool old_hasApplicationMenu = hasApplicationMenu();

    m_applicationMenuObjectPath = objectPath;

    const bool new_hasApplicationMenu = hasApplicationMenu();

    Q_EMIT applicationMenuChanged();
    if (old_hasApplicationMenu != new_hasApplicationMenu) {
        Q_EMIT hasApplicationMenuChanged(new_hasApplicationMenu);
    }
}

void Window::setApplicationMenuActive(bool applicationMenuActive)
{
    if (m_applicationMenuActive != applicationMenuActive) {
        m_applicationMenuActive = applicationMenuActive;
        Q_EMIT applicationMenuActiveChanged(applicationMenuActive);
    }
}

void Window::showApplicationMenu(int actionId)
{
    if (isDecorated()) {
        decoration()->showApplicationMenu(actionId);
    } else {
        // we don't know where the application menu button will be, show it in the top left corner instead
        Workspace::self()->showApplicationMenu(QRect(), this, actionId);
    }
}

bool Window::unresponsive() const
{
    return m_unresponsive;
}

void Window::setUnresponsive(bool unresponsive)
{
    if (m_unresponsive != unresponsive) {
        m_unresponsive = unresponsive;
        Q_EMIT unresponsiveChanged(m_unresponsive);
        Q_EMIT captionChanged();
    }
}

QString Window::shortcutCaptionSuffix() const
{
    if (shortcut().isEmpty()) {
        return QString();
    }
    return QLatin1String(" {") + shortcut().toString() + QLatin1Char('}');
}

QString Window::caption() const
{
    QString cap = captionNormal() + captionSuffix();
    if (unresponsive()) {
        cap += QLatin1String(" ");
        cap += i18nc("Application is not responding, appended to window title", "(Not Responding)");
    }
    return cap;
}

/**
 * Returns the list of activities the window window is on.
 * if it's on all activities, the list will be empty.
 * Don't use this, use isOnActivity() and friends (from class Window)
 */
QStringList Window::activities() const
{
    return m_activityList;
}

bool Window::isOnCurrentActivity() const
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return true;
    }
    return isOnActivity(Workspace::self()->activities()->current());
#else
    return true;
#endif
}

/**
 * Sets whether the window is on @p activity.
 * If you remove it from its last activity, then it's on all activities.
 *
 * Note: If it was on all activities and you try to remove it from one, nothing will happen;
 * I don't think that's an important enough use case to handle here.
 */
void Window::setOnActivity(const QString &activity, bool enable)
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return;
    }
    QStringList newActivitiesList = activities();
    if (newActivitiesList.contains(activity) == enable) {
        // nothing to do
        return;
    }
    if (enable) {
        QStringList allActivities = Workspace::self()->activities()->all();
        if (!allActivities.contains(activity)) {
            // bogus ID
            return;
        }
        newActivitiesList.append(activity);
    } else {
        newActivitiesList.removeOne(activity);
    }
    setOnActivities(newActivitiesList);
#endif
}

/**
 * set exactly which activities this window is on
 */
void Window::setOnActivities(const QStringList &newActivitiesList)
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return;
    }
    if (Workspace::self()->activities()->serviceStatus() != KActivities::Consumer::Running) {
        return;
    }
    const auto allActivities = Workspace::self()->activities()->all();
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
#endif
}

void Window::doSetOnActivities(const QStringList &activityList)
{
}

/**
 * if @p all is true, sets on all activities.
 * if it's false, sets it to only be on the current activity
 */
void Window::setOnAllActivities(bool all)
{
#if KWIN_BUILD_ACTIVITIES
    if (all == isOnAllActivities()) {
        return;
    }
    if (all) {
        setOnActivities(QStringList());
    } else {
        setOnActivity(Workspace::self()->activities()->current(), true);
    }
#endif
}

/**
 * update after activities changed
 */
void Window::updateActivities(bool includeTransients)
{
    if (m_activityUpdatesBlocked) {
        m_blockedActivityUpdatesRequireTransients |= includeTransients;
        return;
    }
    Q_EMIT activitiesChanged();
    m_blockedActivityUpdatesRequireTransients = false; // reset
    Workspace::self()->focusChain()->update(this, FocusChain::MakeFirst);
    updateWindowRules(Rules::Activity);
}

void Window::blockActivityUpdates(bool b)
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

bool Window::groupTransient() const
{
    return false;
}

const Group *Window::group() const
{
    return nullptr;
}

Group *Window::group()
{
    return nullptr;
}

QPointF Window::framePosToClientPos(const QPointF &point) const
{
    QMarginsF borders;
    if (decoration()) {
        borders = decoration()->currentState()->borders();
    }
    return point + QPointF(borders.left(), borders.top());
}

QPointF Window::nextFramePosToClientPos(const QPointF &point) const
{
    QMarginsF borders;
    if (auto decoration = nextDecoration()) {
        borders = decoration->nextState()->borders();
    }
    return point + QPointF(borders.left(), borders.top());
}

QPointF Window::clientPosToFramePos(const QPointF &point) const
{
    QMarginsF borders;
    if (decoration()) {
        borders = decoration()->currentState()->borders();
    }
    return point - QPointF(borders.left(), borders.top());
}

QPointF Window::nextClientPosToFramePos(const QPointF &point) const
{
    QMarginsF borders;
    if (auto decoration = nextDecoration()) {
        borders = decoration->nextState()->borders();
    }
    return point - QPointF(borders.left(), borders.top());
}

QSizeF Window::frameSizeToClientSize(const QSizeF &size) const
{
    QMarginsF borders;
    if (decoration()) {
        borders = decoration()->currentState()->borders();
    }
    return size.shrunkBy(borders);
}

QSizeF Window::nextFrameSizeToClientSize(const QSizeF &size) const
{
    QMarginsF borders;
    if (auto decoration = nextDecoration()) {
        borders = decoration->nextState()->borders();
    }
    return size.shrunkBy(borders);
}

QSizeF Window::clientSizeToFrameSize(const QSizeF &size) const
{
    QMarginsF borders;
    if (decoration()) {
        borders = decoration()->currentState()->borders();
    }
    return size.grownBy(borders);
}

QSizeF Window::nextClientSizeToFrameSize(const QSizeF &size) const
{
    QMarginsF borders;
    if (auto decoration = nextDecoration()) {
        borders = decoration->nextState()->borders();
    }
    return size.grownBy(borders);
}

QRectF Window::frameRectToClientRect(const QRectF &rect) const
{
    return QRectF(framePosToClientPos(rect.topLeft()),
                  frameSizeToClientSize(rect.size()));
}

QRectF Window::nextFrameRectToClientRect(const QRectF &rect) const
{
    return QRectF(nextFramePosToClientPos(rect.topLeft()),
                  nextFrameSizeToClientSize(rect.size()));
}

QRectF Window::clientRectToFrameRect(const QRectF &rect) const
{
    return QRectF(clientPosToFramePos(rect.topLeft()),
                  clientSizeToFrameSize(rect.size()));
}

QRectF Window::nextClientRectToFrameRect(const QRectF &rect) const
{
    return QRectF(nextClientPosToFramePos(rect.topLeft()),
                  nextClientSizeToFrameSize(rect.size()));
}

QRectF Window::moveResizeGeometry() const
{
    return m_moveResizeGeometry;
}

void Window::setMoveResizeGeometry(const QRectF &geo)
{
    m_moveResizeGeometry = geo;
    setMoveResizeOutput(workspace()->outputAt(geo.center()));
}

Output *Window::moveResizeOutput() const
{
    return m_moveResizeOutput;
}

void Window::setMoveResizeOutput(Output *output)
{
    if (m_moveResizeOutput == output) {
        return;
    }

    if (m_moveResizeOutput) {
        disconnect(m_moveResizeOutput, &Output::scaleChanged, this, &Window::updateNextTargetScale);
        disconnect(m_moveResizeOutput, &Output::transformChanged, this, &Window::updatePreferredBufferTransform);
        disconnect(m_moveResizeOutput, &Output::blendingColorChanged, this, &Window::updatePreferredColorDescription);
    }

    m_moveResizeOutput = output;
    if (output) {
        connect(output, &Output::scaleChanged, this, &Window::updateNextTargetScale);
        connect(output, &Output::transformChanged, this, &Window::updatePreferredBufferTransform);
        connect(output, &Output::blendingColorChanged, this, &Window::updatePreferredColorDescription);
    }

    updateNextTargetScale();
    updatePreferredBufferTransform();
    updatePreferredColorDescription();
}

void Window::move(const QPointF &point)
{
    if (isDeleted()) {
        return;
    }

    const QRectF rect = QRectF(point, m_moveResizeGeometry.size());

    setMoveResizeGeometry(rect);
    moveResizeInternal(rect, MoveResizeMode::Move);
}

void Window::resize(const QSizeF &size)
{
    if (isDeleted()) {
        return;
    }

    const QRectF rect = QRectF(m_moveResizeGeometry.topLeft(), size);

    setMoveResizeGeometry(rect);
    moveResizeInternal(rect, MoveResizeMode::Resize);
}

void Window::moveResize(const QRectF &rect)
{
    if (isDeleted()) {
        return;
    }

    setMoveResizeGeometry(rect);
    moveResizeInternal(rect, MoveResizeMode::MoveResize);
}

bool Window::isPlaced() const
{
    return m_placed;
}

void Window::place(const PlacementCommand &placement)
{
    if (auto position = std::get_if<QPointF>(&placement)) {
        move(*position);
    } else if (auto rect = std::get_if<QRectF>(&placement)) {
        moveResize(*rect);
    } else if (auto maximizeMode = std::get_if<MaximizeMode>(&placement)) {
        maximize(*maximizeMode);
    }
    markAsPlaced();
}

void Window::markAsPlaced()
{
    m_placed = true;
}

void Window::setElectricBorderMode(std::optional<ElectricBorderMode> mode)
{
    m_electricMode = mode;
}

void Window::setElectricBorderMaximizing(bool maximizing)
{
    m_electricMaximizing = maximizing;
    if (maximizing) {
        if (auto quickTileMode = std::get_if<QuickTileMode>(&m_electricMode.value())) {
            workspace()->outline()->show(quickTileGeometry(*quickTileMode, interactiveMoveResizeAnchor()).toRect(), moveResizeGeometry().toRect());
        } else if (std::get_if<MaximizeMode>(&m_electricMode.value())) {
            workspace()->outline()->show(workspace()->clientArea(MaximizeArea, this, interactiveMoveResizeAnchor()).toRect(), moveResizeGeometry().toRect());
        }
    } else {
        workspace()->outline()->hide();
    }
}

QRectF Window::quickTileGeometry(QuickTileMode mode, const QPointF &pos) const
{
    Output *output = workspace()->outputAt(pos);

    if (mode & QuickTileFlag::Custom) {
        Tile *tile = workspace()->rootTile(output)->pick(pos);
        if (tile) {
            return tile->windowGeometry();
        } else {
            return QRectF();
        }
    }

    Tile *tile = workspace()->tileManager(output)->quickTile(mode);
    if (tile) {
        return tile->windowGeometry();
    }
    return workspace()->clientArea(MaximizeArea, this, pos);
}

void Window::exitQuickTileMode()
{
    const auto outputs = workspace()->outputs();
    for (Output *output : outputs) {
        workspace()->tileManager(output)->forgetWindow(this, nullptr);
    }
}

void Window::updateElectricGeometryRestore()
{
    m_electricGeometryRestore = geometryRestore();
    if (m_interactiveMoveResize.initialQuickTileMode == QuickTileMode(QuickTileFlag::None)) {
        if (!(requestedMaximizeMode() & MaximizeHorizontal)) {
            m_electricGeometryRestore.setX(x());
            m_electricGeometryRestore.setWidth(width());
        }
        if (!(requestedMaximizeMode() & MaximizeVertical)) {
            m_electricGeometryRestore.setY(y());
            m_electricGeometryRestore.setHeight(height());
        }
    }
}

QRectF Window::quickTileGeometryRestore() const
{
    if (requestedQuickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        // If the window is tiled, geometryRestore() already has a good value.
        return geometryRestore();
    }

    if (isElectricBorderMaximizing()) {
        return m_electricGeometryRestore;
    } else {
        return moveResizeGeometry();
    }
}

static QuickTileMode combineQuickTileMode(QuickTileMode mode, QuickTileMode other)
{
    const bool isHorizontal = (other & (QuickTileFlag::Left | QuickTileFlag::Right)) == QuickTileFlag::None;
    const bool isVertical = (other & (QuickTileFlag::Top | QuickTileFlag::Bottom)) == QuickTileFlag::None;
    if (isHorizontal != isVertical) {
        if (mode == QuickTileFlag::Left) {
            return (other & ~QuickTileFlag::Right) | QuickTileFlag::Left;
        } else if (mode == QuickTileFlag::Right) {
            return (other & ~QuickTileFlag::Left) | QuickTileFlag::Right;
        } else if (mode == QuickTileFlag::Top) {
            return (other & ~QuickTileFlag::Bottom) | QuickTileFlag::Top;
        } else if (mode == QuickTileFlag::Bottom) {
            return (other & ~QuickTileFlag::Top) | QuickTileFlag::Bottom;
        }
    }

    QuickTileMode combined = other;

    if (mode & QuickTileFlag::Left) {
        if (other & QuickTileFlag::Right) {
            combined &= ~QuickTileFlag::Right;
        } else {
            combined |= QuickTileFlag::Left;
        }
    }
    if (mode & QuickTileFlag::Right) {
        if (other & QuickTileFlag::Left) {
            combined &= ~QuickTileFlag::Left;
        } else {
            combined |= QuickTileFlag::Right;
        }
    }
    if (mode & QuickTileFlag::Top) {
        if (other & QuickTileFlag::Bottom) {
            combined &= ~QuickTileFlag::Bottom;
        } else {
            combined |= QuickTileFlag::Top;
        }
    }
    if (mode & QuickTileFlag::Bottom) {
        if (other & QuickTileFlag::Top) {
            combined &= ~QuickTileFlag::Top;
        } else {
            combined |= QuickTileFlag::Bottom;
        }
    }

    return combined;
}

void Window::handleQuickTileShortcut(QuickTileMode mode)
{
    QPointF tileAtPoint = moveResizeGeometry().center();
    if (mode != QuickTileFlag::None) {
        const QuickTileMode oldMode = requestedQuickTileMode();
        if (oldMode == QuickTileMode(QuickTileFlag::None) && requestedMaximizeMode() == MaximizeMode::MaximizeRestore) {
            // Not coming out of an existing tile, not shifting monitors, we're setting a brand new tile.
            // Store geometry first, so we can go out of this tile later.
            setGeometryRestore(quickTileGeometryRestore());
        } else {
            // If the window is asked to be tiled in a screen corner, don't combine the new mode with the old one.
            QuickTileMode combined;
            switch (mode) {
            case QuickTileMode(QuickTileFlag::Left):
            case QuickTileMode(QuickTileFlag::Top):
            case QuickTileMode(QuickTileFlag::Right):
            case QuickTileMode(QuickTileFlag::Bottom):
                combined = combineQuickTileMode(mode, oldMode);
                break;
            default:
                combined = mode;
            }

            // If trying to tile to the side that the window is already tiled to move the window to the next
            // screen near the tile if it exists and swap the tile side, otherwise toggle the mode (set QuickTileFlag::None)
            if (combined == oldMode) {
                Output *currentOutput = moveResizeOutput();
                Output *nextOutput = currentOutput;
                Output *candidateOutput = currentOutput;
                if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Left)) {
                    candidateOutput = workspace()->findOutput(nextOutput, Workspace::DirectionWest);
                } else if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Right)) {
                    candidateOutput = workspace()->findOutput(nextOutput, Workspace::DirectionEast);
                }
                bool shiftHorizontal = candidateOutput != nextOutput;
                nextOutput = candidateOutput;
                if ((mode & QuickTileFlag::Vertical) == QuickTileMode(QuickTileFlag::Top)) {
                    candidateOutput = workspace()->findOutput(nextOutput, Workspace::DirectionNorth);
                } else if ((mode & QuickTileFlag::Vertical) == QuickTileMode(QuickTileFlag::Bottom)) {
                    candidateOutput = workspace()->findOutput(nextOutput, Workspace::DirectionSouth);
                }
                bool shiftVertical = candidateOutput != nextOutput;
                nextOutput = candidateOutput;

                if (nextOutput != currentOutput) {
                    // Move to other screen
                    tileAtPoint = nextOutput->geometry().center();

                    // Swap sides
                    if (shiftHorizontal) {
                        combined = (~combined & QuickTileFlag::Horizontal) | (combined & QuickTileFlag::Vertical);
                    }
                    if (shiftVertical) {
                        combined = (~combined & QuickTileFlag::Vertical) | (combined & QuickTileFlag::Horizontal);
                    }
                }
            }

            mode = combined;
        }
    }
    setQuickTileMode(mode, tileAtPoint);
}

void Window::handleCustomQuickTileShortcut(QuickTileMode mode)
{
    if (mode == QuickTileFlag::None) {
        return;
    }
    // if window is not tiled already, set it to nearest one
    Tile *tileAtPoint = workspace()->rootTile(workspace()->outputAt(moveResizeGeometry().center()))->pick(moveResizeGeometry().center());
    if (!tileAtPoint) {
        return;
    }
    if (tileAtPoint != m_requestedTile) {
        tileAtPoint->manage(this);
        return;
    }
    const auto customTile = qobject_cast<CustomTile *>(tileAtPoint);
    if (!customTile) {
        return;
    }
    // Get the next tile that is not a layout tile from the edge
    Qt::Edge edge;
    if (mode & QuickTileFlag::Left) {
        edge = Qt::LeftEdge;
    }
    if (mode & QuickTileFlag::Right) {
        edge = Qt::RightEdge;
    }
    if (mode & QuickTileFlag::Top) {
        edge = Qt::TopEdge;
    }
    if (mode & QuickTileFlag::Bottom) {
        edge = Qt::BottomEdge;
    }
    CustomTile *next = customTile->nextNonLayoutTileAt(edge);
    if (next) {
        next->manage(this);
    }
}

void Window::setQuickTileModeAtCurrentPosition(QuickTileMode mode)
{
    setQuickTileMode(mode, m_moveResizeGeometry.center());
}

void Window::setQuickTileMode(QuickTileMode mode, const QPointF &tileAtPoint)
{
    workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event

    // sanitize the mode, ie. simplify "invalid" combinations
    if ((mode & QuickTileFlag::Horizontal) == QuickTileMode(QuickTileFlag::Horizontal)) {
        mode &= ~QuickTileMode(QuickTileFlag::Horizontal);
    }
    if ((mode & QuickTileFlag::Vertical) == QuickTileMode(QuickTileFlag::Vertical)) {
        mode &= ~QuickTileMode(QuickTileFlag::Vertical);
    }

    Tile *tile = nullptr;
    if (mode == QuickTileMode(QuickTileFlag::Custom)) {
        tile = workspace()->rootTile(workspace()->outputAt(tileAtPoint))->pick(tileAtPoint);
    } else {
        tile = workspace()->tileManager(workspace()->outputAt(tileAtPoint))->quickTile(mode);
    }

    if (m_requestedTile != tile) {
        if (tile) {
            tile->manage(this);
        } else if (m_requestedTile && m_requestedTile->isActive()) {
            m_requestedTile->unmanage(this);
        }
    }
}

QuickTileMode Window::quickTileMode() const
{
    if (m_tile) {
        return m_tile->quickTileMode();
    } else {
        return QuickTileFlag::None;
    }
}

QuickTileMode Window::requestedQuickTileMode() const
{
    if (m_requestedTile) {
        return m_requestedTile->quickTileMode();
    }

    return QuickTileFlag::None;
}

void Window::commitTile(Tile *tile)
{
    if (m_tile == tile) {
        return;
    }

    QuickTileMode oldTileMode = quickTileMode();

    m_tile = tile;

    Q_EMIT tileChanged(tile);

    if (oldTileMode != quickTileMode()) {
        Q_EMIT quickTileModeChanged();
    }
}

Tile *Window::tile() const
{
    return m_tile;
}

Tile *Window::requestedTile() const
{
    return m_requestedTile;
}

void Window::requestTile(Tile *tile)
{
    if (isDeleted()) {
        return;
    }

    if (m_requestedTile == tile) {
        return;
    }

    if (tile && requestedMaximizeMode() != MaximizeRestore) {
        setMaximize(false, false);
        if (requestedMaximizeMode() != MaximizeRestore) {
            // window rules may enforce a different maximize mode, we can't do anything here
            return;
        }
    }

    if (tile && !m_requestedTile && !m_tile) {
        setGeometryRestore(quickTileGeometryRestore());
    }

    m_requestedTile = tile;
    if (tile) {
        moveResize(tile->windowGeometry());
    } else {
        QRectF geometry = moveResizeGeometry();
        if (geometryRestore().isValid()) {
            geometry = geometryRestore();
        }
        if (isInteractiveMove()) {
            const QPointF anchor = interactiveMoveResizeAnchor();
            const QPointF offset = interactiveMoveOffset();
            geometry.moveTopLeft(QPointF(anchor.x() - geometry.width() * offset.x(),
                                         anchor.y() - geometry.height() * offset.y()));
        }
        moveResize(geometry);
    }
    doSetQuickTileMode();
    Q_EMIT requestedTileChanged();
}

void Window::forgetTile(Tile *tile)
{
    if (m_requestedTile != tile) {
        return;
    }

    m_requestedTile = nullptr;
    doSetQuickTileMode();
    Q_EMIT requestedTileChanged();
}

void Window::setTileCompatibility(Tile *tile)
{
    qCWarning(KWIN_CORE) << "Writing to the property window.tile is deprecated: use tile.manage() instead";

    if (m_requestedTile == tile) {
        return;
    }

    Tile *previousTile = m_requestedTile;
    if (tile) {
        tile->manage(this);
    }

    if (previousTile) {
        previousTile->unmanage(this);
    }
}

void Window::doSetQuickTileMode()
{
}

void Window::doSetHidden()
{
}

void Window::doSetHiddenByShowDesktop()
{
}

QRectF Window::moveToArea(const QRectF &geometry, const QRectF &oldArea, const QRectF &newArea)
{
    QRectF ret = geometry;
    // move the window to have the same relative position to the center of the screen
    // (i.e. one near the middle of the right edge will also end up near the middle of the right edge)
    QPointF center = geometry.center() - oldArea.center();
    center.setX(center.x() * newArea.width() / oldArea.width());
    center.setY(center.y() * newArea.height() / oldArea.height());
    center += newArea.center();
    ret.moveCenter(center);

    // If the window was inside the old screen area, explicitly make sure its inside also the new screen area
    if (oldArea.contains(geometry)) {
        ret = keepInArea(ret, newArea);
    }
    return ret;
}

QRectF Window::ensureSpecialStateGeometry(const QRectF &geometry)
{
    if (isRequestedFullScreen()) {
        return workspace()->clientArea(FullScreenArea, this, geometry.center());
    } else if (requestedMaximizeMode() != MaximizeRestore) {
        const QRectF maximizeArea = workspace()->clientArea(MaximizeArea, this, geometry.center());
        QRectF ret = geometry;
        if (requestedMaximizeMode() & MaximizeHorizontal) {
            ret.setX(maximizeArea.x());
            ret.setWidth(maximizeArea.width());
        }
        if (requestedMaximizeMode() & MaximizeVertical) {
            ret.setY(maximizeArea.y());
            ret.setHeight(maximizeArea.height());
        }
        return keepInArea(ret, maximizeArea, false);
    } else if (requestedTile()) {
        return requestedTile()->windowGeometry();
    } else {
        return geometry;
    }
}

void Window::sendToOutput(Output *newOutput)
{
    newOutput = rules()->checkOutput(newOutput);
    if (isActive()) {
        workspace()->setActiveOutput(newOutput);
        // might impact the layer of a fullscreen window
        const auto windows = workspace()->windows();
        for (Window *other : windows) {
            if (other->isFullScreen() && other->output() == newOutput) {
                other->updateLayer();
            }
        }
    }
    if (moveResizeOutput() == newOutput) {
        return;
    }

    const QRectF oldGeom = moveResizeGeometry();
    const QRectF oldScreenArea = workspace()->clientArea(MaximizeArea, this, moveResizeOutput());
    const QRectF screenArea = workspace()->clientArea(MaximizeArea, this, newOutput);

    if (requestedQuickTileMode() == QuickTileMode(QuickTileFlag::Custom)) {
        workspace()->tileManager(moveResizeOutput())->forgetWindow(this, nullptr);
    } else {
        Tile *newTile = workspace()->tileManager(newOutput)->quickTile(requestedQuickTileMode());
        if (newTile) {
            newTile->manage(this);
        }
    }

    QRectF newGeom = moveToArea(oldGeom, oldScreenArea, screenArea);
    newGeom = ensureSpecialStateGeometry(newGeom);
    moveResize(newGeom);

    // move geometry restores to the new output as well
    setFullscreenGeometryRestore(moveToArea(m_fullscreenGeometryRestore, oldScreenArea, screenArea));
    setGeometryRestore(moveToArea(m_maximizeGeometryRestore, oldScreenArea, screenArea));

    auto tso = workspace()->ensureStackingOrder(transients());
    for (auto it = tso.constBegin(), end = tso.constEnd(); it != end; ++it) {
        (*it)->sendToOutput(newOutput);
    }
}

void Window::checkWorkspacePosition(QRectF oldGeometry, const VirtualDesktop *oldDesktop)
{
    if (isDeleted()) {
        qCWarning(KWIN_CORE) << "Window::checkWorkspacePosition: called for a closed window. Consider this a bug";
        return;
    }
    if (isDock() || isDesktop() || !isPlaceable()) {
        return;
    }

    QRectF newGeom = moveResizeGeometry();

    if (!oldGeometry.isValid()) {
        oldGeometry = newGeom;
    }

    VirtualDesktop *desktop = !isOnCurrentDesktop() ? desktops().constLast() : VirtualDesktopManager::self()->currentDesktop();
    if (!oldDesktop) {
        oldDesktop = desktop;
    }

    // If the window was touching an edge before but not now move it so it is again.
    // Old and new maximums have different starting values so windows on the screen
    // edge will move when a new strut is placed on the edge.
    QRect oldScreenArea;
    QRect screenArea;
    if (workspace()->inRearrange()) {
        // check if the window is on an about to be destroyed output
        Output *newOutput = moveResizeOutput();
        if (!workspace()->outputs().contains(newOutput)) {
            newOutput = workspace()->outputAt(newGeom.center());
        }
        // we need to find the screen area as it was before the change
        oldScreenArea = workspace()->previousScreenSizes().value(moveResizeOutput());
        if (oldScreenArea.isNull()) {
            oldScreenArea = newOutput->geometry();
        }
        screenArea = newOutput->geometry();
        newGeom.translate(screenArea.topLeft() - oldScreenArea.topLeft());
    } else {
        oldScreenArea = workspace()->clientArea(ScreenArea, workspace()->outputAt(oldGeometry.center()), oldDesktop).toRect();
        screenArea = workspace()->clientArea(ScreenArea, this, newGeom.center()).toRect();
    }

    if (isRequestedFullScreen() || requestedMaximizeMode() != MaximizeRestore || requestedQuickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        moveResize(ensureSpecialStateGeometry(newGeom));
        setFullscreenGeometryRestore(moveToArea(m_fullscreenGeometryRestore, oldScreenArea, screenArea));
        setGeometryRestore(moveToArea(m_maximizeGeometryRestore, oldScreenArea, screenArea));
        return;
    }

    const QRect oldGeomTall = QRect(oldGeometry.x(), oldScreenArea.y(), oldGeometry.width(), oldScreenArea.height()); // Full screen height
    const QRect oldGeomWide = QRect(oldScreenArea.x(), oldGeometry.y(), oldScreenArea.width(), oldGeometry.height()); // Full screen width
    int oldTopMax = oldScreenArea.y();
    int oldRightMax = oldScreenArea.x() + oldScreenArea.width();
    int oldBottomMax = oldScreenArea.y() + oldScreenArea.height();
    int oldLeftMax = oldScreenArea.x();
    int topMax = screenArea.y();
    int rightMax = screenArea.x() + screenArea.width();
    int bottomMax = screenArea.y() + screenArea.height();
    int leftMax = screenArea.x();
    const QRect newGeomTall = QRect(newGeom.x(), screenArea.y(), newGeom.width(), screenArea.height()); // Full screen height
    const QRect newGeomWide = QRect(screenArea.x(), newGeom.y(), screenArea.width(), newGeom.height()); // Full screen width
    // Get the max strut point for each side where the window is (E.g. Highest point for
    // the bottom struts bounded by the window's left and right sides).

    // These 4 compute old bounds ...
    auto moveAreaFunc = workspace()->inRearrange() ? &Workspace::previousRestrictedMoveArea : //... the restricted areas changed
        &Workspace::restrictedMoveArea; //... when e.g. active desktop or screen changes

    const auto oldStrutsTop = (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaTop);
    for (const QRect &r : oldStrutsTop) {
        QRect rect = r & oldGeomTall;
        if (!rect.isEmpty()) {
            oldTopMax = std::max(oldTopMax, rect.y() + rect.height());
        }
    }
    const auto oldStrutsRight = (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaRight);
    for (const QRect &r : oldStrutsRight) {
        QRect rect = r & oldGeomWide;
        if (!rect.isEmpty()) {
            oldRightMax = std::min(oldRightMax, rect.x());
        }
    }
    const auto oldStrutsBottom = (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaBottom);
    for (const QRect &r : oldStrutsBottom) {
        QRect rect = r & oldGeomTall;
        if (!rect.isEmpty()) {
            oldBottomMax = std::min(oldBottomMax, rect.y());
        }
    }
    const auto oldStrutsLeft = (workspace()->*moveAreaFunc)(oldDesktop, StrutAreaLeft);
    for (const QRect &r : oldStrutsLeft) {
        QRect rect = r & oldGeomWide;
        if (!rect.isEmpty()) {
            oldLeftMax = std::max(oldLeftMax, rect.x() + rect.width());
        }
    }

    // These 4 compute new bounds
    const auto newStrutsTop = workspace()->restrictedMoveArea(desktop, StrutAreaTop);
    for (const QRect &r : newStrutsTop) {
        QRect rect = r & newGeomTall;
        if (!rect.isEmpty()) {
            topMax = std::max(topMax, rect.y() + rect.height());
        }
    }
    const auto newStrutsRight = workspace()->restrictedMoveArea(desktop, StrutAreaRight);
    for (const QRect &r : newStrutsRight) {
        QRect rect = r & newGeomWide;
        if (!rect.isEmpty()) {
            rightMax = std::min(rightMax, rect.x());
        }
    }
    const auto newStrutsBottom = workspace()->restrictedMoveArea(desktop, StrutAreaBottom);
    for (const QRect &r : newStrutsBottom) {
        QRect rect = r & newGeomTall;
        if (!rect.isEmpty()) {
            bottomMax = std::min(bottomMax, rect.y());
        }
    }
    const auto newStrutsLeft = workspace()->restrictedMoveArea(desktop, StrutAreaLeft);
    for (const QRect &r : newStrutsLeft) {
        QRect rect = r & newGeomWide;
        if (!rect.isEmpty()) {
            leftMax = std::max(leftMax, rect.x() + rect.width());
        }
    }

    // Check if the sides were inside or touching but are no longer
    enum {
        Left = 0,
        Top,
        Right,
        Bottom,
    };
    bool keep[4] = {false, false, false, false};
    bool save[4] = {false, false, false, false};
    if (oldGeometry.x() >= oldLeftMax) {
        save[Left] = newGeom.x() < leftMax;
    }
    if (oldGeometry.x() == oldLeftMax) {
        keep[Left] = newGeom.x() != leftMax;
    }

    if (oldGeometry.y() >= oldTopMax) {
        save[Top] = newGeom.y() < topMax;
    }
    if (oldGeometry.y() == oldTopMax) {
        keep[Top] = newGeom.y() != topMax;
    }

    if (oldGeometry.right() <= oldRightMax) {
        save[Right] = newGeom.right() > rightMax;
    }
    if (oldGeometry.right() == oldRightMax) {
        keep[Right] = newGeom.right() != rightMax;
    }

    if (oldGeometry.bottom() <= oldBottomMax) {
        save[Bottom] = newGeom.bottom() > bottomMax;
    }
    if (oldGeometry.bottom() == oldBottomMax) {
        keep[Bottom] = newGeom.bottom() != bottomMax;
    }

    // if randomly touches opposing edges, do not favor either
    if (keep[Left] && keep[Right]) {
        keep[Left] = keep[Right] = false;
    }
    if (keep[Top] && keep[Bottom]) {
        keep[Top] = keep[Bottom] = false;
    }

    if (save[Left] || keep[Left]) {
        newGeom.moveLeft(std::max(leftMax, screenArea.x()));
    }
    if (save[Top] || keep[Top]) {
        newGeom.moveTop(std::max(topMax, screenArea.y()));
    }
    if (save[Right] || keep[Right]) {
        newGeom.moveRight(std::min(rightMax, screenArea.right()) + 1);
    }
    if (save[Bottom] || keep[Bottom]) {
        newGeom.moveBottom(std::min(bottomMax, screenArea.bottom()) + 1);
    }

    if (oldGeometry.x() >= oldLeftMax && newGeom.x() < leftMax) {
        newGeom.setLeft(std::max(leftMax, screenArea.x()));
    }
    if (oldGeometry.y() >= oldTopMax && newGeom.y() < topMax) {
        newGeom.setTop(std::max(topMax, screenArea.y()));
    }

    checkOffscreenPosition(&newGeom, screenArea);
    // Obey size hints. TODO: We really should make sure it stays in the right place
    if (!isShade()) {
        newGeom.setSize(constrainFrameSize(newGeom.size()));
    }

    moveResize(m_rules.checkGeometry(newGeom));
}

void Window::checkOffscreenPosition(QRectF *geom, const QRectF &screenArea)
{
    if (geom->left() > screenArea.right()) {
        geom->moveLeft(screenArea.right() - screenArea.width() / 4);
    } else if (geom->right() < screenArea.left()) {
        geom->moveRight(screenArea.left() + screenArea.width() / 4);
    }
    if (geom->top() > screenArea.bottom()) {
        geom->moveTop(screenArea.bottom() - screenArea.height() / 4);
    } else if (geom->bottom() < screenArea.top()) {
        geom->moveBottom(screenArea.top() + screenArea.width() / 4);
    }
}

/**
 * Constrains the client size @p size according to a set of the window's size hints.
 *
 * Default implementation applies only minimum and maximum size constraints.
 */
QSizeF Window::constrainClientSize(const QSizeF &size, SizeMode mode) const
{
    qreal width = size.width();
    qreal height = size.height();

    // When user is resizing the window, the move resize geometry may have negative width or
    // height. In which case, we need to set negative dimensions to reasonable values.
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    const QSizeF minimumSize = minSize();
    const QSizeF maximumSize = maxSize();

    width = std::clamp(width, minimumSize.width(), maximumSize.width());
    height = std::clamp(height, minimumSize.height(), maximumSize.height());

    return QSizeF(width, height);
}

/**
 * Constrains the frame size @p size according to a set of the window's size hints.
 */
QSizeF Window::constrainFrameSize(const QSizeF &size, SizeMode mode) const
{
    const QSizeF unconstrainedClientSize = nextFrameSizeToClientSize(size);
    const QSizeF constrainedClientSize = constrainClientSize(unconstrainedClientSize, mode);
    return nextClientSizeToFrameSize(constrainedClientSize);
}

QRectF Window::fullscreenGeometryRestore() const
{
    return m_fullscreenGeometryRestore;
}

void Window::setFullscreenGeometryRestore(const QRectF &geom)
{
    if (m_fullscreenGeometryRestore != geom) {
        m_fullscreenGeometryRestore = geom;
        Q_EMIT fullscreenGeometryRestoreChanged();
    }
}

/**
 * Returns @c true if the Window can be shown in full screen mode; otherwise @c false.
 *
 * Default implementation returns @c false.
 */
bool Window::isFullScreenable() const
{
    return false;
}

/**
 * Returns @c true if the Window is currently being shown in full screen mode; otherwise @c false.
 *
 * A window in full screen mode occupies the entire screen with no window frame around it.
 *
 * Default implementation returns @c false.
 */
bool Window::isFullScreen() const
{
    return false;
}

bool Window::isRequestedFullScreen() const
{
    return isFullScreen();
}

/**
 * Asks the Window to enter or leave full screen mode.
 *
 * Default implementation does nothing.
 *
 * @p set @c true if the Window has to be shown in full screen mode, otherwise @c false
 */
void Window::setFullScreen(bool set)
{
    qCWarning(KWIN_CORE, "%s doesn't support setting fullscreen state", metaObject()->className());
}

bool Window::wantsAdaptiveSync() const
{
    return rules()->checkAdaptiveSync(isFullScreen());
}

bool Window::wantsTearing(bool tearingRequested) const
{
    return rules()->checkTearing(tearingRequested);
}

/**
 * Returns @c true if the Window can be minimized; otherwise @c false.
 *
 * Default implementation returns @c false.
 */
bool Window::isMinimizable() const
{
    return false;
}

/**
 * Returns @c true if the Window can be maximized; otherwise @c false.
 *
 * Default implementation returns @c false.
 */
bool Window::isMaximizable() const
{
    return false;
}

/**
 * Returns the currently applied maximize mode.
 *
 * Default implementation returns MaximizeRestore.
 */
MaximizeMode Window::maximizeMode() const
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
MaximizeMode Window::requestedMaximizeMode() const
{
    return maximizeMode();
}

/**
 * Returns the geometry of the Window before it was maximized or quick tiled.
 */
QRectF Window::geometryRestore() const
{
    return m_maximizeGeometryRestore;
}

/**
 * Sets the geometry of the Window before it was maximized or quick tiled to @p rect.
 */
void Window::setGeometryRestore(const QRectF &rect)
{
    if (m_maximizeGeometryRestore != rect) {
        m_maximizeGeometryRestore = rect;
        Q_EMIT maximizeGeometryRestoreChanged();
    }
}

void Window::invalidateDecoration()
{
}

bool Window::noBorder() const
{
    return true;
}

bool Window::userCanSetNoBorder() const
{
    return false;
}

void Window::setNoBorder(bool set)
{
    qCWarning(KWIN_CORE, "%s doesn't support setting decorations", metaObject()->className());
}

void Window::checkNoBorder()
{
    setNoBorder(false);
}

void Window::showOnScreenEdge()
{
    qCWarning(KWIN_CORE, "%s doesn't support screen edge activation", metaObject()->className());
}

bool Window::isPlaceable() const
{
    return true;
}

void Window::cleanTabBox()
{
#if KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = workspace()->tabbox();
    if (tabBox && tabBox->isDisplayed() && tabBox->currentClient() == this) {
        tabBox->nextPrev(true);
    }
#endif
}

bool Window::supportsWindowRules() const
{
    return false;
}

void Window::removeRule(Rules *rule)
{
    m_rules.remove(rule);
}

void Window::evaluateWindowRules()
{
    setupWindowRules();
    applyWindowRules();
}

void Window::setupWindowRules()
{
    disconnect(this, &Window::captionNormalChanged, this, &Window::evaluateWindowRules);
    m_rules = workspace()->rulebook()->find(this);
    // check only after getting the rules, because there may be a rule forcing window type
}

void Window::updateWindowRules(Rules::Types selection)
{
    if (workspace()->rulebook()->areUpdatesDisabled()) {
        return;
    }
    m_rules.update(this, selection);
}

void Window::finishWindowRules()
{
    disconnect(this, &Window::captionNormalChanged, this, &Window::evaluateWindowRules);
    updateWindowRules(Rules::All);
    m_rules = WindowRules();
}

// Applies Force, ForceTemporarily and ApplyNow rules
// Used e.g. after the rules have been modified using the kcm.
void Window::applyWindowRules()
{
    Q_ASSERT(!isDeleted());
    // apply force rules
    // Placement - does need explicit update, just like some others below
    // Geometry : setGeometry() doesn't check rules
    auto client_rules = rules();
    const QRectF oldGeometry = moveResizeGeometry();
    const QRectF geometry = client_rules->checkGeometrySafe(oldGeometry);
    if (geometry != oldGeometry) {
        moveResize(geometry);
    }
    // MinSize, MaxSize handled by Geometry
    // IgnoreGeometry
    setDesktops(desktops());
    sendToOutput(moveResizeOutput());
    setOnActivities(activities());
    // Type
    maximize(requestedMaximizeMode());
    setMinimized(isMinimized());
    setShade(shadeMode());
    setOriginalSkipTaskbar(skipTaskbar());
    setSkipPager(skipPager());
    setSkipSwitcher(skipSwitcher());
    setKeepAbove(keepAbove());
    setKeepBelow(keepBelow());
    setFullScreen(isRequestedFullScreen());
    setNoBorder(noBorder());
    updateColorScheme();
    updateLayer();
    // FSP
    // AcceptFocus :
    if (workspace()->mostRecentlyActivatedWindow() == this
        && !client_rules->checkAcceptFocus(true)) {
        workspace()->activateNextWindow(this);
    }
    // Autogrouping : Only checked on window manage
    // AutogroupInForeground : Only checked on window manage
    // AutogroupById : Only checked on window manage
    // StrictGeometry
    setShortcut(rules()->checkShortcut(shortcut().toString()));
    // see also X11Window::setActive()
    if (isActive()) {
        setOpacity(rules()->checkOpacityActive(qRound(opacity() * 100.0)) / 100.0);
        workspace()->disableGlobalShortcutsForClient(rules()->checkDisableGlobalShortcuts(false));
    } else {
        setOpacity(rules()->checkOpacityInactive(qRound(opacity() * 100.0)) / 100.0);
    }
    setDesktopFileName(rules()->checkDesktopFile(desktopFileName()));
}

void Window::setLastUsageSerial(quint32 serial)
{
    if (m_lastUsageSerial < serial) {
        m_lastUsageSerial = serial;
    }
}

quint32 Window::lastUsageSerial() const
{
    return m_lastUsageSerial;
}

uint32_t Window::interactiveMoveResizeCount() const
{
    return m_interactiveMoveResize.counter;
}

void Window::setLockScreenOverlay(bool allowed)
{
    if (m_lockScreenOverlay == allowed) {
        return;
    }
    m_lockScreenOverlay = allowed;
    Q_EMIT lockScreenOverlayChanged();
}

bool Window::isLockScreenOverlay() const
{
    return m_lockScreenOverlay;
}

void Window::refOffscreenRendering()
{
    m_offscreenRenderCount++;
    if (m_offscreenRenderCount == 1) {
        m_offscreenFramecallbackTimer.start(1'000'000 / output()->refreshRate());
        Q_EMIT offscreenRenderingChanged();
    }
}

void Window::unrefOffscreenRendering()
{
    Q_ASSERT(m_offscreenRenderCount);
    m_offscreenRenderCount--;
    if (m_offscreenRenderCount == 0) {
        m_offscreenFramecallbackTimer.stop();
        Q_EMIT offscreenRenderingChanged();
    }
}

bool Window::isOffscreenRendering() const
{
    return m_offscreenRenderCount > 0;
}

void Window::maybeSendFrameCallback()
{
    if (m_windowItem && !m_windowItem->isVisible()) {
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
        m_windowItem->framePainted(output(), nullptr, timestamp);
        // update refresh rate, it might have changed
        m_offscreenFramecallbackTimer.start(1'000'000 / output()->refreshRate());
    }
}

qreal Window::targetScale() const
{
    return m_targetScale;
}

bool Window::isShown() const
{
    return !isDeleted() && !isHidden() && !isHiddenByShowDesktop() && !isMinimized();
}

bool Window::isHidden() const
{
    return m_hidden;
}

void Window::setHidden(bool hidden)
{
    if (m_hidden == hidden) {
        return;
    }
    m_hidden = hidden;
    doSetHidden();
    if (hidden) {
        workspace()->activateNextWindow(this);
    }
    Q_EMIT hiddenChanged();
}

bool Window::isHiddenByShowDesktop() const
{
    return m_hiddenByShowDesktop;
}

void Window::setHiddenByShowDesktop(bool hidden)
{
    if (m_hiddenByShowDesktop != hidden) {
        m_hiddenByShowDesktop = hidden;
        doSetHiddenByShowDesktop();
        Q_EMIT hiddenByShowDesktopChanged();
    }
}

bool Window::isSuspended() const
{
    return m_suspended;
}

void Window::setSuspended(bool suspended)
{
    if (isDeleted()) {
        return;
    }
    if (m_suspended != suspended) {
        m_suspended = suspended;
        doSetSuspended();
    }
}

void Window::doSetSuspended()
{
}

void Window::doSetModal()
{
}

qreal Window::nextTargetScale() const
{
    return m_nextTargetScale;
}

void Window::setNextTargetScale(qreal scale)
{
    if (m_nextTargetScale != scale) {
        m_nextTargetScale = scale;
        doSetNextTargetScale();
        Q_EMIT nextTargetScaleChanged();
    }
}

void Window::doSetNextTargetScale()
{
}

void Window::updateNextTargetScale()
{
    setNextTargetScale(m_moveResizeOutput->scale());
}

void Window::setTargetScale(qreal scale)
{
    if (m_targetScale != scale) {
        m_targetScale = scale;
        Q_EMIT targetScaleChanged();
    }
}

OutputTransform Window::preferredBufferTransform() const
{
    return m_preferredBufferTransform;
}

void Window::setPreferredBufferTransform(OutputTransform transform)
{
    if (m_preferredBufferTransform != transform) {
        m_preferredBufferTransform = transform;
        doSetPreferredBufferTransform();
    }
}

void Window::doSetPreferredBufferTransform()
{
}

void Window::updatePreferredBufferTransform()
{
    setPreferredBufferTransform(m_moveResizeOutput->transform());
}

const ColorDescription &Window::preferredColorDescription() const
{
    return m_preferredColorDescription;
}

void Window::setPreferredColorDescription(const ColorDescription &description)
{
    if (m_preferredColorDescription != description) {
        m_preferredColorDescription = description;
        doSetPreferredColorDescription();
    }
}

void Window::doSetPreferredColorDescription()
{
}

void Window::updatePreferredColorDescription()
{
    setPreferredColorDescription(m_moveResizeOutput->blendingColor());
}

QString Window::tag() const
{
    return m_tag;
}

QString Window::description() const
{
    return m_description;
}

void Window::setDescription(const QString &description)
{
    if (m_description != description) {
        m_description = description;
        Q_EMIT descriptionChanged();
    }
}

} // namespace KWin

#include "moc_window.cpp"
