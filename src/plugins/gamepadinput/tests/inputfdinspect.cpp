/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleixpol@kde.org>
 *
 *   SPDX-License-Identifier: MIT
 */

#include "inputfd_v1.h"
#include <QGuiApplication>
#include <QRasterWindow>
#include <QWaylandClientExtensionTemplate>

class InputFdV1Extension : public QWaylandClientExtensionTemplate<InputFdV1Extension>, public InputFdManagerV1
{
public:
    InputFdV1Extension()
        : QWaylandClientExtensionTemplate<InputFdV1Extension>(1)
    {
        initialize();
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    auto input = new InputFdV1Extension;
    auto nativeInterface = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!nativeInterface) {
        qDebug() << "no wayland interface";
        return 123;
    }

    input->seat(nativeInterface->seat());
    QWindow *w = new QRasterWindow();
    w->setBaseSize({100, 100});
    w->show();

    return app.exec();
}
