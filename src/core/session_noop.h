/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "session.h"

namespace KWin
{

class NoopSession : public Session
{
    Q_OBJECT

public:
    static std::unique_ptr<NoopSession> create();
    ~NoopSession() override;

    bool isActive() const override;
    Capabilities capabilities() const override;
    QString seat() const override;
    uint terminal() const override;
    int openRestricted(const QString &fileName) override;
    void closeRestricted(int fileDescriptor) override;
    void switchTo(uint terminal) override;
    FileDescriptor delaySleep(const QString &reason) override;

private:
    explicit NoopSession() = default;
};

} // namespace KWin
