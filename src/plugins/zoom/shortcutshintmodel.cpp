/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <KGlobalAccel>
#include <KLocalizedString>

#include "input.h"

#include "shortcutshintmodel.h"

static QKeySequence keyboardModifiersToShortcut(const Qt::KeyboardModifiers modifiers)
{
    // NOTE: This is potentially fragile. QKeySequence doesn't want to accept
    // modifiers only shortcuts, and this seems to skip some sanity checking.
    // We get QKeySequence("Meta+Ctrl+"), i18n ignores the extra '+' when using
    // "<shortcut>%1</shortcut>".
    return QKeySequence(QKeyCombination::fromCombined(modifiers));
}

static QString shortcutToString(const QKeySequence &shortcut)
{
    return shortcut.toString(QKeySequence::NativeText);
}

static QString shortcutsToTranslatedString(const QList<QKeySequence> &shortcuts)
{
    const int count = shortcuts.count();

    if (count == 0) {
        return QString();
    } else if (count == 1) {
        return xi18nc("@info Shortcut hint", "<shortcut>%1</shortcut>",
                      shortcutToString(shortcuts[0]));
    } else {
        // NOTE: Displaying more than two shortcuts is too granular
        return xi18nc("@info Shortcut hint", "<shortcut>%1</shortcut> or <shortcut>%2</shortcut>",
                      shortcutToString(shortcuts[0]),
                      shortcutToString(shortcuts[1]));
    }
}

namespace KWin
{

ShortcutsHintModel::ShortcutsHintModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &ShortcutsHintModel::slotGlobalShortcutChanged);

    connect(input(), &InputRedirection::deviceAdded, this, &ShortcutsHintModel::updateHasTouchpad);
    connect(input(), &InputRedirection::deviceRemoved, this, &ShortcutsHintModel::updateHasTouchpad);
    updateHasTouchpad();
}

QHash<int, QByteArray> ShortcutsHintModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[TextRole] = "text";
    roleNames[ShortcutsRole] = "shortcuts";
    return roleNames;
}

int ShortcutsHintModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_actions.count();
}

QVariant ShortcutsHintModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const QAction *action = m_actions.at(index.row());
    Q_ASSERT(action);

    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return action->text();
    case ShortcutsRole:
        return shortcutsToTranslatedString(KGlobalAccel::self()->shortcut(action));
    default:
        return {};
    }
}

void ShortcutsHintModel::addAction(QAction *action, int index)
{
    if (!action || m_actions.contains(action)) {
        return;
    }

    if (index < 0 || index > m_actions.size()) {
        index = m_actions.size();
    }

    beginInsertRows(QModelIndex(), index, index);
    m_actions.insert(index, action);
    endInsertRows();
}

bool ShortcutsHintModel::removeAction(QAction *action)
{
    const int index = m_actions.indexOf(action);
    if (index == -1) {
        return false;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_actions.removeAt(index);
    endRemoveRows();

    return true;
}

void ShortcutsHintModel::setPointerAxisGestureModifiers(const Qt::KeyboardModifiers modifiers)
{
    const QString shortcut = shortcutToString(keyboardModifiersToShortcut(modifiers));
    if (m_pointerAxisGestureShortcut != shortcut) {
        m_pointerAxisGestureShortcut = shortcut;
        Q_EMIT pointerAxisGestureShortcutChanged();
    }
}

QString ShortcutsHintModel::pointerAxisGestureShortcut() const
{
    return m_pointerAxisGestureShortcut;
}

void ShortcutsHintModel::updateHasTouchpad()
{
    const auto devices = input()->devices();
    const bool hasTouchpad = std::any_of(devices.cbegin(), devices.cend(), [](const InputDevice *device) {
        return device->isTouchpad();
    });

    if (m_hasTouchpad != hasTouchpad) {
        m_hasTouchpad = hasTouchpad;
        Q_EMIT hasTouchpadChanged();
    }
}

bool ShortcutsHintModel::hasTouchpad() const
{
    return m_hasTouchpad;
}

void ShortcutsHintModel::slotGlobalShortcutChanged(QAction *action, const QKeySequence &seq)
{
    const int index = m_actions.indexOf(action);
    if (index != -1) {
        Q_EMIT dataChanged(this->index(index, 0), this->index(index, 0), {ShortcutsRole});
    }
}

} // namespace 'KWin'

#include "moc_shortcutshintmodel.cpp"
