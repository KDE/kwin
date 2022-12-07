/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "pipewirecore.h"
#include "kwinscreencast_logging.h"
#include <cerrno>

#include <KLocalizedString>

#include <QScopeGuard>
#include <QSocketNotifier>

namespace KWin
{

PipeWireCore::PipeWireCore()
{
    pw_init(nullptr, nullptr);
    pwCoreEvents.version = PW_VERSION_CORE_EVENTS;
    pwCoreEvents.error = &PipeWireCore::onCoreError;
}

PipeWireCore::~PipeWireCore()
{
    if (pwMainLoop) {
        pw_thread_loop_stop(pwMainLoop);
    }

    if (pwCore) {
        pw_core_disconnect(pwCore);
    }

    if (pwContext) {
        pw_context_destroy(pwContext);
    }
    if (pwMainLoop) {
        pw_thread_loop_destroy(pwMainLoop);
    }
}

void PipeWireCore::onCoreError(void *data, uint32_t id, int seq, int res, const char *message)
{
    qCWarning(KWIN_SCREENCAST) << "PipeWire remote error: " << message;
    if (id == PW_ID_CORE && res == -EPIPE) {
        PipeWireCore *pw = static_cast<PipeWireCore *>(data);
        Q_EMIT pw->pipewireFailed(QString::fromUtf8(message));
    }
}

bool PipeWireCore::init()
{
    pwMainLoop = pw_thread_loop_new("kwin-pipewire", nullptr);
    if (!pwMainLoop) {
        qCWarning(KWIN_SCREENCAST, "Failed to create PipeWire loop: %s", strerror(errno));
        m_error = i18n("Failed to start main PipeWire loop");
        return false;
    }

    pw_thread_loop_lock(pwMainLoop);
    if (pw_thread_loop_start(pwMainLoop) < 0) {
        qCWarning(KWIN_SCREENCAST) << "Failed to start main PipeWire loop";
        return false;
    }

    auto x = qScopeGuard([this] {
        pw_thread_loop_unlock(pwMainLoop);
    });

    pwContext = pw_context_new(pw_thread_loop_get_loop(pwMainLoop), nullptr, 0);
    if (!pwContext) {
        qCWarning(KWIN_SCREENCAST) << "Failed to create PipeWire context";
        m_error = i18n("Failed to create PipeWire context");
        return false;
    }

    pwCore = pw_context_connect(pwContext, nullptr, 0);
    if (!pwCore) {
        qCWarning(KWIN_SCREENCAST) << "Failed to connect PipeWire context";
        m_error = i18n("Failed to connect PipeWire context");
        return false;
    }

    pw_core_add_listener(pwCore, &coreListener, &pwCoreEvents, this);
    return true;
}

std::shared_ptr<PipeWireCore> PipeWireCore::self()
{
    static std::weak_ptr<PipeWireCore> global;
    auto ret = global.lock();
    if (!ret) {
        ret = std::make_shared<PipeWireCore>();
        ret->init();
        global = ret;
    }
    return ret;
}

} // namespace KWin
