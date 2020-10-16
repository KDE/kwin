/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QWidget>
#include <unistd.h>

#include <csignal>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
    QApplication app(argc, argv);
    QWidget w;
    w.setGeometry(QRect(0, 0, 100, 200));
    w.show();

    auto freezeHandler = [](int) {
        while(true) {
            sleep(10000);
        }
    };

    signal(SIGUSR1, freezeHandler);

    return app.exec();
}
