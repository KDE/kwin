/*
 * SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "killprompt.h"

#include "client_machine.h"
#include "x11window.h"

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace KWin
{

KillPrompt::KillPrompt(Window *window)
    : m_window(window)
{
    Q_ASSERT(qobject_cast<X11Window *>(window));

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

    if (auto *x11Window = qobject_cast<X11Window *>(m_window)) {
        wid = QString::number(x11Window->window());
        timestampString = QString::number(timestamp);
        if (!x11Window->clientMachine()->isLocal()) {
            hostname = x11Window->clientMachine()->hostName();
        }
    }

    QStringList args{
        QStringLiteral("--pid"),
        QString::number(m_window->pid()),
        QStringLiteral("--windowname"),
        m_window->captionNormal(),
        QStringLiteral("--applicationname"),
        m_window->resourceClass(),
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
