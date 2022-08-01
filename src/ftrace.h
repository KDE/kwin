/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <kwinglobals.h>

#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QTextStream>

namespace KWin
{
/**
 * FTraceLogger is a singleton utility for writing log messages using ftrace
 *
 * Usage: Either:
 *  Set the KWIN_PERF_FTRACE environment variable before starting the application
 *  Calling on DBus /FTrace org.kde.kwin.FTrace.setEnabled true
 * After having created the ftrace mount
 */
class KWIN_EXPORT FTraceLogger : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.FTrace");
    Q_PROPERTY(bool isEnabled READ isEnabled NOTIFY enabledChanged)

public:
    /**
     * Enabled through DBus and logging has started
     */
    bool isEnabled() const;

    /**
     * Main log function
     * Takes any number of arguments that can be written into QTextStream
     */
    template<typename... Args>
    void trace(Args... args)
    {
        Q_ASSERT(isEnabled());
        QMutexLocker lock(&m_mutex);
        if (!m_file.isOpen()) {
            return;
        }
        QTextStream stream(&m_file);
        (stream << ... << args) << Qt::endl;
    }

Q_SIGNALS:
    void enabledChanged();

public Q_SLOTS:
    Q_SCRIPTABLE void setEnabled(bool enabled);

private:
    static QString filePath();
    bool open();
    QFile m_file;
    QMutex m_mutex;
    KWIN_SINGLETON(FTraceLogger)
};

class KWIN_EXPORT FTraceDuration
{
public:
    template<typename... Args>
    FTraceDuration(Args... args)
    {
        static QAtomicInteger<quint32> s_context = 0;
        QTextStream stream(&m_message);
        (stream << ... << args);
        stream.flush();
        m_context = ++s_context;
        FTraceLogger::self()->trace(m_message, " begin_ctx=", m_context);
    }

    ~FTraceDuration();

private:
    QByteArray m_message;
    quint32 m_context;
};

} // namespace KWin

/**
 * Optimised macro, arguments are only copied if tracing is enabled
 */
#define fTrace(...)                              \
    if (KWin::FTraceLogger::self()->isEnabled()) \
        KWin::FTraceLogger::self()->trace(__VA_ARGS__);

/**
 * Will insert two markers into the log. Once when called, and the second at the end of the relevant block
 * In GPUVis this will appear as a timed block with begin_ctx and end_ctx markers
 */
#define fTraceDuration(...) \
    std::unique_ptr<KWin::FTraceDuration> _duration(KWin::FTraceLogger::self()->isEnabled() ? new KWin::FTraceDuration(__VA_ARGS__) : nullptr);
