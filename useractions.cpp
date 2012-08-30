/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

/*

 This file contains things relevant to direct user actions, such as
 responses to global keyboard shortcuts, or selecting actions
 from the window operations menu.

*/

///////////////////////////////////////////////////////////////////////////////
// NOTE: if you change the menu, keep kde-workspace/libs/taskmanager/taskactions.cpp in sync
//////////////////////////////////////////////////////////////////////////////

#include "useractions.h"
#include "client.h"
#include "workspace.h"
#include "effects.h"

#ifdef KWIN_BUILD_ACTIVITIES
#include <KActivities/Info>
#endif

#include <KDE/KProcess>
#include <KDE/KToolInvocation>

#include <X11/extensions/Xrandr.h>
#ifndef KWIN_NO_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif
#include <fixx11h.h>
#include <QPushButton>
#include <QSlider>

#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kconfig.h>
#include <kglobalaccel.h>
#include <kapplication.h>
#include <QRegExp>
#include <QMenu>
#include <QVBoxLayout>
#include <kauthorized.h>
#include <kactioncollection.h>
#include <kaction.h>

#include "killwindow.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif

namespace KWin
{

UserActionsMenu::UserActionsMenu(QObject *parent)
    : QObject(parent)
    , m_menu(NULL)
    , m_desktopMenu(NULL)
    , m_screenMenu(NULL)
    , m_activityMenu(NULL)
    , m_addTabsMenu(NULL)
    , m_switchToTabMenu(NULL)
    , m_resizeOperation(NULL)
    , m_moveOperation(NULL)
    , m_maximizeOperation(NULL)
    , m_shadeOperation(NULL)
    , m_keepAboveOperation(NULL)
    , m_keepBelowOperation(NULL)
    , m_fullScreenOperation(NULL)
    , m_noBorderOperation(NULL)
    , m_minimizeOperation(NULL)
    , m_closeOperation(NULL)
    , m_removeFromTabGroup(NULL)
    , m_closeTabGroup(NULL)
    , m_client(QWeakPointer<Client>())
{
}

UserActionsMenu::~UserActionsMenu()
{
    discard();
}

bool UserActionsMenu::isShown() const
{
    return m_menu && m_menu->isVisible();
}

bool UserActionsMenu::hasClient() const
{
    return !m_client.isNull();
}

void UserActionsMenu::close()
{
    if (!m_menu) {
        return;
    }
    m_menu->close();
    m_client.clear();;
}

bool UserActionsMenu::isMenuClient(const Client *c) const
{
    if (!c || m_client.isNull()) {
        return false;
    }
    return c == m_client.data();
}

void UserActionsMenu::show(const QRect &pos, const QWeakPointer<Client> &cl)
{
    if (!KAuthorized::authorizeKAction("kwin_rmb"))
        return;
    if (cl.isNull())
        return;
    if (!m_client.isNull())   // recursion
        return;
    if (cl.data()->isDesktop()
            || cl.data()->isDock())
        return;

    m_client = cl;
    init();
    Workspace *ws = Workspace::self();
    int x = pos.left();
    int y = pos.bottom();
    if (y == pos.top())
        m_menu->exec(QPoint(x, y));
    else {
        QRect area = ws->clientArea(ScreenArea, QPoint(x, y), ws->currentDesktop());
        menuAboutToShow(); // needed for sizeHint() to be correct :-/
        int popupHeight = m_menu->sizeHint().height();
        if (y + popupHeight < area.height())
            m_menu->exec(QPoint(x, y));
        else
            m_menu->exec(QPoint(x, pos.top() - popupHeight));
    }
    m_client.clear();;
}

void UserActionsMenu::helperDialog(const QString& message, const QWeakPointer<Client> &c)
{
    QStringList args;
    QString type;
    KActionCollection *keys = Workspace::self()->actionCollection();
    if (message == "noborderaltf3") {
        KAction* action = qobject_cast<KAction*>(keys->action("Window Operations Menu"));
        assert(action != NULL);
        QString shortcut = QString("%1 (%2)").arg(action->text())
                           .arg(action->globalShortcut().primary().toString(QKeySequence::NativeText));
        args << "--msgbox" << i18n(
                 "You have selected to show a window without its border.\n"
                 "Without the border, you will not be able to enable the border "
                 "again using the mouse: use the window operations menu instead, "
                 "activated using the %1 keyboard shortcut.",
                 shortcut);
        type = "altf3warning";
    } else if (message == "fullscreenaltf3") {
        KAction* action = qobject_cast<KAction*>(keys->action("Window Operations Menu"));
        assert(action != NULL);
        QString shortcut = QString("%1 (%2)").arg(action->text())
                           .arg(action->globalShortcut().primary().toString(QKeySequence::NativeText));
        args << "--msgbox" << i18n(
                 "You have selected to show a window in fullscreen mode.\n"
                 "If the application itself does not have an option to turn the fullscreen "
                 "mode off you will not be able to disable it "
                 "again using the mouse: use the window operations menu instead, "
                 "activated using the %1 keyboard shortcut.",
                 shortcut);
        type = "altf3warning";
    } else
        abort();
    if (!type.isEmpty()) {
        KConfig cfg("kwin_dialogsrc");
        KConfigGroup cg(&cfg, "Notification Messages");  // Depends on KMessageBox
        if (!cg.readEntry(type, true))
            return;
        args << "--dontagain" << "kwin_dialogsrc:" + type;
    }
    if (!c.isNull())
        args << "--embed" << QString::number(c.data()->window());
    KProcess::startDetached("kdialog", args);
}


QStringList configModules(bool controlCenter)
{
    QStringList args;
    args <<  "kwindecoration";
    if (controlCenter)
        args << "kwinoptions";
    else if (KAuthorized::authorizeControlModule("kde-kwinoptions.desktop"))
        args << "kwinactions" << "kwinfocus" <<  "kwinmoving" << "kwinadvanced"
             << "kwinrules" << "kwincompositing"
#ifdef KWIN_BUILD_TABBOX
             << "kwintabbox"
#endif
#ifdef KWIN_BUILD_SCREENEDGES
             << "kwinscreenedges"
#endif
#ifdef KWIN_BUILD_SCRIPTING
             << "kwinscripts"
#endif
             ;
    return args;
}

void UserActionsMenu::configureWM()
{
    QStringList args;
    args << "--icon" << "preferences-system-windows" << configModules(false);
    KToolInvocation::kdeinitExec("kcmshell4", args);
}

void UserActionsMenu::init()
{
    if (m_menu) {
        return;
    }
    m_menu = new QMenu;
    m_menu->setFont(KGlobalSettings::menuFont());
    connect(m_menu, SIGNAL(aboutToShow()), this, SLOT(menuAboutToShow()));
    connect(m_menu, SIGNAL(triggered(QAction*)), this, SLOT(slotWindowOperation(QAction*)));

    QMenu *advancedMenu = new QMenu(m_menu);
    advancedMenu->setFont(KGlobalSettings::menuFont());

    m_moveOperation = advancedMenu->addAction(i18n("&Move"));
    m_moveOperation->setIcon(KIcon("transform-move"));
    KActionCollection *keys = Workspace::self()->actionCollection();
    KAction *kaction = qobject_cast<KAction*>(keys->action("Window Move"));
    if (kaction != 0)
        m_moveOperation->setShortcut(kaction->globalShortcut().primary());
    m_moveOperation->setData(Options::UnrestrictedMoveOp);

    m_resizeOperation = advancedMenu->addAction(i18n("Re&size"));
    kaction = qobject_cast<KAction*>(keys->action("Window Resize"));
    if (kaction != 0)
        m_resizeOperation->setShortcut(kaction->globalShortcut().primary());
    m_resizeOperation->setData(Options::ResizeOp);

    m_keepAboveOperation = advancedMenu->addAction(i18n("Keep &Above Others"));
    m_keepAboveOperation->setIcon(KIcon("go-up"));
    kaction = qobject_cast<KAction*>(keys->action("Window Above Other Windows"));
    if (kaction != 0)
        m_keepAboveOperation->setShortcut(kaction->globalShortcut().primary());
    m_keepAboveOperation->setCheckable(true);
    m_keepAboveOperation->setData(Options::KeepAboveOp);

    m_keepBelowOperation = advancedMenu->addAction(i18n("Keep &Below Others"));
    m_keepBelowOperation->setIcon(KIcon("go-down"));
    kaction = qobject_cast<KAction*>(keys->action("Window Below Other Windows"));
    if (kaction != 0)
        m_keepBelowOperation->setShortcut(kaction->globalShortcut().primary());
    m_keepBelowOperation->setCheckable(true);
    m_keepBelowOperation->setData(Options::KeepBelowOp);

    m_fullScreenOperation = advancedMenu->addAction(i18n("&Fullscreen"));
    m_fullScreenOperation->setIcon(KIcon("view-fullscreen"));
    kaction = qobject_cast<KAction*>(keys->action("Window Fullscreen"));
    if (kaction != 0)
        m_fullScreenOperation->setShortcut(kaction->globalShortcut().primary());
    m_fullScreenOperation->setCheckable(true);
    m_fullScreenOperation->setData(Options::FullScreenOp);

    m_shadeOperation = advancedMenu->addAction(i18n("Sh&ade"));
    kaction = qobject_cast<KAction*>(keys->action("Window Shade"));
    if (kaction != 0)
        m_shadeOperation->setShortcut(kaction->globalShortcut().primary());
    m_shadeOperation->setCheckable(true);
    m_shadeOperation->setData(Options::ShadeOp);

    m_noBorderOperation = advancedMenu->addAction(i18n("&No Border"));
    kaction = qobject_cast<KAction*>(keys->action("Window No Border"));
    if (kaction != 0)
        m_noBorderOperation->setShortcut(kaction->globalShortcut().primary());
    m_noBorderOperation->setCheckable(true);
    m_noBorderOperation->setData(Options::NoBorderOp);

    advancedMenu->addSeparator();

    QAction *action = advancedMenu->addAction(i18n("Window &Shortcut..."));
    action->setIcon(KIcon("configure-shortcuts"));
    kaction = qobject_cast<KAction*>(keys->action("Setup Window Shortcut"));
    if (kaction != 0)
        action->setShortcut(kaction->globalShortcut().primary());
    action->setData(Options::SetupWindowShortcutOp);

    action = advancedMenu->addAction(i18n("&Special Window Settings..."));
    action->setIcon(KIcon("preferences-system-windows-actions"));
    action->setData(Options::WindowRulesOp);

    action = advancedMenu->addAction(i18n("S&pecial Application Settings..."));
    action->setIcon(KIcon("preferences-system-windows-actions"));
    action->setData(Options::ApplicationRulesOp);
    if (!KGlobal::config()->isImmutable() &&
            !KAuthorized::authorizeControlModules(configModules(true)).isEmpty()) {
        advancedMenu->addSeparator();
        action = advancedMenu->addAction(i18nc("Entry in context menu of window decoration to open the configuration module of KWin",
                                        "Window &Manager Settings..."));
        action->setIcon(KIcon("configure"));
        connect(action, SIGNAL(triggered()), this, SLOT(configureWM()));
    }

    m_minimizeOperation = m_menu->addAction(i18n("Mi&nimize"));
    kaction = qobject_cast<KAction*>(keys->action("Window Minimize"));
    if (kaction != 0)
        m_minimizeOperation->setShortcut(kaction->globalShortcut().primary());
    m_minimizeOperation->setData(Options::MinimizeOp);

    m_maximizeOperation = m_menu->addAction(i18n("Ma&ximize"));
    kaction = qobject_cast<KAction*>(keys->action("Window Maximize"));
    if (kaction != 0)
        m_maximizeOperation->setShortcut(kaction->globalShortcut().primary());
    m_maximizeOperation->setCheckable(true);
    m_maximizeOperation->setData(Options::MaximizeOp);

    m_menu->addSeparator();

    // Actions for window tabbing
    if (Workspace::self()->decorationSupportsTabbing()) {
        m_removeFromTabGroup = m_menu->addAction(i18n("&Untab"));
        kaction = qobject_cast<KAction*>(keys->action("Untab"));
        if (kaction != 0)
            m_removeFromTabGroup->setShortcut(kaction->globalShortcut().primary());
        m_removeFromTabGroup->setData(Options::RemoveTabFromGroupOp);

        m_closeTabGroup = m_menu->addAction(i18n("Close Entire &Group"));
        m_closeTabGroup->setIcon(KIcon("window-close"));
        kaction = qobject_cast<KAction*>(keys->action("Close TabGroup"));
        if (kaction != 0)
            m_closeTabGroup->setShortcut(kaction->globalShortcut().primary());
        m_closeTabGroup->setData(Options::CloseTabGroupOp);

        m_menu->addSeparator();
    }

    m_menu->addSeparator();

    action = m_menu->addMenu(advancedMenu);
    action->setText(i18n("More Actions"));

    m_menu->addSeparator();

    m_closeOperation = m_menu->addAction(i18n("&Close"));
    m_closeOperation->setIcon(KIcon("window-close"));
    kaction = qobject_cast<KAction*>(keys->action("Window Close"));
    if (kaction != 0)
        m_closeOperation->setShortcut(kaction->globalShortcut().primary());
    m_closeOperation->setData(Options::CloseOp);
}

void UserActionsMenu::discard()
{
    delete m_menu;
    m_menu = NULL;
    m_desktopMenu = NULL;
    m_screenMenu = NULL;
    m_activityMenu = NULL;
    m_switchToTabMenu = NULL;
    m_addTabsMenu = NULL;
}

void UserActionsMenu::menuAboutToShow()
{
    if (m_client.isNull() || !m_menu)
        return;
    Workspace *ws = Workspace::self();

    if (ws->numberOfDesktops() == 1) {
        delete m_desktopMenu;
        m_desktopMenu = 0;
    } else {
        initDesktopPopup();
    }
    if (ws->numScreens() == 1 || (!m_client.data()->isMovable() && !m_client.data()->isMovableAcrossScreens())) {
        delete m_screenMenu;
        m_screenMenu = NULL;
    } else {
        initScreenPopup();
    }
#ifdef KWIN_BUILD_ACTIVITIES
    ws->updateActivityList(true, false, this, "showHideActivityMenu");
#endif

    m_resizeOperation->setEnabled(m_client.data()->isResizable());
    m_moveOperation->setEnabled(m_client.data()->isMovableAcrossScreens());
    m_maximizeOperation->setEnabled(m_client.data()->isMaximizable());
    m_maximizeOperation->setChecked(m_client.data()->maximizeMode() == Client::MaximizeFull);
    m_shadeOperation->setEnabled(m_client.data()->isShadeable());
    m_shadeOperation->setChecked(m_client.data()->shadeMode() != ShadeNone);
    m_keepAboveOperation->setChecked(m_client.data()->keepAbove());
    m_keepBelowOperation->setChecked(m_client.data()->keepBelow());
    m_fullScreenOperation->setEnabled(m_client.data()->userCanSetFullScreen());
    m_fullScreenOperation->setChecked(m_client.data()->isFullScreen());
    m_noBorderOperation->setEnabled(m_client.data()->userCanSetNoBorder());
    m_noBorderOperation->setChecked(m_client.data()->noBorder());
    m_minimizeOperation->setEnabled(m_client.data()->isMinimizable());
    m_closeOperation->setEnabled(m_client.data()->isCloseable());

    if (ws->decorationSupportsTabbing()) {
        initTabbingPopups();
    } else {
        delete m_addTabsMenu;
        m_addTabsMenu = 0;
    }
}

void UserActionsMenu::showHideActivityMenu()
{
#ifdef KWIN_BUILD_ACTIVITIES
    const QStringList &openActivities_ = Workspace::self()->openActivities();
    kDebug() << "activities:" << openActivities_.size();
    if (openActivities_.size() < 2) {
        delete m_activityMenu;
        m_activityMenu = 0;
    } else {
        initActivityPopup();
    }
#endif
}

void UserActionsMenu::selectPopupClientTab(QAction* action)
{
    if (!(!m_client.isNull() && m_client.data()->tabGroup()) || !action->data().isValid())
        return;

    if (Client *other = action->data().value<Client*>()) {
        m_client.data()->tabGroup()->setCurrent(other);
        return;
    }

    // failed conversion, try "1" & "2", being prev and next
    int direction = action->data().toInt();
    if (direction == 1)
        m_client.data()->tabGroup()->activatePrev();
    else if (direction == 2)
        m_client.data()->tabGroup()->activateNext();
}

static QString shortCaption(const QString &s)
{
    if (s.length() < 64)
        return s;
    QString ss = s;
    return ss.replace(32,s.length()-64,"...");
}

void UserActionsMenu::rebuildTabListPopup()
{
    Q_ASSERT(m_switchToTabMenu);

    m_switchToTabMenu->clear();
    // whatever happens "0x1" and "0x2" are no heap positions ;-)
    m_switchToTabMenu->addAction(i18nc("Switch to tab -> Previous", "Previous"))->setData(1);
    m_switchToTabMenu->addAction(i18nc("Switch to tab -> Next", "Next"))->setData(2);

    m_switchToTabMenu->addSeparator();

    for (QList<Client*>::const_iterator i = m_client.data()->tabGroup()->clients().constBegin(),
                                        end = m_client.data()->tabGroup()->clients().constEnd(); i != end; ++i) {
        if ((*i)->noBorder() || *i == m_client.data()->tabGroup()->current())
            continue; // cannot tab there anyway
        m_switchToTabMenu->addAction(shortCaption((*i)->caption()))->setData(QVariant::fromValue(*i));
    }

}

void UserActionsMenu::entabPopupClient(QAction* action)
{
    if (m_client.isNull() || !action->data().isValid())
        return;
    Client *other = action->data().value<Client*>();
    if (!Workspace::self()->clientList().contains(other)) // might have been lost betwenn pop-up and selection
        return;
    m_client.data()->tabBehind(other, true);
    if (options->focusPolicyIsReasonable())
        Workspace::self()->requestFocus(m_client.data());
}

void UserActionsMenu::rebuildTabGroupPopup()
{
    Q_ASSERT(m_addTabsMenu);

    m_addTabsMenu->clear();
    QList<Client*> handled;
    const ClientList &clientList = Workspace::self()->clientList();
    for (QList<Client*>::const_iterator i = clientList.constBegin(), end = clientList.constEnd(); i != end; ++i) {
        if (*i == m_client.data() || (*i)->noBorder())
            continue;
        m_addTabsMenu->addAction(shortCaption((*i)->caption()))->setData(QVariant::fromValue(*i));
    }
}

void UserActionsMenu::initTabbingPopups()
{
    bool needTabManagers = false;
    if (m_client.data()->tabGroup() && m_client.data()->tabGroup()->count() > 1) {
        needTabManagers = true;
        if (!m_switchToTabMenu) {
            m_switchToTabMenu = new QMenu(i18n("Switch to Tab"), m_menu);
            m_switchToTabMenu->setFont(KGlobalSettings::menuFont());
            connect(m_switchToTabMenu, SIGNAL(triggered(QAction*)), SLOT(selectPopupClientTab(QAction*)));
            connect(m_switchToTabMenu, SIGNAL(aboutToShow()), SLOT(rebuildTabListPopup()));
            m_menu->insertMenu(m_removeFromTabGroup, m_switchToTabMenu);
        }
    } else {
        delete m_switchToTabMenu;
        m_switchToTabMenu = 0;
    }

    if (!m_addTabsMenu) {
        m_addTabsMenu = new QMenu(i18n("Attach as tab to"), m_menu);
        m_addTabsMenu->setFont(KGlobalSettings::menuFont());
        connect(m_addTabsMenu, SIGNAL(triggered(QAction*)), SLOT(entabPopupClient(QAction*)));
        connect(m_addTabsMenu, SIGNAL(aboutToShow()), SLOT(rebuildTabGroupPopup()));
        m_menu->insertMenu(m_removeFromTabGroup, m_addTabsMenu);
    }

    m_removeFromTabGroup->setVisible(needTabManagers);
    m_closeTabGroup->setVisible(needTabManagers);
}

void UserActionsMenu::initDesktopPopup()
{
    if (m_desktopMenu)
        return;

    m_desktopMenu = new QMenu(m_menu);
    m_desktopMenu->setFont(KGlobalSettings::menuFont());
    connect(m_desktopMenu, SIGNAL(triggered(QAction*)), SLOT(slotSendToDesktop(QAction*)));
    connect(m_desktopMenu, SIGNAL(aboutToShow()), SLOT(desktopPopupAboutToShow()));

    QAction *action = m_desktopMenu->menuAction();
    // set it as the first item
    m_menu->insertAction(m_minimizeOperation, action);
    action->setText(i18n("Move To &Desktop"));
}

void UserActionsMenu::initScreenPopup()
{
    if (m_screenMenu) {
        return;
    }

    m_screenMenu = new QMenu(m_menu);
    m_screenMenu->setFont(KGlobalSettings::menuFont());
    connect(m_screenMenu, SIGNAL(triggered(QAction*)), SLOT(slotSendToScreen(QAction*)));
    connect(m_screenMenu, SIGNAL(aboutToShow()), SLOT(screenPopupAboutToShow()));

    QAction *action = m_screenMenu->menuAction();
    // set it as the first item after desktop
    m_menu->insertAction(m_activityMenu ? m_activityMenu->menuAction() : m_minimizeOperation, action);
    action->setText(i18n("Move To &Screen"));
}

void UserActionsMenu::initActivityPopup()
{
    if (m_activityMenu)
        return;

    m_activityMenu = new QMenu(m_menu);
    m_activityMenu->setFont(KGlobalSettings::menuFont());
    connect(m_activityMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(slotToggleOnActivity(QAction*)));
    connect(m_activityMenu, SIGNAL(aboutToShow()),
            this, SLOT(activityPopupAboutToShow()));

    QAction *action = m_activityMenu->menuAction();
    // set it as the first item
    m_menu->insertAction(m_minimizeOperation, action);
    action->setText(i18n("Ac&tivities"));   //FIXME is that a good string?
}

void UserActionsMenu::desktopPopupAboutToShow()
{
    if (!m_desktopMenu)
        return;
    const Workspace *ws = Workspace::self();

    m_desktopMenu->clear();
    QActionGroup *group = new QActionGroup(m_desktopMenu);
    QAction *action = m_desktopMenu->addAction(i18n("&All Desktops"));
    action->setData(0);
    action->setCheckable(true);
    group->addAction(action);

    if (!m_client.isNull() && m_client.data()->isOnAllDesktops())
        action->setChecked(true);
    m_desktopMenu->addSeparator();

    const int BASE = 10;
    for (int i = 1; i <= ws->numberOfDesktops(); ++i) {
        QString basic_name("%1  %2");
        if (i < BASE) {
            basic_name.prepend('&');
        }
        action = m_desktopMenu->addAction(basic_name.arg(i).arg(ws->desktopName(i).replace('&', "&&")));
        action->setData(i);
        action->setCheckable(true);
        group->addAction(action);

        if (!m_client.isNull() &&
                !m_client.data()->isOnAllDesktops() && m_client.data()->desktop()  == i)
            action->setChecked(true);
    }

    m_desktopMenu->addSeparator();
    action = m_desktopMenu->addAction(i18nc("Create a new desktop and move there the window", "&New Desktop"));
    action->setData(ws->numberOfDesktops() + 1);

    if (ws->numberOfDesktops() >= Workspace::self()->maxNumberOfDesktops())
        action->setEnabled(false);
}

void UserActionsMenu::screenPopupAboutToShow()
{
    if (!m_screenMenu) {
        return;
    }

    m_screenMenu->clear();
    QActionGroup *group = new QActionGroup(m_screenMenu);

    for (int i = 0; i<Workspace::self()->numScreens(); ++i) {
        // TODO: retrieve the screen name?
        // assumption: there are not more than 9 screens attached.
        QAction *action = m_screenMenu->addAction(i18nc("@item:inmenu List of all Screens to send a window to",
                                                        "Screen &%1", (i+1)));
        action->setData(i);
        action->setCheckable(true);
        if (!m_client.isNull() && i == m_client.data()->screen()) {
            action->setChecked(true);
        }
        group->addAction(action);
    }
}

void UserActionsMenu::activityPopupAboutToShow()
{
    if (!m_activityMenu)
        return;

#ifdef KWIN_BUILD_ACTIVITIES
    m_activityMenu->clear();
    QAction *action = m_activityMenu->addAction(i18n("&All Activities"));
    action->setData(QString());
    action->setCheckable(true);

    if (!m_client.isNull() && m_client.data()->isOnAllActivities())
        action->setChecked(true);
    m_activityMenu->addSeparator();

    foreach (const QString &id, Workspace::self()->openActivities()) {
        KActivities::Info activity(id);
        QString name = activity.name();
        name.replace('&', "&&");
        action = m_activityMenu->addAction(KIcon(activity.icon()), name);
        action->setData(id);
        action->setCheckable(true);

        if (!m_client.isNull() &&
                !m_client.data()->isOnAllActivities() && m_client.data()->isOnActivity(id))
            action->setChecked(true);
    }
#endif
}

void UserActionsMenu::slotWindowOperation(QAction *action)
{
    if (!action->data().isValid())
        return;

    Options::WindowOperation op = static_cast< Options::WindowOperation >(action->data().toInt());
    QWeakPointer<Client> c = (!m_client.isNull()) ? m_client : QWeakPointer<Client>(Workspace::self()->activeClient());
    if (c.isNull())
        return;
    QString type;
    switch(op) {
    case Options::FullScreenOp:
        if (!c.data()->isFullScreen() && c.data()->userCanSetFullScreen())
            type = "fullscreenaltf3";
        break;
    case Options::NoBorderOp:
        if (!c.data()->noBorder() && c.data()->userCanSetNoBorder())
            type = "noborderaltf3";
        break;
    default:
        break;
    };
    if (!type.isEmpty())
        helperDialog(type, c);
    Workspace::self()->performWindowOperation(c.data(), op);
}

void UserActionsMenu::slotSendToDesktop(QAction *action)
{
    int desk = action->data().toInt();
    if (m_client.isNull())
        return;
    Workspace *ws = Workspace::self();
    if (desk == 0) {
        // the 'on_all_desktops' menu entry
        m_client.data()->setOnAllDesktops(!m_client.data()->isOnAllDesktops());
        return;
    } else if (desk > ws->numberOfDesktops()) {
        ws->setNumberOfDesktops(desk);
    }

    ws->sendClientToDesktop(m_client.data(), desk, false);
}

void UserActionsMenu::slotSendToScreen(QAction *action)
{
    const int screen = action->data().toInt();
    if (m_client.isNull()) {
        return;
    }
    Workspace *ws = Workspace::self();
    if (screen >= ws->numScreens()) {
        return;
    }

    ws->sendClientToScreen(m_client.data(), screen);
}

void UserActionsMenu::slotToggleOnActivity(QAction *action)
{
    QString activity = action->data().toString();
    if (m_client.isNull())
        return;
    if (activity.isEmpty()) {
        // the 'on_all_activities' menu entry
        m_client.data()->setOnAllActivities(!m_client.data()->isOnAllActivities());
        return;
    }

    Workspace::self()->toggleClientOnActivity(m_client.data(), activity, false);
}

//****************************************
// Workspace
//****************************************

void Workspace::slotIncreaseWindowOpacity()
{
    if (!active_client) {
        return;
    }
    active_client->setOpacity(qMin(active_client->opacity() + 0.05, 1.0));
}

void Workspace::slotLowerWindowOpacity()
{
    if (!active_client) {
        return;
    }
    active_client->setOpacity(qMax(active_client->opacity() - 0.05, 0.05));
}

void Workspace::closeActivePopup()
{
    if (active_popup) {
        active_popup->close();
        active_popup = NULL;
        active_popup_client = NULL;
    }
    m_userActionsMenu->close();
}

/*!
  Create the global accel object \c keys.
 */
void Workspace::initShortcuts()
{
    keys = new KActionCollection(this);
    KActionCollection* actionCollection = keys;
    QAction* a = 0L;

    // a separate KActionCollection is needed for the shortcut for disabling global shortcuts,
    // otherwise it would also disable itself
    disable_shortcuts_keys = new KActionCollection(this);
    // TODO: PORT ME (KGlobalAccel related)
    // FIXME KAccel port... needed?
    //disable_shortcuts_keys->disableBlocking( true );
#define IN_KWIN
#include "kwinbindings.cpp"
#ifdef KWIN_BUILD_TABBOX
    if (tab_box) {
        tab_box->initShortcuts(actionCollection);
    }
#endif
    m_userActionsMenu->discard(); // so that it's recreated next time
}

void Workspace::setupWindowShortcut(Client* c)
{
    assert(client_keys_dialog == NULL);
    // TODO: PORT ME (KGlobalAccel related)
    //keys->setEnabled( false );
    //disable_shortcuts_keys->setEnabled( false );
    //client_keys->setEnabled( false );
    client_keys_dialog = new ShortcutDialog(c->shortcut().primary());
    client_keys_client = c;
    connect(client_keys_dialog, SIGNAL(dialogDone(bool)), SLOT(setupWindowShortcutDone(bool)));
    QRect r = clientArea(ScreenArea, c);
    QSize size = client_keys_dialog->sizeHint();
    QPoint pos = c->pos() + c->clientPos();
    if (pos.x() + size.width() >= r.right())
        pos.setX(r.right() - size.width());
    if (pos.y() + size.height() >= r.bottom())
        pos.setY(r.bottom() - size.height());
    client_keys_dialog->move(pos);
    client_keys_dialog->show();
    active_popup = client_keys_dialog;
    active_popup_client = c;
}

void Workspace::setupWindowShortcutDone(bool ok)
{
//    keys->setEnabled( true );
//    disable_shortcuts_keys->setEnabled( true );
//    client_keys->setEnabled( true );
    if (ok)
        client_keys_client->setShortcut(KShortcut(client_keys_dialog->shortcut()).toString());
    closeActivePopup();
    client_keys_dialog->deleteLater();
    client_keys_dialog = NULL;
    client_keys_client = NULL;
}

void Workspace::clientShortcutUpdated(Client* c)
{
    QString key = QString("_k_session:%1").arg(c->window());
    QAction* action = client_keys->action(key.toLatin1().constData());
    if (!c->shortcut().isEmpty()) {
        if (action == NULL) { // new shortcut
            action = client_keys->addAction(QString(key));
            action->setText(i18n("Activate Window (%1)", c->caption()));
            connect(action, SIGNAL(triggered(bool)), c, SLOT(shortcutActivated()));
        }

        KAction *kaction = qobject_cast<KAction*>(action);
        // no autoloading, since it's configured explicitly here and is not meant to be reused
        // (the key is the window id anyway, which is kind of random)
        kaction->setGlobalShortcut(
            c->shortcut(), KAction::ActiveShortcut, KAction::NoAutoloading);
        kaction->setEnabled(true);
    } else {
        KAction *kaction = qobject_cast<KAction*>(action);
        if (kaction) {
            kaction->forgetGlobalShortcut();
        }
        delete action;
    }
}

void Workspace::performWindowOperation(Client* c, Options::WindowOperation op)
{
    if (!c)
        return;
    if (op == Options::MoveOp || op == Options::UnrestrictedMoveOp)
        QCursor::setPos(c->geometry().center());
    if (op == Options::ResizeOp || op == Options::UnrestrictedResizeOp)
        QCursor::setPos(c->geometry().bottomRight());
    switch(op) {
    case Options::MoveOp:
        c->performMouseCommand(Options::MouseMove, cursorPos());
        break;
    case Options::UnrestrictedMoveOp:
        c->performMouseCommand(Options::MouseUnrestrictedMove, cursorPos());
        break;
    case Options::ResizeOp:
        c->performMouseCommand(Options::MouseResize, cursorPos());
        break;
    case Options::UnrestrictedResizeOp:
        c->performMouseCommand(Options::MouseUnrestrictedResize, cursorPos());
        break;
    case Options::CloseOp:
        QMetaObject::invokeMethod(c, "closeWindow", Qt::QueuedConnection);
        break;
    case Options::MaximizeOp:
        c->maximize(c->maximizeMode() == Client::MaximizeFull
                    ? Client::MaximizeRestore : Client::MaximizeFull);
        break;
    case Options::HMaximizeOp:
        c->maximize(c->maximizeMode() ^ Client::MaximizeHorizontal);
        break;
    case Options::VMaximizeOp:
        c->maximize(c->maximizeMode() ^ Client::MaximizeVertical);
        break;
    case Options::RestoreOp:
        c->maximize(Client::MaximizeRestore);
        break;
    case Options::MinimizeOp:
        c->minimize();
        break;
    case Options::ShadeOp:
        c->performMouseCommand(Options::MouseShade, cursorPos());
        break;
    case Options::OnAllDesktopsOp:
        c->setOnAllDesktops(!c->isOnAllDesktops());
        break;
    case Options::FullScreenOp:
        c->setFullScreen(!c->isFullScreen(), true);
        break;
    case Options::NoBorderOp:
        c->setNoBorder(!c->noBorder());
        break;
    case Options::KeepAboveOp: {
        StackingUpdatesBlocker blocker(this);
        bool was = c->keepAbove();
        c->setKeepAbove(!c->keepAbove());
        if (was && !c->keepAbove())
            raiseClient(c);
        break;
    }
    case Options::KeepBelowOp: {
        StackingUpdatesBlocker blocker(this);
        bool was = c->keepBelow();
        c->setKeepBelow(!c->keepBelow());
        if (was && !c->keepBelow())
            lowerClient(c);
        break;
    }
    case Options::OperationsOp:
        c->performMouseCommand(Options::MouseShade, cursorPos());
        break;
    case Options::WindowRulesOp:
        editWindowRules(c, false);
        break;
    case Options::ApplicationRulesOp:
        editWindowRules(c, true);
        break;
    case Options::SetupWindowShortcutOp:
        setupWindowShortcut(c);
        break;
    case Options::LowerOp:
        lowerClient(c);
        break;
    case Options::TabDragOp: // Handled by decoration itself
    case Options::NoOp:
        break;
    case Options::RemoveTabFromGroupOp:
        if (c->untab(c->geometry().translated(cascadeOffset(c))) && options->focusPolicyIsReasonable())
             takeActivity(c, ActivityFocus | ActivityRaise, true);
        break;
    case Options::ActivateNextTabOp:
        if (c->tabGroup())
            c->tabGroup()->activateNext();
        break;
    case Options::ActivatePreviousTabOp:
        if (c->tabGroup())
            c->tabGroup()->activatePrev();
        break;
    case Options::CloseTabGroupOp:
        c->tabGroup()->closeAll();
        break;
    }
}

/**
 * Called by the decoration in the new API to determine what buttons the user has configured for
 * window tab dragging and the operations menu.
 */
Options::WindowOperation Client::mouseButtonToWindowOperation(Qt::MouseButtons button)
{
    Options::MouseCommand com = Options::MouseNothing;
    bool active = isActive();
    if (!wantsInput())   // we cannot be active, use it anyway
        active = true;

    if (button == Qt::LeftButton)
        com = active ? options->commandActiveTitlebar1() : options->commandInactiveTitlebar1();
    else if (button == Qt::MidButton)
        com = active ? options->commandActiveTitlebar2() : options->commandInactiveTitlebar2();
    else if (button == Qt::RightButton)
        com = active ? options->commandActiveTitlebar3() : options->commandInactiveTitlebar3();

    // TODO: Complete the list
    if (com == Options::MouseDragTab)
        return Options::TabDragOp;
    if (com == Options::MouseOperationsMenu)
        return Options::OperationsOp;
    return Options::NoOp;
}

/*!
  Performs a mouse command on this client (see options.h)
 */
bool Client::performMouseCommand(Options::MouseCommand command, const QPoint &globalPos, bool handled)
{
    bool replay = false;
    switch(command) {
    case Options::MouseRaise:
        workspace()->raiseClient(this);
        break;
    case Options::MouseLower: {
        workspace()->lowerClient(this);
        // used to be activateNextClient(this), then topClientOnDesktop
        // since this is a mouseOp it's however safe to use the client under the mouse instead
        if (isActive() && options->focusPolicyIsReasonable()) {
            Client *next = workspace()->clientUnderMouse(screen());
            if (next && next != this)
                workspace()->requestFocus(next, false);
        }
        break;
    }
    case Options::MouseShade :
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
    case Options::MouseOperationsMenu:
        if (isActive() && options->isClickRaise())
            autoRaise();
        workspace()->showWindowMenu(globalPos, this);
        break;
    case Options::MouseToggleRaiseAndLower:
        workspace()->raiseOrLowerClient(this);
        break;
    case Options::MouseActivateAndRaise: {
        replay = isActive(); // for clickraise mode
        bool mustReplay = !rules()->checkAcceptFocus(input);
        if (mustReplay) {
            ToplevelList::const_iterator  it = workspace()->stackingOrder().constEnd(),
                                     begin = workspace()->stackingOrder().constBegin();
            while (mustReplay && --it != begin && *it != this) {
                Client *c = qobject_cast<Client*>(*it);
                if (!c || (c->keepAbove() && !keepAbove()) || (keepBelow() && !c->keepBelow()))
                    continue; // can never raise above "it"
                mustReplay = !(c->isOnCurrentDesktop() && c->isOnCurrentActivity() && c->geometry().intersects(geometry()));
            }
        }
        workspace()->takeActivity(this, ActivityFocus | ActivityRaise, handled && replay);
        workspace()->setActiveScreenMouse(globalPos);
        replay = replay || mustReplay;
        break;
    }
    case Options::MouseActivateAndLower:
        workspace()->requestFocus(this);
        workspace()->lowerClient(this);
        workspace()->setActiveScreenMouse(globalPos);
        replay = replay || !rules()->checkAcceptFocus(input);
        break;
    case Options::MouseActivate:
        replay = isActive(); // for clickraise mode
        workspace()->takeActivity(this, ActivityFocus, handled && replay);
        workspace()->setActiveScreenMouse(globalPos);
        replay = replay || !rules()->checkAcceptFocus(input);
        break;
    case Options::MouseActivateRaiseAndPassClick:
        workspace()->takeActivity(this, ActivityFocus | ActivityRaise, handled);
        workspace()->setActiveScreenMouse(globalPos);
        replay = true;
        break;
    case Options::MouseActivateAndPassClick:
        workspace()->takeActivity(this, ActivityFocus, handled);
        workspace()->setActiveScreenMouse(globalPos);
        replay = true;
        break;
    case Options::MouseActivateRaiseAndMove:
    case Options::MouseActivateRaiseAndUnrestrictedMove:
        workspace()->raiseClient(this);
        workspace()->requestFocus(this);
        workspace()->setActiveScreenMouse(globalPos);
        // fallthrough
    case Options::MouseMove:
    case Options::MouseUnrestrictedMove: {
        if (!isMovableAcrossScreens())
            break;
        if (moveResizeMode)
            finishMoveResize(false);
        mode = PositionCenter;
        buttonDown = true;
        moveOffset = QPoint(globalPos.x() - x(), globalPos.y() - y());  // map from global
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        unrestrictedMoveResize = (command == Options::MouseActivateRaiseAndUnrestrictedMove
                                  || command == Options::MouseUnrestrictedMove);
        if (!startMoveResize())
            buttonDown = false;
        updateCursor();
        break;
    }
    case Options::MouseResize:
    case Options::MouseUnrestrictedResize: {
        if (!isResizable() || isShade())
            break;
        if (moveResizeMode)
            finishMoveResize(false);
        buttonDown = true;
        moveOffset = QPoint(globalPos.x() - x(), globalPos.y() - y());  // map from global
        int x = moveOffset.x(), y = moveOffset.y();
        bool left = x < width() / 3;
        bool right = x >= 2 * width() / 3;
        bool top = y < height() / 3;
        bool bot = y >= 2 * height() / 3;
        if (top)
            mode = left ? PositionTopLeft : (right ? PositionTopRight : PositionTop);
        else if (bot)
            mode = left ? PositionBottomLeft : (right ? PositionBottomRight : PositionBottom);
        else
            mode = (x < width() / 2) ? PositionLeft : PositionRight;
        invertedMoveOffset = rect().bottomRight() - moveOffset;
        unrestrictedMoveResize = (command == Options::MouseUnrestrictedResize);
        if (!startMoveResize())
            buttonDown = false;
        updateCursor();
        break;
    }
    case Options::MouseMaximize:
        maximize(Client::MaximizeFull);
        break;
    case Options::MouseRestore:
        maximize(Client::MaximizeRestore);
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
    case Options::MouseDragTab:
    case Options::MouseNothing:
        replay = true;
        break;
    }
    return replay;
}

// KDE4 remove me
void Workspace::showWindowMenuAt(unsigned long, int, int)
{
    slotWindowOperations();
}

void Workspace::slotActivateAttentionWindow()
{
    if (attention_chain.count() > 0)
        activateClient(attention_chain.first());
}

void Workspace::slotSwitchDesktopNext()
{
    int d = currentDesktop() + 1;
    if (d > numberOfDesktops()) {
        if (options->isRollOverDesktops()) {
            d = 1;
        } else {
            return;
        }
    }
    setCurrentDesktop(d);
}

void Workspace::slotSwitchDesktopPrevious()
{
    int d = currentDesktop() - 1;
    if (d <= 0) {
        if (options->isRollOverDesktops())
            d = numberOfDesktops();
        else
            return;
    }
    setCurrentDesktop(d);
}

void Workspace::slotSwitchDesktopRight()
{
    int desktop = desktopToRight(currentDesktop(), options->isRollOverDesktops());
    if (desktop == currentDesktop())
        return;
    setCurrentDesktop(desktop);
}

void Workspace::slotSwitchDesktopLeft()
{
    int desktop = desktopToLeft(currentDesktop(), options->isRollOverDesktops());
    if (desktop == currentDesktop())
        return;
    setCurrentDesktop(desktop);
}

void Workspace::slotSwitchDesktopUp()
{
    int desktop = desktopAbove(currentDesktop(), options->isRollOverDesktops());
    if (desktop == currentDesktop())
        return;
    setCurrentDesktop(desktop);
}

void Workspace::slotSwitchDesktopDown()
{
    int desktop = desktopBelow(currentDesktop(), options->isRollOverDesktops());
    if (desktop == currentDesktop())
        return;
    setCurrentDesktop(desktop);
}

static int senderValue(QObject *sender)
{
    QAction *act = qobject_cast<QAction*>(sender);
    bool ok = false; int i = -1;
    if (act)
        i = act->data().toUInt(&ok);
    if (ok)
        return i;
    return -1;
}

void Workspace::slotSwitchToDesktop()
{
    const int i = senderValue(sender());
    if (i > 0)
        setCurrentDesktop(i);
}

#define USABLE_ACTIVE_CLIENT (active_client && !(active_client->isDesktop() || active_client->isDock()))

void Workspace::slotWindowToDesktop()
{
    if (USABLE_ACTIVE_CLIENT) {
        const int i = senderValue(sender());
        if (i < 1)
            return;

        if (i >= 1 && i <= numberOfDesktops())
            sendClientToDesktop(active_client, i, true);
    }
}

void Workspace::slotSwitchToScreen()
{
    const int i = senderValue(sender());
    if (i > -1)
        setCurrentScreen(i);
}

void Workspace::slotSwitchToNextScreen()
{
    setCurrentScreen((activeScreen() + 1) % numScreens());
}

void Workspace::slotWindowToScreen()
{
    if (USABLE_ACTIVE_CLIENT) {
        const int i = senderValue(sender());
        if (i < 0)
            return;
        if (i >= 0 && i <= numScreens()) {
            sendClientToScreen(active_client, i);
        }
    }
}

void Workspace::slotWindowToNextScreen()
{
    if (USABLE_ACTIVE_CLIENT)
        sendClientToScreen(active_client, (active_client->screen() + 1) % numScreens());
}

/*!
  Maximizes the popup client
 */
void Workspace::slotWindowMaximize()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::MaximizeOp);
}

/*!
  Maximizes the popup client vertically
 */
void Workspace::slotWindowMaximizeVertical()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::VMaximizeOp);
}

/*!
  Maximizes the popup client horiozontally
 */
void Workspace::slotWindowMaximizeHorizontal()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::HMaximizeOp);
}


/*!
  Minimizes the popup client
 */
void Workspace::slotWindowMinimize()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::MinimizeOp);
}

/*!
  Shades/unshades the popup client respectively
 */
void Workspace::slotWindowShade()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::ShadeOp);
}

/*!
  Raises the popup client
 */
void Workspace::slotWindowRaise()
{
    if (USABLE_ACTIVE_CLIENT)
        raiseClient(active_client);
}

/*!
  Lowers the popup client
 */
void Workspace::slotWindowLower()
{
    if (USABLE_ACTIVE_CLIENT) {
        lowerClient(active_client);
        // As this most likely makes the window no longer visible change the
        // keyboard focus to the next available window.
        //activateNextClient( c ); // Doesn't work when we lower a child window
        if (active_client->isActive() && options->focusPolicyIsReasonable()) {
            if (options->isNextFocusPrefersMouse()) {
                Client *next = clientUnderMouse(active_client->screen());
                if (next && next != active_client)
                    requestFocus(next, false);
            } else {
                activateClient(topClientOnDesktop(currentDesktop(), -1));
            }
        }
    }
}

/*!
  Does a toggle-raise-and-lower on the popup client;
  */
void Workspace::slotWindowRaiseOrLower()
{
    if (USABLE_ACTIVE_CLIENT)
        raiseOrLowerClient(active_client);
}

void Workspace::slotWindowOnAllDesktops()
{
    if (USABLE_ACTIVE_CLIENT)
        active_client->setOnAllDesktops(!active_client->isOnAllDesktops());
}

void Workspace::slotWindowFullScreen()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::FullScreenOp);
}

void Workspace::slotWindowNoBorder()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::NoBorderOp);
}

void Workspace::slotWindowAbove()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::KeepAboveOp);
}

void Workspace::slotWindowBelow()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::KeepBelowOp);
}
void Workspace::slotSetupWindowShortcut()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::SetupWindowShortcutOp);
}

/*!
  Toggles show desktop
 */
void Workspace::slotToggleShowDesktop()
{
    setShowingDesktop(!showingDesktop());
}

/*!
  Move window to next desktop
 */
void Workspace::slotWindowToNextDesktop()
{
    if (USABLE_ACTIVE_CLIENT)
        windowToNextDesktop(active_client);
}

void Workspace::windowToNextDesktop(Client* c)
{
    int d = currentDesktop() + 1;
    if (d > numberOfDesktops())
        d = 1;
    if (c && !c->isDesktop()
            && !c->isDock()) {
        setClientIsMoving(c);
        setCurrentDesktop(d);
        setClientIsMoving(NULL);
    }
}

/*!
  Move window to previous desktop
 */
void Workspace::slotWindowToPreviousDesktop()
{
    if (USABLE_ACTIVE_CLIENT)
        windowToPreviousDesktop(active_client);
}

void Workspace::windowToPreviousDesktop(Client* c)
{
    int d = currentDesktop() - 1;
    if (d <= 0)
        d = numberOfDesktops();
    if (c && !c->isDesktop()
            && !c->isDock()) {
        setClientIsMoving(c);
        setCurrentDesktop(d);
        setClientIsMoving(NULL);
    }
}

void Workspace::slotWindowToDesktopRight()
{
    if (USABLE_ACTIVE_CLIENT) {
        int d = desktopToRight(currentDesktop(), options->isRollOverDesktops());
        if (d == currentDesktop())
            return;

        setClientIsMoving(active_client);
        setCurrentDesktop(d);
        setClientIsMoving(NULL);
    }
}

void Workspace::slotWindowToDesktopLeft()
{
    if (USABLE_ACTIVE_CLIENT) {
        int d = desktopToLeft(currentDesktop(), options->isRollOverDesktops());
        if (d == currentDesktop())
            return;

        setClientIsMoving(active_client);
        setCurrentDesktop(d);
        setClientIsMoving(NULL);
    }
}

void Workspace::slotWindowToDesktopUp()
{
    if (USABLE_ACTIVE_CLIENT) {
        int d = desktopAbove(currentDesktop(), options->isRollOverDesktops());
        if (d == currentDesktop())
            return;

        setClientIsMoving(active_client);
        setCurrentDesktop(d);
        setClientIsMoving(NULL);
    }
}

void Workspace::slotWindowToDesktopDown()
{
    if (USABLE_ACTIVE_CLIENT) {
        int d = desktopBelow(currentDesktop(), options->isRollOverDesktops());
        if (d == currentDesktop())
            return;

        setClientIsMoving(active_client);
        setCurrentDesktop(d);
        setClientIsMoving(NULL);
    }
}

void Workspace::slotActivateNextTab()
{
    if (active_client && active_client->tabGroup())
        active_client->tabGroup()->activateNext();
}

void Workspace::slotActivatePrevTab()
{
    if (active_client && active_client->tabGroup())
        active_client->tabGroup()->activatePrev();
}

void Workspace::slotUntab()
{
    if (active_client)
        active_client->untab(active_client->geometry().translated(cascadeOffset(active_client)));
}

/*!
  Kill Window feature, similar to xkill
 */
void Workspace::slotKillWindow()
{
    KillWindow kill(this);
    kill.start();
}

/*!
  Switches to the nearest window in given direction
 */
void Workspace::switchWindow(Direction direction)
{
    if (!active_client)
        return;
    Client *c = active_client;
    Client *switchTo = 0;
    int bestScore = 0;
    int d = c->desktop();
    // Centre of the active window
    QPoint curPos(c->pos().x() + c->geometry().width() / 2,
                  c->pos().y() + c->geometry().height() / 2);

    ToplevelList clist = stackingOrder();
    for (ToplevelList::Iterator i = clist.begin(); i != clist.end(); ++i) {
        Client *client = qobject_cast<Client*>(*i);
        if (!client) {
            continue;
        }
        if (client->wantsTabFocus() && *i != c &&
                client->desktop() == d && !client->isMinimized() && (*i)->isOnCurrentActivity()) {
            // Centre of the other window
            QPoint other(client->pos().x() + client->geometry().width() / 2,
                         client->pos().y() + client->geometry().height() / 2);

            int distance;
            int offset;
            switch(direction) {
            case DirectionNorth:
                distance = curPos.y() - other.y();
                offset = qAbs(other.x() - curPos.x());
                break;
            case DirectionEast:
                distance = other.x() - curPos.x();
                offset = qAbs(other.y() - curPos.y());
                break;
            case DirectionSouth:
                distance = other.y() - curPos.y();
                offset = qAbs(other.x() - curPos.x());
                break;
            case DirectionWest:
                distance = curPos.x() - other.x();
                offset = qAbs(other.y() - curPos.y());
                break;
            default:
                distance = -1;
                offset = -1;
            }

            if (distance > 0) {
                // Inverse score
                int score = distance + offset + ((offset * offset) / distance);
                if (score < bestScore || !switchTo) {
                    switchTo = client;
                    bestScore = score;
                }
            }
        }
    }
    if (switchTo) {
        if (switchTo->tabGroup())
            switchTo = switchTo->tabGroup()->current();
        activateClient(switchTo);
    }
}

/*!
  Switches to upper window
 */
void Workspace::slotSwitchWindowUp()
{
    switchWindow(DirectionNorth);
}

/*!
  Switches to lower window
 */
void Workspace::slotSwitchWindowDown()
{
    switchWindow(DirectionSouth);
}

/*!
  Switches to window on the right
 */
void Workspace::slotSwitchWindowRight()
{
    switchWindow(DirectionEast);
}

/*!
  Switches to window on the left
 */
void Workspace::slotSwitchWindowLeft()
{
    switchWindow(DirectionWest);
}

/*!
  Shows the window operations popup menu for the activeClient()
 */
void Workspace::slotWindowOperations()
{
    if (!active_client)
        return;
    QPoint pos = active_client->pos() + active_client->clientPos();
    showWindowMenu(pos.x(), pos.y(), active_client);
}

void Workspace::showWindowMenu(const QRect &pos, Client* cl)
{
    m_userActionsMenu->show(pos, cl);
}

/*!
  Closes the popup client
 */
void Workspace::slotWindowClose()
{
    // TODO: why?
//     if ( tab_box->isVisible())
//         return;
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::CloseOp);
}

/*!
  Starts keyboard move mode for the popup client
 */
void Workspace::slotWindowMove()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::UnrestrictedMoveOp);
}

/*!
  Starts keyboard resize mode for the popup client
 */
void Workspace::slotWindowResize()
{
    if (USABLE_ACTIVE_CLIENT)
        performWindowOperation(active_client, Options::UnrestrictedResizeOp);
}

void Workspace::slotInvertScreen()
{
    bool succeeded = false;

    //BEGIN Xrandr inversion - does atm NOT work with the nvidia blob
    XRRScreenResources *res = XRRGetScreenResources(display(), active_client ? active_client->window() : rootWindow());
    if (res) {
        for (int j = 0; j < res->ncrtc; ++j) {
            XRRCrtcGamma *gamma = XRRGetCrtcGamma(display(), res->crtcs[j]);
            if (gamma && gamma->size) {
                kDebug(1212) << "inverting screen using XRRSetCrtcGamma";
                const int half = gamma->size / 2 + 1;
                unsigned short swap;
                for (int i = 0; i < half; ++i) {
#define INVERT(_C_) swap = gamma->_C_[i]; gamma->_C_[i] = gamma->_C_[gamma->size - 1 - i]; gamma->_C_[gamma->size - 1 - i] = swap
                    INVERT(red);
                    INVERT(green);
                    INVERT(blue);
#undef INVERT
                }
                XRRSetCrtcGamma(display(), res->crtcs[j], gamma);
                XRRFreeGamma(gamma);
                succeeded = true;
            }
        }
        XRRFreeScreenResources(res);
    }
    if (succeeded)
        return;

    //BEGIN XF86VidMode inversion - only works if optionally libXxf86vm is linked
#ifndef KWIN_NO_XF86VM
    int size = 0;
    // TODO: this doesn't work with screen numbers in twinview - probably relevant only for multihead?
    const int scrn = 0; // active_screen
    if (XF86VidModeGetGammaRampSize(display(), scrn, &size)) {
        unsigned short *red, *green, *blue;
        red = new unsigned short[size];
        green = new unsigned short[size];
        blue = new unsigned short[size];
        if (XF86VidModeGetGammaRamp(display(), scrn, size, red, green, blue)) {
            kDebug(1212) << "inverting screen using XF86VidModeSetGammaRamp";
            const int half = size / 2 + 1;
            unsigned short swap;
            for (int i = 0; i < half; ++i) {
                swap = red[i]; red[i] = red[size - 1 - i]; red[size - 1 - i] = swap;
                swap = green[i]; green[i] = green[size - 1 - i]; green[size - 1 - i] = swap;
                swap = blue[i]; blue[i] = blue[size - 1 - i]; blue[size - 1 - i] = swap;
            }
            XF86VidModeSetGammaRamp(display(), scrn, size, red, green, blue);
            succeeded = true;
        }
        delete [] red;
        delete [] green;
        delete [] blue;
    }

    if (succeeded)
        return;
#endif

    //BEGIN effect plugin inversion - atm only works with OpenGL and has an overhead to it
    if (effects) {
        if (Effect *inverter = static_cast<EffectsHandlerImpl*>(effects)->provides(Effect::ScreenInversion)) {
            kDebug(1212) << "inverting screen using Effect plugin";
            QMetaObject::invokeMethod(inverter, "toggleScreenInversion", Qt::DirectConnection);
        }
    }

    if (!succeeded)
        kDebug(1212) << "sorry - neither Xrandr, nor XF86VidModeSetGammaRamp worked and there's no inversion supplying effect plugin either";

}

#undef USABLE_ACTIVE_CLIENT

void Client::setShortcut(const QString& _cut)
{
    QString cut = rules()->checkShortcut(_cut);
    if (cut.isEmpty())
        return setShortcutInternal(KShortcut());
// Format:
// base+(abcdef)<space>base+(abcdef)
// E.g. Alt+Ctrl+(ABCDEF) Win+X,Win+(ABCDEF)
    if (!cut.contains('(') && !cut.contains(')') && !cut.contains(' ')) {
        if (workspace()->shortcutAvailable(KShortcut(cut), this))
            setShortcutInternal(KShortcut(cut));
        else
            setShortcutInternal(KShortcut());
        return;
    }
    QList< KShortcut > keys;
    QStringList groups = cut.split(' ');
    for (QStringList::ConstIterator it = groups.constBegin();
            it != groups.constEnd();
            ++it) {
        QRegExp reg("(.*\\+)\\((.*)\\)");
        if (reg.indexIn(*it) > -1) {
            QString base = reg.cap(1);
            QString list = reg.cap(2);
            for (int i = 0;
                    i < list.length();
                    ++i) {
                KShortcut c(base + list[ i ]);
                if (!c.isEmpty())
                    keys.append(c);
            }
        }
    }
    for (QList< KShortcut >::ConstIterator it = keys.constBegin();
            it != keys.constEnd();
            ++it) {
        if (_shortcut == *it)   // current one is in the list
            return;
    }
    for (QList< KShortcut >::ConstIterator it = keys.constBegin();
            it != keys.constEnd();
            ++it) {
        if (workspace()->shortcutAvailable(*it, this)) {
            setShortcutInternal(*it);
            return;
        }
    }
    setShortcutInternal(KShortcut());
}

void Client::setShortcutInternal(const KShortcut& cut)
{
    if (_shortcut == cut)
        return;
    _shortcut = cut;
    updateCaption();
#if 0
    workspace()->clientShortcutUpdated(this);
#else
    // Workaround for kwin<->kglobalaccel deadlock, when KWin has X grab and the kded
    // kglobalaccel module tries to create the key grab. KWin should preferably grab
    // they keys itself anyway :(.
    QTimer::singleShot(0, this, SLOT(delayedSetShortcut()));
#endif
}

void Client::delayedSetShortcut()
{
    workspace()->clientShortcutUpdated(this);
}

bool Workspace::shortcutAvailable(const KShortcut& cut, Client* ignore) const
{
    // TODO check global shortcuts etc.
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it) {
        if ((*it) != ignore && (*it)->shortcut() == cut)
            return false;
    }
    return true;
}

} // namespace
