/*
 * Copyright © 2018-2020 Red Hat, Inc
 * Copyright © 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jan Grulich <jgrulich@redhat.com>
 *       Aleix Pol Gonzalez <aleixpol@kde.org>
 */

#pragma once

#include <QObject>
#include <QDebug>
#include <spa/utils/hook.h>
#include <pipewire/pipewire.h>

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
