/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ftrace.h"

#include <QDBusConnection>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTextStream>

namespace KWin
{
KWIN_SINGLETON_FACTORY(KWin::FTraceLogger)

FTraceLogger::FTraceLogger(QObject *parent)
    : QObject(parent)
{
    if (qEnvironmentVariableIsSet("KWIN_PERF_FTRACE")) {
        setEnabled(true);
    } else {
        QDBusConnection::sessionBus().registerObject(QStringLiteral("/FTrace"), this, QDBusConnection::ExportScriptableContents);
    }
}

bool FTraceLogger::isEnabled() const
{
    return m_file.isOpen();
}

void FTraceLogger::setEnabled(bool enabled)
{
    QMutexLocker lock(&m_mutex);
    if (enabled == isEnabled()) {
        return;
    }

    if (enabled) {
        open();
    } else {
        m_file.close();
    }
    Q_EMIT enabledChanged();
}

bool FTraceLogger::open()
{
    const QString path = filePath();
    if (path.isEmpty()) {
        return false;
    }

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly)) {
        qWarning() << "No access to trace marker file at:" << path;
    }
    return true;
}

QString FTraceLogger::filePath()
{
    if (qEnvironmentVariableIsSet("KWIN_PERF_FTRACE_FILE")) {
        return qgetenv("KWIN_PERF_FTRACE_FILE");
    }

    QFile mountsFile("/proc/mounts");
    if (!mountsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "No access to mounts file. Can not determine trace marker file location.";
        return QString();
    }

    auto lineInfo = [](const QString &line) {
        const int start = line.indexOf(' ') + 1;
        const int end = line.indexOf(' ', start);
        const QString dirPath(line.mid(start, end - start));
        if (dirPath.isEmpty() || !QFileInfo::exists(dirPath)) {
            return QFileInfo();
        }
        return QFileInfo(QDir(dirPath), QStringLiteral("trace_marker"));
    };
    QFileInfo markerFileInfo;
    QTextStream mountsIn(&mountsFile);
    QString mountsLine = mountsIn.readLine();

    while (!mountsLine.isNull()) {
        if (mountsLine.startsWith("tracefs")) {
            const auto info = lineInfo(mountsLine);
            if (info.exists()) {
                markerFileInfo = info;
                break;
            }
        }
        if (mountsLine.startsWith("debugfs")) {
            markerFileInfo = lineInfo(mountsLine);
        }
        mountsLine = mountsIn.readLine();
    }
    mountsFile.close();
    if (!markerFileInfo.exists()) {
        qWarning() << "Could not determine trace marker file location from mounts.";
        return QString();
    }

    return markerFileInfo.absoluteFilePath();
}

FTraceDuration::~FTraceDuration()
{
    FTraceLogger::self()->trace(m_message, " end_ctx=", m_context);
}

}
