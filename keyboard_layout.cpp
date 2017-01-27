/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

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
#include "keyboard_layout.h"
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
    m_notifierItem->setStatus(KStatusNotifierItem::Active);
    m_notifierItem->setToolTipTitle(i18nc("tooltip title", "Keyboard Layout"));
    m_notifierItem->setTitle(i18nc("tooltip title", "Keyboard Layout"));
    m_notifierItem->setToolTipIconByName(QStringLiteral("preferences-desktop-keyboard"));
    m_notifierItem->setStandardActionsEnabled(false);

    // TODO: proper icon
    m_notifierItem->setIconByName(QStringLiteral("preferences-desktop-keyboard"));

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

    m_notifierItem->setStatus(KStatusNotifierItem::Active);
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
    }
    m_xkb->reconfigure();
    resetLayout();
}

void KeyboardLayout::resetLayout()
{
    m_layout = m_xkb->currentLayout();
    initNotifierItem();
    updateNotifier();
    reinitNotifierMenu();
    loadShortcuts();
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

    m_notifierItem->setContextMenu(menu);
}

}
