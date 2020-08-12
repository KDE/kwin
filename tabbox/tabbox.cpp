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
#include "tabbox/desktopmodel.h"
#include "tabbox/tabboxconfig.h"
#include "tabbox/desktopchain.h"
#include "tabbox/tabbox_logging.h"
#include "tabbox/x11_filter.h"
// kwin
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "x11client.h"
#include "effects.h"
#include "input.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "focuschain.h"
#include "screenedge.h"
#include "screens.h"
#include "unmanaged.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include "xcbutils.h"
// Qt
#include <QAction>
#include <QKeyEvent>
// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <kkeyserver.h>
// X11
#include <X11/keysym.h>
#include <X11/keysymdef.h>
// xcb
#include <xcb/xcb_keysyms.h>

// specify externals before namespace

namespace KWin
{

namespace TabBox
{

TabBoxHandlerImpl::TabBoxHandlerImpl(TabBox* tabBox)
    : TabBoxHandler(tabBox)
    , m_tabBox(tabBox)
    , m_desktopFocusChain(new DesktopChainManager(this))
{
    // connects for DesktopFocusChainManager
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(vds, SIGNAL(countChanged(uint,uint)), m_desktopFocusChain, SLOT(resize(uint,uint)));
    connect(vds, SIGNAL(currentChanged(uint,uint)), m_desktopFocusChain, SLOT(addDesktop(uint,uint)));
#ifdef KWIN_BUILD_ACTIVITIES
    if (Activities::self()) {
        connect(Activities::self(), SIGNAL(currentChanged(QString)), m_desktopFocusChain, SLOT(useChain(QString)));
    }
#endif
}

TabBoxHandlerImpl::~TabBoxHandlerImpl()
{
}

int TabBoxHandlerImpl::activeScreen() const
{
    return screens()->current();
}

int TabBoxHandlerImpl::currentDesktop() const
{
    return VirtualDesktopManager::self()->current();
}

QString TabBoxHandlerImpl::desktopName(TabBoxClient* client) const
{
    if (TabBoxClientImpl* c = static_cast< TabBoxClientImpl* >(client)) {
        if (!c->client()->isOnAllDesktops())
            return VirtualDesktopManager::self()->name(c->client()->desktop());
    }
    return VirtualDesktopManager::self()->name(VirtualDesktopManager::self()->current());
}

QString TabBoxHandlerImpl::desktopName(int desktop) const
{
    return VirtualDesktopManager::self()->name(desktop);
}

QWeakPointer<TabBoxClient> TabBoxHandlerImpl::nextClientFocusChain(TabBoxClient* client) const
{
    if (TabBoxClientImpl* c = static_cast< TabBoxClientImpl* >(client)) {
        auto next = FocusChain::self()->nextMostRecentlyUsed(c->client());
        if (next)
            return next->tabBoxClient();
    }
    return QWeakPointer<TabBoxClient>();
}

QWeakPointer< TabBoxClient > TabBoxHandlerImpl::firstClientFocusChain() const
{
    if (auto c = FocusChain::self()->firstMostRecentlyUsed()) {
        return QWeakPointer<TabBoxClient>(c->tabBoxClient());
    } else {
        return QWeakPointer<TabBoxClient>();
    }
}

bool TabBoxHandlerImpl::isInFocusChain(TabBoxClient *client) const
{
    if (TabBoxClientImpl *c = static_cast<TabBoxClientImpl*>(client)) {
        return FocusChain::self()->contains(c->client());
    }
    return false;
}

int TabBoxHandlerImpl::nextDesktopFocusChain(int desktop) const
{
    return m_desktopFocusChain->next(desktop);
}

int TabBoxHandlerImpl::numberOfDesktops() const
{
    return VirtualDesktopManager::self()->count();
}

QWeakPointer<TabBoxClient> TabBoxHandlerImpl::activeClient() const
{
    if (Workspace::self()->activeClient())
        return Workspace::self()->activeClient()->tabBoxClient();
    else
        return QWeakPointer<TabBoxClient>();
}

bool TabBoxHandlerImpl::checkDesktop(TabBoxClient* client, int desktop) const
{
    auto current = (static_cast< TabBoxClientImpl* >(client))->client();

    switch (config().clientDesktopMode()) {
    case TabBoxConfig::AllDesktopsClients:
        return true;
    case TabBoxConfig::ExcludeCurrentDesktopClients:
        return !current->isOnDesktop(desktop);
    default:       // TabBoxConfig::OnlyCurrentDesktopClients
        return current->isOnDesktop(desktop);
    }
}

bool TabBoxHandlerImpl::checkActivity(TabBoxClient* client) const
{
    auto current = (static_cast< TabBoxClientImpl* >(client))->client();

    switch (config().clientActivitiesMode()) {
    case TabBoxConfig::AllActivitiesClients:
        return true;
    case TabBoxConfig::ExcludeCurrentActivityClients:
        return !current->isOnCurrentActivity();
    default:       // TabBoxConfig::OnlyCurrentActivityClients
        return current->isOnCurrentActivity();
    }
}

bool TabBoxHandlerImpl::checkApplications(TabBoxClient* client) const
{
    auto current = (static_cast< TabBoxClientImpl* >(client))->client();
    TabBoxClientImpl* c;
    QListIterator< QWeakPointer<TabBoxClient> > i(clientList());

    switch (config().clientApplicationsMode()) {
    case TabBoxConfig::OneWindowPerApplication:
        // check if the list already contains an entry of this application
        while (i.hasNext()) {
            QSharedPointer<TabBoxClient> client = i.next().toStrongRef();
            if (!client) {
                continue;
            }
            if ((c = dynamic_cast< TabBoxClientImpl* >(client.data()))) {
                if (AbstractClient::belongToSameApplication(c->client(), current, AbstractClient::SameApplicationCheck::AllowCrossProcesses)) {
                    return false;
                }
            }
	}
        return true;
    case TabBoxConfig::AllWindowsCurrentApplication: {
        QSharedPointer<TabBoxClient> pointer = tabBox->activeClient().toStrongRef();
        if (!pointer) {
            return false;
        }
        if ((c = dynamic_cast< TabBoxClientImpl* >(pointer.data()))) {
            if (AbstractClient::belongToSameApplication(c->client(), current, AbstractClient::SameApplicationCheck::AllowCrossProcesses)) {
                return true;
            }
        }
        return false;
    }
    default:       // TabBoxConfig::AllWindowsAllApplications
      return true;
    }
}

bool TabBoxHandlerImpl::checkMinimized(TabBoxClient* client) const
{
    switch (config().clientMinimizedMode()) {
    case TabBoxConfig::ExcludeMinimizedClients:
        return !client->isMinimized();
    case TabBoxConfig::OnlyMinimizedClients:
        return client->isMinimized();
    default:       // TabBoxConfig::IgnoreMinimizedStatus
        return true;
    }
}

bool TabBoxHandlerImpl::checkMultiScreen(TabBoxClient* client) const
{
    auto current = (static_cast< TabBoxClientImpl* >(client))->client();

    switch (config().clientMultiScreenMode()) {
    case TabBoxConfig::IgnoreMultiScreen:
        return true;
    case TabBoxConfig::ExcludeCurrentScreenClients:
        return current->screen() != screens()->current();
    default:       // TabBoxConfig::OnlyCurrentScreenClients
        return current->screen() == screens()->current();
    }
}

QWeakPointer<TabBoxClient> TabBoxHandlerImpl::clientToAddToList(TabBoxClient* client, int desktop) const
{
    if (!client) {
        return QWeakPointer<TabBoxClient>();
    }
    AbstractClient* ret = nullptr;
    AbstractClient* current = (static_cast< TabBoxClientImpl* >(client))->client();

    bool addClient = checkDesktop(client, desktop)
                  && checkActivity(client)
                  && checkApplications(client)
                  && checkMinimized(client)
                  && checkMultiScreen(client);
    addClient = addClient && current->wantsTabFocus() && !current->skipSwitcher();
    if (addClient) {
        // don't add windows that have modal dialogs
        AbstractClient* modal = current->findModal();
        if (modal == nullptr || modal == current)
            ret = current;
        else if (!clientList().contains(modal->tabBoxClient()))
            ret = modal;
        else {
            // nothing
        }
    }
    if (ret)
        return ret->tabBoxClient();
    else
        return QWeakPointer<TabBoxClient>();
}

TabBoxClientList TabBoxHandlerImpl::stackingOrder() const
{
    QList<Toplevel *> stacking = Workspace::self()->stackingOrder();
    TabBoxClientList ret;
    foreach (Toplevel *toplevel, stacking) {
        if (auto client = qobject_cast<AbstractClient*>(toplevel)) {
            ret.append(client->tabBoxClient());
        }
    }
    return ret;
}

bool TabBoxHandlerImpl::isKWinCompositing() const {
    return Workspace::self()->compositing();
}

void TabBoxHandlerImpl::raiseClient(TabBoxClient* c) const
{
    Workspace::self()->raiseClient(static_cast<TabBoxClientImpl*>(c)->client());
}

void TabBoxHandlerImpl::restack(TabBoxClient *c, TabBoxClient *under)
{
    Workspace::self()->restack(static_cast<TabBoxClientImpl*>(c)->client(),
                               static_cast<TabBoxClientImpl*>(under)->client(), true);
}

void TabBoxHandlerImpl::elevateClient(TabBoxClient *c, QWindow *tabbox, bool b) const
{
    auto cl = static_cast<TabBoxClientImpl*>(c)->client();
    cl->elevate(b);
    if (Toplevel *w = Workspace::self()->findInternal(tabbox))
        w->elevate(b);
}

void TabBoxHandlerImpl::shadeClient(TabBoxClient *c, bool b) const
{
    AbstractClient *client = static_cast<TabBoxClientImpl *>(c)->client();
    client->cancelShadeHoverTimer(); // stop core shading action
    if (!b && client->shadeMode() == ShadeNormal)
        client->setShade(ShadeHover);
    else if (b && client->shadeMode() == ShadeHover)
        client->setShade(ShadeNormal);
}

QWeakPointer<TabBoxClient> TabBoxHandlerImpl::desktopClient() const
{
    foreach (Toplevel *toplevel, Workspace::self()->stackingOrder()) {
        auto client = qobject_cast<AbstractClient*>(toplevel);
        if (client && client->isDesktop() && client->isOnCurrentDesktop() && client->screen() == screens()->current()) {
            return client->tabBoxClient();
        }
    }
    return QWeakPointer<TabBoxClient>();
}

void TabBoxHandlerImpl::activateAndClose()
{
    m_tabBox->accept();
}

void TabBoxHandlerImpl::highlightWindows(TabBoxClient *window, QWindow *controller)
{
    if (!effects) {
        return;
    }
    QVector<EffectWindow*> windows;
    if (window) {
        windows << static_cast<TabBoxClientImpl*>(window)->client()->effectWindow();
    }
    if (Toplevel *t = workspace()->findInternal(controller)) {
        windows << t->effectWindow();
    }
    static_cast<EffectsHandlerImpl*>(effects)->highlightWindows(windows);
}

bool TabBoxHandlerImpl::noModifierGrab() const
{
    return m_tabBox->noModifierGrab();
}

/*********************************************************
* TabBoxClientImpl
*********************************************************/

TabBoxClientImpl::TabBoxClientImpl(AbstractClient *client)
    : TabBoxClient()
    , m_client(client)
{
}

TabBoxClientImpl::~TabBoxClientImpl()
{
}

QString TabBoxClientImpl::caption() const
{
    if (m_client->isDesktop())
        return i18nc("Special entry in alt+tab list for minimizing all windows",
                     "Show Desktop");
    return m_client->caption();
}

QIcon TabBoxClientImpl::icon() const
{
    if (m_client->isDesktop()) {
        return QIcon::fromTheme(QStringLiteral("user-desktop"));
    }
    return m_client->icon();
}

bool TabBoxClientImpl::isMinimized() const
{
    return m_client->isMinimized();
}

int TabBoxClientImpl::x() const
{
    return m_client->x();
}

int TabBoxClientImpl::y() const
{
    return m_client->y();
}

int TabBoxClientImpl::width() const
{
    return m_client->width();
}

int TabBoxClientImpl::height() const
{
    return m_client->height();
}

bool TabBoxClientImpl::isCloseable() const
{
    return m_client->isCloseable();
}

void TabBoxClientImpl::close()
{
    m_client->closeWindow();
}

bool TabBoxClientImpl::isFirstInTabBox() const
{
    return m_client->isFirstInTabBox();
}

QUuid TabBoxClientImpl::internalId() const
{
    return m_client->internalId();
}

/*********************************************************
* TabBox
*********************************************************/
TabBox *TabBox::s_self = nullptr;

TabBox *TabBox::create(QObject *parent)
{
    Q_ASSERT(!s_self);
    s_self = new TabBox(parent);
    return s_self;
}

TabBox::TabBox(QObject *parent)
    : QObject(parent)
    , m_displayRefcount(0)
    , m_desktopGrab(false)
    , m_tabGrab(false)
    , m_noModifierGrab(false)
    , m_forcedGlobalMouseGrab(false)
    , m_ready(false)
{
    m_isShown = false;
    m_defaultConfig = TabBoxConfig();
    m_defaultConfig.setTabBoxMode(TabBoxConfig::ClientTabBox);
    m_defaultConfig.setClientDesktopMode(TabBoxConfig::OnlyCurrentDesktopClients);
    m_defaultConfig.setClientActivitiesMode(TabBoxConfig::OnlyCurrentActivityClients);
    m_defaultConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsAllApplications);
    m_defaultConfig.setClientMinimizedMode(TabBoxConfig::IgnoreMinimizedStatus);
    m_defaultConfig.setShowDesktopMode(TabBoxConfig::DoNotShowDesktopClient);
    m_defaultConfig.setClientMultiScreenMode(TabBoxConfig::IgnoreMultiScreen);
    m_defaultConfig.setClientSwitchingMode(TabBoxConfig::FocusChainSwitching);

    m_alternativeConfig = TabBoxConfig();
    m_alternativeConfig.setTabBoxMode(TabBoxConfig::ClientTabBox);
    m_alternativeConfig.setClientDesktopMode(TabBoxConfig::AllDesktopsClients);
    m_alternativeConfig.setClientActivitiesMode(TabBoxConfig::OnlyCurrentActivityClients);
    m_alternativeConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsAllApplications);
    m_alternativeConfig.setClientMinimizedMode(TabBoxConfig::IgnoreMinimizedStatus);
    m_alternativeConfig.setShowDesktopMode(TabBoxConfig::DoNotShowDesktopClient);
    m_alternativeConfig.setClientMultiScreenMode(TabBoxConfig::IgnoreMultiScreen);
    m_alternativeConfig.setClientSwitchingMode(TabBoxConfig::FocusChainSwitching);

    m_defaultCurrentApplicationConfig = m_defaultConfig;
    m_defaultCurrentApplicationConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsCurrentApplication);

    m_alternativeCurrentApplicationConfig = m_alternativeConfig;
    m_alternativeCurrentApplicationConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsCurrentApplication);

    m_desktopConfig = TabBoxConfig();
    m_desktopConfig.setTabBoxMode(TabBoxConfig::DesktopTabBox);
    m_desktopConfig.setShowTabBox(true);
    m_desktopConfig.setShowDesktopMode(TabBoxConfig::DoNotShowDesktopClient);
    m_desktopConfig.setDesktopSwitchingMode(TabBoxConfig::MostRecentlyUsedDesktopSwitching);

    m_desktopListConfig = TabBoxConfig();
    m_desktopListConfig.setTabBoxMode(TabBoxConfig::DesktopTabBox);
    m_desktopListConfig.setShowTabBox(true);
    m_desktopListConfig.setShowDesktopMode(TabBoxConfig::DoNotShowDesktopClient);
    m_desktopListConfig.setDesktopSwitchingMode(TabBoxConfig::StaticDesktopSwitching);
    m_tabBox = new TabBoxHandlerImpl(this);
    QTimer::singleShot(0, this, SLOT(handlerReady()));

    m_tabBoxMode = TabBoxDesktopMode; // init variables
    connect(&m_delayedShowTimer, SIGNAL(timeout()), this, SLOT(show()));
    connect(Workspace::self(), SIGNAL(configChanged()), this, SLOT(reconfigure()));
}

TabBox::~TabBox()
{
    s_self = nullptr;
}

void TabBox::handlerReady()
{
    m_tabBox->setConfig(m_defaultConfig);
    reconfigure();
    m_ready = true;
}

template <typename Slot>
void TabBox::key(const char *actionName, Slot slot, const QKeySequence &shortcut)
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral(KWIN_NAME));
    a->setObjectName(QString::fromUtf8(actionName));
    a->setText(i18n(actionName));
    KGlobalAccel::self()->setGlobalShortcut(a, QList<QKeySequence>() << shortcut);
    input()->registerShortcut(shortcut, a, TabBox::self(), slot);
    auto cuts = KGlobalAccel::self()->shortcut(a);
    globalShortcutChanged(a, cuts.isEmpty() ? QKeySequence() : cuts.first());
}

static const char s_windows[]        = I18N_NOOP("Walk Through Windows");
static const char s_windowsRev[]     = I18N_NOOP("Walk Through Windows (Reverse)");
static const char s_windowsAlt[]     = I18N_NOOP("Walk Through Windows Alternative");
static const char s_windowsAltRev[]  = I18N_NOOP("Walk Through Windows Alternative (Reverse)");
static const char s_app[]            = I18N_NOOP("Walk Through Windows of Current Application");
static const char s_appRev[]         = I18N_NOOP("Walk Through Windows of Current Application (Reverse)");
static const char s_appAlt[]         = I18N_NOOP("Walk Through Windows of Current Application Alternative");
static const char s_appAltRev[]      = I18N_NOOP("Walk Through Windows of Current Application Alternative (Reverse)");
static const char s_desktops[]       = I18N_NOOP("Walk Through Desktops");
static const char s_desktopsRev[]    = I18N_NOOP("Walk Through Desktops (Reverse)");
static const char s_desktopList[]    = I18N_NOOP("Walk Through Desktop List");
static const char s_desktopListRev[] = I18N_NOOP("Walk Through Desktop List (Reverse)");

void TabBox::initShortcuts()
{
    key(s_windows,        &TabBox::slotWalkThroughWindows, Qt::ALT + Qt::Key_Tab);
    key(s_windowsRev,     &TabBox::slotWalkBackThroughWindows, Qt::ALT + Qt::SHIFT + Qt::Key_Backtab);
    key(s_app,            &TabBox::slotWalkThroughCurrentAppWindows, Qt::ALT + Qt::Key_QuoteLeft);
    key(s_appRev,         &TabBox::slotWalkBackThroughCurrentAppWindows, Qt::ALT + Qt::Key_AsciiTilde);
    key(s_windowsAlt,     &TabBox::slotWalkThroughWindowsAlternative);
    key(s_windowsAltRev,  &TabBox::slotWalkBackThroughWindowsAlternative);
    key(s_appAlt,         &TabBox::slotWalkThroughCurrentAppWindowsAlternative);
    key(s_appAltRev,      &TabBox::slotWalkBackThroughCurrentAppWindowsAlternative);
    key(s_desktops,       &TabBox::slotWalkThroughDesktops);
    key(s_desktopsRev,    &TabBox::slotWalkBackThroughDesktops);
    key(s_desktopList,    &TabBox::slotWalkThroughDesktopList);
    key(s_desktopListRev, &TabBox::slotWalkBackThroughDesktopList);

    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &TabBox::globalShortcutChanged);
}

void TabBox::globalShortcutChanged(QAction *action, const QKeySequence &seq)
{
    if (qstrcmp(qPrintable(action->objectName()), s_windows) == 0) {
        m_cutWalkThroughWindows = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_windowsRev) == 0) {
        m_cutWalkThroughWindowsReverse = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_app) == 0) {
        m_cutWalkThroughCurrentAppWindows = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_appRev) == 0) {
        m_cutWalkThroughCurrentAppWindowsReverse = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_windowsAlt) == 0) {
        m_cutWalkThroughWindowsAlternative = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_windowsAltRev) == 0) {
        m_cutWalkThroughWindowsAlternativeReverse = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_appAlt) == 0) {
        m_cutWalkThroughCurrentAppWindowsAlternative = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_appAltRev) == 0) {
        m_cutWalkThroughCurrentAppWindowsAlternativeReverse = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_desktops) == 0) {
        m_cutWalkThroughDesktops = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_desktopsRev) == 0) {
        m_cutWalkThroughDesktopsReverse = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_desktopList) == 0) {
        m_cutWalkThroughDesktopList = seq;
    } else if (qstrcmp(qPrintable(action->objectName()), s_desktopListRev) == 0) {
        m_cutWalkThroughDesktopListReverse = seq;
    }
}

void TabBox::setMode(TabBoxMode mode)
{
    m_tabBoxMode = mode;
    switch(mode) {
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
    case TabBoxDesktopMode:
        m_tabBox->setConfig(m_desktopConfig);
        break;
    case TabBoxDesktopListMode:
        m_tabBox->setConfig(m_desktopListConfig);
        break;
    }
}

void TabBox::reset(bool partial_reset)
{
    switch(m_tabBox->config().tabBoxMode()) {
    case TabBoxConfig::ClientTabBox:
        m_tabBox->createModel(partial_reset);
        if (!partial_reset) {
            if (Workspace::self()->activeClient())
                setCurrentClient(Workspace::self()->activeClient());
            // it's possible that the active client is not part of the model
            // in that case the index is invalid
            if (!m_tabBox->currentIndex().isValid())
                setCurrentIndex(m_tabBox->first());
        } else {
            if (!m_tabBox->currentIndex().isValid() || !m_tabBox->client(m_tabBox->currentIndex()))
                setCurrentIndex(m_tabBox->first());
        }
        break;
    case TabBoxConfig::DesktopTabBox:
        m_tabBox->createModel();

        if (!partial_reset)
            setCurrentDesktop(VirtualDesktopManager::self()->current());
        break;
    }

    emit tabBoxUpdated();
}

void TabBox::nextPrev(bool next)
{
    setCurrentIndex(m_tabBox->nextPrev(next), false);
    emit tabBoxUpdated();
}

AbstractClient* TabBox::currentClient()
{
    if (TabBoxClientImpl* client = static_cast< TabBoxClientImpl* >(m_tabBox->client(m_tabBox->currentIndex()))) {
        if (!Workspace::self()->hasClient(client->client()))
            return nullptr;
        return client->client();
    } else
        return nullptr;
}

QList<AbstractClient*> TabBox::currentClientList()
{
    TabBoxClientList list = m_tabBox->clientList();
    QList<AbstractClient*> ret;
    foreach (const QWeakPointer<TabBoxClient> &clientPointer, list) {
        QSharedPointer<TabBoxClient> client = clientPointer.toStrongRef();
        if (!client)
            continue;
        if (const TabBoxClientImpl* c = static_cast< const TabBoxClientImpl* >(client.data()))
            ret.append(c->client());
    }
    return ret;
}

int TabBox::currentDesktop()
{
    return m_tabBox->desktop(m_tabBox->currentIndex());
}

QList< int > TabBox::currentDesktopList()
{
    return m_tabBox->desktopList();
}

void TabBox::setCurrentClient(AbstractClient *newClient)
{
    setCurrentIndex(m_tabBox->index(newClient->tabBoxClient()));
}

void TabBox::setCurrentDesktop(int newDesktop)
{
    setCurrentIndex(m_tabBox->desktopIndex(newDesktop));
}

void TabBox::setCurrentIndex(QModelIndex index, bool notifyEffects)
{
    if (!index.isValid())
        return;
    m_tabBox->setCurrentIndex(index);
    if (notifyEffects) {
        emit tabBoxUpdated();
    }
}

void TabBox::show()
{
    emit tabBoxAdded(m_tabBoxMode);
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
    emit tabBoxClosed();
    if (isDisplayed())
        qCDebug(KWIN_TABBOX) << "Tab box was not properly closed by an effect";
    m_tabBox->hide(abort);
    if (kwinApp()->x11Connection()) {
        Xcb::sync();
    }
}

void TabBox::reconfigure()
{
    KSharedConfigPtr c = kwinApp()->config();
    KConfigGroup config = c->group("TabBox");

    loadConfig(c->group("TabBox"), m_defaultConfig);
    loadConfig(c->group("TabBoxAlternative"), m_alternativeConfig);

    m_defaultCurrentApplicationConfig = m_defaultConfig;
    m_defaultCurrentApplicationConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsCurrentApplication);
    m_alternativeCurrentApplicationConfig = m_alternativeConfig;
    m_alternativeCurrentApplicationConfig.setClientApplicationsMode(TabBoxConfig::AllWindowsCurrentApplication);

    m_tabBox->setConfig(m_defaultConfig);

    m_delayShow = config.readEntry<bool>("ShowDelay", true);
    m_delayShowTime = config.readEntry<int>("DelayTime", 90);

    const QString defaultDesktopLayout = QStringLiteral("org.kde.breeze.desktop");
    m_desktopConfig.setLayoutName(config.readEntry("DesktopLayout", defaultDesktopLayout));
    m_desktopListConfig.setLayoutName(config.readEntry("DesktopListLayout", defaultDesktopLayout));

    QList<ElectricBorder> *borders = &m_borderActivate;
    QString borderConfig = QStringLiteral("BorderActivate");
    for (int i = 0; i < 2; ++i) {
        foreach (ElectricBorder border, *borders) {
            ScreenEdges::self()->unreserve(border, this);
        }
        borders->clear();
        QStringList list = config.readEntry(borderConfig, QStringList());
        foreach (const QString &s, list) {
            bool ok;
            const int i = s.toInt(&ok);
            if (!ok)
                continue;
            borders->append(ElectricBorder(i));
            ScreenEdges::self()->reserve(ElectricBorder(i), this, "toggle");
        }
        borders = &m_borderAlternativeActivate;
        borderConfig = QStringLiteral("BorderAlternativeActivate");
    }

    auto touchConfig = [this, config] (const QString &key, QHash<ElectricBorder, QAction *> &actions, TabBoxMode mode, const QStringList &defaults = QStringList{}) {
        // fist erase old config
        for (auto it = actions.begin(); it != actions.end(); ) {
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
            ScreenEdges::self()->reserveTouch(ElectricBorder(i), a);
            actions.insert(ElectricBorder(i), a);
        }
    };
    touchConfig(QStringLiteral("TouchBorderActivate"), m_touchActivate, TabBoxWindowsMode, QStringList{QString::number(int(ElectricLeft))});
    touchConfig(QStringLiteral("TouchBorderAlternativeActivate"), m_touchAlternativeActivate, TabBoxWindowsAlternativeMode);
}

void TabBox::loadConfig(const KConfigGroup& config, TabBoxConfig& tabBoxConfig)
{
    tabBoxConfig.setClientDesktopMode(TabBoxConfig::ClientDesktopMode(
                                       config.readEntry<int>("DesktopMode", TabBoxConfig::defaultDesktopMode())));
    tabBoxConfig.setClientActivitiesMode(TabBoxConfig::ClientActivitiesMode(
                                       config.readEntry<int>("ActivitiesMode", TabBoxConfig::defaultActivitiesMode())));
    tabBoxConfig.setClientApplicationsMode(TabBoxConfig::ClientApplicationsMode(
                                       config.readEntry<int>("ApplicationsMode", TabBoxConfig::defaultApplicationsMode())));
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
    if (isDisplayed() || m_delayedShowTimer.isActive())
        // already called show - no need to call it twice
        return;

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
        if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(event)) {
            return true;
        }
    }
    switch (event->type()) {
    case QEvent::MouseMove:
        if (!m_tabBox->containsPos(event->globalPos())) {
            // filter out all events which are not on the TabBox window.
            // We don't want windows to react on the mouse events
            return true;
        }
        return false;
    case QEvent::MouseButtonPress:
        if ((!m_isShown && isDisplayed()) || !m_tabBox->containsPos(event->globalPos())) {
            close();  // click outside closes tab
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
        if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(event)) {
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

void TabBox::grabbedKeyEvent(QKeyEvent* event)
{
    emit tabBoxKeyEvent(event);
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

struct KeySymbolsDeleter
{
    static inline void cleanup(xcb_key_symbols_t *symbols)
    {
        xcb_key_symbols_free(symbols);
    }
};

/**
 * Handles alt-tab / control-tab
 */
static bool areKeySymXsDepressed(const uint keySyms[], int nKeySyms)
{
    Xcb::QueryKeymap keys;

    QScopedPointer<xcb_key_symbols_t, KeySymbolsDeleter> symbols(xcb_key_symbols_alloc(connection()));
    if (symbols.isNull() || !keys) {
        return false;
    }
    const auto keymap = keys->keys;

    bool depressed = false;
    for (int iKeySym = 0; iKeySym < nKeySyms; iKeySym++) {
        uint keySymX = keySyms[ iKeySym ];
        xcb_keycode_t *keyCodes = xcb_key_symbols_get_keycode(symbols.data(), keySymX);
        if (!keyCodes) {
            continue;
        }

        int j = 0;
        while (keyCodes[j] != XCB_NO_SYMBOL) {
            const xcb_keycode_t keyCodeX = keyCodes[j++];
            int i = keyCodeX / 8;
            char mask = 1 << (keyCodeX - (i * 8));

            if (i < 0 || i >= 32) {
                continue;
            }

            qCDebug(KWIN_TABBOX)    << iKeySym << ": keySymX=0x" << QString::number(keySymX, 16)
                        << " i=" << i << " mask=0x" << QString::number(mask, 16)
                        << " keymap[i]=0x" << QString::number(keymap[i], 16);

            if (keymap[i] & mask) {
                depressed = true;
                break;
            }
        }

        free(keyCodes);
    }

    return depressed;
}

static bool areModKeysDepressedX11(const QKeySequence &seq)
{
    uint rgKeySyms[10];
    int nKeySyms = 0;
    int mod = seq[seq.count()-1] & Qt::KeyboardModifierMask;

    if (mod & Qt::SHIFT) {
        rgKeySyms[nKeySyms++] = XK_Shift_L;
        rgKeySyms[nKeySyms++] = XK_Shift_R;
    }
    if (mod & Qt::CTRL) {
        rgKeySyms[nKeySyms++] = XK_Control_L;
        rgKeySyms[nKeySyms++] = XK_Control_R;
    }
    if (mod & Qt::ALT) {
        rgKeySyms[nKeySyms++] = XK_Alt_L;
        rgKeySyms[nKeySyms++] = XK_Alt_R;
    }
    if (mod & Qt::META) {
        // It would take some code to determine whether the Win key
        // is associated with Super or Meta, so check for both.
        // See bug #140023 for details.
        rgKeySyms[nKeySyms++] = XK_Super_L;
        rgKeySyms[nKeySyms++] = XK_Super_R;
        rgKeySyms[nKeySyms++] = XK_Meta_L;
        rgKeySyms[nKeySyms++] = XK_Meta_R;
    }

    return areKeySymXsDepressed(rgKeySyms, nKeySyms);
}

static bool areModKeysDepressedWayland(const QKeySequence &seq)
{
    const int mod = seq[seq.count()-1] & Qt::KeyboardModifierMask;
    const Qt::KeyboardModifiers mods = input()->modifiersRelevantForGlobalShortcuts();
    if ((mod & Qt::SHIFT) && mods.testFlag(Qt::ShiftModifier)) {
        return true;
    }
    if ((mod & Qt::CTRL) && mods.testFlag(Qt::ControlModifier)) {
        return true;
    }
    if ((mod & Qt::ALT) && mods.testFlag(Qt::AltModifier)) {
        return true;
    }
    if ((mod & Qt::META) && mods.testFlag(Qt::MetaModifier)) {
        return true;
    }
    return false;
}

static bool areModKeysDepressed(const QKeySequence& seq) {
    if (seq.isEmpty())
        return false;
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        return areModKeysDepressedWayland(seq);
    } else {
        return areModKeysDepressedX11(seq);
    }
}

void TabBox::navigatingThroughWindows(bool forward, const QKeySequence &shortcut, TabBoxMode mode)
{
    if (!m_ready || isGrabbed() || !Workspace::self()->isOnCurrentHead()) {
        return;
    }
    if (!options->focusPolicyIsReasonable()) {
        //ungrabXKeyboard(); // need that because of accelerator raw mode
        // CDE style raise / lower
        CDEWalkThroughWindows(forward);
    } else {
        if (areModKeysDepressed(shortcut)) {
            if (startKDEWalkThroughWindows(mode))
                KDEWalkThroughWindows(forward);
        } else
            // if the shortcut has no modifiers, don't show the tabbox,
            // don't grab, but simply go to the next window
            KDEOneStepThroughWindows(forward, mode);
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

void TabBox::slotWalkThroughDesktops()
{
    if (!m_ready || isGrabbed() || !Workspace::self()->isOnCurrentHead()) {
        return;
    }
    if (areModKeysDepressed(m_cutWalkThroughDesktops)) {
        if (startWalkThroughDesktops())
            walkThroughDesktops(true);
    } else {
        oneStepThroughDesktops(true);
    }
}

void TabBox::slotWalkBackThroughDesktops()
{
    if (!m_ready || isGrabbed() || !Workspace::self()->isOnCurrentHead()) {
        return;
    }
    if (areModKeysDepressed(m_cutWalkThroughDesktopsReverse)) {
        if (startWalkThroughDesktops())
            walkThroughDesktops(false);
    } else {
        oneStepThroughDesktops(false);
    }
}

void TabBox::slotWalkThroughDesktopList()
{
    if (!m_ready || isGrabbed() || !Workspace::self()->isOnCurrentHead()) {
        return;
    }
    if (areModKeysDepressed(m_cutWalkThroughDesktopList)) {
        if (startWalkThroughDesktopList())
            walkThroughDesktops(true);
    } else {
        oneStepThroughDesktopList(true);
    }
}

void TabBox::slotWalkBackThroughDesktopList()
{
    if (!m_ready || isGrabbed() || !Workspace::self()->isOnCurrentHead()) {
        return;
    }
    if (areModKeysDepressed(m_cutWalkThroughDesktopListReverse)) {
        if (startWalkThroughDesktopList())
            walkThroughDesktops(false);
    } else {
        oneStepThroughDesktopList(false);
    }
}

void TabBox::shadeActivate(AbstractClient *c)
{
    if ((c->shadeMode() == ShadeNormal || c->shadeMode() == ShadeHover) && options->isShadeHover())
        c->setShade(ShadeActivated);
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
    if (!options->focusPolicyIsReasonable())
        return false; // not supported.
    if (isDisplayed()) {
        accept();
        return true;
    }
    if (!establishTabBoxGrab())
        return false;
    m_noModifierGrab = m_tabGrab = true;
    setMode(mode);
    reset();
    show();
    return true;
}

bool TabBox::startKDEWalkThroughWindows(TabBoxMode mode)
{
    if (!establishTabBoxGrab())
        return false;
    m_tabGrab = true;
    m_noModifierGrab = false;
    setMode(mode);
    reset();
    return true;
}

bool TabBox::startWalkThroughDesktops(TabBoxMode mode)
{
    if (!establishTabBoxGrab())
        return false;
    m_desktopGrab = true;
    m_noModifierGrab = false;
    setMode(mode);
    reset();
    return true;
}

bool TabBox::startWalkThroughDesktops()
{
    return startWalkThroughDesktops(TabBoxDesktopMode);
}

bool TabBox::startWalkThroughDesktopList()
{
    return startWalkThroughDesktops(TabBoxDesktopListMode);
}

void TabBox::KDEWalkThroughWindows(bool forward)
{
    nextPrev(forward);
    delayedShow();
}

void TabBox::walkThroughDesktops(bool forward)
{
    nextPrev(forward);
    delayedShow();
}

void TabBox::CDEWalkThroughWindows(bool forward)
{
    AbstractClient* c = nullptr;
// this function find the first suitable client for unreasonable focus
// policies - the topmost one, with some exceptions (can't be keepabove/below,
// otherwise it gets stuck on them)
//     Q_ASSERT(Workspace::self()->block_stacking_updates == 0);
    for (int i = Workspace::self()->stackingOrder().size() - 1;
            i >= 0 ;
            --i) {
        auto it = qobject_cast<AbstractClient*>(Workspace::self()->stackingOrder().at(i));
        if (it && it->isOnCurrentActivity() && it->isOnCurrentDesktop() && !it->isSpecialWindow()
                && it->isShown(false) && it->wantsTabFocus()
                && !it->keepAbove() && !it->keepBelow()) {
            c = it;
            break;
        }
    }
    AbstractClient* nc = c;
    bool options_traverse_all;
    {
        KConfigGroup group(kwinApp()->config(), "TabBox");
        options_traverse_all = group.readEntry("TraverseAll", false);
    }

    AbstractClient* firstClient = nullptr;
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
    } while (nc && nc != c &&
            ((!options_traverse_all && !nc->isOnDesktop(currentDesktop())) ||
             nc->isMinimized() || !nc->wantsTabFocus() || nc->keepAbove() || nc->keepBelow() || !nc->isOnCurrentActivity()));
    if (nc) {
        if (c && c != nc)
            Workspace::self()->lowerClient(c);
        if (options->focusPolicyIsReasonable()) {
            Workspace::self()->activateClient(nc);
            shadeActivate(nc);
        } else {
            if (!nc->isOnDesktop(currentDesktop()))
                setCurrentDesktop(nc->desktop());
            Workspace::self()->raiseClient(nc);
        }
    }
}

void TabBox::KDEOneStepThroughWindows(bool forward, TabBoxMode mode)
{
    setMode(mode);
    reset();
    nextPrev(forward);
    if (AbstractClient* c = currentClient()) {
        Workspace::self()->activateClient(c);
        shadeActivate(c);
    }
}

void TabBox::oneStepThroughDesktops(bool forward, TabBoxMode mode)
{
    setMode(mode);
    reset();
    nextPrev(forward);
    if (currentDesktop() != -1)
        setCurrentDesktop(currentDesktop());
}

void TabBox::oneStepThroughDesktops(bool forward)
{
    oneStepThroughDesktops(forward, TabBoxDesktopMode);
}

void TabBox::oneStepThroughDesktopList(bool forward)
{
    oneStepThroughDesktops(forward, TabBoxDesktopListMode);
}

void TabBox::keyPress(int keyQt)
{
    enum Direction { Backward = -1, Steady = 0, Forward = 1 };
    Direction direction(Steady);

    auto contains = [](const QKeySequence &shortcut, int key) -> bool {
        for (int i = 0; i < shortcut.count(); ++i) {
            if (shortcut[i] == key) {
                return true;
            }
        }
        return false;
    };

    // tests whether a shortcut matches and handles pitfalls on ShiftKey invocation
    auto directionFor = [keyQt, contains](const QKeySequence &forward, const QKeySequence &backward) -> Direction {
        if (contains(forward, keyQt))
            return Forward;
        if (contains(backward, keyQt))
            return Backward;
        if (!(keyQt & Qt::ShiftModifier))
            return Steady;

        // Before testing the unshifted key (Ctrl+A vs. Ctrl+Shift+a etc.), see whether this is +Shift+Tab
        // and check that against +Shift+Backtab (as well)
        Qt::KeyboardModifiers mods = Qt::ShiftModifier|Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier|Qt::KeypadModifier|Qt::GroupSwitchModifier;
        mods &= keyQt;
        if ((keyQt & ~mods) == Qt::Key_Tab) {
            if (contains(forward, mods | Qt::Key_Backtab))
                return Forward;
            if (contains(backward, mods | Qt::Key_Backtab))
                return Backward;
        }

        // if the shortcuts do not match, try matching again after filtering the shift key from keyQt
        // it is needed to handle correctly the ALT+~ shorcut for example as it is coded as ALT+SHIFT+~ in keyQt
        if (contains(forward, keyQt & ~Qt::ShiftModifier))
            return Forward;
        if (contains(backward, keyQt & ~Qt::ShiftModifier))
            return Backward;

        return Steady;
    };

    if (m_tabGrab) {
        static const int ModeCount = 4;
        static const TabBoxMode modes[ModeCount] = {
            TabBoxWindowsMode, TabBoxWindowsAlternativeMode,
            TabBoxCurrentAppWindowsMode, TabBoxCurrentAppWindowsAlternativeMode
        };
        static const QKeySequence cuts[2*ModeCount] = {
            // forward
            m_cutWalkThroughWindows, m_cutWalkThroughWindowsAlternative,
            m_cutWalkThroughCurrentAppWindows, m_cutWalkThroughCurrentAppWindowsAlternative,
            // backward
            m_cutWalkThroughWindowsReverse, m_cutWalkThroughWindowsAlternativeReverse,
            m_cutWalkThroughCurrentAppWindowsReverse, m_cutWalkThroughCurrentAppWindowsAlternativeReverse
        };
        bool testedCurrent = false; // in case of collision, prefer to stay in the current mode
        int i = 0, j = 0;
        while (true) {
            if (!testedCurrent && modes[i] != mode()) {
                ++j;
                i = (i+1) % ModeCount;
                continue;
            }
            if (testedCurrent && modes[i] == mode()) {
                break;
            }
            testedCurrent = true;
            direction = directionFor(cuts[i], cuts[i+ModeCount]);
            if (direction != Steady) {
                if (modes[i] != mode()) {
                    accept(false);
                    setMode(modes[i]);
                    auto replayWithChangedTabboxMode = [this, direction]() {
                        reset();
                        nextPrev(direction == Forward);
                    };
                    QTimer::singleShot(50, this, replayWithChangedTabboxMode);
                }
                break;
            } else if (++j > 2*ModeCount) { // guarding counter for invalid modes
                qCDebug(KWIN_TABBOX) << "Invalid TabBoxMode";
                return;
            }
            i = (i+1) % ModeCount;
        }
        if (direction != Steady) {
            qCDebug(KWIN_TABBOX) << "== " << cuts[i].toString() << " or " << cuts[i+ModeCount].toString();
            KDEWalkThroughWindows(direction == Forward);
        }
    } else if (m_desktopGrab) {
        direction = directionFor(m_cutWalkThroughDesktops, m_cutWalkThroughDesktopsReverse);
        if (direction == Steady)
            direction = directionFor(m_cutWalkThroughDesktopList, m_cutWalkThroughDesktopListReverse);
        if (direction != Steady)
            walkThroughDesktops(direction == Forward);
    }

    if (m_desktopGrab || m_tabGrab) {
        if (((keyQt & ~Qt::KeyboardModifierMask) == Qt::Key_Escape) && direction == Steady) {
            // if Escape is part of the shortcut, don't cancel
            close(true);
        } else if (direction == Steady) {
            QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, keyQt & ~Qt::KeyboardModifierMask, Qt::NoModifier);
            grabbedKeyEvent(event);
        }
    }
}

void TabBox::close(bool abort)
{
    if (isGrabbed()) {
        removeTabBoxGrab();
    }
    hide(abort);
    input()->pointer()->setEnableConstraints(true);
    m_tabGrab = false;
    m_desktopGrab = false;
    m_noModifierGrab = false;
}

void TabBox::accept(bool closeTabBox)
{
    AbstractClient *c = currentClient();
    if (closeTabBox)
        close();
    if (c) {
        Workspace::self()->activateClient(c);
        shadeActivate(c);
        if (c->isDesktop())
            Workspace::self()->setShowingDesktop(!Workspace::self()->showingDesktop());
    }
}

void TabBox::modifiersReleased()
{
    if (m_noModifierGrab) {
        return;
    }
    if (m_tabGrab) {
        bool old_control_grab = m_desktopGrab;
        accept();
        m_desktopGrab = old_control_grab;
    }
    if (m_desktopGrab) {
        bool old_tab_grab = m_tabGrab;
        int desktop = currentDesktop();
        close();
        m_tabGrab = old_tab_grab;
        if (desktop != -1) {
            setCurrentDesktop(desktop);
            VirtualDesktopManager::self()->setCurrent(desktop);
        }
    }
}

int TabBox::nextDesktopStatic(int iDesktop) const
{
    DesktopNext functor;
    return functor(iDesktop, true);
}

int TabBox::previousDesktopStatic(int iDesktop) const
{
    DesktopPrevious functor;
    return functor(iDesktop, true);
}

/**
 * Auxiliary functions to travers all clients according to the static
 * order. Useful for the CDE-style Alt-tab feature.
 */
AbstractClient* TabBox::nextClientStatic(AbstractClient* c) const
{
    const auto &list = Workspace::self()->allClientList();
    if (!c || list.isEmpty())
        return nullptr;
    int pos = list.indexOf(c);
    if (pos == -1)
        return list.first();
    ++pos;
    if (pos == list.count())
        return list.first();
    return list.at(pos);
}

/**
 * Auxiliary functions to travers all clients according to the static
 * order. Useful for the CDE-style Alt-tab feature.
 */
AbstractClient* TabBox::previousClientStatic(AbstractClient* c) const
{
    const auto &list = Workspace::self()->allClientList();
    if (!c || list.isEmpty())
        return nullptr;
    int pos = list.indexOf(c);
    if (pos == -1)
        return list.last();
    if (pos == 0)
        return list.last();
    --pos;
    return list.at(pos);
}

bool TabBox::establishTabBoxGrab()
{
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        m_forcedGlobalMouseGrab = true;
        return true;
    }
    updateXTime();
    if (!grabXKeyboard())
        return false;
    // Don't try to establish a global mouse grab using XGrabPointer, as that would prevent
    // using Alt+Tab while DND (#44972). However force passive grabs on all windows
    // in order to catch MouseRelease events and close the tabbox (#67416).
    // All clients already have passive grabs in their wrapper windows, so check only
    // the active client, which may not have it.
    Q_ASSERT(!m_forcedGlobalMouseGrab);
    m_forcedGlobalMouseGrab = true;
    if (Workspace::self()->activeClient() != nullptr)
        Workspace::self()->activeClient()->updateMouseGrab();
    m_x11EventFilter.reset(new X11Filter);
    return true;
}

void TabBox::removeTabBoxGrab()
{
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        m_forcedGlobalMouseGrab = false;
        return;
    }
    updateXTime();
    ungrabXKeyboard();
    Q_ASSERT(m_forcedGlobalMouseGrab);
    m_forcedGlobalMouseGrab = false;
    if (Workspace::self()->activeClient() != nullptr)
        Workspace::self()->activeClient()->updateMouseGrab();
    m_x11EventFilter.reset();
}
} // namespace TabBox
} // namespace

