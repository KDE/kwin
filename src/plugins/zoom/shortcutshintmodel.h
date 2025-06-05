/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAbstractListModel>
#include <QAction>

namespace KWin
{

class ShortcutsHintModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString pointerAxisGestureShortcut READ pointerAxisGestureShortcut NOTIFY pointerAxisGestureShortcutChanged)
    Q_PROPERTY(bool hasTouchpad READ hasTouchpad NOTIFY hasTouchpadChanged)

public:
    ShortcutsHintModel(QObject *parent = nullptr);

    enum AdditionalRoles {
        TextRole = Qt::UserRole + 1,
        ShortcutsRole,
    };
    Q_ENUM(AdditionalRoles)

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void addAction(QAction *action, int index = -1);
    bool removeAction(QAction *action);

    void setPointerAxisGestureModifiers(const Qt::KeyboardModifiers modifiers);
    QString pointerAxisGestureShortcut() const;

    void updateHasTouchpad();
    bool hasTouchpad() const;

Q_SIGNALS:
    void pointerAxisGestureShortcutChanged();
    void hasTouchpadChanged();

private Q_SLOTS:
    void slotGlobalShortcutChanged(QAction *action, const QKeySequence &seq);

private:
    QList<QAction *> m_actions;
    QString m_pointerAxisGestureShortcut;
    bool m_hasTouchpad;
};

} // namespace 'KWin'
