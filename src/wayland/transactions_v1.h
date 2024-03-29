/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWin
{

class Display;
class TransactionsManagerV1Private;

/**
 * The TransactionsManagerV1 type provides support for group surface transactions.
 */
class KWIN_EXPORT TransactionsManagerV1 : public QObject
{
    Q_OBJECT

public:
    explicit TransactionsManagerV1(Display *display, QObject *parent = nullptr);
    ~TransactionsManagerV1() override;

private:
    std::unique_ptr<TransactionsManagerV1Private> d;
};

} // namespace KWin
