/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "shortcutsettings.h"

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>

// Implementation of a KConfigSkeletonItem that uses KGlobalAccel to retrieve and store
// shortcut settings instead of storing them in a config file
class ShortcutItem : public KConfigSkeletonItem
{
public:
    ShortcutItem(QAction *action, KActionCollection *actionCollection);

    void readConfig(KConfig *config) override;
    void writeConfig(KConfig *config) override;

    void readDefault(KConfig *config) override;
    void setDefault() override;
    void swapDefault() override;

    bool isEqual(const QVariant &p) const override;
    QVariant property() const override;
    void setProperty(const QVariant &p) override;

private:
    KActionCollection *m_actionCollection = nullptr;
    QAction *m_action = nullptr;
    QList<QKeySequence> m_savedShortcuts;
};

ShortcutItem::ShortcutItem(QAction *action, KActionCollection *actionCollection)
    : KConfigSkeletonItem(actionCollection->componentName(), action->text())
    , m_actionCollection(actionCollection)
    , m_action(action)
{
    setGetDefaultImpl([this] {
        return QVariant::fromValue(m_actionCollection->defaultShortcuts(m_action));
    });

    setIsDefaultImpl([this] {
        return m_action->shortcuts() == m_actionCollection->defaultShortcuts(m_action);
    });

    setIsSaveNeededImpl([this] {
        return (m_action->shortcuts() != m_savedShortcuts);
    });
}

void ShortcutItem::readConfig(KConfig *config)
{
    m_savedShortcuts = KGlobalAccel::self()->globalShortcut(m_actionCollection->componentName(), m_action->objectName());
    m_action->setShortcuts(m_savedShortcuts);
}

void ShortcutItem::writeConfig(KConfig *config)
{
    m_savedShortcuts = m_action->shortcuts();
    KGlobalAccel::self()->setShortcut(m_action, m_action->shortcuts(), KGlobalAccel::NoAutoloading);
}

void ShortcutItem::readDefault(KConfig *config)
{
}

void ShortcutItem::setDefault()
{
    m_action->setShortcuts(m_actionCollection->defaultShortcuts(m_action));
}

void ShortcutItem::swapDefault()
{
    const QList<QKeySequence> previousShortcut = m_action->shortcuts();
    m_action->setShortcuts(m_actionCollection->defaultShortcuts(m_action));
    m_actionCollection->setDefaultShortcuts(m_action, previousShortcut);
}

bool ShortcutItem::isEqual(const QVariant &p) const
{
    if (!p.canConvert<QList<QKeySequence>>()) {
        return false;
    }
    return m_action->shortcuts() == p.value<QList<QKeySequence>>();
}

QVariant ShortcutItem::property() const
{
    return QVariant::fromValue<QList<QKeySequence>>(m_action->shortcuts());
}

void ShortcutItem::setProperty(const QVariant &p)
{
    m_action->setShortcuts(p.value<QList<QKeySequence>>());
}

namespace KWin
{
namespace TabBox
{

ShortcutSettings::ShortcutSettings(QObject *parent)
    : KConfigSkeleton(nullptr, parent)
    , m_actionCollection(new KActionCollection(this, QStringLiteral("kwin")))
{
    m_actionCollection->setConfigGroup("Navigation");
    m_actionCollection->setConfigGlobal(true);

    auto addShortcut = [this](const KLocalizedString &name, const QList<QKeySequence> &shortcuts = QList<QKeySequence>()) {
        const QString untranslatedName = QString::fromUtf8(name.untranslatedText());
        QAction *action = m_actionCollection->addAction(untranslatedName);
        action->setObjectName(untranslatedName);
        action->setProperty("isConfigurationAction", true);
        action->setText(name.toString());

        m_actionCollection->setDefaultShortcuts(action, shortcuts);

        addItem(new ShortcutItem(action, m_actionCollection));
    };

    // TabboxType::Main
    addShortcut(ki18nd("kwin", "Walk Through Windows"), {Qt::ALT | Qt::Key_Tab});
    addShortcut(ki18nd("kwin", "Walk Through Windows (Reverse)"), {Qt::ALT | Qt::SHIFT | Qt::Key_Tab});
    addShortcut(ki18nd("kwin", "Walk Through Windows of Current Application"), {Qt::ALT | Qt::Key_QuoteLeft});
    addShortcut(ki18nd("kwin", "Walk Through Windows of Current Application (Reverse)"), {Qt::ALT | Qt::Key_AsciiTilde});
    // TabboxType::Alternative
    addShortcut(ki18nd("kwin", "Walk Through Windows Alternative"));
    addShortcut(ki18nd("kwin", "Walk Through Windows Alternative (Reverse)"));
    addShortcut(ki18nd("kwin", "Walk Through Windows of Current Application Alternative"));
    addShortcut(ki18nd("kwin", "Walk Through Windows of Current Application Alternative (Reverse)"));
}

KActionCollection *ShortcutSettings::actionCollection() const
{
    return m_actionCollection;
}

QKeySequence ShortcutSettings::primaryShortcut(const QString &name) const
{
    QAction *action = m_actionCollection->action(name);
    Q_ASSERT(action);
    return action->shortcuts().value(0);
}

QKeySequence ShortcutSettings::alternateShortcut(const QString &name) const
{
    QAction *action = m_actionCollection->action(name);
    Q_ASSERT(action);
    return action->shortcuts().value(1);
}

void ShortcutSettings::setShortcuts(const QString &name, const QList<QKeySequence> &shortcuts)
{
    QAction *action = m_actionCollection->action(name);
    Q_ASSERT(action);
    action->setShortcuts(shortcuts);
}

bool ShortcutSettings::isDefault(const QString &name) const
{
    QAction *action = m_actionCollection->action(name);
    Q_ASSERT(action);
    return action->shortcuts() == m_actionCollection->defaultShortcuts(action);
}

} // namespace TabBox
} // namespace KWin
