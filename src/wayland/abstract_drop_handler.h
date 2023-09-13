/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWin
{
class SurfaceInterface;

class KWIN_EXPORT AbstractDropHandler : public QObject
{
    Q_OBJECT
public:
    AbstractDropHandler(QObject *parent = nullptr);
    virtual void updateDragTarget(SurfaceInterface *surface, quint32 serial) = 0;
    virtual void drop() = 0;
};
}
