/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eiscontext.h"

#include "config-eis.h"
#include "eisbackend.h"
#include "libeis_logging.h"
#include "options.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "xdgactivationv1.h"
#include <unistd.h>

#include <QDBusMessage>

namespace KWin
{

XWaylandEisContext::XWaylandEisContext(KWin::EisBackend *backend)
    : EisContext(backend, {EIS_DEVICE_CAP_POINTER | EIS_DEVICE_CAP_POINTER_ABSOLUTE | EIS_DEVICE_CAP_KEYBOARD | EIS_DEVICE_CAP_TOUCH | EIS_DEVICE_CAP_SCROLL | EIS_DEVICE_CAP_BUTTON
#if EIS_HAVE_16
                           | EIS_DEVICE_CAP_TEXT
#endif
                          })
    , socketName(qgetenv("XDG_RUNTIME_DIR") + QByteArrayLiteral("/kwin-xwayland-eis-socket.") + QByteArray::number(getpid()))
{
    eis_setup_backend_socket(m_eisContext, socketName.constData());
}

void XWaylandEisContext::connectionRequested(eis_client *client)
{
#if EIS_HAVE_GET_CLIENT_PID
    const auto pid = eis_backend_socket_get_client_pid(client);
    if (kwinApp()->xwaylandPid() != pid) {
        qCWarning(KWIN_EIS) << "Non-xwayland process" << pid << "trying to connect to Xwayland socket - disconnecting";
        eis_client_disconnect(client);
        return;
    }
#endif
    const QString clientName = QString::fromUtf8(eis_client_get_name(client));
    if (options->xwaylandEisNoPrompt() || options->xwaylandEisNoPromptApps().contains(clientName)) {
        connectClient(client);
        return;
    }

    QProcessEnvironment env = kwinApp()->processStartupEnvironment();
    env.remove(QStringLiteral("QT_WAYLAND_RECONNECT"));
    auto seat = waylandServer()->seat();
    const QString token = waylandServer()->xdgActivationIntegration()->requestPrivilegedToken(nullptr, seat->display()->serial(), seat, QString());
    env.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), token);

    auto promptProcess = new QProcess;
    promptProcess->setProgram(QStringLiteral(PROMPTER_BIN));
    promptProcess->setProcessEnvironment(env);
    promptProcess->setArguments({QStringLiteral("--client"), clientName});
    QObject::connect(promptProcess, &QProcess::finished, m_backend, [client = eis_client_ref(client), promptProcess, this](int code, QProcess::ExitStatus status) {
        promptProcess->deleteLater();
        if (status == QProcess::NormalExit && code == 0) {
            connectClient(client);
        } else {
            eis_client_disconnect(client);
        }
        eis_client_unref(client);
    });
    promptProcess->start();
}

}
