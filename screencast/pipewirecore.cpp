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

#include "pipewirecore.h"
#include <QDebug>
#include <QSocketNotifier>
#include <KLocalizedString>
#include "kwinpipewire_logging.h"

PipeWireCore::PipeWireCore()
{
    pw_init(nullptr, nullptr);
    pwCoreEvents.version = PW_VERSION_CORE_EVENTS;
    pwCoreEvents.error = &PipeWireCore::onCoreError;
}

PipeWireCore::~PipeWireCore()
{
    if (pwMainLoop) {
        pw_loop_leave(pwMainLoop);
    }

    if (pwCore) {
        pw_core_disconnect(pwCore);
    }

    if (pwContext) {
        pw_context_destroy(pwContext);
    }

    if (pwMainLoop) {
        pw_loop_destroy(pwMainLoop);
    }
}

void PipeWireCore::onCoreError(void* data, uint32_t id, int seq, int res, const char* message)
{
    Q_UNUSED(seq)

    qCWarning(KWIN_PIPEWIRE) << "PipeWire remote error: " << message;
    if (id == PW_ID_CORE && res == -EPIPE) {
        PipeWireCore *pw = static_cast<PipeWireCore*>(data);
        Q_EMIT pw->pipewireFailed(QString::fromUtf8(message));
    }
}

bool PipeWireCore::init()
{
    pwMainLoop = pw_loop_new(nullptr);
    pw_loop_enter(pwMainLoop);

    QSocketNotifier *notifier = new QSocketNotifier(pw_loop_get_fd(pwMainLoop), QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [this] {
            int result = pw_loop_iterate (pwMainLoop, 0);
            if (result < 0)
                qCWarning(KWIN_PIPEWIRE) << "pipewire_loop_iterate failed: " << result;
        }
    );

    pwContext = pw_context_new(pwMainLoop, nullptr, 0);
    if (!pwContext) {
        qCWarning(KWIN_PIPEWIRE) << "Failed to create PipeWire context";
        m_error = i18n("Failed to create PipeWire context");
        return false;
    }

    pwCore = pw_context_connect(pwContext, nullptr, 0);
    if (!pwCore) {
        qCWarning(KWIN_PIPEWIRE) << "Failed to connect PipeWire context";
        m_error = i18n("Failed to connect PipeWire context");
        return false;
    }

    if (pw_loop_iterate(pwMainLoop, 0) < 0) {
        qCWarning(KWIN_PIPEWIRE) << "Failed to start main PipeWire loop";
        m_error = i18n("Failed to start main PipeWire loop");
        return false;
    }

    pw_core_add_listener(pwCore, &coreListener, &pwCoreEvents, this);
    return true;
}

QSharedPointer< PipeWireCore > PipeWireCore::self()
{
    static QWeakPointer<PipeWireCore> global;
    QSharedPointer<PipeWireCore> ret;
    if (global) {
        ret = global.toStrongRef();
    } else {
        ret.reset(new PipeWireCore);
        ret->init();
        global = ret;
    }
    return ret;
}
