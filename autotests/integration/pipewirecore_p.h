/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>
#include <QVersionNumber>
#include <pipewire/pipewire.h>

class PipeWireCore : public QObject
{
    Q_OBJECT
public:
    PipeWireCore();

    static void onCoreError(void *data, uint32_t id, int seq, int res, const char *message);
    static void onCoreInfo(void *data, const struct pw_core_info *info);

    ~PipeWireCore();

    bool init(int fd);
    bool init_core();
    QString error() const;
    QVersionNumber serverVersion() const
    {
        return m_serverVersion;
    }

    pw_loop *loop() const
    {
        return m_pwMainLoop;
    }

    pw_core *operator*() const
    {
        return m_pwCore;
    };
    static QSharedPointer<PipeWireCore> fetch(int fd);

private:
    int m_fd = 0;
    pw_core *m_pwCore = nullptr;
    pw_context *m_pwContext = nullptr;
    pw_loop *m_pwMainLoop = nullptr;
    spa_hook m_coreListener;
    QString m_error;
    QVersionNumber m_serverVersion;

    static pw_core_events s_pwCoreEvents;

Q_SIGNALS:
    void pipewireFailed(const QString &message);

    /**
     * Clients should disconnect from the core and reconnect to it on receiving this signal
     */
    void pipeBroken();
};
