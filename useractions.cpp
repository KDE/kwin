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
#include "cursor.h"
#include "client.h"
#include "composite.h"
#include "input.h"
#include "workspace.h"
#include "effects.h"
#include "platform.h"
#include "screens.h"
#include "shell_client.h"
#include "virtualdesktops.h"
#include "scripting/scripting.h"

#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#include <kactivities/info.h>
#endif
#include "appmenu.h"

#include <KProcess>

#include <QAction>
#include <QCheckBox>
#include <QtConcurrentRun>
#include <QPointer>
#include <QPushButton>

#include <KGlobalAccel>
#include <KLocalizedString>
#include <kconfig.h>
#include <QRegExp>
#include <QMenu>
#include <QWidgetAction>
#include <kauthorized.h>

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
    , m_scriptsMenu(NULL)
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
    , m_client(QWeakPointer<AbstractClient>())
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
    return !m_client.isNull() && isShown();
}

void UserActionsMenu::close()
{
    if (!m_menu) {
        return;
    }
    m_menu->close();
    m_client.clear();
}

bool UserActionsMenu::isMenuClient(const AbstractClient *c) const
{
    if (!c || m_client.isNull()) {
        return false;
    }
    return c == m_client.data();
}

void UserActionsMenu::show(const QRect &pos, const QWeakPointer<AbstractClient> &cl)
{
    if (!KAuthorized::authorizeAction(QStringLiteral("kwin_rmb")))
        return;
    if (cl.isNull())
        return;
    if (isShown())   // recursion
        return;
    if (cl.data()->isDesktop()
            || cl.data()->isDock())
        return;

    m_client = cl;
    init();
    Workspace *ws = Workspace::self();
    int x = pos.left();
    int y = pos.bottom();
    const bool needsPopup = kwinApp()->shouldUseWaylandForCompositing();
    if (y == pos.top()) {
        m_client.data()->blockActivityUpdates(true);
        if (needsPopup) {
            m_menu->popup(QPoint(x, y));
        } else {
            m_menu->exec(QPoint(x, y));
        }
        if (!m_client.isNull())
            m_client.data()->blockActivityUpdates(false);
    }
    else {
        m_client.data()->blockActivityUpdates(true);
        QRect area = ws->clientArea(ScreenArea, QPoint(x, y), VirtualDesktopManager::self()->current());
        menuAboutToShow(); // needed for sizeHint() to be correct :-/
        int popupHeight = m_menu->sizeHint().height();
        if (y + popupHeight < area.height()) {
            if (needsPopup) {
                m_menu->popup(QPoint(x, y));
            } else {
                m_menu->exec(QPoint(x, y));
            }
        } else {
            if (needsPopup) {
                m_menu->popup(QPoint(x, pos.top() - popupHeight));
            } else {
                m_menu->exec(QPoint(x, pos.top() - popupHeight));
            }
        }
        if (!m_client.isNull())
            m_client.data()->blockActivityUpdates(true);
    }
}

void UserActionsMenu::grabInput()
{
    m_menu->windowHandle()->setMouseGrabEnabled(true);
    m_menu->windowHandle()->setKeyboardGrabEnabled(true);
}

void UserActionsMenu::helperDialog(const QString& message, const QWeakPointer<AbstractClient> &c)
{
    QStringList args;
    QString type;
    auto shortcut = [](const QString &name) {
        QAction* action = Workspace::self()->findChild<QAction*>(name);
        assert(action != NULL);
        const auto shortcuts = KGlobalAccel::self()->shortcut(action);
        return QStringLiteral("%1 (%2)").arg(action->text())
                             .arg(shortcuts.isEmpty() ? QString() : shortcuts.first().toString(QKeySequence::NativeText));
    };
    if (message == QStringLiteral("noborderaltf3")) {
        args << QStringLiteral("--msgbox") << i18n(
                 "You have selected to show a window without its border.\n"
                 "Without the border, you will not be able to enable the border "
                 "again using the mouse: use the window operations menu instead, "
                 "activated using the %1 keyboard shortcut.",
                 shortcut(QStringLiteral("Window Operations Menu")));
        type = QStringLiteral("altf3warning");
    } else if (message == QLatin1String("fullscreenaltf3")) {
        args << QStringLiteral("--msgbox") << i18n(
                 "You have selected to show a window in fullscreen mode.\n"
                 "If the application itself does not have an option to turn the fullscreen "
                 "mode off you will not be able to disable it "
                 "again using the mouse: use the window operations menu instead, "
                 "activated using the %1 keyboard shortcut.",
                 shortcut(QStringLiteral("Window Operations Menu")));
        type = QStringLiteral("altf3warning");
    } else
        abort();
    if (!type.isEmpty()) {
        KConfig cfg(QStringLiteral("kwin_dialogsrc"));
        KConfigGroup cg(&cfg, "Notification Messages");  // Depends on KMessageBox
        if (!cg.readEntry(type, true))
            return;
        args << QStringLiteral("--dontagain") << QLatin1String("kwin_dialogsrc:") + type;
    }
    if (!c.isNull())
        args << QStringLiteral("--embed") << QString::number(c.data()->windowId());
    QtConcurrent::run([args]() {
        KProcess::startDetached(QStringLiteral("kdialog"), args);
    });
}


QStringList configModules(bool controlCenter)
{
    QStringList args;
    args <<  QStringLiteral("kwindecoration");
    if (controlCenter)
        args << QStringLiteral("kwinoptions");
    else if (KAuthorized::authorizeControlModule(QStringLiteral("kde-kwinoptions.desktop")))
        args << QStringLiteral("kwinactions") << QStringLiteral("kwinfocus") <<  QStringLiteral("kwinmoving") << QStringLiteral("kwinadvanced")
             << QStringLiteral("kwinrules") << QStringLiteral("kwincompositing") << QStringLiteral("kwineffects")
#ifdef KWIN_BUILD_TABBOX
             << QStringLiteral("kwintabbox")
#endif
             << QStringLiteral("kwinscreenedges")
             << QStringLiteral("kwinscripts")
             ;
    return args;
}

void UserActionsMenu::init()
{
    if (m_menu) {
        return;
    }
    m_menu = new QMenu;
    connect(m_menu, &QMenu::aboutToShow, this, &UserActionsMenu::menuAboutToShow);
    connect(m_menu, &QMenu::triggered, this, &UserActionsMenu::slotWindowOperation, Qt::QueuedConnection);

    QMenu *advancedMenu = new QMenu(m_menu);
    connect(advancedMenu, &QMenu::aboutToShow, [this, advancedMenu]() {
        advancedMenu->setPalette(m_client.data()->palette());
    });

    auto setShortcut = [](QAction *action, const QString &actionName) {
        const auto shortcuts = KGlobalAccel::self()->shortcut(Workspace::self()->findChild<QAction*>(actionName));
        if (!shortcuts.isEmpty()) {
            action->setShortcut(shortcuts.first());
        }
    };

    m_moveOperation = advancedMenu->addAction(i18n("&Move"));
    m_moveOperation->setIcon(QIcon::fromTheme(QStringLiteral("transform-move")));
    setShortcut(m_moveOperation, QStringLiteral("Window Move"));
    m_moveOperation->setData(Options::UnrestrictedMoveOp);

    m_resizeOperation = advancedMenu->addAction(i18n("&Resize"));
    setShortcut(m_resizeOperation, QStringLiteral("Window Resize"));
    m_resizeOperation->setData(Options::ResizeOp);

    m_keepAboveOperation = advancedMenu->addAction(i18n("Keep &Above Others"));
    m_keepAboveOperation->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    setShortcut(m_keepAboveOperation, QStringLiteral("Window Above Other Windows"));
    m_keepAboveOperation->setCheckable(true);
    m_keepAboveOperation->setData(Options::KeepAboveOp);

    m_keepBelowOperation = advancedMenu->addAction(i18n("Keep &Below Others"));
    m_keepBelowOperation->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    setShortcut(m_keepBelowOperation, QStringLiteral("Window Below Other Windows"));
    m_keepBelowOperation->setCheckable(true);
    m_keepBelowOperation->setData(Options::KeepBelowOp);

    m_fullScreenOperation = advancedMenu->addAction(i18n("&Fullscreen"));
    m_fullScreenOperation->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
    setShortcut(m_fullScreenOperation, QStringLiteral("Window Fullscreen"));
    m_fullScreenOperation->setCheckable(true);
    m_fullScreenOperation->setData(Options::FullScreenOp);

    m_shadeOperation = advancedMenu->addAction(i18n("&Shade"));
    setShortcut(m_shadeOperation, QStringLiteral("Window Shade"));
    m_shadeOperation->setCheckable(true);
    m_shadeOperation->setData(Options::ShadeOp);

    m_noBorderOperation = advancedMenu->addAction(i18n("&No Border"));
    setShortcut(m_noBorderOperation, QStringLiteral("Window No Border"));
    m_noBorderOperation->setCheckable(true);
    m_noBorderOperation->setData(Options::NoBorderOp);

    advancedMenu->addSeparator();

    m_shortcutOperation = advancedMenu->addAction(i18n("Window Short&cut..."));
    m_shortcutOperation->setIcon(QIcon::fromTheme(QStringLiteral("configure-shortcuts")));
    setShortcut(m_shortcutOperation, QStringLiteral("Setup Window Shortcut"));
    m_shortcutOperation->setData(Options::SetupWindowShortcutOp);

    QAction *action = advancedMenu->addAction(i18n("Special &Window Settings..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-actions")));
    action->setData(Options::WindowRulesOp);
    m_rulesOperation = action;

    action = advancedMenu->addAction(i18n("S&pecial Application Settings..."));
    action->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-actions")));
    action->setData(Options::ApplicationRulesOp);
    m_applicationRulesOperation = action;
    if (!kwinApp()->config()->isImmutable() &&
            !KAuthorized::authorizeControlModules(configModules(true)).isEmpty()) {
        advancedMenu->addSeparator();
        action = advancedMenu->addAction(i18nc("Entry in context menu of window decoration to open the configuration module of KWin",
                                        "Window Manager S&ettings..."));
        action->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
        connect(action, &QAction::triggered, this,
            [this]() {
                // opens the KWin configuration
                QStringList args;
                args << QStringLiteral("--icon") << QStringLiteral("preferences-system-windows");
                const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                            QStringLiteral("kservices5/kwinfocus.desktop"));
                if (!path.isEmpty()) {
                    args << QStringLiteral("--desktopfile") << path;
                }
                args << configModules(false);
                QProcess *p = new Process(this);
                p->setArguments(args);
                p->setProcessEnvironment(kwinApp()->processStartupEnvironment());
                p->setProgram(QStringLiteral("kcmshell5"));
                connect(p, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), p, &QProcess::deleteLater);
                connect(p, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error), this,
                    [p] (QProcess::ProcessError e) {
                        if (e == QProcess::FailedToStart) {
                            qCDebug(KWIN_CORE) << "Failed to start kcmshell5";
                        }
                    }
                );
                p->start();
            }
        );
    }

    m_minimizeOperation = m_menu->addAction(i18n("Mi&nimize"));
    setShortcut(m_minimizeOperation, QStringLiteral("Window Minimize"));
    m_minimizeOperation->setData(Options::MinimizeOp);

    m_maximizeOperation = m_menu->addAction(i18n("Ma&ximize"));
    setShortcut(m_maximizeOperation, QStringLiteral("Window Maximize"));
    m_maximizeOperation->setCheckable(true);
    m_maximizeOperation->setData(Options::MaximizeOp);

    m_menu->addSeparator();

    // Actions for window tabbing
    if (false) {
        m_removeFromTabGroup = m_menu->addAction(i18n("&Untab"));
        setShortcut(m_removeFromTabGroup, QStringLiteral("Untab"));
        m_removeFromTabGroup->setData(Options::RemoveTabFromGroupOp);

        m_closeTabGroup = m_menu->addAction(i18n("Close Entire &Group"));
        m_closeTabGroup->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
        setShortcut(m_closeTabGroup, QStringLiteral("Close TabGroup"));
        m_closeTabGroup->setData(Options::CloseTabGroupOp);

        m_menu->addSeparator();
    }

    m_menu->addSeparator();

    action = m_menu->addMenu(advancedMenu);
    action->setText(i18n("&More Actions"));

    m_menu->addSeparator();

    m_closeOperation = m_menu->addAction(i18n("&Close"));
    m_closeOperation->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    setShortcut(m_closeOperation, QStringLiteral("Window Close"));
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
    m_scriptsMenu = NULL;
}

void UserActionsMenu::menuAboutToShow()
{
    if (m_client.isNull() || !m_menu)
        return;

    if (VirtualDesktopManager::self()->count() == 1) {
        delete m_desktopMenu;
        m_desktopMenu = 0;
    } else {
        initDesktopPopup();
    }
    if (screens()->count() == 1 || (!m_client.data()->isMovable() && !m_client.data()->isMovableAcrossScreens())) {
        delete m_screenMenu;
        m_screenMenu = NULL;
    } else {
        initScreenPopup();
    }

    m_menu->setPalette(m_client.data()->palette());
    m_resizeOperation->setEnabled(m_client.data()->isResizable());
    m_moveOperation->setEnabled(m_client.data()->isMovableAcrossScreens());
    m_maximizeOperation->setEnabled(m_client.data()->isMaximizable());
    m_maximizeOperation->setChecked(m_client.data()->maximizeMode() == MaximizeFull);
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
    m_shortcutOperation->setEnabled(m_client.data()->rules()->checkShortcut(QString()).isNull());

    if (false) {
        initTabbingPopups();
        m_addTabsMenu->setPalette(m_client.data()->palette());
    } else {
        delete m_addTabsMenu;
        m_addTabsMenu = 0;
    }

    // drop the existing scripts menu
    delete m_scriptsMenu;
    m_scriptsMenu = NULL;
    // ask scripts whether they want to add entries for the given Client
    QList<QAction*> scriptActions = Scripting::self()->actionsForUserActionMenu(m_client.data(), m_scriptsMenu);
    if (!scriptActions.isEmpty()) {
        m_scriptsMenu = new QMenu(m_menu);
        m_scriptsMenu->setPalette(m_client.data()->palette());
        m_scriptsMenu->addActions(scriptActions);

        QAction *action = m_scriptsMenu->menuAction();
        // set it as the first item after desktop
        m_menu->insertAction(m_closeOperation, action);
        action->setText(i18n("&Extensions"));
    }

    // disable rules for Wayland windows - dialog is X11 only
    if (qobject_cast<ShellClient*>(m_client.data())) {
        m_rulesOperation->setEnabled(false);
        m_applicationRulesOperation->setEnabled(false);
    } else {
        m_rulesOperation->setEnabled(true);
        m_applicationRulesOperation->setEnabled(true);
    }

    showHideActivityMenu();
}

void UserActionsMenu::showHideActivityMenu()
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return;
    }
    const QStringList &openActivities_ = Activities::self()->running();
    qCDebug(KWIN_CORE) << "activities:" << openActivities_.size();
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
    return ss.replace(32,s.length()-64, QStringLiteral("..."));
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
    Client *other = dynamic_cast<Client*>(action->data().value<AbstractClient*>());
    if (!Workspace::self()->clientList().contains(other)) // might have been lost betwenn pop-up and selection
        return;
    Client *c = dynamic_cast<Client*>(m_client.data());
    if (!c) {
        return;
    }
    c->tabBehind(other, true);
    if (options->focusPolicyIsReasonable())
        Workspace::self()->requestFocus(c);
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
    if (m_addTabsMenu->actions().isEmpty())
        m_addTabsMenu->addAction(i18nc("There's no window available to be attached as tab to this one", "None available"))->setEnabled(false);
}

void UserActionsMenu::initTabbingPopups()
{
    bool needTabManagers = false;
    if (m_client.data()->tabGroup() && m_client.data()->tabGroup()->count() > 1) {
        needTabManagers = true;
        if (!m_switchToTabMenu) {
            m_switchToTabMenu = new QMenu(i18n("Switch to Tab"), m_menu);
            connect(m_switchToTabMenu, &QMenu::triggered,   this, &UserActionsMenu::selectPopupClientTab);
            connect(m_switchToTabMenu, &QMenu::aboutToShow, this, &UserActionsMenu::rebuildTabListPopup);
            m_menu->insertMenu(m_removeFromTabGroup, m_switchToTabMenu);
        }
    } else {
        delete m_switchToTabMenu;
        m_switchToTabMenu = 0;
    }

    if (!m_addTabsMenu) {
        m_addTabsMenu = new QMenu(i18n("&Attach as tab to"), m_menu);
        connect(m_addTabsMenu, &QMenu::triggered,   this, &UserActionsMenu::entabPopupClient);
        connect(m_addTabsMenu, &QMenu::aboutToShow, this, &UserActionsMenu::rebuildTabGroupPopup);
        m_menu->insertMenu(m_removeFromTabGroup, m_addTabsMenu);
    }

    m_addTabsMenu->menuAction()->setEnabled(!m_client.data()->isFullScreen());
    m_removeFromTabGroup->setVisible(needTabManagers);
    m_closeTabGroup->setVisible(needTabManagers);
}

void UserActionsMenu::initDesktopPopup()
{
    if (m_desktopMenu)
        return;

    m_desktopMenu = new QMenu(m_menu);
    connect(m_desktopMenu, &QMenu::triggered,   this, &UserActionsMenu::slotSendToDesktop);
    connect(m_desktopMenu, &QMenu::aboutToShow, this, &UserActionsMenu::desktopPopupAboutToShow);

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
    connect(m_screenMenu, &QMenu::triggered,   this, &UserActionsMenu::slotSendToScreen);
    connect(m_screenMenu, &QMenu::aboutToShow, this, &UserActionsMenu::screenPopupAboutToShow);

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
    connect(m_activityMenu, &QMenu::triggered, this,   &UserActionsMenu::slotToggleOnActivity);
    connect(m_activityMenu, &QMenu::aboutToShow, this, &UserActionsMenu::activityPopupAboutToShow);

    QAction *action = m_activityMenu->menuAction();
    // set it as the first item
    m_menu->insertAction(m_minimizeOperation, action);
    action->setText(i18n("Ac&tivities"));   //FIXME is that a good string?
}

void UserActionsMenu::desktopPopupAboutToShow()
{
    if (!m_desktopMenu)
        return;
    const VirtualDesktopManager *vds = VirtualDesktopManager::self();

    m_desktopMenu->clear();
    m_desktopMenu->setPalette(m_client.data()->palette());
    QActionGroup *group = new QActionGroup(m_desktopMenu);
    QAction *action = m_desktopMenu->addAction(i18n("&All Desktops"));
    action->setData(0);
    action->setCheckable(true);
    group->addAction(action);

    if (!m_client.isNull() && m_client.data()->isOnAllDesktops())
        action->setChecked(true);
    m_desktopMenu->addSeparator();

    const uint BASE = 10;
    for (uint i = 1; i <= vds->count(); ++i) {
        QString basic_name(QStringLiteral("%1  %2"));
        if (i < BASE) {
            basic_name.prepend(QLatin1Char('&'));
        }
        action = m_desktopMenu->addAction(basic_name.arg(i).arg(vds->name(i).replace(QLatin1Char('&'), QStringLiteral("&&"))));
        action->setData(i);
        action->setCheckable(true);
        group->addAction(action);

        if (!m_client.isNull() &&
                !m_client.data()->isOnAllDesktops() && m_client.data()->isOnDesktop(i))
            action->setChecked(true);
    }

    m_desktopMenu->addSeparator();
    action = m_desktopMenu->addAction(i18nc("Create a new desktop and move there the window", "&New Desktop"));
    action->setData(vds->count() + 1);

    if (vds->count() >= vds->maximum())
        action->setEnabled(false);
}

void UserActionsMenu::screenPopupAboutToShow()
{
    if (!m_screenMenu) {
        return;
    }

    m_screenMenu->clear();
    m_screenMenu->setPalette(m_client.data()->palette());
    QActionGroup *group = new QActionGroup(m_screenMenu);

    for (int i = 0; i<screens()->count(); ++i) {
        // assumption: there are not more than 9 screens attached.
        QAction *action = m_screenMenu->addAction(i18nc("@item:inmenu List of all Screens to send a window to. First argument is a number, second the output identifier. E.g. Screen 1 (HDMI1)",
                                                        "Screen &%1 (%2)", (i+1), screens()->name(i)));
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
    if (!Activities::self()) {
        return;
    }
    m_activityMenu->clear();
    m_activityMenu->setPalette(m_client.data()->palette());
    QAction *action = m_activityMenu->addAction(i18n("&All Activities"));
    action->setData(QString());
    action->setCheckable(true);
    static QPointer<QActionGroup> allActivitiesGroup;
    if (!allActivitiesGroup) {
        allActivitiesGroup = new QActionGroup(m_activityMenu);
    }
    allActivitiesGroup->addAction(action);

    if (!m_client.isNull() && m_client.data()->isOnAllActivities())
        action->setChecked(true);
    m_activityMenu->addSeparator();

    foreach (const QString &id, Activities::self()->running()) {
        KActivities::Info activity(id);
        QString name = activity.name();
        name.replace('&', "&&");
        QWidgetAction *action = new QWidgetAction(m_activityMenu);
        QCheckBox *box = new QCheckBox(name, m_activityMenu);
        action->setDefaultWidget(box);
        const QString icon = activity.icon();
        if (!icon.isEmpty())
            box->setIcon(QIcon::fromTheme(icon));
        box->setBackgroundRole(m_activityMenu->backgroundRole());
        box->setForegroundRole(m_activityMenu->foregroundRole());
        box->setPalette(m_activityMenu->palette());
        connect(box, &QCheckBox::clicked, action, &QAction::triggered);
        m_activityMenu->addAction(action);
        action->setData(id);

        if (!m_client.isNull() &&
                !m_client.data()->isOnAllActivities() && m_client.data()->isOnActivity(id))
            box->setChecked(true);
    }
#endif
}

void UserActionsMenu::slotWindowOperation(QAction *action)
{
    if (!action->data().isValid())
        return;

    Options::WindowOperation op = static_cast< Options::WindowOperation >(action->data().toInt());
    QWeakPointer<AbstractClient> c = (!m_client.isNull()) ? m_client : QWeakPointer<AbstractClient>(Workspace::self()->activeClient());
    if (c.isNull())
        return;
    QString type;
    switch(op) {
    case Options::FullScreenOp:
        if (!c.data()->isFullScreen() && c.data()->userCanSetFullScreen())
            type = QStringLiteral("fullscreenaltf3");
        break;
    case Options::NoBorderOp:
        if (!c.data()->noBorder() && c.data()->userCanSetNoBorder())
            type = QStringLiteral("noborderaltf3");
        break;
    default:
        break;
    };
    if (!type.isEmpty())
        helperDialog(type, c);
    // need to delay performing the window operation as we need to have the
    // user actions menu closed before we destroy the decoration. Otherwise Qt crashes
    qRegisterMetaType<Options::WindowOperation>();
    QMetaObject::invokeMethod(workspace(), "performWindowOperation",
                              Qt::QueuedConnection,
                              Q_ARG(KWin::AbstractClient*, c.data()),
                              Q_ARG(Options::WindowOperation, op));
}

void UserActionsMenu::slotSendToDesktop(QAction *action)
{
    bool ok = false;
    uint desk = action->data().toUInt(&ok);
    if (!ok) {
        return;
    }
    if (m_client.isNull())
        return;
    Workspace *ws = Workspace::self();
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    if (desk == 0) {
        // the 'on_all_desktops' menu entry
        m_client.data()->setOnAllDesktops(!m_client.data()->isOnAllDesktops());
        return;
    } else if (desk > vds->count()) {
        vds->setCount(desk);
    }

    ws->sendClientToDesktop(m_client.data(), desk, false);
}

void UserActionsMenu::slotSendToScreen(QAction *action)
{
    const int screen = action->data().toInt();
    if (m_client.isNull()) {
        return;
    }
    if (screen >= screens()->count()) {
        return;
    }

    Workspace::self()->sendClientToScreen(m_client.data(), screen);
}

void UserActionsMenu::slotToggleOnActivity(QAction *action)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return;
    }
    QString activity = action->data().toString();
    if (m_client.isNull())
        return;
    if (activity.isEmpty()) {
        // the 'on_all_activities' menu entry
        m_client.data()->setOnAllActivities(!m_client.data()->isOnAllActivities());
        return;
    }

    Client *c = dynamic_cast<Client*>(m_client.data());
    if (!c) {
        return;
    }

    Activities::self()->toggleClientOnActivity(c, activity, false);
    if (m_activityMenu && m_activityMenu->isVisible() && m_activityMenu->actions().count()) {
        const bool isOnAll = m_client.data()->isOnAllActivities();
        m_activityMenu->actions().at(0)->setChecked(isOnAll);
        if (isOnAll) {
            // toggleClientOnActivity interprets "on all" as "on none" and
            // susequent toggling ("off") would move the client to only that activity.
            // bug #330838 -> set all but "on all" off to "force proper usage"
            for (int i = 1; i < m_activityMenu->actions().count(); ++i) {
                if (QWidgetAction *qwa = qobject_cast<QWidgetAction*>(m_activityMenu->actions().at(i))) {
                    if (QCheckBox *qcb = qobject_cast<QCheckBox*>(qwa->defaultWidget())) {
                        qcb->setChecked(false);
                    }
                }
            }
        }
    }
#else
    Q_UNUSED(action)
#endif
}

//****************************************
// ShortcutDialog
//****************************************
ShortcutDialog::ShortcutDialog(const QKeySequence& cut)
    : _shortcut(cut)
{
    m_ui.setupUi(this);
    m_ui.keySequenceEdit->setKeySequence(cut);
    m_ui.warning->hide();

    // Listen to changed shortcuts
    connect(m_ui.keySequenceEdit, &QKeySequenceEdit::editingFinished, this, &ShortcutDialog::keySequenceChanged);
    connect(m_ui.clearButton, &QToolButton::clicked, [this]{
        _shortcut = QKeySequence();
    });
    m_ui.keySequenceEdit->setFocus();

    setWindowFlags(Qt::Popup | Qt::X11BypassWindowManagerHint);
}

void ShortcutDialog::accept()
{
    QKeySequence seq = shortcut();
    if (!seq.isEmpty()) {
        if (seq[0] == Qt::Key_Escape) {
            reject();
            return;
        }
        if (seq[0] == Qt::Key_Space
        || (seq[0] & Qt::KeyboardModifierMask) == 0) {
            // clear
            m_ui.keySequenceEdit->clear();
            QDialog::accept();
            return;
        }
    }
    QDialog::accept();
}

void ShortcutDialog::done(int r)
{
    QDialog::done(r);
    emit dialogDone(r == Accepted);
}

void ShortcutDialog::keySequenceChanged()
{
    activateWindow(); // where is the kbd focus lost? cause of popup state?
    QKeySequence seq = m_ui.keySequenceEdit->keySequence();
    if (_shortcut == seq)
        return; // don't try to update the same

    if (seq.isEmpty()) { // clear
        _shortcut = seq;
        return;
    }
    if (seq.count() > 1) {
        seq = QKeySequence(seq[0]);
        m_ui.keySequenceEdit->setKeySequence(seq);
    }

    // Check if the key sequence is used currently
    QString sc = seq.toString();
    // NOTICE - seq.toString() & the entries in "conflicting" randomly get invalidated after the next call (if no sc has been set & conflicting isn't empty?!)
    QList<KGlobalShortcutInfo> conflicting = KGlobalAccel::getGlobalShortcutsByKey(seq);
    if (!conflicting.isEmpty()) {
        const KGlobalShortcutInfo &conflict = conflicting.at(0);
        m_ui.warning->setText(i18nc("'%1' is a keyboard shortcut like 'ctrl+w'",
        "<b>%1</b> is already in use", sc));
        m_ui.warning->setToolTip(i18nc("keyboard shortcut '%1' is used by action '%2' in application '%3'",
        "<b>%1</b> is used by %2 in %3", sc, conflict.friendlyName(), conflict.componentFriendlyName()));
        m_ui.warning->show();
        m_ui.keySequenceEdit->setKeySequence(shortcut());
    } else if (seq != _shortcut) {
        m_ui.warning->hide();
        if (QPushButton *ok = m_ui.buttonBox->button(QDialogButtonBox::Ok))
            ok->setFocus();
    }

    _shortcut = seq;
}

QKeySequence ShortcutDialog::shortcut() const
{
    return _shortcut;
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


template <typename Slot>
void Workspace::initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, Slot slot, const QVariant &data)
{
    initShortcut(actionName, description, shortcut, this, slot, data);
}

template <typename T, typename Slot>
void Workspace::initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, T *receiver, Slot slot, const QVariant &data)
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral(KWIN_NAME));
    a->setObjectName(actionName);
    a->setText(description);
    if (data.isValid()) {
        a->setData(data);
    }
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << shortcut);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << shortcut);
    input()->registerShortcut(shortcut, a, receiver, slot);
}

/*!
  Create the global accel object \c keys.
 */
void Workspace::initShortcuts()
{
#define IN_KWIN
#include "kwinbindings.cpp"
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox::self()->initShortcuts();
#endif
    VirtualDesktopManager::self()->initShortcuts();
    m_userActionsMenu->discard(); // so that it's recreated next time
}

void Workspace::setupWindowShortcut(AbstractClient* c)
{
    assert(client_keys_dialog == NULL);
    // TODO: PORT ME (KGlobalAccel related)
    //keys->setEnabled( false );
    //disable_shortcuts_keys->setEnabled( false );
    //client_keys->setEnabled( false );
    client_keys_dialog = new ShortcutDialog(c->shortcut());
    client_keys_client = c;
    connect(client_keys_dialog, &ShortcutDialog::dialogDone, this, &Workspace::setupWindowShortcutDone);
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
        client_keys_client->setShortcut(client_keys_dialog->shortcut().toString());
    closeActivePopup();
    client_keys_dialog->deleteLater();
    client_keys_dialog = NULL;
    client_keys_client = NULL;
    if (active_client)
        active_client->takeFocus();
}

void Workspace::clientShortcutUpdated(AbstractClient* c)
{
    QString key = QStringLiteral("_k_session:%1").arg(c->window());
    QAction* action = findChild<QAction*>(key);
    if (!c->shortcut().isEmpty()) {
        if (action == NULL) { // new shortcut
            action = new QAction(this);
            kwinApp()->platform()->setupActionForGlobalAccel(action);
            action->setProperty("componentName", QStringLiteral(KWIN_NAME));
            action->setObjectName(key);
            action->setText(i18n("Activate Window (%1)", c->caption()));
            connect(action, &QAction::triggered, c, std::bind(&Workspace::activateClient, this, c, true));
        }

        // no autoloading, since it's configured explicitly here and is not meant to be reused
        // (the key is the window id anyway, which is kind of random)
        KGlobalAccel::self()->setShortcut(action, QList<QKeySequence>() << c->shortcut(),
                                          KGlobalAccel::NoAutoloading);
        action->setEnabled(true);
    } else {
        KGlobalAccel::self()->removeAllShortcuts(action);
        delete action;
    }
}

void Workspace::performWindowOperation(AbstractClient* c, Options::WindowOperation op)
{
    if (!c)
        return;
    if (op == Options::MoveOp || op == Options::UnrestrictedMoveOp)
        Cursor::setPos(c->geometry().center());
    if (op == Options::ResizeOp || op == Options::UnrestrictedResizeOp)
        Cursor::setPos(c->geometry().bottomRight());
    switch(op) {
    case Options::MoveOp:
        c->performMouseCommand(Options::MouseMove, Cursor::pos());
        break;
    case Options::UnrestrictedMoveOp:
        c->performMouseCommand(Options::MouseUnrestrictedMove, Cursor::pos());
        break;
    case Options::ResizeOp:
        c->performMouseCommand(Options::MouseResize, Cursor::pos());
        break;
    case Options::UnrestrictedResizeOp:
        c->performMouseCommand(Options::MouseUnrestrictedResize, Cursor::pos());
        break;
    case Options::CloseOp:
        QMetaObject::invokeMethod(c, "closeWindow", Qt::QueuedConnection);
        break;
    case Options::MaximizeOp:
        c->maximize(c->maximizeMode() == MaximizeFull
                    ? MaximizeRestore : MaximizeFull);
        break;
    case Options::HMaximizeOp:
        c->maximize(c->maximizeMode() ^ MaximizeHorizontal);
        break;
    case Options::VMaximizeOp:
        c->maximize(c->maximizeMode() ^ MaximizeVertical);
        break;
    case Options::RestoreOp:
        c->maximize(MaximizeRestore);
        break;
    case Options::MinimizeOp:
        c->minimize();
        break;
    case Options::ShadeOp:
        c->performMouseCommand(Options::MouseShade, Cursor::pos());
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
        c->performMouseCommand(Options::MouseShade, Cursor::pos());
        break;
    case Options::WindowRulesOp:
        RuleBook::self()->edit(c, false);
        break;
    case Options::ApplicationRulesOp:
        RuleBook::self()->edit(c, true);
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
             takeActivity(c, ActivityFocus | ActivityRaise);
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
bool Client::performMouseCommand(Options::MouseCommand command, const QPoint &globalPos)
{
    bool replay = false;
    switch(command) {
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
    default:
        return AbstractClient::performMouseCommand(command, globalPos);
    }
    return replay;
}

void Workspace::slotActivateAttentionWindow()
{
    if (attention_chain.count() > 0)
        activateClient(attention_chain.first());
}

static uint senderValue(QObject *sender)
{
    QAction *act = qobject_cast<QAction*>(sender);
    bool ok = false; uint i = -1;
    if (act)
        i = act->data().toUInt(&ok);
    if (ok)
        return i;
    return -1;
}

#define USABLE_ACTIVE_CLIENT (active_client && !(active_client->isDesktop() || active_client->isDock()))

void Workspace::slotWindowToDesktop(uint i)
{
    if (USABLE_ACTIVE_CLIENT) {
        if (i < 1)
            return;

        if (i >= 1 && i <= VirtualDesktopManager::self()->count())
            sendClientToDesktop(active_client, i, true);
    }
}

static bool screenSwitchImpossible()
{
    if (!screens()->isCurrentFollowsMouse())
        return false;
    QStringList args;
    args << QStringLiteral("--passivepopup") << i18n("The window manager is configured to consider the screen with the mouse on it as active one.\n"
                                     "Therefore it is not possible to switch to a screen explicitly.") << QStringLiteral("20");
    KProcess::startDetached(QStringLiteral("kdialog"), args);
    return true;
}

void Workspace::slotSwitchToScreen()
{
    if (screenSwitchImpossible())
        return;
    const int i = senderValue(sender());
    if (i > -1)
        setCurrentScreen(i);
}

void Workspace::slotSwitchToNextScreen()
{
    if (screenSwitchImpossible())
        return;
    setCurrentScreen((screens()->current() + 1) % screens()->count());
}

void Workspace::slotSwitchToPrevScreen()
{
    if (screenSwitchImpossible())
        return;
    setCurrentScreen((screens()->current() + screens()->count() - 1) % screens()->count());
}

void Workspace::slotWindowToScreen()
{
    if (USABLE_ACTIVE_CLIENT) {
        const int i = senderValue(sender());
        if (i < 0)
            return;
        if (i >= 0 && i <= screens()->count()) {
            sendClientToScreen(active_client, i);
        }
    }
}

void Workspace::slotWindowToNextScreen()
{
    if (USABLE_ACTIVE_CLIENT)
        sendClientToScreen(active_client, (active_client->screen() + 1) % screens()->count());
}

void Workspace::slotWindowToPrevScreen()
{
    if (USABLE_ACTIVE_CLIENT)
        sendClientToScreen(active_client, (active_client->screen() + screens()->count() - 1) % screens()->count());
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
                AbstractClient *next = clientUnderMouse(active_client->screen());
                if (next && next != active_client)
                    requestFocus(next, false);
            } else {
                activateClient(topClientOnDesktop(VirtualDesktopManager::self()->current(), -1));
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

template <typename Direction>
void windowToDesktop(AbstractClient *c)
{
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    Workspace *ws = Workspace::self();
    Direction functor;
    // TODO: why is options->isRollOverDesktops() not honored?
    const auto desktop = functor(nullptr, true);
    if (c && !c->isDesktop()
            && !c->isDock()) {
        ws->setClientIsMoving(c);
        vds->setCurrent(desktop);
        ws->setClientIsMoving(NULL);
    }
}

/*!
  Move window to next desktop
 */
void Workspace::slotWindowToNextDesktop()
{
    if (USABLE_ACTIVE_CLIENT)
        windowToNextDesktop(active_client);
}

void Workspace::windowToNextDesktop(AbstractClient* c)
{
    windowToDesktop<DesktopNext>(c);
}

/*!
  Move window to previous desktop
 */
void Workspace::slotWindowToPreviousDesktop()
{
    if (USABLE_ACTIVE_CLIENT)
        windowToPreviousDesktop(active_client);
}

void Workspace::windowToPreviousDesktop(AbstractClient* c)
{
    windowToDesktop<DesktopPrevious>(c);
}

template <typename Direction>
void activeClientToDesktop()
{
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    Workspace *ws = Workspace::self();
    const int current = vds->current();
    Direction functor;
    const int d = functor(current, options->isRollOverDesktops());
    if (d == current) {
        return;
    }
    ws->setClientIsMoving(ws->activeClient());
    vds->setCurrent(d);
    ws->setClientIsMoving(NULL);
}

void Workspace::slotWindowToDesktopRight()
{
    if (USABLE_ACTIVE_CLIENT) {
        activeClientToDesktop<DesktopRight>();
    }
}

void Workspace::slotWindowToDesktopLeft()
{
    if (USABLE_ACTIVE_CLIENT) {
        activeClientToDesktop<DesktopLeft>();
    }
}

void Workspace::slotWindowToDesktopUp()
{
    if (USABLE_ACTIVE_CLIENT) {
        activeClientToDesktop<DesktopAbove>();
    }
}

void Workspace::slotWindowToDesktopDown()
{
    if (USABLE_ACTIVE_CLIENT) {
        activeClientToDesktop<DesktopBelow>();
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
    if (m_windowKiller.isNull()) {
        m_windowKiller.reset(new KillWindow());
    }
    m_windowKiller->start();
}

/*!
  Switches to the nearest window in given direction
 */
void Workspace::switchWindow(Direction direction)
{
    if (!active_client)
        return;
    AbstractClient *c = active_client;
    int desktopNumber = c->isOnAllDesktops() ? VirtualDesktopManager::self()->current() : c->desktop();

    // Centre of the active window
    QPoint curPos(c->pos().x() + c->geometry().width() / 2,
                  c->pos().y() + c->geometry().height() / 2);

    if (!switchWindow(c, direction, curPos, desktopNumber)) {
        auto opposite = [&] {
            switch(direction) {
            case DirectionNorth:
                return QPoint(curPos.x(), screens()->geometry().height());
            case DirectionSouth:
                return QPoint(curPos.x(), 0);
            case DirectionEast:
                return QPoint(0, curPos.y());
            case DirectionWest:
                return QPoint(screens()->geometry().width(), curPos.y());
            default:
                Q_UNREACHABLE();
            }
        };

        switchWindow(c, direction, opposite(), desktopNumber);
    }
}

bool Workspace::switchWindow(AbstractClient *c, Direction direction, QPoint curPos, int d)
{
    AbstractClient *switchTo = nullptr;
    int bestScore = 0;

    ToplevelList clist = stackingOrder();
    for (ToplevelList::Iterator i = clist.begin(); i != clist.end(); ++i) {
        auto client = qobject_cast<AbstractClient*>(*i);
        if (!client) {
            continue;
        }
        if (client->wantsTabFocus() && *i != c &&
                client->isOnDesktop(d) && !client->isMinimized() && (*i)->isOnCurrentActivity()) {
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

    return switchTo;
}

/*!
  Shows the window operations popup menu for the activeClient()
 */
void Workspace::slotWindowOperations()
{
    if (!active_client)
        return;
    QPoint pos = active_client->pos() + active_client->clientPos();
    showWindowMenu(QRect(pos, pos), active_client);
}

void Workspace::showWindowMenu(const QRect &pos, AbstractClient* cl)
{
    m_userActionsMenu->show(pos, cl);
}

void Workspace::showApplicationMenu(const QRect &pos, AbstractClient *c, int actionId)
{
    ApplicationMenu::self()->showApplicationMenu(c->geometry().topLeft() + pos.bottomLeft(), c, actionId);
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

#undef USABLE_ACTIVE_CLIENT

void AbstractClient::setShortcut(const QString& _cut)
{
    QString cut = rules()->checkShortcut(_cut);
    auto updateShortcut  = [this](const QKeySequence &cut = QKeySequence()) {
        if (_shortcut == cut)
            return;
        _shortcut = cut;
        setShortcutInternal();
    };
    if (cut.isEmpty()) {
        updateShortcut();
        return;
    }
    if (cut == shortcut().toString()) {
        return; // no change
    }
// Format:
// base+(abcdef)<space>base+(abcdef)
// E.g. Alt+Ctrl+(ABCDEF);Meta+X,Meta+(ABCDEF)
    if (!cut.contains(QLatin1Char('(')) && !cut.contains(QLatin1Char(')')) && !cut.contains(QLatin1String(" - "))) {
        if (workspace()->shortcutAvailable(cut, this))
            updateShortcut(QKeySequence(cut));
        else
            updateShortcut();
        return;
    }
    QList< QKeySequence > keys;
    QStringList groups = cut.split(QStringLiteral(" - "));
    for (QStringList::ConstIterator it = groups.constBegin();
            it != groups.constEnd();
            ++it) {
        QRegExp reg(QStringLiteral("(.*\\+)\\((.*)\\)"));
        if (reg.indexIn(*it) > -1) {
            QString base = reg.cap(1);
            QString list = reg.cap(2);
            for (int i = 0;
                    i < list.length();
                    ++i) {
                QKeySequence c(base + list[ i ]);
                if (!c.isEmpty())
                    keys.append(c);
            }
        } else {
            // regexp doesn't match, so it should be a normal shortcut
            QKeySequence c(*it);
            if (!c.isEmpty()) {
                keys.append(c);
            }
        }
    }
    for (auto it = keys.constBegin();
            it != keys.constEnd();
            ++it) {
        if (_shortcut == *it)   // current one is in the list
            return;
    }
    for (auto it = keys.constBegin();
            it != keys.constEnd();
            ++it) {
        if (workspace()->shortcutAvailable(*it, this)) {
            updateShortcut(*it);
            return;
        }
    }
    updateShortcut();
}

void AbstractClient::setShortcutInternal()
{
    updateCaption();
    workspace()->clientShortcutUpdated(this);
}

void Client::setShortcutInternal()
{
    updateCaption();
#if 0
    workspace()->clientShortcutUpdated(this);
#else
    // Workaround for kwin<->kglobalaccel deadlock, when KWin has X grab and the kded
    // kglobalaccel module tries to create the key grab. KWin should preferably grab
    // they keys itself anyway :(.
    QTimer::singleShot(0, this, std::bind(&Workspace::clientShortcutUpdated, workspace(), this));
#endif
}

bool Workspace::shortcutAvailable(const QKeySequence &cut, AbstractClient* ignore) const
{
    if (ignore && cut == ignore->shortcut())
        return true;

    if (!KGlobalAccel::getGlobalShortcutsByKey(cut).isEmpty()) {
        return false;
    }
    for (auto it = m_allClients.constBegin();
            it != m_allClients.constEnd();
            ++it) {
        if ((*it) != ignore && (*it)->shortcut() == cut)
            return false;
    }
    return true;
}

} // namespace
