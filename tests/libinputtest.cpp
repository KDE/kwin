/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "../input.h"
#include "../libinput/connection.h"
#include "../libinput/device.h"
#include "../logind.h"

#include <QCoreApplication>
#include <QLoggingCategory>

#include <linux/input.h>

#include <iostream>

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core")

int main(int argc, char **argv)
{
    using namespace KWin::LibInput;

    QCoreApplication app(argc, argv);

    KWin::LogindIntegration *logind = KWin::LogindIntegration::create(&app);
    QObject::connect(logind, &KWin::LogindIntegration::connectedChanged,
        [logind]() {
            if (logind->isConnected()) {
                logind->takeControl();
            }
        }
    );
    QObject::connect(logind, &KWin::LogindIntegration::hasSessionControlChanged,
        [&app](bool valid) {
            if (!valid) {
                return;
            }
            Connection *conn = Connection::create(&app);
            if (!conn) {
                std::cerr << "Failed to create LibInput integration" << std::endl;
                ::exit(1);
            }
            conn->setScreenSize(QSize(100, 100));
            conn->setup();

            QObject::connect(conn, &Connection::keyChanged,
                [](uint32_t key, KWin::InputRedirection::KeyboardKeyState state) {
                    std::cout << "Got key event" << std::endl;;
                    if (key == KEY_Q && state == KWin::InputRedirection::KeyboardKeyReleased) {
                        QCoreApplication::instance()->quit();
                    }
                }
            );
            QObject::connect(conn, &Connection::pointerButtonChanged,
                [](uint32_t button, KWin::InputRedirection::PointerButtonState state) {
                    std::cout << "Got pointer button event" << std::endl;
                    if (button == BTN_MIDDLE && state == KWin::InputRedirection::PointerButtonReleased) {
                        QCoreApplication::instance()->quit();
                    }
                }
            );
            QObject::connect(conn, &Connection::pointerMotion,
                [](const QSizeF &delta) {
                    std::cout << "Got pointer motion: " << delta.width() << "/" << delta.height() << std::endl;
                }
            );
            QObject::connect(conn, &Connection::pointerAxisChanged,
                [](KWin::InputRedirection::PointerAxis axis, qreal delta) {
                    std::cout << "Axis: " << axis << " with delta" << delta << std::endl;
                }
            );
            QObject::connect(conn, &Connection::touchDown,
                [](qint32 id, const QPointF &position, quint32 time) {
                    std::cout << "Touch down at: " << position.x() << "/" << position.y() << " id " << id << " timestamp: " << time << std::endl;
                }
            );
            QObject::connect(conn, &Connection::touchMotion,
                [](qint32 id, const QPointF &position, quint32 time) {
                    std::cout << "Touch motion at: " << position.x() << "/" << position.y() << " id " << id << " timestamp: " << time << std::endl;
                }
            );
            QObject::connect(conn, &Connection::touchUp,
                [](qint32 id, quint32 time) {
                    std::cout << "Touch up for id " << id << " timestamp: " << time << std::endl;
                }
            );
            QObject::connect(conn, &Connection::touchCanceled,
                []() {
                    std::cout << "Touch canceled" << std::endl;
                }
            );
            QObject::connect(conn, &Connection::touchFrame,
                []() {
                    std::cout << "Touch frame " << std::endl;
                }
            );
            QObject::connect(&app, &QCoreApplication::aboutToQuit, [conn] { delete conn; });
        }
    );

    return app.exec();
}
