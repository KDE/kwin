/*
 * Copyright 2016  Martin Graesslin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef KWIN_ONSCREENNOTIFICATION_H
#define KWIN_ONSCREENNOTIFICATION_H

#include <QObject>

#include <KSharedConfig>

class QPropertyAnimation;
class QTimer;
class QQmlContext;
class QQmlComponent;
class QQmlEngine;

namespace KWin {

class OnScreenNotificationInputEventSpy;

class OnScreenNotification : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged)
    Q_PROPERTY(int timeout READ timeout WRITE setTimeout NOTIFY timeoutChanged)

public:
    explicit OnScreenNotification(QObject *parent = nullptr);
    ~OnScreenNotification() override;
    bool isVisible() const;
    QString message() const;
    QString iconName() const;
    int timeout() const;

    QRect geometry() const;

    void setVisible(bool m_visible);
    void setMessage(const QString &message);
    void setIconName(const QString &iconName);
    void setTimeout(int timeout);

    void setConfig(KSharedConfigPtr config);
    void setEngine(QQmlEngine *engine);

    void setContainsPointer(bool contains);

Q_SIGNALS:
    void visibleChanged();
    void messageChanged();
    void iconNameChanged();
    void timeoutChanged();

private:
    void show();
    void ensureQmlContext();
    void ensureQmlComponent();
    void createInputSpy();
    bool m_visible = false;
    QString m_message;
    QString m_iconName;
    QTimer *m_timer;
    KSharedConfigPtr m_config;
    QScopedPointer<QQmlContext> m_qmlContext;
    QScopedPointer<QQmlComponent> m_qmlComponent;
    QQmlEngine *m_qmlEngine = nullptr;
    QScopedPointer<QObject> m_mainItem;
    QScopedPointer<OnScreenNotificationInputEventSpy> m_spy;
    QPropertyAnimation *m_animation = nullptr;
    bool m_containsPointer = false;
};
}

#endif // KWIN_ONSCREENNOTIFICATION_H
