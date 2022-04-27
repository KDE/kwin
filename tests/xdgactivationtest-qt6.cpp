/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2022 Ilya Fedin <fedin-ilja2010@ya.ru>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QtWidgets>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QWidget window1(nullptr, Qt::Window);
    window1.setWindowTitle("Window 1");
    window1.setLayout(new QVBoxLayout);
    QPushButton p("Raise the Window 2");
    window1.layout()->addWidget(&p);
    window1.show();

    QWidget window2(nullptr, Qt::Window);
    window2.setWindowTitle("Window 2");
    window2.show();

    QObject::connect(&p, &QPushButton::clicked, window2.windowHandle(), &QWindow::requestActivate);

    return app.exec();
}
