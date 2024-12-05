/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QString>

namespace KWin
{

class IdleInhibitor : public QObject
{
    Q_OBJECT
public:
    explicit IdleInhibitor(const QString &reason);
    ~IdleInhibitor();

    QString m_requestPath;
};

}
