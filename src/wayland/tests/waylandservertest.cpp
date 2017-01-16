/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "../src/server/compositor_interface.h"
#include "../src/server/display.h"
#include "../src/server/output_interface.h"
#include "../src/server/seat_interface.h"
#include "../src/server/shell_interface.h"

#include <QGuiApplication>
#include <QFile>
#include <QtCore/private/qeventdispatcher_glib_p.h>

#include <unistd.h>
#include <iostream>

static int startXServer()
{
    const QByteArray process = QByteArrayLiteral("Xwayland");
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start X Server "
                  << process.constData()
                  << std::endl;
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
    displayNumber.remove(displayNumber.size() -1, 1);
    std::cout << "X-Server started on display " << displayNumber.constData() << std::endl;

    setenv("DISPLAY", displayNumber.constData(), true);

    // close our pipe
    close(pipe);
}

int main(int argc, char **argv)
{
    using namespace KWayland::Server;

    // set our own event dispatcher to be able to dispatch events before the event loop is started
    QAbstractEventDispatcher *eventDispatcher = new QEventDispatcherGlib();
    QCoreApplication::setEventDispatcher(eventDispatcher);

    // first create the Server and setup with minimum to get an XWayland connected
    Display display;
    display.start();
    display.createShm();
    CompositorInterface *compositor = display.createCompositor(&display);
    compositor->create();
    ShellInterface *shell = display.createShell();
    shell->create();
    OutputInterface *output = display.createOutput(&display);
    output->setPhysicalSize(QSize(10, 10));
    output->addMode(QSize(1024, 768));
    output->create();

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

    SeatInterface *seat = display.createSeat();
    seat->setName(QStringLiteral("testSeat0"));
    seat->create();

    return app.exec();
}
