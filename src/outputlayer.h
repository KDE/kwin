/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QRegion>

namespace KWin
{

class KWIN_EXPORT OutputLayer : public QObject
{
    Q_OBJECT

public:
    explicit OutputLayer(QObject *parent = nullptr);

    QRegion repaints() const;
    void resetRepaints();
    void addRepaint(const QRegion &region);

private:
    QRegion m_repaints;
};

} // namespace KWin
