/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QKeySequence>
#include <QObject>
#include <QQmlParserStatus>
#include <QVariant>

class QAction;

namespace KWin
{

/**
 * The ShortcutHandler type provides a way to register global key shorcuts.
 *
 * Example usage:
 * @code
 * ShortcutHandler {
 *     name: "Activate Something"
 *     text: i18n("Activate Something")
 *     sequence: "Meta+Ctrl+S"
 *     onActivated: doSomething()
 * }
 * @endcode
 */
class ShortcutHandler : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    /**
     * This property specifies the unique shortcut identifier, not localized.
     *
     * The shortcut name cannot be changed once the ShortcutHandler is constructed.
     */
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    /**
     * This property specifies human readable name of the shortcut.
     */
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    /**
     * This property holds the key sequence for this shortcut. The key sequence
     * can be specified using a string containing a sequence of keys that are needed
     * to be pressed to activate the shortcut, e.g. `Meta+K`.
     *
     * The key sequence is optional. If omitted, the user can assign a key sequence
     * to this shortcut in system settings.
     *
     * The key sequence cannot be changed once the ShortcutHandler is constructed.
     */
    Q_PROPERTY(QVariant sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)

public:
    explicit ShortcutHandler(QObject *parent = nullptr);

    void classBegin() override;
    void componentComplete() override;

    QString name() const;
    void setName(const QString &name);

    QString text() const;
    void setText(const QString &text);

    QVariant sequence() const;
    void setSequence(const QVariant &sequence);

Q_SIGNALS:
    void nameChanged();
    void textChanged();
    void sequenceChanged();
    void activated();

private:
    QString m_name;
    QString m_text;
    QVariant m_userSequence;
    QKeySequence m_keySequence;
    QAction *m_action = nullptr;
};

} // namespace KWin
