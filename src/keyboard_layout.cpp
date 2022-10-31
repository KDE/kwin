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

KeyboardLayout::KeyboardLayout(Xkb *xkb, const KSharedConfigPtr &config)
    : QObject()
    , m_xkb(xkb)
    , m_configGroup(config->group("Layout"))
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
    switchKeyboardAction->setProperty("componentDisplayName", i18n("Keyboard Layout Switcher"));
    const QKeySequence sequence = QKeySequence(Qt::META | Qt::ALT | Qt::Key_K);
    KGlobalAccel::self()->setDefaultShortcut(switchKeyboardAction, QList<QKeySequence>({sequence}));
    KGlobalAccel::self()->setShortcut(switchKeyboardAction, QList<QKeySequence>({sequence}));

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
    m_dbusInterface = new KeyboardLayoutDBusInterface(m_xkb, m_configGroup, this);
    connect(this, &KeyboardLayout::layoutChanged,
            m_dbusInterface, &KeyboardLayoutDBusInterface::layoutChanged);
    // TODO: the signal might be emitted even if the list didn't change
    connect(this, &KeyboardLayout::layoutsReconfigured, m_dbusInterface, &KeyboardLayoutDBusInterface::layoutListChanged);
}

void KeyboardLayout::switchToNextLayout()
{
    const quint32 previousLayout = m_xkb->currentLayout();
    m_xkb->switchToNextLayout();
    checkLayoutChange(previousLayout);
}

void KeyboardLayout::switchToPreviousLayout()
{
    const quint32 previousLayout = m_xkb->currentLayout();
    m_xkb->switchToPreviousLayout();
    checkLayoutChange(previousLayout);
}

void KeyboardLayout::switchToLayout(xkb_layout_index_t index)
{
    const quint32 previousLayout = m_xkb->currentLayout();
    m_xkb->switchToLayout(index);
    checkLayoutChange(previousLayout);
}

void KeyboardLayout::reconfigure()
{
    if (m_configGroup.isValid()) {
        m_configGroup.config()->reparseConfiguration();
        const QString policyKey = m_configGroup.readEntry("SwitchMode", QStringLiteral("Global"));
        m_xkb->reconfigure();
        if (!m_policy || m_policy->name() != policyKey) {
            m_policy = KeyboardLayoutSwitching::Policy::create(m_xkb, this, m_configGroup, policyKey);
        }
    } else {
        m_xkb->reconfigure();
    }
    resetLayout();
}

void KeyboardLayout::resetLayout()
{
    m_layout = m_xkb->currentLayout();
    loadShortcuts();

    initDBusInterface();
    Q_EMIT layoutsReconfigured();
}

void KeyboardLayout::loadShortcuts()
{
    qDeleteAll(m_layoutShortcuts);
    m_layoutShortcuts.clear();
    const QString componentName = QStringLiteral("KDE Keyboard Layout Switcher");
    const quint32 count = m_xkb->numberOfLayouts();
    for (uint i = 0; i < count; ++i) {
        // layout name is translated in the action name in keyboard kcm!
        const QString action = QStringLiteral("Switch keyboard layout to %1").arg(translatedLayout(m_xkb->layoutName(i)));
        const auto shortcuts = KGlobalAccel::self()->globalShortcut(componentName, action);
        if (shortcuts.isEmpty()) {
            continue;
        }
        QAction *a = new QAction(this);
        a->setObjectName(action);
        a->setProperty("componentName", componentName);
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
    const uint currentLayout = m_xkb->currentLayout();
    if (m_layout != currentLayout || previousLayout != currentLayout) {
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

    msg << translatedLayout(m_xkb->layoutName());

    QDBusConnection::sessionBus().asyncCall(msg);
}

static const QString s_keyboardService = QStringLiteral("org.kde.keyboard");
static const QString s_keyboardObject = QStringLiteral("/Layouts");

KeyboardLayoutDBusInterface::KeyboardLayoutDBusInterface(Xkb *xkb, const KConfigGroup &configGroup, KeyboardLayout *parent)
    : QObject(parent)
    , m_xkb(xkb)
    , m_configGroup(configGroup)
    , m_keyboardLayout(parent)
{
    qRegisterMetaType<QVector<LayoutNames>>("QVector<LayoutNames>");
    qDBusRegisterMetaType<LayoutNames>();
    qDBusRegisterMetaType<QVector<LayoutNames>>();

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
    const quint32 previousLayout = m_xkb->currentLayout();
    if (!m_xkb->switchToLayout(index)) {
        return false;
    }
    m_keyboardLayout->checkLayoutChange(previousLayout);
    return true;
}

uint KeyboardLayoutDBusInterface::getLayout() const
{
    return m_xkb->currentLayout();
}

QVector<KeyboardLayoutDBusInterface::LayoutNames> KeyboardLayoutDBusInterface::getLayoutsList() const
{
    // TODO: - should be handled by layout applet itself, it has nothing to do with KWin
    const QStringList displayNames = m_configGroup.readEntry("DisplayNames", QStringList());

    QVector<LayoutNames> ret;
    const int layoutsSize = m_xkb->numberOfLayouts();
    const int displayNamesSize = displayNames.size();
    for (int i = 0; i < layoutsSize; ++i) {
        ret.append({m_xkb->layoutShortName(i), i < displayNamesSize ? displayNames.at(i) : QString(), translatedLayout(m_xkb->layoutName(i))});
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
