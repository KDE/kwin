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

#include "client.h"
#include "workspace.h"
#include "effects.h"
#ifdef KWIN_BUILD_TILING
#include "tiling/tile.h"
#include "tiling/tilinglayout.h"
#include "tiling/tiling.h"
#endif

#include <KActivities/Info>

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

//****************************************
// Workspace
//****************************************

QMenu* Workspace::clientPopup()
{
    if (!popup) {
        popup = new QMenu;
        popup->setFont(KGlobalSettings::menuFont());
        connect(popup, SIGNAL(aboutToShow()), this, SLOT(clientPopupAboutToShow()));
        connect(popup, SIGNAL(triggered(QAction*)), this, SLOT(clientPopupActivated(QAction*)));

        advanced_popup = new QMenu(popup);
        advanced_popup->setFont(KGlobalSettings::menuFont());

        mMoveOpAction = advanced_popup->addAction(i18n("&Move"));
        mMoveOpAction->setIcon(KIcon("transform-move"));
        KAction *kaction = qobject_cast<KAction*>(keys->action("Window Move"));
        if (kaction != 0)
            mMoveOpAction->setShortcut(kaction->globalShortcut().primary());
        mMoveOpAction->setData(Options::MoveOp);

        mResizeOpAction = advanced_popup->addAction(i18n("Re&size"));
        kaction = qobject_cast<KAction*>(keys->action("Window Resize"));
        if (kaction != 0)
            mResizeOpAction->setShortcut(kaction->globalShortcut().primary());
        mResizeOpAction->setData(Options::ResizeOp);

        mKeepAboveOpAction = advanced_popup->addAction(i18n("Keep &Above Others"));
        mKeepAboveOpAction->setIcon(KIcon("go-up"));
        kaction = qobject_cast<KAction*>(keys->action("Window Above Other Windows"));
        if (kaction != 0)
            mKeepAboveOpAction->setShortcut(kaction->globalShortcut().primary());
        mKeepAboveOpAction->setCheckable(true);
        mKeepAboveOpAction->setData(Options::KeepAboveOp);

        mKeepBelowOpAction = advanced_popup->addAction(i18n("Keep &Below Others"));
        mKeepBelowOpAction->setIcon(KIcon("go-down"));
        kaction = qobject_cast<KAction*>(keys->action("Window Below Other Windows"));
        if (kaction != 0)
            mKeepBelowOpAction->setShortcut(kaction->globalShortcut().primary());
        mKeepBelowOpAction->setCheckable(true);
        mKeepBelowOpAction->setData(Options::KeepBelowOp);

        mFullScreenOpAction = advanced_popup->addAction(i18n("&Fullscreen"));
        mFullScreenOpAction->setIcon(KIcon("view-fullscreen"));
        kaction = qobject_cast<KAction*>(keys->action("Window Fullscreen"));
        if (kaction != 0)
            mFullScreenOpAction->setShortcut(kaction->globalShortcut().primary());
        mFullScreenOpAction->setCheckable(true);
        mFullScreenOpAction->setData(Options::FullScreenOp);

        mShadeOpAction = advanced_popup->addAction(i18n("Sh&ade"));
        kaction = qobject_cast<KAction*>(keys->action("Window Shade"));
        if (kaction != 0)
            mShadeOpAction->setShortcut(kaction->globalShortcut().primary());
        mShadeOpAction->setCheckable(true);
        mShadeOpAction->setData(Options::ShadeOp);

        mNoBorderOpAction = advanced_popup->addAction(i18n("&No Border"));
        kaction = qobject_cast<KAction*>(keys->action("Window No Border"));
        if (kaction != 0)
            mNoBorderOpAction->setShortcut(kaction->globalShortcut().primary());
        mNoBorderOpAction->setCheckable(true);
        mNoBorderOpAction->setData(Options::NoBorderOp);

        advanced_popup->addSeparator();

        QAction *action = advanced_popup->addAction(i18n("Window &Shortcut..."));
        action->setIcon(KIcon("configure-shortcuts"));
        kaction = qobject_cast<KAction*>(keys->action("Setup Window Shortcut"));
        if (kaction != 0)
            action->setShortcut(kaction->globalShortcut().primary());
        action->setData(Options::SetupWindowShortcutOp);

        action = advanced_popup->addAction(i18n("&Special Window Settings..."));
        action->setIcon(KIcon("preferences-system-windows-actions"));
        action->setData(Options::WindowRulesOp);

        action = advanced_popup->addAction(i18n("S&pecial Application Settings..."));
        action->setIcon(KIcon("preferences-system-windows-actions"));
        action->setData(Options::ApplicationRulesOp);
        if (!KGlobal::config()->isImmutable() &&
                !KAuthorized::authorizeControlModules(Workspace::configModules(true)).isEmpty()) {
            advanced_popup->addSeparator();
            action = advanced_popup->addAction(i18nc("Entry in context menu of window decoration to open the configuration module of KWin",
                                            "Window &Manager Settings..."));
            action->setIcon(KIcon("configure"));
            connect(action, SIGNAL(triggered()), this, SLOT(configureWM()));
        }

        mMinimizeOpAction = popup->addAction(i18n("Mi&nimize"));
        kaction = qobject_cast<KAction*>(keys->action("Window Minimize"));
        if (kaction != 0)
            mMinimizeOpAction->setShortcut(kaction->globalShortcut().primary());
        mMinimizeOpAction->setData(Options::MinimizeOp);

        mMaximizeOpAction = popup->addAction(i18n("Ma&ximize"));
        kaction = qobject_cast<KAction*>(keys->action("Window Maximize"));
        if (kaction != 0)
            mMaximizeOpAction->setShortcut(kaction->globalShortcut().primary());
        mMaximizeOpAction->setCheckable(true);
        mMaximizeOpAction->setData(Options::MaximizeOp);

        popup->addSeparator();

        // Actions for window tabbing
        if (decorationSupportsTabbing()) {
            mRemoveFromTabGroup = popup->addAction(i18n("&Untab"));
            kaction = qobject_cast<KAction*>(keys->action("Untab"));
            if (kaction != 0)
                mRemoveFromTabGroup->setShortcut(kaction->globalShortcut().primary());
            mRemoveFromTabGroup->setData(Options::RemoveTabFromGroupOp);

            mCloseTabGroup = popup->addAction(i18n("Close Entire &Group"));
            mCloseTabGroup->setIcon(KIcon("window-close"));
            kaction = qobject_cast<KAction*>(keys->action("Close TabGroup"));
            if (kaction != 0)
                mCloseTabGroup->setShortcut(kaction->globalShortcut().primary());
            mCloseTabGroup->setData(Options::CloseTabGroupOp);

            popup->addSeparator();
        }

        // create it anyway
        mTilingStateOpAction = popup->addAction(i18nc("When in tiling mode, toggle's the window's floating/tiled state", "&Float Window"));
        // then hide it
        mTilingStateOpAction->setVisible(false);
#ifdef KWIN_BUILD_TILING
        // actions for window tiling
        if (m_tiling->isEnabled()) {
            kaction = qobject_cast<KAction*>(keys->action("Toggle Floating"));
            mTilingStateOpAction->setCheckable(true);
            mTilingStateOpAction->setData(Options::ToggleClientTiledStateOp);
            if (kaction != 0)
                mTilingStateOpAction->setShortcut(kaction->globalShortcut().primary());
        }
#endif

        popup->addSeparator();

        action = popup->addMenu(advanced_popup);
        action->setText(i18n("More Actions"));

        popup->addSeparator();

        mCloseOpAction = popup->addAction(i18n("&Close"));
        mCloseOpAction->setIcon(KIcon("window-close"));
        kaction = qobject_cast<KAction*>(keys->action("Window Close"));
        if (kaction != 0)
            mCloseOpAction->setShortcut(kaction->globalShortcut().primary());
        mCloseOpAction->setData(Options::CloseOp);
    }
    return popup;
}

void Workspace::discardPopup()
{
    delete popup;
    popup = NULL;
    desk_popup = NULL;
    activity_popup = NULL;
    switch_to_tab_popup = NULL;
    add_tabs_popup = NULL;
}

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

/*!
  The client popup menu will become visible soon.

  Adjust the items according to the respective popup client.
 */
void Workspace::clientPopupAboutToShow()
{
    if (!active_popup_client || !popup)
        return;

    if (numberOfDesktops() == 1) {
        delete desk_popup;
        desk_popup = 0;
    } else {
        initDesktopPopup();
    }
    QStringList act = openActivityList();
    kDebug() << "activities:" << act.size();
    if (act.size() < 2) {
        delete activity_popup;
        activity_popup = 0;
    } else {
        initActivityPopup();
    }

    mResizeOpAction->setEnabled(active_popup_client->isResizable());
    mMoveOpAction->setEnabled(active_popup_client->isMovableAcrossScreens());
    mMaximizeOpAction->setEnabled(active_popup_client->isMaximizable());
    mMaximizeOpAction->setChecked(active_popup_client->maximizeMode() == Client::MaximizeFull);
    mShadeOpAction->setEnabled(active_popup_client->isShadeable());
    mShadeOpAction->setChecked(active_popup_client->shadeMode() != ShadeNone);
    mKeepAboveOpAction->setChecked(active_popup_client->keepAbove());
    mKeepBelowOpAction->setChecked(active_popup_client->keepBelow());
    mFullScreenOpAction->setEnabled(active_popup_client->userCanSetFullScreen());
    mFullScreenOpAction->setChecked(active_popup_client->isFullScreen());
    mNoBorderOpAction->setEnabled(active_popup_client->userCanSetNoBorder());
    mNoBorderOpAction->setChecked(active_popup_client->noBorder());
    mMinimizeOpAction->setEnabled(active_popup_client->isMinimizable());
    mCloseOpAction->setEnabled(active_popup_client->isCloseable());

#ifdef KWIN_BUILD_TILING
    if (m_tiling->isEnabled()) {
        int desktop = active_popup_client->desktop();
        if (m_tiling->tilingLayouts().value(desktop)) {
            Tile *t = m_tiling->tilingLayouts()[desktop]->findTile(active_popup_client);
            if (t)
                mTilingStateOpAction->setChecked(t->floating());
        }
    }
    mTilingStateOpAction->setVisible(m_tiling->isEnabled());
#endif

    if (decorationSupportsTabbing()) {
        initTabbingPopups();
    } else {
        delete add_tabs_popup;
        add_tabs_popup = 0;
    }
}

void Workspace::selectPopupClientTab(QAction* action)
{
    if (!(active_popup_client && active_popup_client->tabGroup()) || !action->data().isValid())
        return;

    if (Client *other = action->data().value<Client*>()) {
        active_popup_client->tabGroup()->setCurrent(other);
        return;
    }

    // failed conversion, try "1" & "2", being prev and next
    int direction = action->data().toInt();
    if (direction == 1)
        active_popup_client->tabGroup()->activatePrev();
    else if (direction == 2)
        active_popup_client->tabGroup()->activateNext();
}

static QString shortCaption(const QString &s)
{
    if (s.length() < 64)
        return s;
    QString ss = s;
    return ss.replace(32,s.length()-64,"...");
}

void Workspace::rebuildTabListPopup()
{
    Q_ASSERT(switch_to_tab_popup);

    switch_to_tab_popup->clear();
    // whatever happens "0x1" and "0x2" are no heap positions ;-)
    switch_to_tab_popup->addAction(i18nc("Switch to tab -> Previous", "Previous"))->setData(1);
    switch_to_tab_popup->addAction(i18nc("Switch to tab -> Next", "Next"))->setData(2);

    switch_to_tab_popup->addSeparator();

    for (QList<Client*>::const_iterator i = active_popup_client->tabGroup()->clients().constBegin(),
                                        end = active_popup_client->tabGroup()->clients().constEnd(); i != end; ++i) {
        if ((*i)->noBorder() || *i == active_popup_client->tabGroup()->current())
            continue; // cannot tab there anyway
        switch_to_tab_popup->addAction(shortCaption((*i)->caption()))->setData(QVariant::fromValue(*i));
    }

}

void Workspace::entabPopupClient(QAction* action)
{
    if (!active_popup_client || !action->data().isValid())
        return;
    Client *other = action->data().value<Client*>();
    if (!clients.contains(other)) // might have been lost betwenn pop-up and selection
        return;
    active_popup_client->tabBehind(other, true);
    if (options->focusPolicyIsReasonable())
        requestFocus(active_popup_client);
}

void Workspace::rebuildTabGroupPopup()
{
    Q_ASSERT(add_tabs_popup);

    add_tabs_popup->clear();
    QList<Client*> handled;
    for (QList<Client*>::const_iterator i = clientList().constBegin(), end = clientList().constEnd(); i != end; ++i) {
        if (*i == active_popup_client || (*i)->noBorder())
            continue;
        add_tabs_popup->addAction(shortCaption((*i)->caption()))->setData(QVariant::fromValue(*i));
    }
}

void Workspace::initTabbingPopups()
{
    bool needTabManagers = false;
    if (active_popup_client->tabGroup() && active_popup_client->tabGroup()->count() > 1) {
        needTabManagers = true;
        if (!switch_to_tab_popup) {
            switch_to_tab_popup = new QMenu(i18n("Switch to Tab"), popup);
            switch_to_tab_popup->setFont(KGlobalSettings::menuFont());
            connect(switch_to_tab_popup, SIGNAL(triggered(QAction*)), SLOT(selectPopupClientTab(QAction*)));
            connect(switch_to_tab_popup, SIGNAL(aboutToShow()), SLOT(rebuildTabListPopup()));
            popup->insertMenu(mRemoveFromTabGroup, switch_to_tab_popup);
        }
    } else {
        delete switch_to_tab_popup;
        switch_to_tab_popup = 0;
    }

    if (!add_tabs_popup) {
        add_tabs_popup = new QMenu(i18n("Tab behind"), popup);
        add_tabs_popup->setFont(KGlobalSettings::menuFont());
        connect(add_tabs_popup, SIGNAL(triggered(QAction*)), SLOT(entabPopupClient(QAction*)));
        connect(add_tabs_popup, SIGNAL(aboutToShow()), SLOT(rebuildTabGroupPopup()));
        popup->insertMenu(mRemoveFromTabGroup, add_tabs_popup);
    }

    mRemoveFromTabGroup->setVisible(needTabManagers);
    mCloseTabGroup->setVisible(needTabManagers);
}

void Workspace::initDesktopPopup()
{
    if (desk_popup)
        return;

    desk_popup = new QMenu(popup);
    desk_popup->setFont(KGlobalSettings::menuFont());
    connect(desk_popup, SIGNAL(triggered(QAction*)), SLOT(slotSendToDesktop(QAction*)));
    connect(desk_popup, SIGNAL(aboutToShow()), SLOT(desktopPopupAboutToShow()));

    QAction *action = desk_popup->menuAction();
    // set it as the first item
    popup->insertAction(mMinimizeOpAction, action);
    action->setText(i18n("Move To &Desktop"));
}

/*!
  Creates activity popup.
  I'm going with checkable ones instead of "copy to" and "move to" menus; I *think* it's an easier way.
  Oh, and an 'all' option too of course
 */
void Workspace::initActivityPopup()
{
    if (activity_popup)
        return;

    activity_popup = new QMenu(popup);
    activity_popup->setFont(KGlobalSettings::menuFont());
    connect(activity_popup, SIGNAL(triggered(QAction*)),
            this, SLOT(slotToggleOnActivity(QAction*)));
    connect(activity_popup, SIGNAL(aboutToShow()),
            this, SLOT(activityPopupAboutToShow()));

    QAction *action = activity_popup->menuAction();
    // set it as the first item
    popup->insertAction(mMinimizeOpAction, action);
    action->setText(i18n("Ac&tivities"));   //FIXME is that a good string?
}

/*!
  Adjusts the desktop popup to the current values and the location of
  the popup client.
 */
void Workspace::desktopPopupAboutToShow()
{
    if (!desk_popup)
        return;

    desk_popup->clear();
    QActionGroup *group = new QActionGroup(desk_popup);
    QAction *action = desk_popup->addAction(i18n("&All Desktops"));
    action->setData(0);
    action->setCheckable(true);
    group->addAction(action);

    if (active_popup_client && active_popup_client->isOnAllDesktops())
        action->setChecked(true);
    desk_popup->addSeparator();

    const int BASE = 10;
    for (int i = 1; i <= numberOfDesktops(); i++) {
        QString basic_name("%1  %2");
        if (i < BASE) {
            basic_name.prepend('&');
        }
        action = desk_popup->addAction(basic_name.arg(i).arg(desktopName(i).replace('&', "&&")));
        action->setData(i);
        action->setCheckable(true);
        group->addAction(action);

        if (active_popup_client &&
                !active_popup_client->isOnAllDesktops() && active_popup_client->desktop()  == i)
            action->setChecked(true);
    }
}

/*!
  Adjusts the activity popup to the current values and the location of
  the popup client.
 */
void Workspace::activityPopupAboutToShow()
{
    if (!activity_popup)
        return;

    activity_popup->clear();
    QAction *action = activity_popup->addAction(i18n("&All Activities"));
    action->setData(QString());
    action->setCheckable(true);

    if (active_popup_client && active_popup_client->isOnAllActivities())
        action->setChecked(true);
    activity_popup->addSeparator();

    foreach (const QString & id, openActivityList()) {
        KActivities::Info activity(id);
        QString name = activity.name();
        name.replace('&', "&&");
        action = activity_popup->addAction(KIcon(activity.icon()), name);
        action->setData(id);
        action->setCheckable(true);

        if (active_popup_client &&
                !active_popup_client->isOnAllActivities() && active_popup_client->isOnActivity(id))
            action->setChecked(true);
    }
}

void Workspace::closeActivePopup()
{
    if (active_popup) {
        active_popup->close();
        active_popup = NULL;
        active_popup_client = NULL;
    }
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
#ifdef KWIN_BUILD_TILING
    if (m_tiling) {
        m_tiling->initShortcuts(actionCollection);
    }
#endif
    discardPopup(); // so that it's recreated next time
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

void Workspace::clientPopupActivated(QAction *action)
{
    if (!action->data().isValid())
        return;

    WindowOperation op = static_cast< WindowOperation >(action->data().toInt());
    Client* c = active_popup_client ? active_popup_client : active_client;
    if (!c)
        return;
    QString type;
    switch(op) {
    case FullScreenOp:
        if (!c->isFullScreen() && c->userCanSetFullScreen())
            type = "fullscreenaltf3";
        break;
    case NoBorderOp:
        if (!c->noBorder() && c->userCanSetNoBorder())
            type = "noborderaltf3";
        break;
    default:
        break;
    };
    if (!type.isEmpty())
        helperDialog(type, c);
    performWindowOperation(c, op);
}


void Workspace::performWindowOperation(Client* c, Options::WindowOperation op)
{
    if (!c)
        return;
#ifdef KWIN_BUILD_TILING
    // Allows us to float a window when it is maximized, if it is tiled.
    if (m_tiling->isEnabled()
            && (op == Options::MaximizeOp
                || op == Options::HMaximizeOp
                || op == Options::VMaximizeOp
                || op == Options::RestoreOp)) {
        m_tiling->notifyTilingWindowMaximized(c, op);
    }
#endif
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
        c->closeWindow();
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
        if (c->untab())
        if (options->focusPolicyIsReasonable())
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
    case Options::ToggleClientTiledStateOp: {
#ifdef KWIN_BUILD_TILING
        int desktop = c->desktop();
        if (m_tiling->tilingLayouts().value(desktop)) {
            m_tiling->tilingLayouts()[desktop]->toggleFloatTile(c);
        }
#endif
    }
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
            ClientList::const_iterator  it = workspace()->stackingOrder().constEnd(),
                                     begin = workspace()->stackingOrder().constBegin();
            while (mustReplay && --it != begin && *it != this) {
                if (((*it)->keepAbove() && !keepAbove()) || (keepBelow() && !(*it)->keepBelow()))
                    continue; // can never raise above "it"
                mustReplay = !((*it)->isOnCurrentDesktop() && (*it)->isOnCurrentActivity() && (*it)->geometry().intersects(geometry()));
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

void Workspace::loadEffect(const QString& name)
{
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->loadEffect(name);
}

void Workspace::toggleEffect(const QString& name)
{
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->toggleEffect(name);
}

void Workspace::unloadEffect(const QString& name)
{
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->unloadEffect(name);
}

void Workspace::reconfigureEffect(const QString& name)
{
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->reconfigureEffect(name);
}

QStringList Workspace::loadedEffects() const
{
    QStringList listModulesLoaded;
    if (effects)
        listModulesLoaded = static_cast<EffectsHandlerImpl*>(effects)->loadedEffects();
    return listModulesLoaded;
}

QStringList Workspace::listOfEffects() const
{
    QStringList listModules;
    if (effects)
        listModules = static_cast<EffectsHandlerImpl*>(effects)->listOfEffects();
    return listModules;
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
        active_client->untab();
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
  Sends the popup client to desktop \a desk

  Internal slot for the window operation menu
 */
void Workspace::slotSendToDesktop(QAction *action)
{
    int desk = action->data().toInt();
    if (!active_popup_client)
        return;
    if (desk == 0) {
        // the 'on_all_desktops' menu entry
        active_popup_client->setOnAllDesktops(!active_popup_client->isOnAllDesktops());
        return;
    }

    sendClientToDesktop(active_popup_client, desk, false);

}

/*!
  Toggles whether the popup client is on the \a activity

  Internal slot for the window operation menu
 */
void Workspace::slotToggleOnActivity(QAction *action)
{
    QString activity = action->data().toString();
    if (!active_popup_client)
        return;
    if (activity.isEmpty()) {
        // the 'on_all_activities' menu entry
        active_popup_client->setOnAllActivities(!active_popup_client->isOnAllActivities());
        return;
    }

    toggleClientOnActivity(active_popup_client, activity, false);

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

    QList<Client *> clist = stackingOrder();
    for (QList<Client *>::Iterator i = clist.begin(); i != clist.end(); ++i) {
        if ((*i)->wantsTabFocus() && *i != c &&
                (*i)->desktop() == d && !(*i)->isMinimized() && (*i)->isOnCurrentActivity()) {
            // Centre of the other window
            QPoint other((*i)->pos().x() + (*i)->geometry().width() / 2,
                         (*i)->pos().y() + (*i)->geometry().height() / 2);

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
                    switchTo = *i;
                    bestScore = score;
                }
            }
        }
    }
    if (switchTo)
        activateClient(switchTo);
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
    if (!KAuthorized::authorizeKAction("kwin_rmb"))
        return;
    if (!cl)
        return;
    if (active_popup_client != NULL)   // recursion
        return;
    if (cl->isDesktop()
            || cl->isDock())
        return;

    active_popup_client = cl;
    QMenu* p = clientPopup();
    active_popup = p;
    int x = pos.left();
    int y = pos.bottom();
    if (y == pos.top())
        p->exec(QPoint(x, y));
    else {
        QRect area = clientArea(ScreenArea, QPoint(x, y), currentDesktop());
        clientPopupAboutToShow(); // needed for sizeHint() to be correct :-/
        int popupHeight = p->sizeHint().height();
        if (y + popupHeight < area.height())
            p->exec(QPoint(x, y));
        else
            p->exec(QPoint(x, pos.top() - popupHeight));
    }
    // active popup may be already changed (e.g. the window shortcut dialog)
    if (active_popup == p)
        closeActivePopup();
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
