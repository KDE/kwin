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

#include <QCoreApplication>

int main(int argc, char **argv)
{
    using namespace KWayland::Server;
    QCoreApplication app(argc, argv);

    Display display;
    display.start();
    OutputInterface *output = display.createOutput(&display);
    output->setPhysicalSize(QSize(10, 10));
    output->addMode(QSize(1024, 768));
    output->create();
    CompositorInterface *compositor = display.createCompositor(&display);
    compositor->create();
    display.createShm();
    ShellInterface *shell = display.createShell();
    shell->create();
    SeatInterface *seat = display.createSeat();
    seat->setName(QStringLiteral("testSeat0"));
    seat->create();

    return app.exec();
}
