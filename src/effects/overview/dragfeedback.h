/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QQuickItem>

namespace KWin
{

class DragMultiplexer : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY windowIdChanged)
    Q_PROPERTY(QString windowId READ windowId NOTIFY windowIdChanged)
    Q_PROPERTY(QPointF position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QObject *source READ source NOTIFY sourceChanged)

public:
    explicit DragMultiplexer(QQuickItem *parent = nullptr);
    ~DragMultiplexer() override;

    bool isActive() const;
    QString windowId() const;
    QObject *source() const;

    QPointF position() const;
    void setPosition(const QPointF &position);

public Q_SLOTS:
    void drop();
    void start(const QString &windowId, QObject *source);

Q_SIGNALS:
    void windowIdChanged();
    void positionChanged();
    void sourceChanged();
    void dropped();

private:
    QPointF m_position;
    QString m_windowId;
    QPointer<QObject> m_source;
    static DragMultiplexer *s_primary;
};

} // namespace KWin
