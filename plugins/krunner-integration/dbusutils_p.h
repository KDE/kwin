/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 David Edmundson <kde@davidedmundson.co.uk>
    SPDX-FileCopyrightText: 2018 Laurent Montel <montel@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QString>
#include <QDBusArgument>
#include <QVariantMap>
#include <KRunner/QueryMatch>

struct RemoteMatch
{
    //sssuda{sv}
    QString id;
    QString text;
    QString iconName;
    Plasma::QueryMatch::Type type = Plasma::QueryMatch::NoMatch;
    qreal relevance = 0;
    QVariantMap properties;
};

typedef QList<RemoteMatch> RemoteMatches;

struct RemoteAction
{
    QString id;
    QString text;
    QString iconName;
};

typedef QList<RemoteAction> RemoteActions;

inline QDBusArgument &operator<< (QDBusArgument &argument, const RemoteMatch &match) {
    argument.beginStructure();
    argument << match.id;
    argument << match.text;
    argument << match.iconName;
    argument << match.type;
    argument << match.relevance;
    argument << match.properties;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteMatch &match) {
    argument.beginStructure();
    argument >> match.id;
    argument >> match.text;
    argument >> match.iconName;
    uint type;
    argument >> type;
    match.type = (Plasma::QueryMatch::Type)type;
    argument >> match.relevance;
    argument >> match.properties;
    argument.endStructure();

    return argument;
}

inline QDBusArgument &operator<< (QDBusArgument &argument, const RemoteAction &action)
{
    argument.beginStructure();
    argument << action.id;
    argument << action.text;
    argument << action.iconName;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteAction &action) {
    argument.beginStructure();
    argument >> action.id;
    argument >> action.text;
    argument >> action.iconName;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(RemoteMatch)
Q_DECLARE_METATYPE(RemoteMatches)
Q_DECLARE_METATYPE(RemoteAction)
Q_DECLARE_METATYPE(RemoteActions)

