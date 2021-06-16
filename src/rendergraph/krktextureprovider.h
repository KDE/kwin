/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinglobals.h>

#include <QObject>

namespace KWin
{
class KrkTexture;

class KWIN_EXPORT KrkTextureProvider : public QObject
{
    Q_OBJECT

public:
    explicit KrkTextureProvider(QObject *parent = nullptr);

    virtual KrkTexture *texture() const = 0;
};

} // namespace KWin
