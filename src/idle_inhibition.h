/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QMap>
#include <QObject>
#include <QVector>

namespace KWin
{
class Window;

class IdleInhibition : public QObject
{
    Q_OBJECT
public:
    explicit IdleInhibition(QObject *parent = nullptr);
    ~IdleInhibition() override;

    void registerClient(Window *client);

private Q_SLOTS:
    void slotWorkspaceCreated();
    void slotDesktopChanged();

private:
    void inhibit(Window *client);
    void uninhibit(Window *client);
    void update(Window *client);

    QMap<Window *, QMetaObject::Connection> m_connections;
};
}
