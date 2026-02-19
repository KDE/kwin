/*
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: MIT

*/

#include "config-kwin.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KLocalizedString>
#include <KService>
#include <KWindowSystem>

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QWindow>

#include <QtConcurrentRun>

#include <qpa/qplatformwindow_p.h>

#include <private/qtx11extras_p.h>
#include <xcb/xcb.h>

#include <cerrno>
#include <csignal>
#include <memory>

#include "debug.h"
#include "killdialog.h"

using namespace std::chrono_literals;

namespace
{
#if defined(Q_OS_LINUX)
std::optional<QString> exeOf(pid_t pid)
{
    const QFileInfo info(QStringLiteral("/proc/%1/exe").arg(QString::number(pid)));
    auto baseName = QFileInfo(info.canonicalFilePath()).baseName(); // not const to allow move return
    if (baseName.isEmpty()) {
        qCWarning(KWIN_KILLER) << "Failed to resolve exe of pid" << pid;
        return std::nullopt;
    }
    return baseName;
}

std::optional<QString> bootId()
{
    QFile file(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    if (!file.open(QFile::ReadOnly)) {
        qCWarning(KWIN_KILLER) << "Failed to read /proc/sys/kernel/random/boot_id" << file.errorString();
        return std::nullopt;
    }
    return QString::fromUtf8(file.readAll().simplified().replace('-', QByteArrayView()));
}

void writeApplicationNotResponding(pid_t pid)
{
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1StringView("/drkonqi/application-not-responding/");
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(dirPath)) {
            qCWarning(KWIN_KILLER) << "Failed to create ApplicationNotResponding path" << dirPath;
            return;
        }
    }
    // $exe.$bootid.$pid.$time_at_time_of_crash.ini
    const auto optionalExe = exeOf(pid);
    if (!optionalExe) {
        return;
    }
    const auto optionalBootId = bootId();
    if (!optionalBootId) {
        return;
    }
    const QString anrPath = dirPath + QStringLiteral("/%1.%2.%3.%4.json").arg(optionalExe.value(), optionalBootId.value(), QString::number(pid), QString::number(QDateTime::currentMSecsSinceEpoch()));
    QFile file(anrPath);
    if (!file.open(QFile::NewOnly)) {
        qCWarning(KWIN_KILLER) << "Failed to create ApplicationNotResponding file" << anrPath << file.error() << file.errorString();
        return;
    }
    // No content for now, simply close it once created
}
#endif // Q_OS_LINUX

bool hasPidAborted(pid_t pid)
{
    constexpr auto timeout = 4000ms;
    constexpr auto interval = 250ms;
    constexpr auto iterations = timeout / interval;
    for (int i = 0; i < iterations; i++) {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG | WNOWAIT) == pid) {
            if (WIFSIGNALED(status) || WIFEXITED(status)) {
                // PID terminated by signal … or exited, but that should not really happen as a result of ABRT
                return true;
            }
        }
        QThread::sleep(interval);
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kwin"));
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("tools-report-bug")));
    QCoreApplication::setApplicationName(QStringLiteral("kwin_killer_helper"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    QApplication::setApplicationDisplayName(i18n("Window Manager"));
    QCoreApplication::setApplicationVersion(KWIN_VERSION_STRING);
    QApplication::setDesktopFileName(QStringLiteral("org.kde.kwin.killer"));

    QCommandLineOption pidOption(QStringLiteral("pid"),
                                 i18n("PID of the application to terminate"), i18n("pid"));
    QCommandLineOption hostNameOption(QStringLiteral("hostname"),
                                      i18n("Hostname on which the application is running"), i18n("hostname"));
    QCommandLineOption windowNameOption(QStringLiteral("windowname"),
                                        i18n("Caption of the window to be terminated"), i18n("caption"));
    QCommandLineOption applicationNameOption(QStringLiteral("applicationname"),
                                             i18n("Name of the application to be terminated"), i18n("name"));
    QCommandLineOption widOption(QStringLiteral("wid"),
                                 i18n("ID of resource belonging to the application"), i18n("id"));
    QCommandLineOption timestampOption(QStringLiteral("timestamp"),
                                       i18n("Time of user action causing termination"), i18n("time"));
    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("KWin helper utility"));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(pidOption);
    parser.addOption(hostNameOption);
    parser.addOption(windowNameOption);
    parser.addOption(applicationNameOption);
    parser.addOption(widOption);
    parser.addOption(timestampOption);

    parser.process(app);

    const bool isX11 = app.platformName() == QLatin1StringView("xcb");

    QString hostname = parser.value(hostNameOption);
    bool pid_ok = false;
    pid_t pid = parser.value(pidOption).toULong(&pid_ok);
    QString caption = parser.value(windowNameOption);
    QString appname = parser.value(applicationNameOption);
    QString windowHandle = parser.value(widOption);

    bool time_ok = false;
    xcb_timestamp_t timestamp = parser.value(timestampOption).toULong(&time_ok);

    if (!pid_ok || pid == 0 || windowHandle.isEmpty()
        || (isX11 && (!time_ok || timestamp == XCB_CURRENT_TIME))
        || hostname.isEmpty() || caption.isEmpty() || appname.isEmpty()) {
        fprintf(stdout, "%s\n", qPrintable(i18n("This helper utility is not supposed to be called directly.")));
        parser.showHelp(1);
    }
    bool isLocal = hostname == QLatin1StringView("localhost");

    QIcon icon;

    const auto service = KService::serviceByDesktopName(appname);
    if (service) {
        appname = service->name();
        icon = QIcon::fromTheme(service->icon());
        QApplication::setApplicationDisplayName(appname);
    }

    KillDialog dialog(appname, icon);
    dialog.setPid(pid);
    dialog.setHostName(hostname);

    if (isX11) {
        QX11Info::setAppUserTime(timestamp);
    }

    QObject::connect(&dialog, &KillDialog::terminateRequested, &dialog, [&dialog, pid, &hostname, &isLocal] {
        if (!isLocal) {
            const QStringList args{
                hostname, QStringLiteral("kill"), QString::number(pid)};
            QProcess::startDetached(QStringLiteral("xon"), args);
            dialog.reject();
            return;
        }

        dialog.setBusy(true);

        auto future = QtConcurrent::run([pid] {
#if defined(Q_OS_LINUX)
            writeApplicationNotResponding(pid);
#endif
            // First try to abort so KCrash (or other handlers) and/or coredumpd can kick in and record the malfunction.
            // This specifically allows application authors to notice that something is broken.
            if (::kill(pid, SIGABRT) == 0 && hasPidAborted(pid)) {
                return 0;
            }

            // If that did not work send a kill. Kill cannot be ignored and always terminates.
            if (::kill(pid, SIGKILL) == 0) {
                return 0;
            }

            return errno;
        }).then(&dialog, [&dialog, pid](int result) {
            if (result == EPERM) {
                // killing failed on permissions try again with the polkit helper.
                KAuth::Action killer(QStringLiteral("org.kde.ksysguard.processlisthelper.sendsignal"));
                killer.setHelperId(QStringLiteral("org.kde.ksysguard.processlisthelper"));
                killer.addArgument(QStringLiteral("pid0"), pid);
                killer.addArgument(QStringLiteral("pidcount"), 1);
                killer.addArgument(QStringLiteral("signal"), SIGKILL);
                killer.setParentWindow(dialog.windowHandle());
                if (!killer.isValid()) {
                    qDebug() << "KWin process killer action not valid";
                } else {
                    qDebug() << "Using KAuth to kill pid: " << pid;
                    auto *job = killer.execute();
                    if (!job->exec()) {
                        // Don't reject dialog on failure, give the user a chance to try again.
                        dialog.setBusy(false);
                        return;
                    }
                }
            }

            dialog.reject();
        });
    });

    dialog.winId();
    KWindowSystem::setMainWindow(dialog.windowHandle(), windowHandle);

    dialog.show();
    dialog.windowHandle()->requestActivate();

    return app.exec();
}
