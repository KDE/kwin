/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2022 Natalie Clarius <natalie_clarius@yahoo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 This file contains things relevant to direct user actions, such as
 responses to global keyboard shortcuts, or selecting actions
 from the window menu.

*/

///////////////////////////////////////////////////////////////////////////////
// NOTE: if you change the menu, keep
//       plasma-desktop/applets/taskmanager/package/contents/ui/ContextMenu.qml
//       in sync
//////////////////////////////////////////////////////////////////////////////

#include "config-kwin.h"

#include "core/output.h"
#include "cursor.h"
#include "input.h"
#include "options.h"
#include "pointer_input.h"
#include "scripting/scripting.h"
#include "useractions.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"
#if KWIN_BUILD_X11
#include "x11window.h"
#endif

#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#include <PlasmaActivities/Info>
#endif
#include "appmenu.h"

#include <KProcess>

#include <QAction>
#include <QCheckBox>
#include <QPushButton>
#include <QWindow>

#include <KGlobalAccel>
#include <KLazyLocalizedString>
#include <KLocalizedString>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QRegularExpression>
#include <kauthorized.h>
#include <kconfig.h>

#include "killwindow.h"
#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif

namespace KWin
{

UserActionsMenu::UserActionsMenu(QObject *parent)
    : QObject(parent)
    , m_menu(nullptr)
    , m_desktopMenu(nullptr)
    , m_screenMenu(nullptr)
    , m_activityMenu(nullptr)
    , m_scriptsMenu(nullptr)
    , m_resizeOperation(nullptr)
    , m_moveOperation(nullptr)
    , m_maximizeOperation(nullptr)
    , m_keepAboveOperation(nullptr)
    , m_keepBelowOperation(nullptr)
    , m_fullScreenOperation(nullptr)
    , m_noBorderOperation(nullptr)
    , m_minimizeOperation(nullptr)
    , m_closeOperation(nullptr)
    , m_shortcutOperation(nullptr)
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

bool UserActionsMenu::hasWindow() const
{
    return m_window && isShown();
}

void UserActionsMenu::close()
{
    if (!m_menu) {
        return;
    }
    m_menu->close();
}

bool UserActionsMenu::isMenuWindow(const Window *window) const
{
    return window && window == m_window;
}

void UserActionsMenu::show(const QRect &pos, Window *window)
{
    Q_ASSERT(window);
    QPointer<Window> windowPtr(window);
    // Presumably window will never be nullptr,
    // but play it safe and make sure not to crash.
    if (windowPtr.isNull()) {
        return;
    }
    if (isShown()) { // recursion
        return;
    }
    if (windowPtr->isDesktop() || windowPtr->isDock()) {
        return;
    }
    if (!KAuthorized::authorizeAction(QStringLiteral("kwin_rmb"))) {
        return;
    }
    m_window = windowPtr;
    init();
    m_menu->popup(pos.bottomLeft());
}

void UserActionsMenu::grabInput()
{
    m_menu->windowHandle()->setMouseGrabEnabled(true);
    m_menu->windowHandle()->setKeyboardGrabEnabled(true);
}

void UserActionsMenu::helperDialog(const QString &message)
{
    QStringList args;
    QString type;
    auto shortcut = [](const QString &name) {
        QAction *action = Workspace::self()->findChild<QAction *>(name);
        Q_ASSERT(action != nullptr);
        const auto shortcuts = KGlobalAccel::self()->shortcut(action);
        return QStringLiteral("%1").arg(shortcuts.isEmpty() ? QString() : shortcuts.first().toString(QKeySequence::NativeText));
    };
    if (message == QLatin1StringView("noborderaltf3")) {
        args << QStringLiteral("--msgbox") << i18n("You have selected to show a window without its border.\n"
                                                   "Without the border, you will not be able to enable the border "
                                                   "again using the mouse: use the window menu instead, "
                                                   "activated using the %1 keyboard shortcut.",
                                                   shortcut(QStringLiteral("Window Operations Menu")));
        type = QStringLiteral("altf3warning");
    } else if (message == QLatin1String("fullscreenaltf3")) {
        args << QStringLiteral("--msgbox") << i18n("You have selected to show a window in fullscreen mode.\n"
                                                   "If the application itself does not have an option to turn the fullscreen "
                                                   "mode off you will not be able to disable it "
                                                   "again using the mouse: use the window menu instead, "
                                                   "activated using the %1 keyboard shortcut.",
                                                   shortcut(QStringLiteral("Window Operations Menu")));
        type = QStringLiteral("altf3warning");
    } else {
        Q_UNREACHABLE();
    }
    if (!type.isEmpty()) {
        KConfig cfg(QStringLiteral("kwin_dialogsrc"));
        KConfigGroup cg(&cfg, QStringLiteral("Notification Messages")); // Depends on KMessageBox
        if (!cg.readEntry(type, true)) {
            return;
        }
        args << QStringLiteral("--dontagain") << QLatin1String("kwin_dialogsrc:") + type;
    }
    KProcess::startDetached(QStringLiteral("kdialog"), args);
}

void UserActionsMenu::setShortcut(QAction *action, const QString &actionName)
{
    const auto shortcuts = KGlobalAccel::self()->shortcut(Workspace::self()->findChild<QAction *>(actionName));
    if (!shortcuts.isEmpty()) {
        action->setShortcut(shortcuts.first());
    }
}

void UserActionsMenu::init()
{
    if (m_menu) {
        return;
    }
    m_menu = new QMenu;
    connect(m_menu, &QMenu::aboutToShow, this, &UserActionsMenu::menuAboutToShow);

    // the toplevel menu gets closed before a submenu's action is invoked
    connect(m_menu, &QMenu::aboutToHide, this, &UserActionsMenu::menuAboutToHide, Qt::QueuedConnection);
    connect(m_menu, &QMenu::triggered, this, &UserActionsMenu::slotWindowOperation);

    QMenu *advancedMenu = new QMenu(m_menu);
    connect(advancedMenu, &QMenu::aboutToShow, this, [this, advancedMenu]() {
        if (m_window) {
            advancedMenu->setPalette(m_window->palette());
        }
    });

    m_moveOperation = advancedMenu->addAction(i18n("&Move"));
    m_moveOperation->setIcon(QIcon::fromTheme(QStringLiteral("transform-move")));
    setShortcut(m_moveOperation, QStringLiteral("Window Move"));
    m_moveOperation->setData(Options::UnrestrictedMoveOp);

    m_resizeOperation = advancedMenu->addAction(i18n("&Resize"));
    m_resizeOperation->setIcon(QIcon::fromTheme(QStringLiteral("transform-scale")));
    setShortcut(m_resizeOperation, QStringLiteral("Window Resize"));
    m_resizeOperation->setData(Options::ResizeOp);

    m_keepAboveOperation = advancedMenu->addAction(i18n("Keep &Above Others"));
    m_keepAboveOperation->setIcon(QIcon::fromTheme(QStringLiteral("window-keep-above")));
    setShortcut(m_keepAboveOperation, QStringLiteral("Window Above Other Windows"));
    m_keepAboveOperation->setCheckable(true);
    m_keepAboveOperation->setData(Options::KeepAboveOp);

    m_keepBelowOperation = advancedMenu->addAction(i18n("Keep &Below Others"));
    m_keepBelowOperation->setIcon(QIcon::fromTheme(QStringLiteral("window-keep-below")));
    setShortcut(m_keepBelowOperation, QStringLiteral("Window Below Other Windows"));
    m_keepBelowOperation->setCheckable(true);
    m_keepBelowOperation->setData(Options::KeepBelowOp);

    m_fullScreenOperation = advancedMenu->addAction(i18n("&Fullscreen"));
    m_fullScreenOperation->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
    setShortcut(m_fullScreenOperation, QStringLiteral("Window Fullscreen"));
    m_fullScreenOperation->setCheckable(true);
    m_fullScreenOperation->setData(Options::FullScreenOp);

    m_noBorderOperation = advancedMenu->addAction(i18n("&No Titlebar and Frame"));
    m_noBorderOperation->setIcon(QIcon::fromTheme(QStringLiteral("edit-none-border")));
    setShortcut(m_noBorderOperation, QStringLiteral("Window No Border"));
    m_noBorderOperation->setCheckable(true);
    m_noBorderOperation->setData(Options::NoBorderOp);

    advancedMenu->addSeparator();

    m_shortcutOperation = advancedMenu->addAction(i18n("Set Window Short&cut…"));
    m_shortcutOperation->setIcon(QIcon::fromTheme(QStringLiteral("configure-shortcuts")));
    setShortcut(m_shortcutOperation, QStringLiteral("Setup Window Shortcut"));
    m_shortcutOperation->setData(Options::SetupWindowShortcutOp);

#if KWIN_BUILD_KCMS
    QAction *action = advancedMenu->addAction(i18n("Configure Special &Window Settings…"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-actions")));
    action->setData(Options::WindowRulesOp);
    m_rulesOperation = action;

    action = advancedMenu->addAction(i18n("Configure S&pecial Application Settings…"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-actions")));
    action->setData(Options::ApplicationRulesOp);
    m_applicationRulesOperation = action;
#endif

    m_maximizeOperation = m_menu->addAction(i18n("Ma&ximize"));
    m_maximizeOperation->setIcon(QIcon::fromTheme(QStringLiteral("window-maximize")));
    setShortcut(m_maximizeOperation, QStringLiteral("Window Maximize"));
    m_maximizeOperation->setCheckable(true);
    m_maximizeOperation->setData(Options::MaximizeOp);

    m_minimizeOperation = m_menu->addAction(i18n("Mi&nimize"));
    m_minimizeOperation->setIcon(QIcon::fromTheme(QStringLiteral("window-minimize")));
    setShortcut(m_minimizeOperation, QStringLiteral("Window Minimize"));
    m_minimizeOperation->setData(Options::MinimizeOp);

    QAction *overflowAction = m_menu->addMenu(advancedMenu);
    overflowAction->setText(i18n("&More Actions"));
    overflowAction->setIcon(QIcon::fromTheme(QStringLiteral("overflow-menu")));

    m_menu->addSeparator();

    m_closeOperation = m_menu->addAction(i18n("&Close"));
    m_closeOperation->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    setShortcut(m_closeOperation, QStringLiteral("Window Close"));
    m_closeOperation->setData(Options::CloseOp);
}

void UserActionsMenu::discard()
{
    delete m_menu;
    m_menu = nullptr;
    m_desktopMenu = nullptr;
    m_screenMenu = nullptr;
    m_activityMenu = nullptr;
    m_scriptsMenu = nullptr;
}

void UserActionsMenu::menuAboutToShow()
{
    if (m_window.isNull() || !m_menu) {
        return;
    }

    m_window->blockActivityUpdates(true);

    if (VirtualDesktopManager::self()->count() == 1) {
        delete m_desktopMenu;
        m_desktopMenu = nullptr;
    } else {
        initDesktopPopup();
    }
    if (workspace()->outputs().count() == 1 || (!m_window->isMovable() && !m_window->isMovableAcrossScreens())) {
        delete m_screenMenu;
        m_screenMenu = nullptr;
    } else {
        initScreenPopup();
    }

    m_menu->setPalette(m_window->palette());
    m_resizeOperation->setEnabled(m_window->isResizable());
    m_moveOperation->setEnabled(m_window->isMovableAcrossScreens());
    m_maximizeOperation->setEnabled(m_window->isMaximizable());
    m_maximizeOperation->setChecked(m_window->maximizeMode() == MaximizeFull);
    m_keepAboveOperation->setChecked(m_window->keepAbove());
    m_keepBelowOperation->setChecked(m_window->keepBelow());
    m_fullScreenOperation->setEnabled(m_window->isFullScreenable());
    m_fullScreenOperation->setChecked(m_window->isFullScreen());
    m_noBorderOperation->setEnabled(m_window->userCanSetNoBorder());
    m_noBorderOperation->setChecked(m_window->noBorder());
    m_minimizeOperation->setEnabled(m_window->isMinimizable());
    m_closeOperation->setEnabled(m_window->isCloseable());
    m_shortcutOperation->setEnabled(m_window->rules()->checkShortcut(QString()).isNull());

    // drop the existing scripts menu
    delete m_scriptsMenu;
    m_scriptsMenu = nullptr;
    // ask scripts whether they want to add entries for the given window
    QList<QAction *> scriptActions = Scripting::self()->actionsForUserActionMenu(m_window.data(), m_scriptsMenu);
    if (!scriptActions.isEmpty()) {
        m_scriptsMenu = new QMenu(m_menu);
        m_scriptsMenu->setPalette(m_window->palette());
        m_scriptsMenu->addActions(scriptActions);

        QAction *action = m_scriptsMenu->menuAction();
        // set it as the first item after desktop
        m_menu->insertAction(m_closeOperation, action);
        action->setText(i18n("&Extensions"));
    }

    if (m_rulesOperation) {
        m_rulesOperation->setEnabled(m_window->supportsWindowRules());
    }
    if (m_applicationRulesOperation) {
        m_applicationRulesOperation->setEnabled(m_window->supportsWindowRules());
    }

    showHideActivityMenu();
}

void UserActionsMenu::menuAboutToHide()
{
    if (m_window && !m_window->isDeleted()) {
        m_window->blockActivityUpdates(false);
    }
    m_window.clear();
}

void UserActionsMenu::showHideActivityMenu()
{
#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return;
    }
    const QStringList &openActivities_ = Workspace::self()->activities()->all();
    qCDebug(KWIN_CORE) << "activities:" << openActivities_.size();
    if (openActivities_.size() < 2) {
        delete m_activityMenu;
        m_activityMenu = nullptr;
    } else {
        initActivityPopup();
    }
#endif
}

void UserActionsMenu::initDesktopPopup()
{
    if (m_desktopMenu) {
        return;
    }

    m_desktopMenu = new QMenu(m_menu);
    connect(m_desktopMenu, &QMenu::aboutToShow, this, &UserActionsMenu::desktopPopupAboutToShow);

    QAction *action = m_desktopMenu->menuAction();
    // set it as the first item
    m_menu->insertAction(m_maximizeOperation, action);
    action->setText(i18n("&Desktops"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("virtual-desktops")));
}

void UserActionsMenu::initScreenPopup()
{
    if (m_screenMenu) {
        return;
    }

    m_screenMenu = new QMenu(m_menu);
    connect(m_screenMenu, &QMenu::aboutToShow, this, &UserActionsMenu::screenPopupAboutToShow);

    QAction *action = m_screenMenu->menuAction();
    // set it as the first item after desktop
    m_menu->insertAction(m_activityMenu ? m_activityMenu->menuAction() : m_minimizeOperation, action);
    action->setText(i18n("Move to &Screen"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("computer")));
}

void UserActionsMenu::initActivityPopup()
{
    if (m_activityMenu) {
        return;
    }

    m_activityMenu = new QMenu(m_menu);
    connect(m_activityMenu, &QMenu::aboutToShow, this, &UserActionsMenu::activityPopupAboutToShow);

    QAction *action = m_activityMenu->menuAction();
    // set it as the first item
    m_menu->insertAction(m_maximizeOperation, action);
    action->setText(i18n("Show in &Activities"));
    action->setIcon(QIcon::fromTheme(QStringLiteral("activities")));
}

void UserActionsMenu::desktopPopupAboutToShow()
{
    if (!m_desktopMenu) {
        return;
    }
    VirtualDesktopManager *vds = VirtualDesktopManager::self();

    m_desktopMenu->clear();
    if (m_window) {
        m_desktopMenu->setPalette(m_window->palette());
    }

    QAction *action = m_desktopMenu->addAction(i18n("&All Desktops"));
    connect(action, &QAction::triggered, this, [this]() {
        if (m_window) {
            m_window->setOnAllDesktops(!m_window->isOnAllDesktops());
        }
    });
    action->setCheckable(true);
    if (m_window && m_window->isOnAllDesktops()) {
        action->setChecked(true);
    }

    m_desktopMenu->addSeparator();

    const auto desktops = vds->desktops();
    for (VirtualDesktop *desktop : desktops) {
        QAction *action = m_desktopMenu->addAction(desktop->name().replace(QLatin1Char('&'), QStringLiteral("&&")));
        connect(action, &QAction::triggered, this, [this, desktop]() {
            if (m_window) {
                if (m_window->desktops().contains(desktop)) {
                    Workspace::self()->removeWindowFromDesktop(m_window, desktop);
                } else {
                    Workspace::self()->addWindowToDesktop(m_window, desktop);
                }
            }
        });
        action->setCheckable(true);
        if (m_window && !m_window->isOnAllDesktops() && m_window->isOnDesktop(desktop)) {
            action->setChecked(true);
        }
    }

    m_desktopMenu->addSeparator();

    for (auto i = 0; i < desktops.size(); ++i) {
        VirtualDesktop *desktop = desktops.at(i);
        QString name = i18n("Move to %1", desktop->name());
        QAction *action = m_desktopMenu->addAction(name);
        setShortcut(action, QStringLiteral("Window to Desktop %1").arg(i + 1));
        connect(action, &QAction::triggered, this, [this, desktop]() {
            if (m_window) {
                Workspace::self()->sendWindowToDesktops(m_window, {desktop}, false);
            }
        });
    }

    m_desktopMenu->addSeparator();

    bool allowNewDesktops = vds->count() < vds->maximum();

    action = m_desktopMenu->addAction(i18nc("Create a new desktop and add the window to that desktop", "Add to &New Desktop"));
    connect(action, &QAction::triggered, this, [this, vds]() {
        if (!m_window) {
            return;
        }
        VirtualDesktop *desktop = vds->createVirtualDesktop(vds->count());
        if (desktop) {
            Workspace::self()->addWindowToDesktop(m_window, desktop);
        }
    });
    action->setEnabled(allowNewDesktops);

    action = m_desktopMenu->addAction(i18nc("Create a new desktop and move the window to that desktop", "Move to New Desktop"));
    connect(action, &QAction::triggered, this, [this, vds]() {
        if (!m_window) {
            return;
        }
        VirtualDesktop *desktop = vds->createVirtualDesktop(vds->count());
        if (desktop) {
            Workspace::self()->sendWindowToDesktops(m_window, {desktop}, false);
        }
    });
    action->setEnabled(allowNewDesktops);
}

void UserActionsMenu::screenPopupAboutToShow()
{
    if (!m_screenMenu) {
        return;
    }
    m_screenMenu->clear();

    if (!m_window) {
        return;
    }
    m_screenMenu->setPalette(m_window->palette());
    QActionGroup *group = new QActionGroup(m_screenMenu);

    const auto outputs = workspace()->outputs();
    for (int i = 0; i < outputs.count(); ++i) {
        Output *output = outputs[i];
        // assumption: there are not more than 9 screens attached.
        QAction *action = m_screenMenu->addAction(i18nc("@item:inmenu List of all Screens to send a window to. First argument is a number, second the output identifier. E.g. Screen 1 (HDMI1)",
                                                        "Screen &%1 (%2)", (i + 1), output->name()));
        setShortcut(action, QStringLiteral("Window to Screen %1").arg(i));
        connect(action, &QAction::triggered, this, [this, output]() {
            m_window->sendToOutput(output);
        });
        action->setCheckable(true);
        if (m_window && output == m_window->output()) {
            action->setChecked(true);
        }
        group->addAction(action);
    }
}

void UserActionsMenu::activityPopupAboutToShow()
{
    if (!m_activityMenu) {
        return;
    }

#if KWIN_BUILD_ACTIVITIES
    if (!Workspace::self()->activities()) {
        return;
    }
    m_activityMenu->clear();
    if (m_window) {
        m_activityMenu->setPalette(m_window->palette());
    }
    QAction *action = m_activityMenu->addAction(i18n("&All Activities"));
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, [this]() {
        if (m_window) {
            m_window->setOnAllActivities(!m_window->isOnAllActivities());
        }
    });
    static QPointer<QActionGroup> allActivitiesGroup;
    if (!allActivitiesGroup) {
        allActivitiesGroup = new QActionGroup(m_activityMenu);
    }
    allActivitiesGroup->addAction(action);

    if (m_window && m_window->isOnAllActivities()) {
        action->setChecked(true);
    }
    m_activityMenu->addSeparator();

    const auto activities = Workspace::self()->activities()->all();
    for (const QString &id : activities) {
        KActivities::Info activity(id);
        QString name = activity.name();
        name.replace('&', "&&");
        auto action = m_activityMenu->addAction(name);
        action->setCheckable(true);
        const QString icon = activity.icon();
        if (!icon.isEmpty()) {
            action->setIcon(QIcon::fromTheme(icon));
        }
        m_activityMenu->addAction(action);
        connect(action, &QAction::triggered, this, [this, id]() {
            if (m_window) {
                Workspace::self()->activities()->toggleWindowOnActivity(m_window, id, false);
            }
        });

        if (m_window && !m_window->isOnAllActivities() && m_window->isOnActivity(id)) {
            action->setChecked(true);
        }
    }

    m_activityMenu->addSeparator();
    for (const QString &id : activities) {
        const KActivities::Info activity(id);
        if (m_window->activities().size() == 1 && m_window->activities().front() == id) {
            // no need to show a button that doesn't do anything
            continue;
        }
        const QString name = i18n("Move to %1", activity.name().replace('&', "&&"));
        const auto action = m_activityMenu->addAction(name);
        if (const QString icon = activity.icon(); !icon.isEmpty()) {
            action->setIcon(QIcon::fromTheme(icon));
        }
        connect(action, &QAction::triggered, this, [this, id] {
            m_window->setOnActivities({id});
        });
        m_activityMenu->addAction(action);
    }
#endif
}

void UserActionsMenu::slotWindowOperation(QAction *action)
{
    if (!action->data().isValid()) {
        return;
    }

    Options::WindowOperation op = static_cast<Options::WindowOperation>(action->data().toInt());
    QPointer<Window> c = m_window ? m_window : QPointer<Window>(Workspace::self()->activeWindow());
    if (c.isNull()) {
        return;
    }
    QString type;
    switch (op) {
    case Options::FullScreenOp:
        if (!c->isFullScreen() && c->isFullScreenable()) {
            type = QStringLiteral("fullscreenaltf3");
        }
        break;
    case Options::NoBorderOp:
        if (!c->noBorder() && c->userCanSetNoBorder()) {
            type = QStringLiteral("noborderaltf3");
        }
        break;
    default:
        break;
    }
    if (!type.isEmpty()) {
        helperDialog(type);
    }
    // need to delay performing the window operation as we need to have the
    // window menu closed before we destroy the decoration. Otherwise Qt crashes
    QMetaObject::invokeMethod(
        workspace(), [c, op]() {
            workspace()->performWindowOperation(c, op);
        },
        Qt::QueuedConnection);
}

//****************************************
// ShortcutDialog
//****************************************
ShortcutDialog::ShortcutDialog(const QKeySequence &cut)
    : _shortcut(cut)
{
    m_ui.setupUi(this);
    m_ui.keySequenceEdit->setKeySequence(cut);
    m_ui.warning->hide();

    // Listen to changed shortcuts
    connect(m_ui.keySequenceEdit, &QKeySequenceEdit::editingFinished, this, &ShortcutDialog::keySequenceChanged);
    connect(m_ui.clearButton, &QToolButton::clicked, this, [this] {
        _shortcut = QKeySequence();
    });
    m_ui.keySequenceEdit->setFocus();

    setWindowFlags(Qt::Popup | Qt::X11BypassWindowManagerHint);
}

void ShortcutDialog::accept()
{
    QKeySequence seq = shortcut();
    if (!seq.isEmpty()) {
        if (seq[0] == QKeyCombination(Qt::Key_Escape)) {
            reject();
            return;
        }
        if (seq[0] == QKeyCombination(Qt::Key_Space) || seq[0].keyboardModifiers() == Qt::NoModifier) {
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
    Q_EMIT dialogDone(r == Accepted);
}

void ShortcutDialog::keySequenceChanged()
{
    activateWindow(); // where is the kbd focus lost? cause of popup state?
    QKeySequence seq = m_ui.keySequenceEdit->keySequence();
    if (_shortcut == seq) {
        return; // don't try to update the same
    }

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
    QList<KGlobalShortcutInfo> conflicting = KGlobalAccel::globalShortcutsByKey(seq);
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
        if (QPushButton *ok = m_ui.buttonBox->button(QDialogButtonBox::Ok)) {
            ok->setFocus();
        }
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
    if (!m_activeWindow) {
        return;
    }
    m_activeWindow->setOpacity(std::min(m_activeWindow->opacity() + 0.05, 1.0));
}

void Workspace::slotLowerWindowOpacity()
{
    if (!m_activeWindow) {
        return;
    }
    m_activeWindow->setOpacity(std::max(m_activeWindow->opacity() - 0.05, 0.05));
}

void Workspace::closeActivePopup()
{
    if (active_popup) {
        active_popup->close();
        active_popup = nullptr;
        m_activePopupWindow = nullptr;
    }
    m_userActionsMenu->close();
}

template<typename Slot>
void Workspace::initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, Slot slot, bool autoRepeat)
{
    initShortcut(actionName, description, shortcut, this, slot, autoRepeat);
}

template<typename T, typename Slot>
void Workspace::initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, T *receiver, Slot slot, bool autoRepeat)
{
    QAction *a = new QAction(this);
    a->setAutoRepeat(autoRepeat);
    a->setProperty("componentName", QStringLiteral("kwin"));
    a->setObjectName(actionName);
    a->setText(description);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << shortcut);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << shortcut);
    connect(a, &QAction::triggered, receiver, slot);
}

/**
 * Creates the global accel object \c keys.
 */
void Workspace::initShortcuts()
{
    // The first argument to initShortcut() is the id for the shortcut in the user's
    // config file, while the second argumewnt is its user-visible text.
    // Normally these should be identical, but if the user-visible text for a new
    // shortcut that you're adding is super long, it is permissible to use a shorter
    // string for name.

    // PLEASE NOTE: Never change the ID of an existing shortcut! It will cause users'
    // custom shortcuts to be lost. Instead, only change the description

    initShortcut("Window Operations Menu", i18n("Window Menu"),
                 Qt::ALT | Qt::Key_F3, &Workspace::slotWindowOperations, true);
    initShortcut("Window Close", i18n("Close Window"),
                 Qt::ALT | Qt::Key_F4, &Workspace::slotWindowClose, true);
    initShortcut("Window Maximize", i18n("Maximize Window"),
                 Qt::META | Qt::Key_PageUp, &Workspace::slotWindowMaximize, false);
    initShortcut("Window Maximize Vertical", i18n("Maximize Window Vertically"),
                 0, &Workspace::slotWindowMaximizeVertical, false);
    initShortcut("Window Maximize Horizontal", i18n("Maximize Window Horizontally"),
                 0, &Workspace::slotWindowMaximizeHorizontal, false);
    initShortcut("Window Minimize", i18n("Minimize Window"),
                 Qt::META | Qt::Key_PageDown, &Workspace::slotWindowMinimize, false);
    initShortcut("Window Move", i18n("Move Window"),
                 0, &Workspace::slotWindowMove, true);
    initShortcut("Window Resize", i18n("Resize Window"),
                 0, &Workspace::slotWindowResize, true);
    initShortcut("Window Raise", i18n("Raise Window"),
                 0, &Workspace::slotWindowRaise, true);
    initShortcut("Window Lower", i18n("Lower Window"),
                 0, &Workspace::slotWindowLower, true);
    initShortcut("Toggle Window Raise/Lower", i18n("Toggle Window Raise/Lower"),
                 0, &Workspace::slotWindowRaiseOrLower, true);
    initShortcut("Window Fullscreen", i18n("Make Window Fullscreen"),
                 0, &Workspace::slotWindowFullScreen, false);
    initShortcut("Window No Border", i18n("Toggle Window Titlebar and Frame"),
                 0, &Workspace::slotWindowNoBorder, true);
    initShortcut("Window Above Other Windows", i18n("Keep Window Above Others"),
                 0, &Workspace::slotWindowAbove, true);
    initShortcut("Window Below Other Windows", i18n("Keep Window Below Others"),
                 0, &Workspace::slotWindowBelow, true);
    initShortcut("Activate Window Demanding Attention", i18n("Activate Window Demanding Attention"),
                 Qt::META | Qt::CTRL | Qt::Key_A, &Workspace::slotActivateAttentionWindow, true);
    initShortcut("Setup Window Shortcut", i18n("Setup Window Shortcut"),
                 0, &Workspace::slotSetupWindowShortcut, true);
    initShortcut("Window Move Center", i18n("Move Window to the Center"), 0,
                 &Workspace::slotWindowCenter, true);
    initShortcut("Window Pack Right", i18n("Move Window Right"),
                 0, &Workspace::slotWindowMoveRight, true);
    initShortcut("Window Pack Left", i18n("Move Window Left"),
                 0, &Workspace::slotWindowMoveLeft, true);
    initShortcut("Window Pack Up", i18n("Move Window Up"),
                 0, &Workspace::slotWindowMoveUp, true);
    initShortcut("Window Pack Down", i18n("Move Window Down"),
                 0, &Workspace::slotWindowMoveDown, true);
    initShortcut("Window Grow Horizontal", i18n("Expand Window Horizontally"),
                 0, &Workspace::slotWindowExpandHorizontal, true);
    initShortcut("Window Grow Vertical", i18n("Expand Window Vertically"),
                 0, &Workspace::slotWindowExpandVertical, true);
    initShortcut("Window Shrink Horizontal", i18n("Shrink Window Horizontally"),
                 0, &Workspace::slotWindowShrinkHorizontal, true);
    initShortcut("Window Shrink Vertical", i18n("Shrink Window Vertically"),
                 0, &Workspace::slotWindowShrinkVertical, true);
    initShortcut("Window Quick Tile Left", i18n("Quick Tile Window to the Left"),
                 Qt::META | Qt::Key_Left, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Left), true);
    initShortcut("Window Quick Tile Right", i18n("Quick Tile Window to the Right"),
                 Qt::META | Qt::Key_Right, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Right), true);
    initShortcut("Window Quick Tile Top", i18n("Quick Tile Window to the Top"),
                 Qt::META | Qt::Key_Up, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Top), true);
    initShortcut("Window Quick Tile Bottom", i18n("Quick Tile Window to the Bottom"),
                 Qt::META | Qt::Key_Down, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Bottom), true);
    initShortcut("Window Quick Tile Top Left", i18n("Quick Tile Window to the Top Left"),
                 0, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Top | QuickTileFlag::Left), true);
    initShortcut("Window Quick Tile Bottom Left", i18n("Quick Tile Window to the Bottom Left"),
                 0, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Bottom | QuickTileFlag::Left), true);
    initShortcut("Window Quick Tile Top Right", i18n("Quick Tile Window to the Top Right"),
                 0, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Top | QuickTileFlag::Right), true);
    initShortcut("Window Quick Tile Bottom Right", i18n("Quick Tile Window to the Bottom Right"),
                 0, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Bottom | QuickTileFlag::Right), true);
    initShortcut("Switch Window Up", i18n("Switch to Window Above"),
                 Qt::META | Qt::ALT | Qt::Key_Up, std::bind(static_cast<void (Workspace::*)(Direction)>(&Workspace::switchWindow), this, DirectionNorth), true);
    initShortcut("Switch Window Down", i18n("Switch to Window Below"),
                 Qt::META | Qt::ALT | Qt::Key_Down, std::bind(static_cast<void (Workspace::*)(Direction)>(&Workspace::switchWindow), this, DirectionSouth), true);
    initShortcut("Switch Window Right", i18n("Switch to Window to the Right"),
                 Qt::META | Qt::ALT | Qt::Key_Right, std::bind(static_cast<void (Workspace::*)(Direction)>(&Workspace::switchWindow), this, DirectionEast), true);
    initShortcut("Switch Window Left", i18n("Switch to Window to the Left"),
                 Qt::META | Qt::ALT | Qt::Key_Left, std::bind(static_cast<void (Workspace::*)(Direction)>(&Workspace::switchWindow), this, DirectionWest), true);
    initShortcut("Increase Opacity", i18n("Increase Opacity of Active Window by 5%"),
                 0, &Workspace::slotIncreaseWindowOpacity, true);
    initShortcut("Decrease Opacity", i18n("Decrease Opacity of Active Window by 5%"),
                 0, &Workspace::slotLowerWindowOpacity, true);

    initShortcut("Window On All Desktops", i18n("Keep Window on All Desktops"),
                 0, &Workspace::slotWindowOnAllDesktops, true);

    initShortcut("Window Custom Quick Tile Left", i18n("Custom Quick Tile Window to the Left"),
                 0, std::bind(&Workspace::customQuickTileWindow, this, QuickTileFlag::Left), true);
    initShortcut("Window Custom Quick Tile Right", i18n("Custom Quick Tile Window to the Right"),
                 0, std::bind(&Workspace::customQuickTileWindow, this, QuickTileFlag::Right), true);
    initShortcut("Window Custom Quick Tile Top", i18n("Custom Quick Tile Window to the Top"),
                 0, std::bind(&Workspace::customQuickTileWindow, this, QuickTileFlag::Top), true);
    initShortcut("Window Custom Quick Tile Bottom", i18n("Custom Quick Tile Window to the Bottom"),
                 0, std::bind(&Workspace::customQuickTileWindow, this, QuickTileFlag::Bottom), true);

    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    for (uint i = 0; i < vds->maximum(); ++i) {
        auto handler = [this, i]() {
            const QList<VirtualDesktop *> desktops = VirtualDesktopManager::self()->desktops();
            if (i < uint(desktops.count())) {
                slotWindowToDesktop(desktops[i]);
            }
        };
        initShortcut(QStringLiteral("Window to Desktop %1").arg(i + 1), i18n("Window to Desktop %1", i + 1), 0, handler, true);
    }
    initShortcut("Window to Next Desktop", i18n("Window to Next Desktop"), 0, &Workspace::slotWindowToNextDesktop, true);
    initShortcut("Window to Previous Desktop", i18n("Window to Previous Desktop"), 0, &Workspace::slotWindowToPreviousDesktop, true);
    initShortcut("Window One Desktop to the Right", i18n("Window One Desktop to the Right"),
                 Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_Right, &Workspace::slotWindowToDesktopRight, true);
    initShortcut("Window One Desktop to the Left", i18n("Window One Desktop to the Left"),
                 Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_Left, &Workspace::slotWindowToDesktopLeft, true);
    initShortcut("Window One Desktop Up", i18n("Window One Desktop Up"),
                 Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_Up, &Workspace::slotWindowToDesktopUp, true);
    initShortcut("Window One Desktop Down", i18n("Window One Desktop Down"),
                 Qt::META | Qt::CTRL | Qt::SHIFT | Qt::Key_Down, &Workspace::slotWindowToDesktopDown, true);

    for (int i = 0; i < 8; ++i) {
        initShortcut(QStringLiteral("Window to Screen %1").arg(i), i18n("Move Window to Screen %1", i), 0, [this, i]() {
            Output *output = outputs().value(i);
            if (output) {
                slotWindowToScreen(output);
            }
        }, true);
    }
    initShortcut("Window to Next Screen", i18n("Move Window to Next Screen"),
                 Qt::META | Qt::SHIFT | Qt::Key_Right, &Workspace::slotWindowToNextScreen, true);
    initShortcut("Window to Previous Screen", i18n("Move Window to Previous Screen"),
                 Qt::META | Qt::SHIFT | Qt::Key_Left, &Workspace::slotWindowToPrevScreen, true);
    initShortcut("Window One Screen to the Right", i18n("Move Window One Screen to the Right"),
                 0, &Workspace::slotWindowToRightScreen, true);
    initShortcut("Window One Screen to the Left", i18n("Move Window One Screen to the Left"),
                 0, &Workspace::slotWindowToLeftScreen, true);
    initShortcut("Window One Screen Up", i18n("Move Window One Screen Up"),
                 0, &Workspace::slotWindowToAboveScreen, true);
    initShortcut("Window One Screen Down", i18n("Move Window One Screen Down"),
                 0, &Workspace::slotWindowToBelowScreen, true);

    for (int i = 0; i < 8; ++i) {
        initShortcut(QStringLiteral("Switch to Screen %1").arg(i), i18n("Switch to Screen %1", i), 0, [this, i]() {
            Output *output = outputs().value(i);
            if (output) {
                slotSwitchToScreen(output);
            }
        }, true);
    }
    initShortcut("Switch to Next Screen", i18n("Switch to Next Screen"), 0, &Workspace::slotSwitchToNextScreen, true);
    initShortcut("Switch to Previous Screen", i18n("Switch to Previous Screen"), 0, &Workspace::slotSwitchToPrevScreen, true);
    initShortcut("Switch to Screen to the Right", i18n("Switch to Screen to the Right"),
                 0, &Workspace::slotSwitchToRightScreen, true);
    initShortcut("Switch to Screen to the Left", i18n("Switch to Screen to the Left"),
                 0, &Workspace::slotSwitchToLeftScreen, true);
    initShortcut("Switch to Screen Above", i18n("Switch to Screen Above"),
                 0, &Workspace::slotSwitchToAboveScreen, true);
    initShortcut("Switch to Screen Below", i18n("Switch to Screen Below"),
                 0, &Workspace::slotSwitchToBelowScreen, true);

    initShortcut("Show Desktop", i18n("Peek at Desktop"),
                 Qt::META | Qt::Key_D, &Workspace::slotToggleShowDesktop, false);

    initShortcut("Kill Window", i18n("Kill Window"), Qt::META | Qt::CTRL | Qt::Key_Escape, &Workspace::slotKillWindow, true);

#if KWIN_BUILD_TABBOX
    m_tabbox->initShortcuts();
#endif
    vds->initShortcuts();
    m_userActionsMenu->discard(); // so that it's recreated next time
}

void Workspace::setupWindowShortcut(Window *window)
{
    if (m_windowKeysDialog) {
        return;
    }
    // TODO: PORT ME (KGlobalAccel related)
    // keys->setEnabled( false );
    // disable_shortcuts_keys->setEnabled( false );
    // client_keys->setEnabled( false );
    m_windowKeysDialog = new ShortcutDialog(window->shortcut());
    m_windowKeysWindow = window;
    connect(m_windowKeysDialog, &ShortcutDialog::dialogDone, this, &Workspace::setupWindowShortcutDone);
    QRect r = clientArea(ScreenArea, window).toRect();
    QSize size = m_windowKeysDialog->sizeHint();
    QPointF pos(window->frameGeometry().left() + window->frameMargins().left(),
                window->frameGeometry().top() + window->frameMargins().top());
    if (pos.x() + size.width() >= r.right()) {
        pos.setX(r.right() - size.width());
    }
    if (pos.y() + size.height() >= r.bottom()) {
        pos.setY(r.bottom() - size.height());
    }
    m_windowKeysDialog->move(pos.toPoint());
    m_windowKeysDialog->show();
    active_popup = m_windowKeysDialog;
    m_activePopupWindow = window;
}

void Workspace::setupWindowShortcutDone(bool ok)
{
    //    keys->setEnabled( true );
    //    disable_shortcuts_keys->setEnabled( true );
    //    client_keys->setEnabled( true );
    if (ok) {
        m_windowKeysWindow->setShortcut(m_windowKeysDialog->shortcut().toString());
    }
    closeActivePopup();
    m_windowKeysDialog->deleteLater();
    m_windowKeysDialog = nullptr;
    m_windowKeysWindow = nullptr;
    if (m_activeWindow) {
        m_activeWindow->takeFocus();
    }
}

void Workspace::windowShortcutUpdated(Window *window)
{
    QString key = QStringLiteral("_k_session:%1").arg(window->internalId().toString());
    QAction *action = findChild<QAction *>(key);
    if (!window->shortcut().isEmpty()) {
        if (action == nullptr) { // new shortcut
            action = new QAction(this);
            connect(window, &Window::closed, action, [action]() {
                KGlobalAccel::self()->removeAllShortcuts(action);
                delete action;
            });

            action->setProperty("componentName", QStringLiteral("kwin"));
            action->setObjectName(key);
            action->setText(i18n("Activate Window (%1)", window->caption()));
            connect(action, &QAction::triggered, window, std::bind(&Workspace::activateWindow, this, window, true));
        }

        // no autoloading, since it's configured explicitly here and is not meant to be reused
        // (the key is the window id anyway, which is kind of random)
        KGlobalAccel::self()->setShortcut(action, QList<QKeySequence>() << window->shortcut(),
                                          KGlobalAccel::NoAutoloading);
        action->setEnabled(true);
    } else {
        KGlobalAccel::self()->removeAllShortcuts(action);
        delete action;
    }
}

void Workspace::performWindowOperation(Window *window, Options::WindowOperation op)
{
    if (!window) {
        return;
    }
    if (op == Options::MoveOp || op == Options::UnrestrictedMoveOp) {
        input()->pointer()->warp(window->frameGeometry().center());
    }
    if (op == Options::ResizeOp || op == Options::UnrestrictedResizeOp) {
        input()->pointer()->warp(window->frameGeometry().bottomRight());
    }
    switch (op) {
    case Options::MoveOp:
        window->performMousePressCommand(Options::MouseMove, Cursors::self()->mouse()->pos());
        break;
    case Options::UnrestrictedMoveOp:
        window->performMousePressCommand(Options::MouseUnrestrictedMove, Cursors::self()->mouse()->pos());
        break;
    case Options::ResizeOp:
        window->performMousePressCommand(Options::MouseResize, Cursors::self()->mouse()->pos());
        break;
    case Options::UnrestrictedResizeOp:
        window->performMousePressCommand(Options::MouseUnrestrictedResize, Cursors::self()->mouse()->pos());
        break;
    case Options::CloseOp:
        QMetaObject::invokeMethod(window, &Window::closeWindow, Qt::QueuedConnection);
        break;
    case Options::MaximizeOp:
        window->maximize(window->maximizeMode() == MaximizeFull
                             ? MaximizeRestore
                             : MaximizeFull);
        takeActivity(window, ActivityFocus | ActivityRaise);
        break;
    case Options::HMaximizeOp:
        window->maximize(window->maximizeMode() ^ MaximizeHorizontal);
        takeActivity(window, ActivityFocus | ActivityRaise);
        break;
    case Options::VMaximizeOp:
        window->maximize(window->maximizeMode() ^ MaximizeVertical);
        takeActivity(window, ActivityFocus | ActivityRaise);
        break;
    case Options::RestoreOp:
        window->maximize(MaximizeRestore);
        takeActivity(window, ActivityFocus | ActivityRaise);
        break;
    case Options::MinimizeOp:
        window->setMinimized(true);
        break;
    case Options::OnAllDesktopsOp:
        window->setOnAllDesktops(!window->isOnAllDesktops());
        break;
    case Options::FullScreenOp:
        window->setFullScreen(!window->isFullScreen());
        break;
    case Options::NoBorderOp:
        if (window->userCanSetNoBorder()) {
            window->setNoBorder(!window->noBorder());
        }
        break;
    case Options::KeepAboveOp: {
        StackingUpdatesBlocker blocker(this);
        bool was = window->keepAbove();
        window->setKeepAbove(!window->keepAbove());
        if (was && !window->keepAbove()) {
            raiseWindow(window);
        }
        break;
    }
    case Options::KeepBelowOp: {
        StackingUpdatesBlocker blocker(this);
        bool was = window->keepBelow();
        window->setKeepBelow(!window->keepBelow());
        if (was && !window->keepBelow()) {
            lowerWindow(window);
        }
        break;
    }
    case Options::WindowRulesOp:
        m_rulebook->edit(window, false);
        break;
    case Options::ApplicationRulesOp:
        m_rulebook->edit(window, true);
        break;
    case Options::SetupWindowShortcutOp:
        setupWindowShortcut(window);
        break;
    case Options::LowerOp:
        lowerWindow(window);
        break;
    case Options::NoOp:
        break;
    }
}

void Workspace::slotActivateAttentionWindow()
{
    if (attention_chain.count() > 0) {
        activateWindow(attention_chain.first());
    }
}

#define USABLE_ACTIVE_WINDOW (m_activeWindow && !(m_activeWindow->isDesktop() || m_activeWindow->isDock()))

void Workspace::slotWindowToDesktop(VirtualDesktop *desktop)
{
    if (USABLE_ACTIVE_WINDOW) {
        sendWindowToDesktops(m_activeWindow, {desktop}, true);
    }
}

void Workspace::slotSwitchToScreen(Output *output)
{
    switchToOutput(output);
}

void Workspace::slotSwitchToLeftScreen()
{
    switchToOutput(findOutput(activeOutput(), Direction::DirectionWest, true));
}

void Workspace::slotSwitchToRightScreen()
{
    switchToOutput(findOutput(activeOutput(), Direction::DirectionEast, true));
}

void Workspace::slotSwitchToAboveScreen()
{
    switchToOutput(findOutput(activeOutput(), Direction::DirectionNorth, true));
}

void Workspace::slotSwitchToBelowScreen()
{
    switchToOutput(findOutput(activeOutput(), Direction::DirectionSouth, true));
}

void Workspace::slotSwitchToPrevScreen()
{
    switchToOutput(findOutput(activeOutput(), Direction::DirectionPrev, true));
}

void Workspace::slotSwitchToNextScreen()
{
    switchToOutput(findOutput(activeOutput(), Direction::DirectionNext, true));
}

void Workspace::slotWindowToScreen(Output *output)
{
    if (USABLE_ACTIVE_WINDOW) {
        m_activeWindow->sendToOutput(output);
    }
}

void Workspace::slotWindowToLeftScreen()
{
    if (USABLE_ACTIVE_WINDOW) {
        m_activeWindow->sendToOutput(findOutput(m_activeWindow->output(), Direction::DirectionWest, true));
    }
}

void Workspace::slotWindowToRightScreen()
{
    if (USABLE_ACTIVE_WINDOW) {
        m_activeWindow->sendToOutput(findOutput(m_activeWindow->output(), Direction::DirectionEast, true));
    }
}

void Workspace::slotWindowToAboveScreen()
{
    if (USABLE_ACTIVE_WINDOW) {
        m_activeWindow->sendToOutput(findOutput(m_activeWindow->output(), Direction::DirectionNorth, true));
    }
}

void Workspace::slotWindowToBelowScreen()
{
    if (USABLE_ACTIVE_WINDOW) {
        m_activeWindow->sendToOutput(findOutput(m_activeWindow->output(), Direction::DirectionSouth, true));
    }
}

void Workspace::slotWindowToPrevScreen()
{
    if (USABLE_ACTIVE_WINDOW) {
        m_activeWindow->sendToOutput(findOutput(m_activeWindow->output(), Direction::DirectionPrev, true));
    }
}

void Workspace::slotWindowToNextScreen()
{
    if (USABLE_ACTIVE_WINDOW) {
        m_activeWindow->sendToOutput(findOutput(m_activeWindow->output(), Direction::DirectionNext, true));
    }
}

/**
 * Maximizes the active window.
 */
void Workspace::slotWindowMaximize()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::MaximizeOp);
    }
}

/**
 * Maximizes the active window vertically.
 */
void Workspace::slotWindowMaximizeVertical()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::VMaximizeOp);
    }
}

/**
 * Maximizes the active window horiozontally.
 */
void Workspace::slotWindowMaximizeHorizontal()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::HMaximizeOp);
    }
}

/**
 * Minimizes the active window.
 */
void Workspace::slotWindowMinimize()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::MinimizeOp);
    }
}

/**
 * Raises the active window.
 */
void Workspace::slotWindowRaise()
{
    if (USABLE_ACTIVE_WINDOW) {
        raiseWindow(m_activeWindow);
    }
}

/**
 * Lowers the active window.
 */
void Workspace::slotWindowLower()
{
    if (USABLE_ACTIVE_WINDOW) {
        lowerWindow(m_activeWindow);
        // As this most likely makes the window no longer visible change the
        // keyboard focus to the next available window.
        // activateNextWindow( c ); // Doesn't work when we lower a child window
        if (m_activeWindow->isActive() && options->focusPolicyIsReasonable()) {
            if (options->isNextFocusPrefersMouse()) {
                Window *next = windowUnderMouse(m_activeWindow->output());
                if (next && next != m_activeWindow) {
                    requestFocus(next, false);
                }
            } else {
                activateWindow(topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()));
            }
        }
    }
}

/**
 * Does a toggle-raise-and-lower on the active window.
 */
void Workspace::slotWindowRaiseOrLower()
{
    if (USABLE_ACTIVE_WINDOW) {
        raiseOrLowerWindow(m_activeWindow);
    }
}

void Workspace::slotWindowOnAllDesktops()
{
    if (USABLE_ACTIVE_WINDOW) {
        m_activeWindow->setOnAllDesktops(!m_activeWindow->isOnAllDesktops());
    }
}

void Workspace::slotWindowFullScreen()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::FullScreenOp);
    }
}

void Workspace::slotWindowNoBorder()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::NoBorderOp);
    }
}

void Workspace::slotWindowAbove()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::KeepAboveOp);
    }
}

void Workspace::slotWindowBelow()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::KeepBelowOp);
    }
}
void Workspace::slotSetupWindowShortcut()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::SetupWindowShortcutOp);
    }
}

/**
 * Toggles show desktop.
 */
void Workspace::slotToggleShowDesktop()
{
    setShowingDesktop(!showingDesktop());
}

void windowToDesktop(Window *window, VirtualDesktopManager::Direction direction)
{
    if (window->isDesktop() || window->isDock()) {
        return;
    }

    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    Workspace *ws = Workspace::self();
    // TODO: why is options->isRollOverDesktops() not honored?
    const auto desktop = vds->inDirection(nullptr, direction, true);
    if (ws->moveResizeWindow()) {
        if (ws->moveResizeWindow() == window) {
            vds->setCurrent(desktop);
        }
    } else {
        ws->setMoveResizeWindow(window);
        vds->setCurrent(desktop);
        ws->setMoveResizeWindow(nullptr);
    }
}

/**
 * Moves the active window to the next desktop.
 */
void Workspace::slotWindowToNextDesktop()
{
    if (USABLE_ACTIVE_WINDOW) {
        windowToNextDesktop(m_activeWindow);
    }
}

void Workspace::windowToNextDesktop(Window *window)
{
    windowToDesktop(window, VirtualDesktopManager::Direction::Next);
}

/**
 * Moves the active window to the previous desktop.
 */
void Workspace::slotWindowToPreviousDesktop()
{
    if (USABLE_ACTIVE_WINDOW) {
        windowToPreviousDesktop(m_activeWindow);
    }
}

void Workspace::windowToPreviousDesktop(Window *window)
{
    windowToDesktop(window, VirtualDesktopManager::Direction::Previous);
}

void activeWindowToDesktop(VirtualDesktopManager::Direction direction)
{
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    Workspace *ws = Workspace::self();
    VirtualDesktop *current = vds->currentDesktop();
    VirtualDesktop *newCurrent = VirtualDesktopManager::self()->inDirection(current, direction, options->isRollOverDesktops());
    if (newCurrent == current) {
        return;
    }
    if (ws->moveResizeWindow()) {
        if (ws->moveResizeWindow() == ws->activeWindow()) {
            vds->setCurrent(newCurrent);
        }
    } else {
        ws->setMoveResizeWindow(ws->activeWindow());
        vds->setCurrent(newCurrent);
        ws->setMoveResizeWindow(nullptr);
    }
}

void Workspace::slotWindowToDesktopRight()
{
    if (USABLE_ACTIVE_WINDOW) {
        activeWindowToDesktop(VirtualDesktopManager::Direction::Right);
    }
}

void Workspace::slotWindowToDesktopLeft()
{
    if (USABLE_ACTIVE_WINDOW) {
        activeWindowToDesktop(VirtualDesktopManager::Direction::Left);
    }
}

void Workspace::slotWindowToDesktopUp()
{
    if (USABLE_ACTIVE_WINDOW) {
        activeWindowToDesktop(VirtualDesktopManager::Direction::Up);
    }
}

void Workspace::slotWindowToDesktopDown()
{
    if (USABLE_ACTIVE_WINDOW) {
        activeWindowToDesktop(VirtualDesktopManager::Direction::Down);
    }
}

/**
 * Kill Window feature, similar to xkill.
 */
void Workspace::slotKillWindow()
{
    if (!m_windowKiller) {
        m_windowKiller = std::make_unique<KillWindow>();
    }
    m_windowKiller->start();
}

/**
 * Switches to the nearest window in given direction.
 */
void Workspace::switchWindow(Direction direction)
{
    if (!m_activeWindow) {
        return;
    }
    Window *window = m_activeWindow;
    VirtualDesktop *desktop = VirtualDesktopManager::self()->currentDesktop();

    // Center of the active window
    QPoint curPos(window->x() + window->width() / 2, window->y() + window->height() / 2);

    if (!switchWindow(window, direction, curPos, desktop)) {
        auto opposite = [&] {
            switch (direction) {
            case DirectionNorth:
                return QPoint(curPos.x(), geometry().height());
            case DirectionSouth:
                return QPoint(curPos.x(), 0);
            case DirectionEast:
                return QPoint(0, curPos.y());
            case DirectionWest:
                return QPoint(geometry().width(), curPos.y());
            default:
                Q_UNREACHABLE();
            }
        };

        switchWindow(window, direction, opposite(), desktop);
    }
}

bool Workspace::switchWindow(Window *window, Direction direction, QPoint curPos, VirtualDesktop *desktop)
{
    Window *switchTo = nullptr;
    int bestScore = 0;

    QList<Window *> clist = stackingOrder();
    for (auto i = clist.rbegin(); i != clist.rend(); ++i) {
        auto other = *i;
        if (!other->isClient()) {
            continue;
        }
        if (other->wantsTabFocus() && *i != window && other->isOnDesktop(desktop) && !other->isMinimized() && (*i)->isOnCurrentActivity()) {
            // Center of the other window
            const QPoint otherCenter(other->x() + other->width() / 2, other->y() + other->height() / 2);

            int distance;
            int offset;
            switch (direction) {
            case DirectionNorth:
                distance = curPos.y() - otherCenter.y();
                offset = std::abs(otherCenter.x() - curPos.x());
                break;
            case DirectionEast:
                distance = otherCenter.x() - curPos.x();
                offset = std::abs(otherCenter.y() - curPos.y());
                break;
            case DirectionSouth:
                distance = otherCenter.y() - curPos.y();
                offset = std::abs(otherCenter.x() - curPos.x());
                break;
            case DirectionWest:
                distance = curPos.x() - otherCenter.x();
                offset = std::abs(otherCenter.y() - curPos.y());
                break;
            default:
                distance = -1;
                offset = -1;
            }

            if (distance > 0) {
                // Inverse score
                int score = distance + offset + ((offset * offset) / distance);
                if (score < bestScore || !switchTo) {
                    switchTo = other;
                    bestScore = score;
                }
            }
        }
    }
    if (switchTo) {
        activateWindow(switchTo);
    }

    return switchTo;
}

/**
 * Shows the window operations popup menu for the active window.
 */
void Workspace::slotWindowOperations()
{
    if (!m_activeWindow) {
        return;
    }
    const QPoint pos(m_activeWindow->frameGeometry().left() + m_activeWindow->frameMargins().left(),
                     m_activeWindow->frameGeometry().top() + m_activeWindow->frameMargins().top());
    showWindowMenu(QRect(pos, pos), m_activeWindow);
}

void Workspace::showWindowMenu(const QRect &pos, Window *window)
{
    m_userActionsMenu->show(pos, window);
}

void Workspace::showApplicationMenu(const QRect &pos, Window *window, int actionId)
{
    Workspace::self()->applicationMenu()->showApplicationMenu(window->pos().toPoint() + pos.bottomLeft(), window, actionId);
}

/**
 * Closes the active window.
 */
void Workspace::slotWindowClose()
{
    // TODO: why?
    //     if ( tab_box->isVisible())
    //         return;
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::CloseOp);
    }
}

/**
 * Starts keyboard move mode for the active window.
 */
void Workspace::slotWindowMove()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::UnrestrictedMoveOp);
    }
}

/**
 * Starts keyboard resize mode for the active window.
 */
void Workspace::slotWindowResize()
{
    if (USABLE_ACTIVE_WINDOW) {
        performWindowOperation(m_activeWindow, Options::UnrestrictedResizeOp);
    }
}

#undef USABLE_ACTIVE_WINDOW

void Window::setShortcut(const QString &_cut)
{
    QString cut = rules()->checkShortcut(_cut);
    auto updateShortcut = [this](const QKeySequence &cut = QKeySequence()) {
        if (_shortcut == cut) {
            return;
        }
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
        if (workspace()->shortcutAvailable(cut, this)) {
            updateShortcut(QKeySequence(cut));
        } else {
            updateShortcut();
        }
        return;
    }
    static const QRegularExpression reg(QStringLiteral("(.*\\+)\\((.*)\\)"));
    QList<QKeySequence> keys;
    const QStringList groups = cut.split(QStringLiteral(" - "));
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const QRegularExpressionMatch match = reg.match(*it);
        if (match.hasMatch()) {
            const QString base = match.captured(1);
            const QString list = match.captured(2);
            for (int i = 0; i < list.length(); ++i) {
                QKeySequence c(base + list[i]);
                if (!c.isEmpty()) {
                    keys.append(c);
                }
            }
        } else {
            // regexp doesn't match, so it should be a normal shortcut
            QKeySequence c(*it);
            if (!c.isEmpty()) {
                keys.append(c);
            }
        }
    }
    for (auto it = keys.constBegin(); it != keys.cend(); ++it) {
        if (_shortcut == *it) { // current one is in the list
            return;
        }
    }
    for (auto it = keys.cbegin(); it != keys.cend(); ++it) {
        if (workspace()->shortcutAvailable(*it, this)) {
            updateShortcut(*it);
            return;
        }
    }
    updateShortcut();
}

void Window::setShortcutInternal()
{
    updateCaption();
    workspace()->windowShortcutUpdated(this);
}

bool Workspace::shortcutAvailable(const QKeySequence &cut, Window *ignore) const
{
    if (ignore && cut == ignore->shortcut()) {
        return true;
    }

    // Check if the shortcut is already registered
    const QList<KGlobalShortcutInfo> registeredShortcuts = KGlobalAccel::globalShortcutsByKey(cut);
    for (const auto &shortcut : registeredShortcuts) {
        // Only return "not available" if is not a window activation shortcut, as it may be no longer valid
        if (!shortcut.uniqueName().startsWith(QStringLiteral("_k_session:"))) {
            return false;
        }
    }
    // Check now conflicts with activation shortcuts for current windows
    for (const auto window : std::as_const(m_windows)) {
        if (window != ignore && window->shortcut() == cut) {
            return false;
        }
    }
    return true;
}

} // namespace

#include "moc_useractions.cpp"
