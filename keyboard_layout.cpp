/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_layout.h"
#include "keyboard_layout_switching.h"
#include "keyboard_input.h"
#include "input_event.h"
#include "main.h"
#include "platform.h"
#include "utils.h"

#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KNotifications/KStatusNotifierItem>
#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QMenu>

namespace KWin
{

KeyboardLayout::KeyboardLayout(Xkb *xkb)
    : QObject()
    , m_xkb(xkb)
    , m_notifierItem(nullptr)
{
}

KeyboardLayout::~KeyboardLayout() = default;

static QString translatedLayout(const QString &layout)
{
    return i18nd("xkeyboard-config", layout.toUtf8().constData());
}

void KeyboardLayout::init()
{
    QAction *switchKeyboardAction = new QAction(this);
    switchKeyboardAction->setObjectName(QStringLiteral("Switch to Next Keyboard Layout"));
    switchKeyboardAction->setProperty("componentName", QStringLiteral("KDE Keyboard Layout Switcher"));
    const QKeySequence sequence = QKeySequence(Qt::ALT+Qt::CTRL+Qt::Key_K);
    KGlobalAccel::self()->setDefaultShortcut(switchKeyboardAction, QList<QKeySequence>({sequence}));
    KGlobalAccel::self()->setShortcut(switchKeyboardAction, QList<QKeySequence>({sequence}));
    kwinApp()->platform()->setupActionForGlobalAccel(switchKeyboardAction);
    connect(switchKeyboardAction, &QAction::triggered, this, &KeyboardLayout::switchToNextLayout);

    QDBusConnection::sessionBus().connect(QString(),
                                          QStringLiteral("/Layouts"),
                                          QStringLiteral("org.kde.keyboard"),
                                          QStringLiteral("reloadConfig"),
                                          this,
                                          SLOT(reconfigure()));

    reconfigure();
}

void KeyboardLayout::initDBusInterface()
{
    if (m_xkb->numberOfLayouts() <= 1) {
        if (m_dbusInterface) {
            m_dbusInterface->deleteLater();
            m_dbusInterface = nullptr;
        }
        return;
    }
    if (m_dbusInterface) {
        return;
    }
    m_dbusInterface = new KeyboardLayoutDBusInterface(m_xkb, this);
    connect(this, &KeyboardLayout::layoutChanged, m_dbusInterface,
        [this] {
            emit m_dbusInterface->currentLayoutChanged(m_xkb->layoutName());
        }
    );
    // TODO: the signal might be emitted even if the list didn't change
    connect(this, &KeyboardLayout::layoutsReconfigured, m_dbusInterface, &KeyboardLayoutDBusInterface::layoutListChanged);
}

void KeyboardLayout::initNotifierItem()
{
    bool showNotifier = true;
    bool showSingle = false;
    if (m_config) {
        const auto config = m_config->group(QStringLiteral("Layout"));
        showNotifier = config.readEntry("ShowLayoutIndicator", true);
        showSingle = config.readEntry("ShowSingle", false);
    }
    const bool shouldShow = showNotifier && (showSingle || m_xkb->numberOfLayouts() > 1);
    if (shouldShow) {
        if (m_notifierItem) {
            return;
        }
    } else {
        delete m_notifierItem;
        m_notifierItem = nullptr;
        return;
    }

    m_notifierItem = new KStatusNotifierItem(this);
    m_notifierItem->setCategory(KStatusNotifierItem::Hardware);
    m_notifierItem->setStatus(KStatusNotifierItem::Passive);
    m_notifierItem->setToolTipTitle(i18nc("tooltip title", "Keyboard Layout"));
    m_notifierItem->setTitle(i18nc("tooltip title", "Keyboard Layout"));
    m_notifierItem->setToolTipIconByName(QStringLiteral("input-keyboard"));
    m_notifierItem->setStandardActionsEnabled(false);

    // TODO: proper icon
    m_notifierItem->setIconByName(QStringLiteral("input-keyboard"));

    connect(m_notifierItem, &KStatusNotifierItem::activateRequested, this, &KeyboardLayout::switchToNextLayout);
    connect(m_notifierItem, &KStatusNotifierItem::scrollRequested, this,
        [this] (int delta, Qt::Orientation orientation) {
            if (orientation == Qt::Horizontal) {
                return;
            }
            if (delta > 0) {
                switchToNextLayout();
            } else {
                switchToPreviousLayout();
            }
        }
    );
}

void KeyboardLayout::switchToNextLayout()
{
    m_xkb->switchToNextLayout();
    checkLayoutChange();
}

void KeyboardLayout::switchToPreviousLayout()
{
    m_xkb->switchToPreviousLayout();
    checkLayoutChange();
}

void KeyboardLayout::switchToLayout(xkb_layout_index_t index)
{
    m_xkb->switchToLayout(index);
    checkLayoutChange();
}

void KeyboardLayout::reconfigure()
{
    if (m_config) {
        m_config->reparseConfiguration();
        const KConfigGroup layoutGroup = m_config->group("Layout");
        const QString policyKey = layoutGroup.readEntry("SwitchMode", QStringLiteral("Global"));
        m_xkb->reconfigure();
        if (!m_policy || m_policy->name() != policyKey) {
            delete m_policy;
            m_policy = KeyboardLayoutSwitching::Policy::create(m_xkb, this, layoutGroup, policyKey);
        }
    } else {
        m_xkb->reconfigure();
    }
    resetLayout();
}

void KeyboardLayout::resetLayout()
{
    m_layout = m_xkb->currentLayout();
    initNotifierItem();
    updateNotifier();
    reinitNotifierMenu();
    loadShortcuts();

    initDBusInterface();
    emit layoutsReconfigured();
}

void KeyboardLayout::loadShortcuts()
{
    qDeleteAll(m_layoutShortcuts);
    m_layoutShortcuts.clear();
    const auto layouts = m_xkb->layoutNames();
    const QString componentName = QStringLiteral("KDE Keyboard Layout Switcher");
    for (auto it = layouts.begin(); it != layouts.end(); it++) {
        // layout name is translated in the action name in keyboard kcm!
        const QString action = QStringLiteral("Switch keyboard layout to %1").arg(translatedLayout(it.value()));
        const auto shortcuts = KGlobalAccel::self()->globalShortcut(componentName, action);
        if (shortcuts.isEmpty()) {
            continue;
        }
        QAction *a = new QAction(this);
        a->setObjectName(action);
        a->setProperty("componentName", componentName);
        connect(a, &QAction::triggered, this,
                std::bind(&KeyboardLayout::switchToLayout, this, it.key()));
        KGlobalAccel::self()->setShortcut(a, shortcuts, KGlobalAccel::Autoloading);
        m_layoutShortcuts << a;
    }
}

void KeyboardLayout::keyEvent(KeyEvent *event)
{
    if (!event->isAutoRepeat()) {
        checkLayoutChange();
    }
}

void KeyboardLayout::checkLayoutChange()
{
    const auto layout = m_xkb->currentLayout();
    if (m_layout == layout) {
        return;
    }
    m_layout = layout;
    notifyLayoutChange();
    updateNotifier();
    emit layoutChanged();
}

void KeyboardLayout::notifyLayoutChange()
{
    // notify OSD service about the new layout
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"),
        QStringLiteral("org.kde.osdService"),
        QStringLiteral("kbdLayoutChanged"));

    msg << translatedLayout(m_xkb->layoutName());

    QDBusConnection::sessionBus().asyncCall(msg);
}

void KeyboardLayout::updateNotifier()
{
    if (!m_notifierItem) {
        return;
    }
    m_notifierItem->setToolTipSubTitle(translatedLayout(m_xkb->layoutName()));
    // TODO: update icon
}

void KeyboardLayout::reinitNotifierMenu()
{
    if (!m_notifierItem) {
        return;
    }
    const auto layouts = m_xkb->layoutNames();

    QMenu *menu = new QMenu;
    for (auto it = layouts.begin(); it != layouts.end(); it++) {
        menu->addAction(translatedLayout(it.value()), std::bind(&KeyboardLayout::switchToLayout, this, it.key()));
    }

    menu->addSeparator();
    menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Configure Layouts..."), this,
        [this] {
            // TODO: introduce helper function to start kcmshell5
            QProcess *p = new Process(this);
            p->setArguments(QStringList{QStringLiteral("--args=--tab=layouts"), QStringLiteral("kcm_keyboard")});
            p->setProcessEnvironment(kwinApp()->processStartupEnvironment());
            p->setProgram(QStringLiteral("kcmshell5"));
            connect(p, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), p, &QProcess::deleteLater);
            connect(p, &QProcess::errorOccurred, this, [](QProcess::ProcessError e) {
                if (e == QProcess::FailedToStart) {
                    qCDebug(KWIN_CORE) << "Failed to start kcmshell5";
                }
            });
            p->start();
        }
    );

    m_notifierItem->setContextMenu(menu);
}

static const QString s_keyboardService = QStringLiteral("org.kde.keyboard");
static const QString s_keyboardObject = QStringLiteral("/Layouts");

KeyboardLayoutDBusInterface::KeyboardLayoutDBusInterface(Xkb *xkb, KeyboardLayout *parent)
    : QObject(parent)
    , m_xkb(xkb)
    , m_keyboardLayout(parent)
{
    QDBusConnection::sessionBus().registerService(s_keyboardService);
    QDBusConnection::sessionBus().registerObject(s_keyboardObject, this, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
}

KeyboardLayoutDBusInterface::~KeyboardLayoutDBusInterface()
{
    QDBusConnection::sessionBus().unregisterService(s_keyboardService);
}

bool KeyboardLayoutDBusInterface::setLayout(const QString &layout)
{
    const auto layouts = m_xkb->layoutNames();
    auto it = layouts.begin();
    for (; it !=layouts.end(); it++) {
        if (it.value() == layout) {
            break;
        }
    }
    if (it == layouts.end()) {
        return false;
    }
    m_xkb->switchToLayout(it.key());
    m_keyboardLayout->checkLayoutChange();
    return true;
}

QString KeyboardLayoutDBusInterface::getCurrentLayout()
{
    return m_xkb->layoutName();
}

QStringList KeyboardLayoutDBusInterface::getLayoutsList()
{
    const auto layouts = m_xkb->layoutNames();
    QStringList ret;
    for (auto it = layouts.begin(); it != layouts.end(); it++) {
        ret << it.value();
    }
    return ret;
}

QString KeyboardLayoutDBusInterface::getLayoutDisplayName(const QString &layout)
{
    return translatedLayout(layout);
}

}
