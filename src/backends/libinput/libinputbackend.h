/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/inputbackend.h"

#include <QThread>

namespace KWin
{

class Session;

namespace LibInput
{
class Connection;
}

class KWIN_EXPORT LibinputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit LibinputBackend(Session *session, QObject *parent = nullptr);
    ~LibinputBackend() override;

    void initialize() override;
    void updateScreens() override;

private:
    QThread m_thread;
    std::unique_ptr<LibInput::Connection> m_connection;
};

} // namespace KWin
