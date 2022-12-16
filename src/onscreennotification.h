/*
    SPDX-FileCopyrightText: 2016 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#pragma once

#include <KSharedConfig>
#include <QObject>
#include <memory>

class QPropertyAnimation;
class QTimer;
class QQmlContext;
class QQmlComponent;
class QQmlEngine;

namespace KWin
{

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
    void setSkipCloseAnimation(bool skip);

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
    std::unique_ptr<QQmlContext> m_qmlContext;
    std::unique_ptr<QQmlComponent> m_qmlComponent;
    QQmlEngine *m_qmlEngine = nullptr;
    std::unique_ptr<QObject> m_mainItem;
    std::unique_ptr<OnScreenNotificationInputEventSpy> m_spy;
    QPropertyAnimation *m_animation = nullptr;
    bool m_containsPointer = false;
};
}
