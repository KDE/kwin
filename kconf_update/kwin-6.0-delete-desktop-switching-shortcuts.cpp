/*
    SPDX-FileCopyrightText: 2023 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KGlobalAccel>

#include <QAction>
#include <QGuiApplication>
#include <QStandardPaths>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    const QStringList actionNames{
        QStringLiteral("Walk Through Desktops"),
        QStringLiteral("Walk Through Desktops (Reverse)"),
        QStringLiteral("Walk Through Desktop List"),
        QStringLiteral("Walk Through Desktop List (Reverse)"),
    };

    for (const QString &actionName : actionNames) {
        QAction action;
        action.setObjectName(actionName);
        action.setProperty("componentName", QStringLiteral("kwin"));
        action.setProperty("componentDisplayName", QStringLiteral("KWin"));
        KGlobalAccel::self()->setShortcut(&action, {QKeySequence()}, KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->removeAllShortcuts(&action);
    }

    return 0;
}
