/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QMap>
#include <QMetaType>
#include <QString>

typedef QMap<QString, QString> CdStringMap;
Q_DECLARE_METATYPE(CdStringMap)
