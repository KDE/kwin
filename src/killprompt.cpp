/*
 * SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "killprompt.h"

#include "client_machine.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland/xdgforeign_v2.h"
#include "wayland_server.h"
#include "xdgactivationv1.h"
#include "xdgshellwindow.h"
#if KWIN_BUILD_X11
#include "x11window.h"
#endif

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace KWin
{

KillPrompt::KillPrompt(Window *window)
    : m_window(window)
{
#if KWIN_BUILD_X11
    Q_ASSERT(qobject_cast<X11Window *>(window) || qobject_cast<XdgToplevelWindow *>(window));
#else
    Q_ASSERT(qobject_cast<XdgToplevelWindow *>(window));
#endif

    m_process.setProcessChannelMode(QProcess::ForwardedChannels);

    const QFileInfo binaryInfo(KWIN_KILLER_BIN);
    const QFileInfo buildDirBinary{QDir{QCoreApplication::applicationDirPath()}, binaryInfo.fileName()};

    if (buildDirBinary.exists()) {
        m_process.setProgram(buildDirBinary.absoluteFilePath());
    } else {
        m_process.setProgram(KWIN_KILLER_BIN);
    }
}

bool KillPrompt::isRunning() const
{
    return m_process.state() == QProcess::Running;
}

void KillPrompt::start(quint32 timestamp)
{
    if (isRunning()) {
        return;
    }

    QProcessEnvironment env = kwinApp()->processStartupEnvironment();

    QString wid;
    QString timestampString;
    QString hostname = QStringLiteral("localhost");
    QString appId = !m_window->desktopFileName().isEmpty() ? m_window->desktopFileName() : m_window->resourceClass();
    QString platform;

#if KWIN_BUILD_X11
    if (auto *x11Window = qobject_cast<X11Window *>(m_window)) {
        platform = QStringLiteral("xcb");
        wid = QString::number(x11Window->window());
        timestampString = QString::number(timestamp);
        if (!x11Window->clientMachine()->isLocal()) {
            hostname = x11Window->clientMachine()->hostName();
        }
    } else
#endif
        if (auto *xdgToplevel = qobject_cast<XdgToplevelWindow *>(m_window)) {
        platform = QStringLiteral("wayland");
        auto *exported = waylandServer()->exportAsForeign(xdgToplevel->surface());
        wid = exported->handle();

        auto *seat = waylandServer()->seat();
        const QString token = waylandServer()->xdgActivationIntegration()->requestPrivilegedToken(nullptr, seat->display()->serial(), seat, QStringLiteral("org.kde.kwin.killer"));
        env.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), token);

        env.remove(QStringLiteral("QT_WAYLAND_RECONNECT"));
    }

    QStringList args{
        QStringLiteral("-platform"),
        platform,
        QStringLiteral("--pid"),
        QString::number(m_window->pid()),
        QStringLiteral("--windowname"),
        m_window->captionNormal(),
        QStringLiteral("--applicationname"),
        appId,
        QStringLiteral("--wid"),
        wid,
        QStringLiteral("--hostname"),
        hostname,
        QStringLiteral("--timestamp"),
        timestampString,
    };

    m_process.setArguments(args);
    m_process.setProcessEnvironment(env);
    m_process.start();
}

void KillPrompt::quit()
{
    m_process.terminate();
}

} // namespace KWin
