/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QWidget>
#include <unistd.h>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
    QApplication app(argc, argv);
    QWidget w;
    w.setGeometry(QRect(0, 0, 100, 200));
    w.show();

    //after showing the window block the main thread
    //1 as we want it to come after the singleshots in qApp construction
    QTimer::singleShot(1, []() {
        //block
        while(true) {
            sleep(100000);
        }
    });

    return app.exec();
}
