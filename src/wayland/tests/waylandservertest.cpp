/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "../compositor_interface.h"
#include "../display.h"
#include "../output_interface.h"
#include "../seat_interface.h"
#include "../xdgshell_interface.h"

#include "fakeoutput.h"

#include <QFile>
#include <QGuiApplication>
#include <private/qeventdispatcher_glib_p.h>

#include <iostream>
#include <unistd.h>

static int startXServer()
{
    const QByteArray process = QByteArrayLiteral("Xwayland");
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start X Server " << process.constData() << std::endl;
        exit(1);
    }

    pid_t pid = fork();
    if (pid == 0) {
        // child process - should be turned into Xwayland
        // writes to pipe, closes read side
        close(pipeFds[0]);
        char fdbuf[16];
        sprintf(fdbuf, "%d", pipeFds[1]);
        execlp(process.constData(), process.constData(), "-displayfd", fdbuf, (char *)nullptr);
        close(pipeFds[1]);
        exit(20);
    }
    // parent process - this is the wayland server
    // reads from pipe, closes write side
    close(pipeFds[1]);
    return pipeFds[0];
}

static void readDisplayFromPipe(int pipe)
{
    QFile readPipe;
    if (!readPipe.open(pipe, QIODevice::ReadOnly)) {
        std::cerr << "FATAL ERROR failed to open pipe to start X Server XWayland" << std::endl;
        exit(1);
    }
    QByteArray displayNumber = readPipe.readLine();

    displayNumber.prepend(QByteArray(":"));
    displayNumber.remove(displayNumber.size() - 1, 1);
    std::cout << "X-Server started on display " << displayNumber.constData() << std::endl;

    setenv("DISPLAY", displayNumber.constData(), true);

    // close our pipe
    close(pipe);
}

int main(int argc, char **argv)
{
    using namespace KWaylandServer;

    // set our own event dispatcher to be able to dispatch events before the event loop is started
    QAbstractEventDispatcher *eventDispatcher = new QEventDispatcherGlib();
    QCoreApplication::setEventDispatcher(eventDispatcher);

    // first create the Server and setup with minimum to get an XWayland connected
    KWaylandServer::Display display;
    display.start();
    display.createShm();
    new CompositorInterface(&display, &display);
    new XdgShellInterface(&display, &display);

    auto outputHandle = std::make_unique<FakeOutput>();
    outputHandle->setMode(QSize(1024, 768), 60000);
    outputHandle->setPhysicalSize(QSize(10, 10));

    auto outputInterface = std::make_unique<OutputInterface>(&display, outputHandle.get());

    // starts XWayland by forking and opening a pipe
    const int pipe = startXServer();
    if (pipe == -1) {
        exit(1);
    }

    fd_set rfds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    do {
        eventDispatcher->processEvents(QEventLoop::WaitForMoreEvents);
        FD_ZERO(&rfds);
        FD_SET(pipe, &rfds);
    } while (select(pipe + 1, &rfds, nullptr, nullptr, &tv) == 0);

    // now Xwayland is ready and we can read the pipe to get the display
    readDisplayFromPipe(pipe);

    QGuiApplication app(argc, argv);

    SeatInterface *seat = new SeatInterface(&display);
    seat->setName(QStringLiteral("testSeat0"));

    return app.exec();
}
