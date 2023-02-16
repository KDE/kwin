/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>

#include "window.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Window w;
    w.show();

    return app.exec();
}
