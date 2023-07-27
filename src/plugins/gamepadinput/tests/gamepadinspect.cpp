/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleixpol@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "gaminginput_v2.h"
#include <QGuiApplication>
#include <QRasterWindow>
#include <QWaylandClientExtensionTemplate>

class GamingInputExtension : public QWaylandClientExtensionTemplate<GamingInputExtension>, public GamingInput
{
public:
    GamingInputExtension()
        : QWaylandClientExtensionTemplate<GamingInputExtension>(2)
    {
        initialize();
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    auto input = new GamingInputExtension;
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
