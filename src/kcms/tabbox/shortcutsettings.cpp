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
    QKeySequence m_savedShortcut;
};

ShortcutItem::ShortcutItem(QAction *action, KActionCollection *actionCollection)
    : KConfigSkeletonItem(actionCollection->componentName(), action->text())
    , m_actionCollection(actionCollection)
    , m_action(action)
{
    setGetDefaultImpl([this] {
        return m_actionCollection->defaultShortcut(m_action);
    });

    setIsDefaultImpl([this] {
        return m_action->shortcut() == m_actionCollection->defaultShortcut(m_action);
    });

    setIsSaveNeededImpl([this] {
        return (m_action->shortcut() != m_savedShortcut);
    });
}

void ShortcutItem::readConfig(KConfig *config)
{
    const auto shortcuts = KGlobalAccel::self()->globalShortcut(m_actionCollection->componentName(), m_action->objectName());
    m_savedShortcut = shortcuts.isEmpty() ? QKeySequence() : shortcuts.first();
    m_action->setShortcut(m_savedShortcut);
}

void ShortcutItem::writeConfig(KConfig *config)
{
    m_savedShortcut = m_action->shortcut();
    KGlobalAccel::self()->setShortcut(m_action, {m_action->shortcut()}, KGlobalAccel::NoAutoloading);
}

void ShortcutItem::readDefault(KConfig *config)
{
}

void ShortcutItem::setDefault()
{
    m_action->setShortcut(m_actionCollection->defaultShortcut(m_action));
}

void ShortcutItem::swapDefault()
{
    QKeySequence previousShortcut = m_action->shortcut();
    m_action->setShortcut(m_actionCollection->defaultShortcut(m_action));
    m_actionCollection->setDefaultShortcut(m_action, previousShortcut);
}

bool ShortcutItem::isEqual(const QVariant &p) const
{
    if (!p.canConvert<QKeySequence>()) {
        return false;
    }
    return m_action->shortcut() == p.value<QKeySequence>();
}

QVariant ShortcutItem::property() const
{
    return QVariant::fromValue<QKeySequence>(m_action->shortcut());
}

void ShortcutItem::setProperty(const QVariant &p)
{
    m_action->setShortcut(p.value<QKeySequence>());
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

    auto addShortcut = [this](const KLocalizedString &name, const QKeySequence &sequence = QKeySequence()) {
        const QString untranslatedName = QString::fromUtf8(name.untranslatedText());
        QAction *action = m_actionCollection->addAction(untranslatedName);
        action->setObjectName(untranslatedName);
        action->setProperty("isConfigurationAction", true);
        action->setText(name.toString());

        m_actionCollection->setDefaultShortcut(action, sequence);

        addItem(new ShortcutItem(action, m_actionCollection));
    };

    // TabboxType::Main
    addShortcut(ki18nd("kwin", "Walk Through Windows"), Qt::ALT | Qt::Key_Tab);
    addShortcut(ki18nd("kwin", "Walk Through Windows (Reverse)"), Qt::ALT | Qt::SHIFT | Qt::Key_Backtab);
    addShortcut(ki18nd("kwin", "Walk Through Windows of Current Application"), Qt::ALT | Qt::Key_QuoteLeft);
    addShortcut(ki18nd("kwin", "Walk Through Windows of Current Application (Reverse)"), Qt::ALT | Qt::Key_AsciiTilde);
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

QKeySequence ShortcutSettings::shortcut(const QString &name) const
{
    QAction *action = m_actionCollection->action(name);
    Q_ASSERT(action);
    return action->shortcut();
}

void ShortcutSettings::setShortcut(const QString &name, const QKeySequence &seq)
{
    QAction *action = m_actionCollection->action(name);
    Q_ASSERT(action);
    action->setShortcut(seq);
}

bool ShortcutSettings::isDefault(const QString &name) const
{
    QAction *action = m_actionCollection->action(name);
    Q_ASSERT(action);
    return action->shortcut() == m_actionCollection->defaultShortcut(action);
}

} // namespace TabBox
} // namespace KWin
