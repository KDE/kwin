/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

//#define QT_CLEAN_NAMESPACE
// own
#include "tabbox.h"
// tabbox
#include "tabbox/clientmodel.h"
#include "tabbox/tabbox_logging.h"
#include "tabbox/tabboxconfig.h"
// kwin
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "compositor.h"
#include "effect/effecthandler.h"
#include "focuschain.h"
#include "input.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "screenedge.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"
// Qt
#include <QAction>
#include <QKeyEvent>
// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLazyLocalizedString>
#include <KLocalizedString>
#include <kkeyserver.h>

namespace KWin
{

namespace TabBox
{

TabBoxHandlerImpl::TabBoxHandlerImpl(TabBox *tabBox)
    : TabBoxHandler(tabBox)
    , m_tabBox(tabBox)
{
}

TabBoxHandlerImpl::~TabBoxHandlerImpl()
{
}

int TabBoxHandlerImpl::activeScreen() const
{
    return workspace()->outputs().indexOf(workspace()->activeOutput());
}

QString TabBoxHandlerImpl::desktopName(Window *client) const
{
    if (!client->isOnAllDesktops()) {
        return client->desktops().last()->name();
    }
    return VirtualDesktopManager::self()->currentDesktop()->name();
}

Window *TabBoxHandlerImpl::nextClientFocusChain(Window *client) const
{
    return Workspace::self()->focusChain()->nextMostRecentlyUsed(client);
}

Window *TabBoxHandlerImpl::firstClientFocusChain() const
{
    return Workspace::self()->focusChain()->firstMostRecentlyUsed();
}

bool TabBoxHandlerImpl::isInFocusChain(Window *client) const
{
    return Workspace::self()->focusChain()->contains(client);
}

Window *TabBoxHandlerImpl::activeClient() const
{
    return Workspace::self()->activeWindow();
}

bool TabBoxHandlerImpl::checkDesktop(Window *client) const
{
    switch (config().clientDesktopMode()) {
    case TabBoxConfig::AllDesktopsClients:
        return true;
    case TabBoxConfig::ExcludeCurrentDesktopClients:
        return !client->isOnCurrentDesktop();
    default: // TabBoxConfig::OnlyCurrentDesktopClients
        return client->isOnCurrentDesktop();
    }
}

bool TabBoxHandlerImpl::checkActivity(Window *client) const
{
    switch (config().clientActivitiesMode()) {
    case TabBoxConfig::AllActivitiesClients:
        return true;
    case TabBoxConfig::ExcludeCurrentActivityClients:
        return !client->isOnCurrentActivity();
    default: // TabBoxConfig::OnlyCurrentActivityClients
        return client->isOnCurrentActivity();
    }
}

bool TabBoxHandlerImpl::checkApplications(Window *client) const
{
    const auto list = clientList();

    switch (config().clientApplicationsMode()) {
    case TabBoxConfig::OneWindowPerApplication:
        // check if the list already contains an entry of this application
        for (const Window *other : list) {
            if (Window::belongToSameApplication(other, client, Window::SameApplicationCheck::AllowCrossProcesses)) {
                return false;
            }
        }
        return true;
    case TabBoxConfig::AllWindowsCurrentApplication: {
        const Window *active = tabBox->activeClient();
        return active && Window::belongToSameApplication(active, client, Window::SameApplicationCheck::AllowCrossProcesses);
    }
    default: // TabBoxConfig::AllWindowsAllApplications
        return true;
    }
}

bool TabBoxHandlerImpl::checkMinimized(Window *client) const
{
    switch (config().clientMinimizedMode()) {
    case TabBoxConfig::ExcludeMinimizedClients:
        return !client->isMinimized();
    case TabBoxConfig::OnlyMinimizedClients:
        return client->isMinimized();
    default: // TabBoxConfig::IgnoreMinimizedStatus
        return true;
    }
}

bool TabBoxHandlerImpl::checkMultiScreen(Window *client) const
{
    switch (config().clientMultiScreenMode()) {
    case TabBoxConfig::IgnoreMultiScreen:
        return true;
    case TabBoxConfig::ExcludeCurrentScreenClients:
        return client->output() != workspace()->activeOutput();
    default: // TabBoxConfig::OnlyCurrentScreenClients
        return client->output() == workspace()->activeOutput();
    }
}

Window *TabBoxHandlerImpl::clientToAddToList(Window *client) const
{
    if (!client || client->isDeleted()) {
        return nullptr;
    }
    Window *ret = nullptr;

    bool addClient = checkDesktop(client)
        && checkActivity(client)
        && checkApplications(client)
        && checkMinimized(client)
        && checkMultiScreen(client);
    addClient = addClient && client->wantsTabFocus() && !client->skipSwitcher();
    if (addClient) {
        // don't add windows that have modal dialogs
        Window *modal = client->findModal();
        if (modal == nullptr || modal == client) {
            ret = client;
        } else if (!clientList().contains(modal)) {
            ret = modal;
        } else {
            // nothing
        }
    }
    return ret;
}

QList<Window *> TabBoxHandlerImpl::stackingOrder() const
{
    const QList<Window *> stacking = Workspace::self()->stackingOrder();
    QList<Window *> ret;
    for (Window *window : stacking) {
        if (window->isDeleted()) {
            continue;
        }
        if (window->isClient()) {
            ret.append(window);
        }
    }
    return ret;
}

bool TabBoxHandlerImpl::isKWinCompositing() const
{
    return Compositor::compositing();
}

void TabBoxHandlerImpl::raiseClient(Window *c) const
{
    Workspace::self()->raiseWindow(c);
}

void TabBoxHandlerImpl::restack(Window *c, Window *under)
{
    Workspace::self()->stackBelow(c, under);
}

void TabBoxHandlerImpl::elevateClient(Window *c, QWindow *tabbox, bool b) const
{
    c->elevate(b);
    if (Window *w = Workspace::self()->findInternal(tabbox)) {
        w->elevate(b);
    }
}

void TabBoxHandlerImpl::shadeClient(Window *c, bool b) const
{
    c->cancelShadeHoverTimer(); // stop core shading action
    if (!b && c->shadeMode() == ShadeNormal) {
        c->setShade(ShadeHover);
    } else if (b && c->shadeMode() == ShadeHover) {
        c->setShade(ShadeNormal);
    }
}

Window *TabBoxHandlerImpl::desktopClient() const
{
    const auto stackingOrder = Workspace::self()->stackingOrder();
    for (Window *window : stackingOrder) {
        if (window->isDeleted()) {
            continue;
        }
        if (window->isClient() && window->isDesktop() && window->isOnCurrentDesktop() && window->output() == workspace()->activeOutput()) {
            return window;
        }
    }
    return nullptr;
}

void TabBoxHandlerImpl::activateAndClose()
{
    m_tabBox->accept();
}

void TabBoxHandlerImpl::highlightWindows(Window *window, QWindow *controller)
{
    if (!effects) {
        return;
    }
    QList<EffectWindow *> windows;
    if (window) {
        windows << window->effectWindow();
    }
    if (Window *t = workspace()->findInternal(controller)) {
        windows << t->effectWindow();
    }
    effects->highlightWindows(windows);
}

bool TabBoxHandlerImpl::noModifierGrab() const
{
    return m_tabBox->noModifierGrab();
}

/*********************************************************
 * TabBox
 *********************************************************/

TabBox::TabBox()
    : m_displayRefcount(0)
    , m_tabGrab(false)
    , m_noModifierGrab(false)
    , m_forcedGlobalMouseGrab(false)
    , m_ready(false)
{
    m_isShown = false;
    m_defaultConfig = TabBoxConfig();
    m_defaultConfig.setClientDesktopMode(TabBoxConfig::OnlyCurrentDesktopClients);
    m_defaultConfig.setClientActivitiesMode(TabBoxConfig::OnlyCurrentActivityClients);
    m_defaultConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsAllApplications);
    m_defaultConfig.setOrderMinimizedMode(TabBoxConfig::NoGroupByMinimized);
    m_defaultConfig.setClientMinimizedMode(TabBoxConfig::IgnoreMinimizedStatus);
    m_defaultConfig.setShowDesktopMode(TabBoxConfig::DoNotShowDesktopClient);
    m_defaultConfig.setClientMultiScreenMode(TabBoxConfig::IgnoreMultiScreen);
    m_defaultConfig.setClientSwitchingMode(TabBoxConfig::FocusChainSwitching);

    m_alternativeConfig = TabBoxConfig();
    m_alternativeConfig.setClientDesktopMode(TabBoxConfig::AllDesktopsClients);
    m_alternativeConfig.setClientActivitiesMode(TabBoxConfig::OnlyCurrentActivityClients);
    m_alternativeConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsAllApplications);
    m_alternativeConfig.setOrderMinimizedMode(TabBoxConfig::NoGroupByMinimized);
    m_alternativeConfig.setClientMinimizedMode(TabBoxConfig::IgnoreMinimizedStatus);
    m_alternativeConfig.setShowDesktopMode(TabBoxConfig::DoNotShowDesktopClient);
    m_alternativeConfig.setClientMultiScreenMode(TabBoxConfig::IgnoreMultiScreen);
    m_alternativeConfig.setClientSwitchingMode(TabBoxConfig::FocusChainSwitching);

    m_defaultCurrentApplicationConfig = m_defaultConfig;
    m_defaultCurrentApplicationConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsCurrentApplication);

    m_alternativeCurrentApplicationConfig = m_alternativeConfig;
    m_alternativeCurrentApplicationConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsCurrentApplication);

    m_tabBox = new TabBoxHandlerImpl(this);
    QTimer::singleShot(0, this, &TabBox::handlerReady);

    m_tabBoxMode = TabBoxWindowsMode; // init variables
    connect(&m_delayedShowTimer, &QTimer::timeout, this, &TabBox::show);
    connect(Workspace::self(), &Workspace::configChanged, this, &TabBox::reconfigure);
}

TabBox::~TabBox() = default;

void TabBox::handlerReady()
{
    m_tabBox->setConfig(m_defaultConfig);
    reconfigure();
    m_ready = true;
}

template<typename Slot>
void TabBox::key(const KLazyLocalizedString &actionName, Slot slot, const QKeySequence &shortcut)
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral("kwin"));
    a->setObjectName(QString::fromUtf8(actionName.untranslatedText()));
    a->setText(actionName.toString());
    KGlobalAccel::self()->setGlobalShortcut(a, QList<QKeySequence>() << shortcut);
    connect(a, &QAction::triggered, this, slot);
    auto cuts = KGlobalAccel::self()->shortcut(a);
    globalShortcutChanged(a, cuts.isEmpty() ? QKeySequence() : cuts.first());
}

static constexpr const auto s_windows = kli18n("Walk Through Windows");
static constexpr const auto s_windowsRev = kli18n("Walk Through Windows (Reverse)");
static constexpr const auto s_windowsAlt = kli18n("Walk Through Windows Alternative");
static constexpr const auto s_windowsAltRev = kli18n("Walk Through Windows Alternative (Reverse)");
static constexpr const auto s_app = kli18n("Walk Through Windows of Current Application");
static constexpr const auto s_appRev = kli18n("Walk Through Windows of Current Application (Reverse)");
static constexpr const auto s_appAlt = kli18n("Walk Through Windows of Current Application Alternative");
static constexpr const auto s_appAltRev = kli18n("Walk Through Windows of Current Application Alternative (Reverse)");

void TabBox::initShortcuts()
{
    key(s_windows, &TabBox::slotWalkThroughWindows, Qt::AltModifier | Qt::Key_Tab);
    key(s_windowsRev, &TabBox::slotWalkBackThroughWindows, Qt::AltModifier | Qt::ShiftModifier | Qt::Key_Tab);
    key(s_app, &TabBox::slotWalkThroughCurrentAppWindows, Qt::AltModifier | Qt::Key_QuoteLeft);
    key(s_appRev, &TabBox::slotWalkBackThroughCurrentAppWindows, Qt::AltModifier | Qt::Key_AsciiTilde);
    key(s_windowsAlt, &TabBox::slotWalkThroughWindowsAlternative);
    key(s_windowsAltRev, &TabBox::slotWalkBackThroughWindowsAlternative);
    key(s_appAlt, &TabBox::slotWalkThroughCurrentAppWindowsAlternative);
    key(s_appAltRev, &TabBox::slotWalkBackThroughCurrentAppWindowsAlternative);

    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &TabBox::globalShortcutChanged);
}

void TabBox::globalShortcutChanged(QAction *action, const QKeySequence &seq)
{
    if (qstrcmp(qPrintable(action->objectName()), s_windows.untranslatedText()) == 0) {
        m_cutWalkThroughWindows = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_windowsRev.untranslatedText()) == 0) {
        m_cutWalkThroughWindowsReverse = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_app.untranslatedText()) == 0) {
        m_cutWalkThroughCurrentAppWindows = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_appRev.untranslatedText()) == 0) {
        m_cutWalkThroughCurrentAppWindowsReverse = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_windowsAlt.untranslatedText()) == 0) {
        m_cutWalkThroughWindowsAlternative = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_windowsAltRev.untranslatedText()) == 0) {
        m_cutWalkThroughWindowsAlternativeReverse = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_appAlt.untranslatedText()) == 0) {
        m_cutWalkThroughCurrentAppWindowsAlternative = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_appAltRev.untranslatedText()) == 0) {
        m_cutWalkThroughCurrentAppWindowsAlternativeReverse = seq;
    }
}

void TabBox::setMode(TabBoxMode mode)
{
    m_tabBoxMode = mode;
    switch (mode) {
    case TabBoxWindowsMode:
        m_tabBox->setConfig(m_defaultConfig);
        break;
    case TabBoxWindowsAlternativeMode:
        m_tabBox->setConfig(m_alternativeConfig);
        break;
    case TabBoxCurrentAppWindowsMode:
        m_tabBox->setConfig(m_defaultCurrentApplicationConfig);
        break;
    case TabBoxCurrentAppWindowsAlternativeMode:
        m_tabBox->setConfig(m_alternativeCurrentApplicationConfig);
        break;
    }
}

void TabBox::reset(bool partial_reset)
{
    m_tabBox->createModel(partial_reset);
    if (!partial_reset) {
        const QModelIndex activeIndex = m_tabBox->index(workspace()->activeWindow());
        if (activeIndex.isValid()) {
            setCurrentIndex(activeIndex);
        } else {
            setCurrentIndex(m_tabBox->first());
        }
    } else {
        if (!m_tabBox->currentIndex().isValid() || !m_tabBox->client(m_tabBox->currentIndex())) {
            setCurrentIndex(m_tabBox->first());
        }
    }

    Q_EMIT tabBoxUpdated();
}

void TabBox::nextPrev(bool next)
{
    setCurrentIndex(m_tabBox->nextPrev(next), false);
    Q_EMIT tabBoxUpdated();
}

Window *TabBox::currentClient()
{
    if (Window *client = m_tabBox->client(m_tabBox->currentIndex())) {
        if (!Workspace::self()->hasWindow(client)) {
            return nullptr;
        }
        return client;
    } else {
        return nullptr;
    }
}

QList<Window *> TabBox::currentClientList()
{
    return m_tabBox->clientList();
}

void TabBox::setCurrentClient(Window *newClient)
{
    setCurrentIndex(m_tabBox->index(newClient));
}

void TabBox::setCurrentIndex(QModelIndex index, bool notifyEffects)
{
    if (!index.isValid()) {
        return;
    }
    m_tabBox->setCurrentIndex(index);
    if (notifyEffects) {
        Q_EMIT tabBoxUpdated();
    }
}

bool TabBox::haveActiveClient()
{
    return m_tabBox->index(m_tabBox->activeClient()).isValid();
}

void TabBox::show()
{
    Q_EMIT tabBoxAdded(m_tabBoxMode);
    if (isDisplayed()) {
        m_isShown = false;
        return;
    }
    workspace()->setShowingDesktop(false);
    reference();
    m_isShown = true;
    m_tabBox->show();
}

void TabBox::hide(bool abort)
{
    m_delayedShowTimer.stop();
    if (m_isShown) {
        m_isShown = false;
        unreference();
    }
    Q_EMIT tabBoxClosed();
    if (isDisplayed()) {
        qCDebug(KWIN_TABBOX) << "Tab box was not properly closed by an effect";
    }
    m_tabBox->hide(abort);
}

void TabBox::reconfigure()
{
    KSharedConfigPtr c = kwinApp()->config();
    KConfigGroup config = c->group(QStringLiteral("TabBox"));

    loadConfig(c->group(QStringLiteral("TabBox")), m_defaultConfig);
    loadConfig(c->group(QStringLiteral("TabBoxAlternative")), m_alternativeConfig);

    m_defaultCurrentApplicationConfig = m_defaultConfig;
    m_defaultCurrentApplicationConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsCurrentApplication);
    m_alternativeCurrentApplicationConfig = m_alternativeConfig;
    m_alternativeCurrentApplicationConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsCurrentApplication);

    m_tabBox->setConfig(m_defaultConfig);

    m_delayShowTime = config.readEntry<int>("DelayTime", 90);

    QList<ElectricBorder> *borders = &m_borderActivate;
    QString borderConfig = QStringLiteral("BorderActivate");
    for (int i = 0; i < 2; ++i) {
        for (ElectricBorder border : std::as_const(*borders)) {
            workspace()->screenEdges()->unreserve(border, this);
        }
        borders->clear();
        QStringList list = config.readEntry(borderConfig, QStringList());
        for (const QString &s : std::as_const(list)) {
            bool ok;
            const int i = s.toInt(&ok);
            if (!ok) {
                continue;
            }
            borders->append(ElectricBorder(i));
            workspace()->screenEdges()->reserve(ElectricBorder(i), this, "toggle");
        }
        borders = &m_borderAlternativeActivate;
        borderConfig = QStringLiteral("BorderAlternativeActivate");
    }

    auto touchConfig = [this, config](const QString &key, QHash<ElectricBorder, QAction *> &actions, TabBoxMode mode, const QStringList &defaults = QStringList{}) {
        // fist erase old config
        for (auto it = actions.begin(); it != actions.end();) {
            delete it.value();
            it = actions.erase(it);
        }
        // now new config
        const QStringList list = config.readEntry(key, defaults);
        for (const auto &s : list) {
            bool ok;
            const int i = s.toInt(&ok);
            if (!ok) {
                continue;
            }
            QAction *a = new QAction(this);
            connect(a, &QAction::triggered, this, std::bind(&TabBox::toggleMode, this, mode));
            workspace()->screenEdges()->reserveTouch(ElectricBorder(i), a);
            actions.insert(ElectricBorder(i), a);
        }
    };
    touchConfig(QStringLiteral("TouchBorderActivate"), m_touchActivate, TabBoxWindowsMode);
    touchConfig(QStringLiteral("TouchBorderAlternativeActivate"), m_touchAlternativeActivate, TabBoxWindowsAlternativeMode);
}

void TabBox::loadConfig(const KConfigGroup &config, TabBoxConfig &tabBoxConfig)
{
    tabBoxConfig.setClientDesktopMode(TabBoxConfig::ClientDesktopMode(
        config.readEntry<int>("DesktopMode", TabBoxConfig::defaultDesktopMode())));
    tabBoxConfig.setClientActivitiesMode(TabBoxConfig::ClientActivitiesMode(
        config.readEntry<int>("ActivitiesMode", TabBoxConfig::defaultActivitiesMode())));
    tabBoxConfig.setClientApplicationsMode(TabBoxConfig::ClientApplicationsMode(
        config.readEntry<int>("ApplicationsMode", TabBoxConfig::defaultApplicationsMode())));
    tabBoxConfig.setOrderMinimizedMode(TabBoxConfig::OrderMinimizedMode(
        config.readEntry<int>("OrderMinimizedMode", TabBoxConfig::defaultOrderMinimizedMode())));
    tabBoxConfig.setClientMinimizedMode(TabBoxConfig::ClientMinimizedMode(
        config.readEntry<int>("MinimizedMode", TabBoxConfig::defaultMinimizedMode())));
    tabBoxConfig.setShowDesktopMode(TabBoxConfig::ShowDesktopMode(
        config.readEntry<int>("ShowDesktopMode", TabBoxConfig::defaultShowDesktopMode())));
    tabBoxConfig.setClientMultiScreenMode(TabBoxConfig::ClientMultiScreenMode(
        config.readEntry<int>("MultiScreenMode", TabBoxConfig::defaultMultiScreenMode())));
    tabBoxConfig.setClientSwitchingMode(TabBoxConfig::ClientSwitchingMode(
        config.readEntry<int>("SwitchingMode", TabBoxConfig::defaultSwitchingMode())));

    tabBoxConfig.setShowTabBox(config.readEntry<bool>("ShowTabBox",
                                                      TabBoxConfig::defaultShowTabBox()));
    tabBoxConfig.setHighlightWindows(config.readEntry<bool>("HighlightWindows",
                                                            TabBoxConfig::defaultHighlightWindow()));

    tabBoxConfig.setLayoutName(config.readEntry<QString>("LayoutName", TabBoxConfig::defaultLayoutName()));
}

void TabBox::delayedShow()
{
    if (isDisplayed() || m_delayedShowTimer.isActive()) {
        // already called show - no need to call it twice
        return;
    }

    if (!m_delayShowTime) {
        show();
        return;
    }

    m_delayedShowTimer.setSingleShot(true);
    m_delayedShowTimer.start(m_delayShowTime);
}

bool TabBox::handleMouseEvent(QMouseEvent *event)
{
    if (!m_isShown && isDisplayed()) {
        // tabbox has been replaced, check effects
        if (effects && effects->checkInputWindowEvent(event)) {
            return true;
        }
    }
    switch (event->type()) {
    case QEvent::MouseMove:
        if (!m_tabBox->containsPos(event->globalPosition().toPoint())) {
            // filter out all events which are not on the TabBox window.
            // We don't want windows to react on the mouse events
            return true;
        }
        return false;
    case QEvent::MouseButtonPress:
        if ((!m_isShown && isDisplayed()) || !m_tabBox->containsPos(event->globalPosition().toPoint())) {
            close(); // click outside closes tab
            return true;
        }
        // fall through
    case QEvent::MouseButtonRelease:
    default:
        // we do not filter it out, the intenal filter takes care
        return false;
    }
    return false;
}

bool TabBox::handleWheelEvent(QWheelEvent *event)
{
    if (!m_isShown && isDisplayed()) {
        // tabbox has been replaced, check effects
        if (effects && effects->checkInputWindowEvent(event)) {
            return true;
        }
    }
    if (event->angleDelta().y() == 0) {
        return false;
    }
    const QModelIndex index = m_tabBox->nextPrev(event->angleDelta().y() > 0);
    if (index.isValid()) {
        setCurrentIndex(index);
    }
    return true;
}

void TabBox::grabbedKeyEvent(QKeyEvent *event)
{
    Q_EMIT tabBoxKeyEvent(event);
    if (!m_isShown && isDisplayed()) {
        // tabbox has been replaced, check effects
        return;
    }
    if (m_noModifierGrab) {
        if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) {
            accept();
            return;
        }
    }
    m_tabBox->grabbedKeyEvent(event);
}

static bool areModKeysDepressed(const QKeySequence &seq)
{
    if (seq.isEmpty()) {
        return false;
    }
    const Qt::KeyboardModifiers mod = seq[seq.count() - 1].keyboardModifiers();
    const Qt::KeyboardModifiers mods = input()->modifiersRelevantForGlobalShortcuts();

    if ((mod & Qt::ShiftModifier) && mods.testFlag(Qt::ShiftModifier)) {
        return true;
    }
    if ((mod & Qt::ControlModifier) && mods.testFlag(Qt::ControlModifier)) {
        return true;
    }
    if ((mod & Qt::AltModifier) && mods.testFlag(Qt::AltModifier)) {
        return true;
    }
    if ((mod & Qt::MetaModifier) && mods.testFlag(Qt::MetaModifier)) {
        return true;
    }
    return false;
}

void TabBox::navigatingThroughWindows(bool forward, const QKeySequence &shortcut, TabBoxMode mode)
{
    if (!m_ready || isGrabbed()) {
        return;
    }
    if (!options->focusPolicyIsReasonable()) {
        // ungrabXKeyboard(); // need that because of accelerator raw mode
        //  CDE style raise / lower
        CDEWalkThroughWindows(forward);
    } else {
        if (areModKeysDepressed(shortcut)) {
            startKDEWalkThroughWindows(forward, mode);
        } else {
            // if the shortcut has no modifiers, don't show the tabbox,
            // don't grab, but simply go to the next window
            KDEOneStepThroughWindows(forward, mode);
        }
    }
}

void TabBox::slotWalkThroughWindows()
{
    navigatingThroughWindows(true, m_cutWalkThroughWindows, TabBoxWindowsMode);
}

void TabBox::slotWalkBackThroughWindows()
{
    navigatingThroughWindows(false, m_cutWalkThroughWindowsReverse, TabBoxWindowsMode);
}

void TabBox::slotWalkThroughWindowsAlternative()
{
    navigatingThroughWindows(true, m_cutWalkThroughWindowsAlternative, TabBoxWindowsAlternativeMode);
}

void TabBox::slotWalkBackThroughWindowsAlternative()
{
    navigatingThroughWindows(false, m_cutWalkThroughWindowsAlternativeReverse, TabBoxWindowsAlternativeMode);
}

void TabBox::slotWalkThroughCurrentAppWindows()
{
    navigatingThroughWindows(true, m_cutWalkThroughCurrentAppWindows, TabBoxCurrentAppWindowsMode);
}

void TabBox::slotWalkBackThroughCurrentAppWindows()
{
    navigatingThroughWindows(false, m_cutWalkThroughCurrentAppWindowsReverse, TabBoxCurrentAppWindowsMode);
}

void TabBox::slotWalkThroughCurrentAppWindowsAlternative()
{
    navigatingThroughWindows(true, m_cutWalkThroughCurrentAppWindowsAlternative, TabBoxCurrentAppWindowsAlternativeMode);
}

void TabBox::slotWalkBackThroughCurrentAppWindowsAlternative()
{
    navigatingThroughWindows(false, m_cutWalkThroughCurrentAppWindowsAlternativeReverse, TabBoxCurrentAppWindowsAlternativeMode);
}

void TabBox::shadeActivate(Window *c)
{
    if ((c->shadeMode() == ShadeNormal || c->shadeMode() == ShadeHover) && options->isShadeHover()) {
        c->setShade(ShadeActivated);
    }
}

bool TabBox::toggle(ElectricBorder eb)
{
    if (m_borderAlternativeActivate.contains(eb)) {
        return toggleMode(TabBoxWindowsAlternativeMode);
    } else {
        return toggleMode(TabBoxWindowsMode);
    }
}

bool TabBox::toggleMode(TabBoxMode mode)
{
    if (!options->focusPolicyIsReasonable()) {
        return false; // not supported.
    }
    if (isDisplayed()) {
        accept();
        return true;
    }
    if (!establishTabBoxGrab()) {
        return false;
    }
    m_noModifierGrab = m_tabGrab = true;

    input()->keyboard()->update();
    input()->pointer()->setEnableConstraints(false);

    setMode(mode);
    reset();
    show();
    return true;
}

bool TabBox::startKDEWalkThroughWindows(bool forward, TabBoxMode mode)
{
    if (!establishTabBoxGrab()) {
        return false;
    }
    m_tabGrab = true;
    m_noModifierGrab = false;

    input()->keyboard()->update();
    input()->pointer()->setEnableConstraints(false);

    setMode(mode);
    reset();

    if (haveActiveClient()) {
        nextPrev(forward);
    }

    delayedShow();

    return true;
}

void TabBox::KDEWalkThroughWindows(bool forward)
{
    nextPrev(forward);
}

void TabBox::CDEWalkThroughWindows(bool forward)
{
    Window *c = nullptr;
    // this function find the first suitable client for unreasonable focus
    // policies - the topmost one, with some exceptions (can't be keepabove/below,
    // otherwise it gets stuck on them)
    //     Q_ASSERT(Workspace::self()->block_stacking_updates == 0);
    for (int i = Workspace::self()->stackingOrder().size() - 1; i >= 0; --i) {
        auto t = Workspace::self()->stackingOrder().at(i);
        if (t->isDeleted()) {
            continue;
        }
        if (t->isClient() && t->isOnCurrentActivity() && t->isOnCurrentDesktop() && !t->isSpecialWindow()
            && !t->isShade() && t->isShown() && t->wantsTabFocus()
            && !t->keepAbove() && !t->keepBelow()) {
            c = t;
            break;
        }
    }
    Window *nc = c;
    bool options_traverse_all;
    {
        KConfigGroup group(kwinApp()->config(), QStringLiteral("TabBox"));
        options_traverse_all = group.readEntry("TraverseAll", false);
    }

    Window *firstClient = nullptr;
    do {
        nc = forward ? nextClientStatic(nc) : previousClientStatic(nc);
        if (!firstClient) {
            // When we see our first client for the second time,
            // it's time to stop.
            firstClient = nc;
        } else if (nc == firstClient) {
            // No candidates found.
            nc = nullptr;
            break;
        }
    } while (nc && nc != c && ((!options_traverse_all && !nc->isOnCurrentDesktop()) || nc->isMinimized() || !nc->wantsTabFocus() || nc->keepAbove() || nc->keepBelow() || !nc->isOnCurrentActivity()));
    if (nc) {
        if (c && c != nc) {
            Workspace::self()->lowerWindow(c);
        }
        if (options->focusPolicyIsReasonable()) {
            Workspace::self()->activateWindow(nc);
            shadeActivate(nc);
        } else {
            if (!nc->isOnCurrentDesktop()) {
                VirtualDesktopManager::self()->setCurrent(nc->desktops().constLast());
            }
            Workspace::self()->raiseWindow(nc);
        }
    }
}

void TabBox::KDEOneStepThroughWindows(bool forward, TabBoxMode mode)
{
    setMode(mode);
    reset();

    if (haveActiveClient()) {
        nextPrev(forward);
    }

    if (Window *c = currentClient()) {
        Workspace::self()->activateWindow(c);
        shadeActivate(c);
    }
}

// Tests whether a key event matches the shortcut for a given mode, either
// forward or backward, returning the direction, or Steady for no match
// Handles pitfalls with the Shift modifier
TabBox::Direction TabBox::matchShortcuts(const KeyboardKeyEvent &keyEvent, const QKeySequence &forward, const QKeySequence &backward) const
{
    auto contains = [](const QKeySequence &shortcut, const QKeyCombination key) -> bool {
        for (int i = 0; i < shortcut.count(); ++i) {
            if (shortcut[i] == key) {
                return true;
            }
        }
        return false;
    };

    if (contains(forward, keyEvent.modifiers | keyEvent.key)) {
        return Forward;
    }
    if (contains(backward, keyEvent.modifiers | keyEvent.key)) {
        return Backward;
    }
    if (!(keyEvent.modifiers & Qt::ShiftModifier)) {
        return Steady;
    }

    // Before testing the unshifted key (Ctrl+A vs. Ctrl+Shift+a etc.),
    // see whether this is +Shift+Tab/Backtab and test that against
    // +Shift+Backtab/Tab as well
    if (keyEvent.key == Qt::Key_Tab || keyEvent.key == Qt::Key_Backtab) {
        if (contains(forward, keyEvent.modifiers | Qt::Key_Backtab) || contains(forward, keyEvent.modifiers | Qt::Key_Tab)) {
            return Forward;
        }
        if (contains(backward, keyEvent.modifiers | Qt::Key_Backtab) || contains(backward, keyEvent.modifiers | Qt::Key_Tab)) {
            return Backward;
        }
    }

    // if the shortcuts do not match, try matching again after filtering the shift key
    // it is needed to handle correctly the ALT+~ shorcut for example as it is coded as ALT+SHIFT+~ in keyQt
    if (contains(forward, (keyEvent.modifiers & ~Qt::ShiftModifier) | keyEvent.key)) {
        return Forward;
    }
    if (contains(backward, (keyEvent.modifiers & ~Qt::ShiftModifier) | keyEvent.key)) {
        return Backward;
    }

    return Steady;
};

void TabBox::keyPress(const KeyboardKeyEvent &keyEvent)
{
    if (!m_tabGrab) {
        return;
    }

    Direction direction(Steady);

    const std::array<std::pair<QKeySequence, QKeySequence>, TABBOX_MODE_COUNT> shortcuts = {{
        {m_cutWalkThroughWindows, m_cutWalkThroughWindowsReverse},
        {m_cutWalkThroughWindowsAlternative, m_cutWalkThroughWindowsAlternativeReverse},
        {m_cutWalkThroughCurrentAppWindows, m_cutWalkThroughCurrentAppWindowsReverse},
        {m_cutWalkThroughCurrentAppWindowsAlternative, m_cutWalkThroughCurrentAppWindowsAlternativeReverse},
    }};

    const int currentModeIdx = static_cast<int>(mode());
    for (int i = 0; i < TABBOX_MODE_COUNT; ++i) {
        // Start checking from the current mode so in case of collision we stay
        const int idx = (i + currentModeIdx) % TABBOX_MODE_COUNT;
        const TabBoxMode testedMode = static_cast<TabBoxMode>(idx);

        direction = matchShortcuts(keyEvent, shortcuts[idx].first, shortcuts[idx].second);
        if (direction == Steady) {
            continue;
        }

        // Check if we need to switch modes
        if (testedMode != mode()) {
            accept(false);
            setMode(testedMode);
            auto replayWithChangedTabboxMode = [this, direction]() {
                reset();
                nextPrev(direction == Forward);
            };
            QTimer::singleShot(50, this, replayWithChangedTabboxMode);
            return;
        }

        break;
    }

    if (direction == Steady) {
        if (keyEvent.key == Qt::Key_Escape) {
            close(true);
        } else {
            QKeyEvent event(QEvent::KeyPress, keyEvent.key, Qt::NoModifier);
            grabbedKeyEvent(&event);
        }
        return;
    }

    // Do not wrap around list on key auto-repeat
    if (keyEvent.state == KeyboardKeyState::Repeated) {
        if (direction == Forward && m_tabBox->currentIndex().row() == m_tabBox->clientList().count() - 1) {
            return;
        } else if (direction == Backward && m_tabBox->currentIndex().row() == 0) {
            return;
        }
    }

    // Finally apply the direction to iterate over the window list
    KDEWalkThroughWindows(direction == Forward);
}

void TabBox::close(bool abort)
{
    if (isGrabbed()) {
        removeTabBoxGrab();
    }
    hide(abort);
    m_tabGrab = false;
    m_noModifierGrab = false;

    input()->keyboard()->update();
    input()->pointer()->setEnableConstraints(true);
}

void TabBox::accept(bool closeTabBox)
{
    Window *c = currentClient();
    if (closeTabBox) {
        close();
    }
    if (c) {
        Workspace::self()->activateWindow(c);
        shadeActivate(c);
        if (c->isDesktop()) {
            Workspace::self()->setShowingDesktop(!Workspace::self()->showingDesktop(), !m_defaultConfig.isHighlightWindows());
        }
    }
}

void TabBox::modifiersReleased()
{
    if (m_noModifierGrab) {
        return;
    }
    if (m_tabGrab) {
        accept();
    }
}

/**
 * Auxiliary functions to travers all clients according to the static
 * order. Useful for the CDE-style Alt-tab feature.
 */
Window *TabBox::nextClientStatic(Window *c) const
{
    const auto &list = Workspace::self()->windows();
    if (!c || list.isEmpty()) {
        return nullptr;
    }
    const int reference = list.indexOf(c);
    if (reference == -1) {
        return list.first();
    }
    for (int i = reference + 1; i < list.count(); ++i) {
        Window *candidate = list[i];
        if (candidate->isClient()) {
            return candidate;
        }
    }
    // wrap around
    for (int i = 0; i < reference; ++i) {
        Window *candidate = list[i];
        if (candidate->isClient()) {
            return candidate;
        }
    }
    return nullptr;
}

/**
 * Auxiliary functions to travers all clients according to the static
 * order. Useful for the CDE-style Alt-tab feature.
 */
Window *TabBox::previousClientStatic(Window *c) const
{
    const auto &list = Workspace::self()->windows();
    if (!c || list.isEmpty()) {
        return nullptr;
    }
    const int reference = list.indexOf(c);
    if (reference == -1) {
        return list.last();
    }
    for (int i = reference - 1; i >= 0; --i) {
        Window *candidate = list[i];
        if (candidate->isClient()) {
            return candidate;
        }
    }
    // wrap around
    for (int i = list.size() - 1; i > reference; --i) {
        Window *candidate = list[i];
        if (candidate->isClient()) {
            return candidate;
        }
    }
    return nullptr;
}

bool TabBox::establishTabBoxGrab()
{
    m_forcedGlobalMouseGrab = true;
    return true;
}

void TabBox::removeTabBoxGrab()
{
    m_forcedGlobalMouseGrab = false;
}
} // namespace TabBox
} // namespace

#include "moc_tabbox.cpp"
