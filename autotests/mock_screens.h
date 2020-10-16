/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MOCK_SCREENS_H
#define KWIN_MOCK_SCREENS_H

#include "../screens.h"

namespace KWin
{

class MockScreens : public Screens
{
    Q_OBJECT
public:
    explicit MockScreens(QObject *parent = nullptr);
    ~MockScreens() override;
    QRect geometry(int screen) const override;
    int number(const QPoint &pos) const override;
    QString name(int screen) const override;
    float refreshRate(int screen) const override;
    QSize size(int screen) const override;
    void init() override;

    void setGeometries(const QList<QRect> &geometries);

protected Q_SLOTS:
    void updateCount() override;

private:
    QList<QRect> m_scheduledGeometries;
    QList<QRect> m_geometries;
};

}

#endif
