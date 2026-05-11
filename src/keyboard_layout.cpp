/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_layout.h"
#include "input_event.h"
#include "keyboard_input.h"
#include "keyboard_layout_switching.h"
#include "xkb.h"

#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>

namespace KWin
{

KeyboardLayout::KeyboardLayout(InputRedirection *input, const KSharedConfigPtr &config)
    : QObject(input)
    , m_input(input)
    , m_configWatcher(KConfigWatcher::create(config))
    , m_configGroup(config->group(QStringLiteral("Layout")))
{
}

KeyboardLayout::~KeyboardLayout() = default;

Xkb *KeyboardLayout::xkb() const
{
    if (const auto keyboard = m_input->keyboard()) {
        return keyboard->xkb();
    }
    return nullptr;
}

static QString translatedLayout(const QString &layout)
{
    return i18nd("xkeyboard-config", layout.toUtf8().constData());
}

void KeyboardLayout::init()
{
    QAction *switchKeyboardAction = new QAction(this);
    switchKeyboardAction->setObjectName(QStringLiteral("Switch to Next Keyboard Layout"));
    switchKeyboardAction->setProperty("componentName", QStringLiteral("KDE Keyboard Layout Switcher"));
    switchKeyboardAction->setProperty("componentDisplayName", i18n("Keyboard Layout Switcher"));
    KGlobalAccel::self()->setGlobalShortcut(switchKeyboardAction, QKeySequence(Qt::META | Qt::ALT | Qt::Key_K));

    connect(switchKeyboardAction, &QAction::triggered, this, &KeyboardLayout::switchToNextLayout);

    QAction *switchLastUsedKeyboardAction = new QAction(this);
    switchLastUsedKeyboardAction->setObjectName(QStringLiteral("Switch to Last-Used Keyboard Layout"));
    switchLastUsedKeyboardAction->setProperty("componentName", QStringLiteral("KDE Keyboard Layout Switcher"));
    switchLastUsedKeyboardAction->setProperty("componentDisplayName", i18n("Keyboard Layout Switcher"));
    KGlobalAccel::self()->setGlobalShortcut(switchLastUsedKeyboardAction, QKeySequence(Qt::META | Qt::ALT | Qt::Key_L));

    connect(switchLastUsedKeyboardAction, &QAction::triggered, this, &KeyboardLayout::switchToLastUsedLayout);

    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, &KeyboardLayout::handleXkbConfigChanged);

    reconfigure();

    m_dbusInterface = new KeyboardLayoutDBusInterface(m_configGroup, this);
    connect(this, &KeyboardLayout::layoutChanged,
            m_dbusInterface, &KeyboardLayoutDBusInterface::layoutChanged);
    // TODO: the signal might be emitted even if the list didn't change
    connect(this, &KeyboardLayout::layoutsReconfigured, m_dbusInterface, &KeyboardLayoutDBusInterface::layoutListChanged);
}

void KeyboardLayout::switchToNextLayout()
{
    Xkb *currentXkb = xkb();
    if (!currentXkb) {
        return;
    }
    const quint32 previousLayout = currentXkb->currentLayout();
    currentXkb->switchToNextLayout();
    checkLayoutChange(previousLayout);
}

void KeyboardLayout::switchToPreviousLayout()
{
    Xkb *currentXkb = xkb();
    if (!currentXkb) {
        return;
    }
    const quint32 previousLayout = currentXkb->currentLayout();
    currentXkb->switchToPreviousLayout();
    checkLayoutChange(previousLayout);
}

void KeyboardLayout::switchToLayout(xkb_layout_index_t index)
{
    Xkb *currentXkb = xkb();
    if (!currentXkb) {
        return;
    }
    const quint32 previousLayout = currentXkb->currentLayout();
    currentXkb->switchToLayout(index);
    checkLayoutChange(previousLayout);
}

void KeyboardLayout::switchToLastUsedLayout()
{
    Xkb *currentXkb = xkb();
    if (!currentXkb) {
        return;
    }
    const quint32 count = currentXkb->numberOfLayouts();
    if (!m_lastUsedLayout.has_value() || *m_lastUsedLayout >= count) {
        switchToPreviousLayout();
    } else {
        switchToLayout(*m_lastUsedLayout);
    }
}

void KeyboardLayout::handleXkbConfigChanged(const KConfigGroup &group)
{
    if (group.name() == QStringLiteral("Layout")) {
        reconfigure();
    }
}

void KeyboardLayout::reconfigure()
{
    Xkb *currentXkb = xkb();
    if (!currentXkb) {
        return;
    }
    if (m_configGroup.isValid()) {
        m_configGroup.config()->reparseConfiguration();
        const QString policyKey = m_configGroup.readEntry("SwitchMode", QStringLiteral("Global"));
        currentXkb->reconfigure();
        if (!m_policy || m_policy->name() != policyKey) {
            m_policy = KeyboardLayoutSwitching::Policy::create(this, m_configGroup, policyKey);
        }
    } else {
        currentXkb->reconfigure();
    }
    resetLayout();
}

void KeyboardLayout::resetLayout()
{
    if (Xkb *currentXkb = xkb()) {
        m_layout = currentXkb->currentLayout();
    } else {
        m_layout = 0;
    }
    loadShortcuts();

    Q_EMIT layoutsReconfigured();
}

void KeyboardLayout::loadShortcuts()
{
    qDeleteAll(m_layoutShortcuts);
    m_layoutShortcuts.clear();
    Xkb *currentXkb = xkb();
    if (!currentXkb) {
        return;
    }
    const QString componentName = QStringLiteral("KDE Keyboard Layout Switcher");
    const QString componentDisplayName = i18n("Keyboard Layout Switcher");
    const quint32 count = currentXkb->numberOfLayouts();
    for (uint i = 0; i < count; ++i) {
        // layout name is translated in the action name in keyboard kcm!
        const QString action = QStringLiteral("Switch keyboard layout to %1").arg(translatedLayout(currentXkb->layoutName(i)));
        const auto shortcuts = KGlobalAccel::self()->globalShortcut(componentName, action);
        if (shortcuts.isEmpty()) {
            continue;
        }
        QAction *a = new QAction(this);
        a->setObjectName(action);
        a->setProperty("componentName", componentName);
        a->setProperty("componentDisplayName", componentDisplayName);
        connect(a, &QAction::triggered, this,
                std::bind(&KeyboardLayout::switchToLayout, this, i));
        KGlobalAccel::self()->setShortcut(a, shortcuts, KGlobalAccel::Autoloading);
        m_layoutShortcuts << a;
    }
}

void KeyboardLayout::checkLayoutChange(uint previousLayout)
{
    // Get here on key event or DBus call.
    // m_layout - layout saved last time OSD occurred
    // previousLayout - actual layout just before potential layout change
    // We need OSD if current layout deviates from any of these
    Xkb *currentXkb = xkb();
    if (!currentXkb) {
        return;
    }
    const uint currentLayout = currentXkb->currentLayout();
    if (m_layout != currentLayout || previousLayout != currentLayout) {
        m_lastUsedLayout = std::optional<uint>{previousLayout};
        m_layout = currentLayout;
        notifyLayoutChange();
        Q_EMIT layoutChanged(currentLayout);
    }
}

void KeyboardLayout::notifyLayoutChange()
{
    // notify OSD service about the new layout
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"),
        QStringLiteral("org.kde.osdService"),
        QStringLiteral("kbdLayoutChanged"));

    if (Xkb *currentXkb = xkb()) {
        msg << translatedLayout(currentXkb->layoutName());
        QDBusConnection::sessionBus().asyncCall(msg);
    }
}

static const QString s_keyboardService = QStringLiteral("org.kde.keyboard");
static const QString s_keyboardObject = QStringLiteral("/Layouts");

KeyboardLayoutDBusInterface::KeyboardLayoutDBusInterface(const KConfigGroup &configGroup, KeyboardLayout *parent)
    : QObject(parent)
    , m_configGroup(configGroup)
    , m_keyboardLayout(parent)
{
    qRegisterMetaType<QList<LayoutNames>>("QList<LayoutNames>");
    qDBusRegisterMetaType<LayoutNames>();
    qDBusRegisterMetaType<QList<LayoutNames>>();

    QDBusConnection::sessionBus().registerObject(s_keyboardObject, this, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    QDBusConnection::sessionBus().registerService(s_keyboardService);
}

KeyboardLayoutDBusInterface::~KeyboardLayoutDBusInterface()
{
    QDBusConnection::sessionBus().unregisterService(s_keyboardService);
}

void KeyboardLayoutDBusInterface::switchToNextLayout()
{
    m_keyboardLayout->switchToNextLayout();
}

void KeyboardLayoutDBusInterface::switchToPreviousLayout()
{
    m_keyboardLayout->switchToPreviousLayout();
}

bool KeyboardLayoutDBusInterface::setLayout(uint index)
{
    Xkb *currentXkb = m_keyboardLayout->xkb();
    if (!currentXkb) {
        return false;
    }
    const quint32 previousLayout = currentXkb->currentLayout();
    if (!currentXkb->switchToLayout(index)) {
        return false;
    }
    m_keyboardLayout->checkLayoutChange(previousLayout);
    return true;
}

uint KeyboardLayoutDBusInterface::getLayout() const
{
    if (Xkb *currentXkb = m_keyboardLayout->xkb()) {
        return currentXkb->currentLayout();
    }
    return 0;
}

QList<KeyboardLayoutDBusInterface::LayoutNames> KeyboardLayoutDBusInterface::getLayoutsList() const
{
    // TODO: - should be handled by layout applet itself, it has nothing to do with KWin
    const QStringList displayNames = m_configGroup.readEntry("DisplayNames", QStringList());

    QList<LayoutNames> ret;
    Xkb *currentXkb = m_keyboardLayout->xkb();
    if (!currentXkb) {
        return ret;
    }
    const int layoutsSize = currentXkb->numberOfLayouts();
    const int displayNamesSize = displayNames.size();
    for (int i = 0; i < layoutsSize; ++i) {
        ret.append({currentXkb->layoutShortName(i), i < displayNamesSize ? displayNames.at(i) : QString(), translatedLayout(currentXkb->layoutName(i))});
    }
    return ret;
}

QDBusArgument &operator<<(QDBusArgument &argument, const KeyboardLayoutDBusInterface::LayoutNames &layoutNames)
{
    argument.beginStructure();
    argument << layoutNames.shortName << layoutNames.displayName << layoutNames.longName;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, KeyboardLayoutDBusInterface::LayoutNames &layoutNames)
{
    argument.beginStructure();
    argument >> layoutNames.shortName >> layoutNames.displayName >> layoutNames.longName;
    argument.endStructure();
    return argument;
}

}

#include "moc_keyboard_layout.cpp"
