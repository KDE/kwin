/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

namespace KWin
{

/**
 * The ClockSkewNotifier class provides a way for monitoring system clock changes.
 *
 * The ClockSkewNotifier class makes it possible to detect discontinuous changes to
 * the system clock. Such changes are usually initiated by the user adjusting values
 * in the Date and Time KCM or calls made to functions like settimeofday().
 */
class ClockSkewNotifier : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit ClockSkewNotifier(QObject *parent = nullptr);
    ~ClockSkewNotifier() override;

    /**
     * Returns @c true if the notifier is active; otherwise returns @c false.
     */
    bool isActive() const;

    /**
     * Sets the active status of the clock skew notifier to @p active.
     *
     * clockSkewed() signal won't be emitted while the notifier is inactive.
     *
     * The notifier is inactive by default.
     *
     * @see activeChanged
     */
    void setActive(bool active);

signals:
    /**
     * This signal is emitted whenever the active property is changed.
     */
    void activeChanged();

    /**
     * This signal is emitted whenever the system clock is changed.
     */
    void clockSkewed();

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace KWin
