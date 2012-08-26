/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

//#define QT_CLEAN_NAMESPACE
// own
#include "tabbox.h"
// tabbox
#include "tabbox/clientmodel.h"
#include "tabbox/desktopmodel.h"
#include "tabbox/tabboxconfig.h"
// kwin
#include "client.h"
#include "effects.h"
#include "workspace.h"
// Qt
#include <QAction>
#include <QX11Info>
#include <QtDBus/QDBusConnection>
// KDE
#include <KActionCollection>
#include <KConfig>
#include <KConfigGroup>
#include <KDE/KAction>
#include <KDebug>
#include <KLocale>
#include <kkeyserver.h>
// X11
#include <fixx11h.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include "outline.h"

// specify externals before namespace

namespace KWin
{

extern QPixmap* kwin_get_menu_pix_hack();

namespace TabBox
{

TabBoxHandlerImpl::TabBoxHandlerImpl(TabBox* tabBox)
    : TabBoxHandler()
    , m_tabBox(tabBox)
{
}

TabBoxHandlerImpl::~TabBoxHandlerImpl()
{
}

int TabBoxHandlerImpl::activeScreen() const
{
    return Workspace::self()->activeScreen();
}

int TabBoxHandlerImpl::currentDesktop() const
{
    return Workspace::self()->currentDesktop();
}

QString TabBoxHandlerImpl::desktopName(TabBoxClient* client) const
{
    if (TabBoxClientImpl* c = static_cast< TabBoxClientImpl* >(client)) {
        if (!c->client()->isOnAllDesktops())
            return Workspace::self()->desktopName(c->client()->desktop());
    }
    return Workspace::self()->desktopName(Workspace::self()->currentDesktop());
}

QString TabBoxHandlerImpl::desktopName(int desktop) const
{
    return Workspace::self()->desktopName(desktop);
}

QWeakPointer<TabBoxClient> TabBoxHandlerImpl::nextClientFocusChain(TabBoxClient* client) const
{
    if (TabBoxClientImpl* c = static_cast< TabBoxClientImpl* >(client)) {
        Client* next = Workspace::self()->tabBox()->nextClientFocusChain(c->client());
        if (next)
            return next->tabBoxClient();
    }
    return QWeakPointer<TabBoxClient>();
}

QWeakPointer< TabBoxClient > TabBoxHandlerImpl::firstClientFocusChain() const
{
    if (Client *c = m_tabBox->firstClientFocusChain()) {
        return QWeakPointer<TabBoxClient>(c->tabBoxClient());
    } else {
        return QWeakPointer<TabBoxClient>();
    }
}

int TabBoxHandlerImpl::nextDesktopFocusChain(int desktop) const
{
    return m_tabBox->nextDesktopFocusChain(desktop);
}

int TabBoxHandlerImpl::numberOfDesktops() const
{
    return Workspace::self()->numberOfDesktops();
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
    Client* current = (static_cast< TabBoxClientImpl* >(client))->client();

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
    Client* current = (static_cast< TabBoxClientImpl* >(client))->client();

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
    Client* current = (static_cast< TabBoxClientImpl* >(client))->client();
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
                if (c->client()->resourceClass() == current->resourceClass()) {
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
            if (c->client()->resourceClass() == current->resourceClass()) {
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
    Client* current = (static_cast< TabBoxClientImpl* >(client))->client();
    Workspace* workspace = Workspace::self();

    switch (config().clientMultiScreenMode()) {
    case TabBoxConfig::IgnoreMultiScreen:
        return true;
    case TabBoxConfig::ExcludeCurrentScreenClients:
        return current->screen() != workspace->activeScreen();
    default:       // TabBoxConfig::OnlyCurrentScreenClients
        return current->screen() == workspace->activeScreen();
    }
}

QWeakPointer<TabBoxClient> TabBoxHandlerImpl::clientToAddToList(TabBoxClient* client, int desktop) const
{
    if (!client) {
        return QWeakPointer<TabBoxClient>();
    }
    Client* ret = NULL;
    Client* current = (static_cast< TabBoxClientImpl* >(client))->client();

    bool addClient = checkDesktop(client, desktop)
                  && checkActivity(client)
                  && checkApplications(client)
                  && checkMinimized(client)
                  && checkMultiScreen(client);
    addClient = addClient && current->wantsTabFocus() && !current->skipSwitcher();
    if (addClient) {
        // don't add windows that have modal dialogs
        Client* modal = current->findModal();
        if (modal == NULL || modal == current)
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
    ToplevelList stacking = Workspace::self()->stackingOrder();
    TabBoxClientList ret;
    foreach (Toplevel *toplevel, stacking) {
        if (Client *client = qobject_cast<Client*>(toplevel)) {
            ret.append(client->tabBoxClient());
        }
    }
    return ret;
}

void TabBoxHandlerImpl::raiseClient(TabBoxClient* c) const
{
    Workspace::self()->raiseClient(static_cast<TabBoxClientImpl*>(c)->client());
}

void TabBoxHandlerImpl::restack(TabBoxClient *c, TabBoxClient *under)
{
    Workspace::self()->restack(static_cast<TabBoxClientImpl*>(c)->client(),
                               static_cast<TabBoxClientImpl*>(under)->client());
}

void TabBoxHandlerImpl::elevateClient(TabBoxClient *c, WId tabbox, bool b) const
{
    if (effects) {
        const Client *cl = static_cast<TabBoxClientImpl*>(c)->client();
        if (EffectWindow *w = static_cast<EffectsHandlerImpl*>(effects)->findWindow(cl->window()))
            static_cast<EffectsHandlerImpl*>(effects)->setElevatedWindow(w, b);
        if (EffectWindow *w = static_cast<EffectsHandlerImpl*>(effects)->findWindow(tabbox))
            static_cast<EffectsHandlerImpl*>(effects)->setElevatedWindow(w, b);
    }
}


QWeakPointer<TabBoxClient> TabBoxHandlerImpl::desktopClient() const
{
    foreach (Toplevel *toplevel, Workspace::self()->stackingOrder()) {
        Client *client = qobject_cast<Client*>(toplevel);
        if (client && client->isDesktop() && client->isOnCurrentDesktop() && client->screen() == Workspace::self()->activeScreen()) {
            return client->tabBoxClient();
        }
    }
    return QWeakPointer<TabBoxClient>();
}

void TabBoxHandlerImpl::showOutline(const QRect &outline)
{
    Workspace::self()->outline()->show(outline);
}

void TabBoxHandlerImpl::hideOutline()
{
    Workspace::self()->outline()->hide();
}

QVector< Window > TabBoxHandlerImpl::outlineWindowIds() const
{
    return Workspace::self()->outline()->windowIds();
}

void TabBoxHandlerImpl::activateAndClose()
{
    m_tabBox->accept();
}

/*********************************************************
* TabBoxClientImpl
*********************************************************/

TabBoxClientImpl::TabBoxClientImpl(Client *client)
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

QPixmap TabBoxClientImpl::icon(const QSize& size) const
{
    if (m_client->isDesktop()) {
        return KIcon("user-desktop").pixmap(size);
    }
    return m_client->icon(size);
}

WId TabBoxClientImpl::window() const
{
    return m_client->window();
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

/*********************************************************
* TabBox
*********************************************************/
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
    connect(m_tabBox, SIGNAL(selectedIndexChanged()), SIGNAL(itemSelected()));

    m_tabBoxMode = TabBoxDesktopMode; // init variables
    connect(&m_delayedShowTimer, SIGNAL(timeout()), this, SLOT(show()));
    connect(Workspace::self(), SIGNAL(configChanged()), this, SLOT(reconfigure()));
    QDBusConnection::sessionBus().registerObject("/TabBox", this, QDBusConnection::ExportScriptableContents);
}

TabBox::~TabBox()
{
    QDBusConnection::sessionBus().unregisterObject("/TabBox");
}

void TabBox::handlerReady()
{
    m_tabBox->setConfig(m_defaultConfig);
    reconfigure();
    m_ready = true;
}

void TabBox::initShortcuts(KActionCollection* keys)
{
    KAction *a = NULL;

    // The setGlobalShortcut(shortcut); shortcut = a->globalShortcut()
    // sequence is necessary in the case where the user has defined a
    // custom key binding which KAction::setGlobalShortcut autoloads.
    #define KEY( name, key, fnSlot, shortcut, shortcutSlot )                    \
    a = keys->addAction( name );                                                \
    a->setText( i18n(name) );                                                   \
    shortcut = KShortcut(key);                                                  \
    qobject_cast<KAction*>( a )->setGlobalShortcut(shortcut);                   \
    shortcut = a->globalShortcut();                                             \
    connect(a, SIGNAL(triggered(bool)), SLOT(fnSlot));                          \
    connect(a, SIGNAL(globalShortcutChanged(QKeySequence)), SLOT(shortcutSlot));

    KEY(I18N_NOOP("Walk Through Windows"),                 Qt::ALT + Qt::Key_Tab, slotWalkThroughWindows(), m_cutWalkThroughWindows, slotWalkThroughWindowsKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Windows (Reverse)"),       Qt::ALT + Qt::SHIFT + Qt::Key_Backtab, slotWalkBackThroughWindows(), m_cutWalkThroughWindowsReverse, slotWalkBackThroughWindowsKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Windows Alternative"),     0, slotWalkThroughWindowsAlternative(), m_cutWalkThroughWindowsAlternative, slotWalkThroughWindowsAlternativeKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Windows Alternative (Reverse)"), 0, slotWalkBackThroughWindowsAlternative(), m_cutWalkThroughWindowsAlternativeReverse, slotWalkBackThroughWindowsAlternativeKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Windows of Current Application"), Qt::ALT + Qt::Key_QuoteLeft, slotWalkThroughCurrentAppWindows(), m_cutWalkThroughCurrentAppWindows, slotWalkThroughCurrentAppWindowsKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Windows of Current Application (Reverse)"), Qt::ALT + Qt::Key_AsciiTilde, slotWalkBackThroughCurrentAppWindows(), m_cutWalkThroughCurrentAppWindowsReverse, slotWalkBackThroughCurrentAppWindowsKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Windows of Current Application Alternative"), 0, slotWalkThroughCurrentAppWindowsAlternative(), m_cutWalkThroughCurrentAppWindowsAlternative, slotWalkThroughCurrentAppWindowsAlternativeKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Windows of Current Application Alternative (Reverse)"), 0, slotWalkBackThroughCurrentAppWindowsAlternative(), m_cutWalkThroughCurrentAppWindowsAlternativeReverse, slotWalkBackThroughCurrentAppWindowsAlternativeKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Desktops"),                0, slotWalkThroughDesktops(), m_cutWalkThroughDesktops, slotWalkThroughDesktopsKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Desktops (Reverse)"),      0, slotWalkBackThroughDesktops(), m_cutWalkThroughDesktopsReverse, slotWalkBackThroughDesktopsKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Desktop List"),            0, slotWalkThroughDesktopList(), m_cutWalkThroughDesktopList, slotWalkThroughDesktopListKeyChanged(QKeySequence))
    KEY(I18N_NOOP("Walk Through Desktop List (Reverse)"),  0, slotWalkBackThroughDesktopList(), m_cutWalkThroughDesktopListReverse, slotWalkBackThroughDesktopListKeyChanged(QKeySequence))
    #undef KEY
}

/*!
  Sets the current mode to \a mode, either TabBoxDesktopListMode or TabBoxWindowsMode

  \sa mode()
 */
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

/*!
  Resets the tab box to display the active client in TabBoxWindowsMode, or the
  current desktop in TabBoxDesktopListMode
 */
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
            setCurrentDesktop(Workspace::self()->currentDesktop());
        break;
    }

    emit tabBoxUpdated();
}

/*!
  Shows the next or previous item, depending on \a next
 */
void TabBox::nextPrev(bool next)
{
    setCurrentIndex(m_tabBox->nextPrev(next), false);
    emit tabBoxUpdated();
}

/*!
  Returns the currently displayed client ( only works in TabBoxWindowsMode ).
  Returns 0 if no client is displayed.
 */
Client* TabBox::currentClient()
{
    if (TabBoxClientImpl* client = static_cast< TabBoxClientImpl* >(m_tabBox->client(m_tabBox->currentIndex()))) {
        if (!Workspace::self()->hasClient(client->client()))
            return NULL;
        return client->client();
    } else
        return NULL;
}

/*!
  Returns the list of clients potentially displayed ( only works in
  TabBoxWindowsMode ).
  Returns an empty list if no clients are available.
 */
ClientList TabBox::currentClientList()
{
    TabBoxClientList list = m_tabBox->clientList();
    ClientList ret;
    foreach (QWeakPointer<TabBoxClient> clientPointer, list) {
        QSharedPointer<TabBoxClient> client = clientPointer.toStrongRef();
        if (!client)
            continue;
        if (const TabBoxClientImpl* c = static_cast< const TabBoxClientImpl* >(client.data()))
            ret.append(c->client());
    }
    return ret;
}

/*!
  Returns the currently displayed virtual desktop ( only works in
  TabBoxDesktopListMode )
  Returns -1 if no desktop is displayed.
 */
int TabBox::currentDesktop()
{
    return m_tabBox->desktop(m_tabBox->currentIndex());
}

/*!
  Returns the list of desktops potentially displayed ( only works in
  TabBoxDesktopListMode )
  Returns an empty list if no are available.
 */
QList< int > TabBox::currentDesktopList()
{
    return m_tabBox->desktopList();
}

/*!
  Change the currently selected client, and notify the effects.

  \sa setCurrentDesktop()
 */
void TabBox::setCurrentClient(Client* newClient)
{
    setCurrentIndex(m_tabBox->index(newClient->tabBoxClient()));
}

/*!
  Change the currently selected desktop, and notify the effects.

  \sa setCurrentClient()
 */
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

/*!
  Notify effects that the tab box is being shown, and only display the
  default tab box QFrame if no effect has referenced the tab box.
*/
void TabBox::show()
{
    emit tabBoxAdded(m_tabBoxMode);
    if (isDisplayed()) {
        m_isShown = false;
        return;
    }
    reference();
    m_isShown = true;
    m_tabBox->show();
}

/*!
  Notify effects that the tab box is being hidden.
*/
void TabBox::hide(bool abort)
{
    m_delayedShowTimer.stop();
    if (m_isShown) {
        m_isShown = false;
        unreference();
    }
    emit tabBoxClosed();
    if (isDisplayed())
        kDebug(1212) << "Tab box was not properly closed by an effect";
    m_tabBox->hide(abort);
    QApplication::syncX();
    XEvent otherEvent;
    while (XCheckTypedEvent(display(), EnterNotify, &otherEvent))
        ;
}

void TabBox::reconfigure()
{
    KSharedConfigPtr c(KGlobal::config());
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

    tabBoxConfig.setShowOutline(config.readEntry<bool>("ShowOutline",
                                TabBoxConfig::defaultShowOutline()));
    tabBoxConfig.setShowTabBox(config.readEntry<bool>("ShowTabBox",
                               TabBoxConfig::defaultShowTabBox()));
    tabBoxConfig.setHighlightWindows(config.readEntry<bool>("HighlightWindows",
                                     TabBoxConfig::defaultHighlightWindow()));

    tabBoxConfig.setLayoutName(config.readEntry<QString>("LayoutName", TabBoxConfig::defaultLayoutName()));
}

/*!
  Rikkus: please document!   (Matthias)

  Ok, here's the docs :)

  You call delayedShow() instead of show() directly.

  If the 'ShowDelay' setting is false, show() is simply called.

  Otherwise, we start a timer for the delay given in the settings and only
  do a show() when it times out.

  This means that you can alt-tab between windows and you don't see the
  tab box immediately. Not only does this make alt-tabbing faster, it gives
  less 'flicker' to the eyes. You don't need to see the tab box if you're
  just quickly switching between 2 or 3 windows. It seems to work quite
  nicely.
 */
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


bool TabBox::handleMouseEvent(XEvent* e)
{
    XAllowEvents(display(), AsyncPointer, xTime());
    if (!m_isShown && isDisplayed()) {
        // tabbox has been replaced, check effects
        if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(e))
            return true;
    }
    if (e->type == ButtonPress) {
        // press outside Tabbox?
        QPoint pos(e->xbutton.x_root, e->xbutton.y_root);

        if ((!m_isShown && isDisplayed())
                || (!m_tabBox->containsPos(pos) &&
                    (e->xbutton.button == Button1 || e->xbutton.button == Button2 || e->xbutton.button == Button3))) {
            close();  // click outside closes tab
            return true;
        }
        if (e->xbutton.button == Button5 || e->xbutton.button == Button4) {
            // mouse wheel event
            const QModelIndex index = m_tabBox->nextPrev(e->xbutton.button == Button5);
            if (index.isValid()) {
                setCurrentIndex(index);
            }
            return true;
        }
    }
    return false;
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

/*!
  Handles alt-tab / control-tab
 */
static bool areKeySymXsDepressed(bool bAll, const uint keySyms[], int nKeySyms) {
    char keymap[32];

    kDebug(125) << "areKeySymXsDepressed: " << (bAll ? "all of " : "any of ") << nKeySyms;

    XQueryKeymap(display(), keymap);

    for (int iKeySym = 0; iKeySym < nKeySyms; iKeySym++) {
        uint keySymX = keySyms[ iKeySym ];
        uchar keyCodeX = XKeysymToKeycode(display(), keySymX);
        int i = keyCodeX / 8;
        char mask = 1 << (keyCodeX - (i * 8));

        // Abort if bad index value,
        if (i < 0 || i >= 32)
            return false;

        kDebug(125) << iKeySym << ": keySymX=0x" << QString::number(keySymX, 16)
                    << " i=" << i << " mask=0x" << QString::number(mask, 16)
                    << " keymap[i]=0x" << QString::number(keymap[i], 16) << endl;

        // If ALL keys passed need to be depressed,
        if (bAll) {
            if ((keymap[i] & mask) == 0)
                return false;
        } else {
            // If we are looking for ANY key press, and this key is depressed,
            if (keymap[i] & mask)
                return true;
        }
    }

    // If we were looking for ANY key press, then none was found, return false,
    // If we were looking for ALL key presses, then all were found, return true.
    return bAll;
}

static bool areModKeysDepressed(const QKeySequence& seq) {
    uint rgKeySyms[10];
    int nKeySyms = 0;
    if (seq.isEmpty())
        return false;
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

    return areKeySymXsDepressed(false, rgKeySyms, nKeySyms);
}

static bool areModKeysDepressed(const KShortcut& cut)
{
    if (areModKeysDepressed(cut.primary()) || areModKeysDepressed(cut.alternate()))
        return true;

    return false;
}

void TabBox::navigatingThroughWindows(bool forward, const KShortcut& shortcut, TabBoxMode mode)
{
    if (isGrabbed())
        return;
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
    if (!m_ready){
        return;
    }
    navigatingThroughWindows(true, m_cutWalkThroughWindows, TabBoxWindowsMode);
}

void TabBox::slotWalkBackThroughWindows()
{
    if (!m_ready){
        return;
    }
    navigatingThroughWindows(false, m_cutWalkThroughWindowsReverse, TabBoxWindowsMode);
}

void TabBox::slotWalkThroughWindowsAlternative()
{
    if (!m_ready){
        return;
    }
    navigatingThroughWindows(true, m_cutWalkThroughWindowsAlternative, TabBoxWindowsAlternativeMode);
}

void TabBox::slotWalkBackThroughWindowsAlternative()
{
    if (!m_ready){
        return;
    }
    navigatingThroughWindows(false, m_cutWalkThroughWindowsAlternativeReverse, TabBoxWindowsAlternativeMode);
}

void TabBox::slotWalkThroughCurrentAppWindows()
{
    if (!m_ready){
        return;
    }
    navigatingThroughWindows(true, m_cutWalkThroughCurrentAppWindows, TabBoxCurrentAppWindowsMode);
}

void TabBox::slotWalkBackThroughCurrentAppWindows()
{
    if (!m_ready){
        return;
    }
    navigatingThroughWindows(false, m_cutWalkThroughCurrentAppWindowsReverse, TabBoxCurrentAppWindowsMode);
}

void TabBox::slotWalkThroughCurrentAppWindowsAlternative()
{
    if (!m_ready){
        return;
    }
    navigatingThroughWindows(true, m_cutWalkThroughCurrentAppWindowsAlternative, TabBoxCurrentAppWindowsAlternativeMode);
}

void TabBox::slotWalkBackThroughCurrentAppWindowsAlternative()
{
    if (!m_ready){
        return;
    }
    navigatingThroughWindows(false, m_cutWalkThroughCurrentAppWindowsAlternativeReverse, TabBoxCurrentAppWindowsAlternativeMode);
}

void TabBox::slotWalkThroughDesktops()
{
    if (!m_ready){
        return;
    }
    if (isGrabbed())
        return;
    if (areModKeysDepressed(m_cutWalkThroughDesktops)) {
        if (startWalkThroughDesktops())
            walkThroughDesktops(true);
    } else {
        oneStepThroughDesktops(true);
    }
}

void TabBox::slotWalkBackThroughDesktops()
{
    if (!m_ready){
        return;
    }
    if (isGrabbed())
        return;
    if (areModKeysDepressed(m_cutWalkThroughDesktopsReverse)) {
        if (startWalkThroughDesktops())
            walkThroughDesktops(false);
    } else {
        oneStepThroughDesktops(false);
    }
}

void TabBox::slotWalkThroughDesktopList()
{
    if (!m_ready){
        return;
    }
    if (isGrabbed())
        return;
    if (areModKeysDepressed(m_cutWalkThroughDesktopList)) {
        if (startWalkThroughDesktopList())
            walkThroughDesktops(true);
    } else {
        oneStepThroughDesktopList(true);
    }
}

void TabBox::slotWalkBackThroughDesktopList()
{
    if (!m_ready){
        return;
    }
    if (isGrabbed())
        return;
    if (areModKeysDepressed(m_cutWalkThroughDesktopListReverse)) {
        if (startWalkThroughDesktopList())
            walkThroughDesktops(false);
    } else {
        oneStepThroughDesktopList(false);
    }
}

void TabBox::slotWalkThroughDesktopsKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughDesktops = KShortcut(seq);
}

void TabBox::slotWalkBackThroughDesktopsKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughDesktopsReverse = KShortcut(seq);
}

void TabBox::slotWalkThroughDesktopListKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughDesktopList = KShortcut(seq);
}

void TabBox::slotWalkBackThroughDesktopListKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughDesktopListReverse = KShortcut(seq);
}

void TabBox::slotWalkThroughWindowsKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughWindows = KShortcut(seq);
}

void TabBox::slotWalkBackThroughWindowsKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughWindowsReverse = KShortcut(seq);
}

void TabBox::slotMoveToTabLeftKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughGroupWindows = KShortcut(seq);
}
void TabBox::slotMoveToTabRightKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughGroupWindowsReverse = KShortcut(seq);
}

void TabBox::slotWalkThroughWindowsAlternativeKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughWindowsAlternative = KShortcut(seq);
}

void TabBox::slotWalkBackThroughWindowsAlternativeKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughWindowsAlternativeReverse = KShortcut(seq);
}

void TabBox::slotWalkThroughCurrentAppWindowsKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughCurrentAppWindows = KShortcut(seq);
}

void TabBox::slotWalkBackThroughCurrentAppWindowsKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughCurrentAppWindowsReverse = KShortcut(seq);
}

void TabBox::slotWalkThroughCurrentAppWindowsAlternativeKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughCurrentAppWindowsAlternative = KShortcut(seq);
}

void TabBox::slotWalkBackThroughCurrentAppWindowsAlternativeKeyChanged(const QKeySequence& seq)
{
    m_cutWalkThroughCurrentAppWindowsAlternativeReverse = KShortcut(seq);
}

void TabBox::modalActionsSwitch(bool enabled)
{
    QList<KActionCollection*> collections;
    collections.append(Workspace::self()->actionCollection());
    collections.append(Workspace::self()->disableShortcutsKeys());
    collections.append(Workspace::self()->clientKeys());
    foreach (KActionCollection * collection, collections)
    foreach (QAction * action, collection->actions())
    action->setEnabled(enabled);
}

void TabBox::open(bool modal, const QString &layout)
{
    if (isDisplayed()) {
        return;
    }
    if (modal) {
        if (!establishTabBoxGrab()) {
            return;
        }
        m_tabGrab = true;
    } else {
        m_tabGrab = false;
    }
    m_noModifierGrab = !modal;
    setMode(TabBoxWindowsMode);
    if (!layout.isNull()) {
        TabBoxConfig tempConfig;
        tempConfig = tabBox->config();
        tempConfig.setLayoutName(layout);
        tempConfig.setShowTabBox(true);
        tabBox->setConfig(tempConfig);
    }
    reset();
    show();
}

void TabBox::openEmbedded(qulonglong wid, QPoint offset, QSize size, int horizontalAlignment, int verticalAlignment, const QString &layout)
{
    if (isDisplayed()) {
        return;
    }
    m_tabGrab = false;
    m_noModifierGrab = true;
    tabBox->setEmbedded(static_cast<WId>(wid));
    tabBox->setEmbeddedOffset(offset);
    tabBox->setEmbeddedSize(size);
    tabBox->setEmbeddedAlignment(static_cast<Qt::AlignmentFlag>(horizontalAlignment) | static_cast<Qt::AlignmentFlag>(verticalAlignment));
    setMode(TabBoxWindowsMode);
    if (!layout.isNull()) {
        TabBoxConfig tempConfig;
        tempConfig = tabBox->config();
        tempConfig.setLayoutName(layout);
        tabBox->setConfig(tempConfig);
    }
    reset();
    show();
}

bool TabBox::startKDEWalkThroughWindows(TabBoxMode mode)
{
    if (!establishTabBoxGrab())
        return false;
    m_tabGrab = true;
    m_noModifierGrab = false;
    tabBox->resetEmbedded();
    modalActionsSwitch(false);
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
    modalActionsSwitch(false);
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
    Client* c = NULL;
// this function find the first suitable client for unreasonable focus
// policies - the topmost one, with some exceptions (can't be keepabove/below,
// otherwise it gets stuck on them)
//     Q_ASSERT(Workspace::self()->block_stacking_updates == 0);
    for (int i = Workspace::self()->stackingOrder().size() - 1;
            i >= 0 ;
            --i) {
        Client* it = qobject_cast<Client*>(Workspace::self()->stackingOrder().at(i));
        if (it && it->isOnCurrentActivity() && it->isOnCurrentDesktop() && !it->isSpecialWindow()
                && it->isShown(false) && it->wantsTabFocus()
                && !it->keepAbove() && !it->keepBelow()) {
            c = it;
            break;
        }
    }
    Client* nc = c;
    bool options_traverse_all;
    {
        KConfigGroup group(KGlobal::config(), "TabBox");
        options_traverse_all = group.readEntry("TraverseAll", false);
    }

    Client* firstClient = 0;
    do {
        nc = forward ? nextClientStatic(nc) : previousClientStatic(nc);
        if (!firstClient) {
            // When we see our first client for the second time,
            // it's time to stop.
            firstClient = nc;
        } else if (nc == firstClient) {
            // No candidates found.
            nc = 0;
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
            if (nc->isShade() && options->isShadeHover())
                nc->setShade(ShadeActivated);
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
    if (Client* c = currentClient()) {
        Workspace::self()->activateClient(c);
        if (c->isShade() && options->isShadeHover())
            c->setShade(ShadeActivated);
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

/*!
  Handles holding alt-tab / control-tab
 */
void TabBox::keyPress(int keyQt)
{
    bool forward = false;
    bool backward = false;

    if (m_tabGrab) {
        KShortcut forwardShortcut;
        KShortcut backwardShortcut;
        switch (mode()) {
            case TabBoxWindowsMode:
                forwardShortcut = m_cutWalkThroughWindows;
                backwardShortcut = m_cutWalkThroughWindowsReverse;
                break;
            case TabBoxWindowsAlternativeMode:
                forwardShortcut = m_cutWalkThroughWindowsAlternative;
                backwardShortcut = m_cutWalkThroughWindowsAlternativeReverse;
                break;
            case TabBoxCurrentAppWindowsMode:
                forwardShortcut = m_cutWalkThroughCurrentAppWindows;
                backwardShortcut = m_cutWalkThroughCurrentAppWindowsReverse;
                break;
            case TabBoxCurrentAppWindowsAlternativeMode:
                forwardShortcut = m_cutWalkThroughCurrentAppWindowsAlternative;
                backwardShortcut = m_cutWalkThroughCurrentAppWindowsAlternativeReverse;
                break;
            default:
                kDebug(125) << "Invalid TabBoxMode";
                return;
        }
        forward = forwardShortcut.contains(keyQt);
        backward = backwardShortcut.contains(keyQt);
        if (!forward && !backward) {
            // if the shortcuts do not match, try matching again after filtering the shift key from keyQt
            // it is needed to handle correctly the ALT+~ shorcut for example as it is coded as ALT+SHIFT+~ in keyQt
            keyQt &= ~Qt::ShiftModifier;
            forward = forwardShortcut.contains(keyQt);
            backward = backwardShortcut.contains(keyQt);
        }
        if (forward || backward) {
            kDebug(125) << "== " << forwardShortcut.toString()
                        << " or " << backwardShortcut.toString() << endl;
            KDEWalkThroughWindows(forward);
        }
    } else if (m_desktopGrab) {
        forward = m_cutWalkThroughDesktops.contains(keyQt) ||
                  m_cutWalkThroughDesktopList.contains(keyQt);
        backward = m_cutWalkThroughDesktopsReverse.contains(keyQt) ||
                   m_cutWalkThroughDesktopListReverse.contains(keyQt);
        if (forward || backward)
            walkThroughDesktops(forward);
    }

    if (m_desktopGrab || m_tabGrab) {
        if (((keyQt & ~Qt::KeyboardModifierMask) == Qt::Key_Escape)
                && !(forward || backward)) {
            // if Escape is part of the shortcut, don't cancel
            close(true);
        } else if (!(forward || backward)) {
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
    modalActionsSwitch(true);
    m_tabGrab = false;
    m_desktopGrab = false;
    m_noModifierGrab = false;
}

void TabBox::accept()
{
    Client* c = currentClient();
    close();
    if (c) {
        Workspace::self()->activateClient(c);
        if (c->isShade() && options->isShadeHover())
            c->setShade(ShadeActivated);
        if (c->isDesktop())
            Workspace::self()->setShowingDesktop(!Workspace::self()->showingDesktop());
    }
}

void TabBox::reject()
{
    close(true);
}

/*!
  Handles alt-tab / control-tab releasing
 */
void TabBox::keyRelease(const XKeyEvent& ev)
{
    if (m_noModifierGrab) {
        return;
    }
    unsigned int mk = ev.state &
                      (KKeyServer::modXShift() |
                       KKeyServer::modXCtrl() |
                       KKeyServer::modXAlt() |
                       KKeyServer::modXMeta());
    // ev.state is state before the key release, so just checking mk being 0 isn't enough
    // using XQueryPointer() also doesn't seem to work well, so the check that all
    // modifiers are released: only one modifier is active and the currently released
    // key is this modifier - if yes, release the grab
    int mod_index = -1;
    for (int i = ShiftMapIndex;
            i <= Mod5MapIndex;
            ++i)
        if ((mk & (1 << i)) != 0) {
            if (mod_index >= 0)
                return;
            mod_index = i;
        }
    bool release = false;
    if (mod_index == -1)
        release = true;
    else {
        XModifierKeymap* xmk = XGetModifierMapping(display());
        for (int i = 0; i < xmk->max_keypermod; i++)
            if (xmk->modifiermap[xmk->max_keypermod * mod_index + i]
                    == ev.keycode)
                release = true;
        XFreeModifiermap(xmk);
    }
    if (!release)
        return;
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
            Workspace::self()->setCurrentDesktop(desktop);
        }
    }
}

int TabBox::nextDesktopFocusChain(int iDesktop) const
{
    const QVector<int> &desktopFocusChain = Workspace::self()->desktopFocusChain();
    int i = desktopFocusChain.indexOf(iDesktop);
    if (i >= 0 && i + 1 < desktopFocusChain.size())
        return desktopFocusChain[i+1];
    else if (desktopFocusChain.size() > 0)
        return desktopFocusChain[ 0 ];
    else
        return 1;
}

int TabBox::previousDesktopFocusChain(int iDesktop) const
{
    const QVector<int> &desktopFocusChain = Workspace::self()->desktopFocusChain();
    int i = desktopFocusChain.indexOf(iDesktop);
    if (i - 1 >= 0)
        return desktopFocusChain[i-1];
    else if (desktopFocusChain.size() > 0)
        return desktopFocusChain[desktopFocusChain.size()-1];
    else
        return Workspace::self()->numberOfDesktops();
}

int TabBox::nextDesktopStatic(int iDesktop) const
{
    int i = ++iDesktop;
    if (i > Workspace::self()->numberOfDesktops())
        i = 1;
    return i;
}

int TabBox::previousDesktopStatic(int iDesktop) const
{
    int i = --iDesktop;
    if (i < 1)
        i = Workspace::self()->numberOfDesktops();
    return i;
}

/*!
  auxiliary functions to travers all clients according to the focus
  order. Useful for kwms Alt-tab feature.
*/
Client* TabBox::nextClientFocusChain(Client* c) const
{
    const ClientList &globalFocusChain = Workspace::self()->globalFocusChain();
    if (globalFocusChain.isEmpty())
        return 0;
    int pos = globalFocusChain.indexOf(c);
    if (pos == -1)
        return globalFocusChain.last();
    if (pos == 0)
        return globalFocusChain.last();
    pos--;
    return globalFocusChain[ pos ];
}

/*!
  auxiliary functions to travers all clients according to the focus
  order. Useful for kwms Alt-tab feature.
*/
Client* TabBox::previousClientFocusChain(Client* c) const
{
    const ClientList &globalFocusChain = Workspace::self()->globalFocusChain();
    if (globalFocusChain.isEmpty())
        return 0;
    int pos = globalFocusChain.indexOf(c);
    if (pos == -1)
        return globalFocusChain.first();
    pos++;
    if (pos == globalFocusChain.count())
        return globalFocusChain.first();
    return globalFocusChain[ pos ];
}

Client *TabBox::firstClientFocusChain() const
{
    const ClientList &globalFocusChain = Workspace::self()->globalFocusChain();
    if (globalFocusChain.isEmpty())
        return NULL;
    return globalFocusChain.first();
}

/*!
  auxiliary functions to travers all clients according to the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* TabBox::nextClientStatic(Client* c) const
{
    if (!c || Workspace::self()->clientList().isEmpty())
        return 0;
    int pos = Workspace::self()->clientList().indexOf(c);
    if (pos == -1)
        return Workspace::self()->clientList().first();
    ++pos;
    if (pos == Workspace::self()->clientList().count())
        return Workspace::self()->clientList().first();
    return Workspace::self()->clientList()[ pos ];
}

/*!
  auxiliary functions to travers all clients according to the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* TabBox::previousClientStatic(Client* c) const
{
    if (!c || Workspace::self()->clientList().isEmpty())
        return 0;
    int pos = Workspace::self()->clientList().indexOf(c);
    if (pos == -1)
        return Workspace::self()->clientList().last();
    if (pos == 0)
        return Workspace::self()->clientList().last();
    --pos;
    return Workspace::self()->clientList()[ pos ];
}

bool TabBox::establishTabBoxGrab()
{
    if (!grabXKeyboard())
        return false;
    // Don't try to establish a global mouse grab using XGrabPointer, as that would prevent
    // using Alt+Tab while DND (#44972). However force passive grabs on all windows
    // in order to catch MouseRelease events and close the tabbox (#67416).
    // All clients already have passive grabs in their wrapper windows, so check only
    // the active client, which may not have it.
    assert(!m_forcedGlobalMouseGrab);
    m_forcedGlobalMouseGrab = true;
    if (Workspace::self()->activeClient() != NULL)
        Workspace::self()->activeClient()->updateMouseGrab();
    return true;
}

void TabBox::removeTabBoxGrab()
{
    ungrabXKeyboard();
    assert(m_forcedGlobalMouseGrab);
    m_forcedGlobalMouseGrab = false;
    if (Workspace::self()->activeClient() != NULL)
        Workspace::self()->activeClient()->updateMouseGrab();
}
} // namespace TabBox
} // namespace

#include "tabbox.moc"
