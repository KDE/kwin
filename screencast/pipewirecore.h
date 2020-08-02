/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QDebug>
#include <QObject>

#include <pipewire/pipewire.h>
#include <spa/utils/hook.h>

namespace KWin
{

class PipeWireCore : public QObject
{
    Q_OBJECT
public:
    PipeWireCore();

    static void onCoreError(void *data, uint32_t id, int seq, int res, const char *message);

    ~PipeWireCore();

    bool init();

    static QSharedPointer<PipeWireCore> self();

    struct pw_core *pwCore = nullptr;
    struct pw_context *pwContext = nullptr;
    struct pw_loop *pwMainLoop = nullptr;
    spa_hook coreListener;
    QString m_error;

    pw_core_events pwCoreEvents = {};

Q_SIGNALS:
    void pipewireFailed(const QString &message);
};

} // namespace KWin
