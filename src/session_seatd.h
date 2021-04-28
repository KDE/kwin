/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "session.h"

extern "C"
{
#include <libseat.h>
}

namespace KWin
{

class SeatdSession : public Session
{
    Q_OBJECT

public:
    static SeatdSession *create(QObject *parent = nullptr);
    ~SeatdSession() override;

    bool isActive() const override;
    Capabilities capabilities() const override;
    QString seat() const override;
    uint terminal() const override;
    int openRestricted(const QString &fileName) override;
    void closeRestricted(int fileDescriptor) override;
    void switchTo(uint terminal) override;

private:
    explicit SeatdSession(QObject *parent = nullptr);

    bool initialize();
    void dispatchEvents();
    void updateActive(bool active);

    static void handleEnableSeat(libseat *seat, void *userData);
    static void handleDisableSeat(libseat *seat, void *userData);

    libseat *m_seat = nullptr;
    bool m_isActive = true;
    QHash<int, int> m_devices;
};

} // namespace KWin
