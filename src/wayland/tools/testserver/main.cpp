/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "testserver.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    auto arguments = QCoreApplication::arguments();
    // get rid of our own application path
    arguments.removeFirst();
    if (arguments.size() < 1) {
        return 1;
    }

    TestServer *server = new TestServer(&a);
    server->init();
    server->startTestApp(arguments.takeFirst(), arguments);

    return a.exec();
}
