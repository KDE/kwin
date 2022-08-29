/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "session.h"

#include <QDBusUnixFileDescriptor>

namespace KWin
{

class LogindSession : public Session
{
    Q_OBJECT

public:
    static std::unique_ptr<LogindSession> create();
    ~LogindSession() override;

    bool isActive() const override;
    Capabilities capabilities() const override;
    QString seat() const override;
    uint terminal() const override;
    int openRestricted(const QString &fileName) override;
    void closeRestricted(int fileDescriptor) override;
    void switchTo(uint terminal) override;

private Q_SLOTS:
    void handleResumeDevice(uint major, uint minor, QDBusUnixFileDescriptor fileDescriptor);
    void handlePauseDevice(uint major, uint minor, const QString &type);
    void handlePropertiesChanged(const QString &interfaceName, const QVariantMap &properties);
    void handlePrepareForSleep(bool sleep);

private:
    explicit LogindSession(const QString &sessionPath);

    bool initialize();
    void updateActive(bool active);

    QString m_sessionPath;
    QString m_seatId;
    QString m_seatPath;
    uint m_terminal = 0;
    bool m_isActive = false;
};

} // namespace KWin
