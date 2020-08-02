/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

namespace KWin
{

class ClockSkewNotifierEngine : public QObject
{
    Q_OBJECT

public:
    static ClockSkewNotifierEngine *create(QObject *parent);

protected:
    explicit ClockSkewNotifierEngine(QObject *parent);

Q_SIGNALS:
    void clockSkewed();
};

} // namespace KWin
