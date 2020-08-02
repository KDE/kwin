/*
    SPDX-FileCopyrightText: 2016 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#ifndef ONSCREENNOTIFICATIONTEST_H
#define ONSCREENNOTIFICATIONTEST_H

#include <QObject>

class OnScreenNotificationTest : public QObject
{
    Q_OBJECT
private slots:

    void show();
    void timeout();
    void iconName();
    void message();
};

#endif // ONSCREENNOTIFICATIONTEST_H
