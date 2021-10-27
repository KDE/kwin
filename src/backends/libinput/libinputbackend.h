/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "inputbackend.h"

#include <QThread>

namespace KWin
{

namespace LibInput
{
class Connection;
}

class KWIN_EXPORT LibinputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit LibinputBackend(QObject *parent = nullptr);
    ~LibinputBackend() override;

    void initialize() override;

private:
    QThread *m_thread = nullptr;
    LibInput::Connection *m_connection = nullptr;
};

} // namespace KWin
