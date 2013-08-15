/****************************************************************************

 Copyright (C) 2003 Lubos Lunak        <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

****************************************************************************/

#include <kcmdlineargs.h>
#include <kmessagebox_kiw.h>
#include <KDE/KLocalizedString>
#include <KDE/KAuth/Action>
#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QtX11Extras/QX11Info>
#include <signal.h>
#include <errno.h>
#include <xcb/xcb.h>

int main(int argc, char* argv[])
{
    KCmdLineArgs::init(argc, argv, "kwin_killer_helper", "kwin", ki18n("Window Manager"), "1.0" ,
                       ki18n("KWin helper utility"));

    KCmdLineOptions options;
    options.add("pid <pid>", ki18n("PID of the application to terminate"));
    options.add("hostname <hostname>", ki18n("Hostname on which the application is running"));
    options.add("windowname <caption>", ki18n("Caption of the window to be terminated"));
    options.add("applicationname <name>", ki18n("Name of the application to be terminated"));
    options.add("wid <id>", ki18n("ID of resource belonging to the application"));
    options.add("timestamp <time>", ki18n("Time of user action causing termination"));
    KCmdLineArgs::addCmdLineOptions(options);
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("kwin")));
    QCoreApplication::setApplicationName(QStringLiteral("kwin_killer_helper"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    QApplication::setApplicationDisplayName(i18n("Window Manager"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    QString hostname = args->getOption("hostname");
    bool pid_ok = false;
    pid_t pid = QString(args->getOption("pid")).toULong(&pid_ok);
    QString caption = args->getOption("windowname");
    QString appname = args->getOption("applicationname");
    bool id_ok = false;
    xcb_window_t id = QString(args->getOption("wid")).toULong(&id_ok);
    bool time_ok = false;
    xcb_timestamp_t timestamp = QString(args->getOption("timestamp")).toULong(&time_ok);
    args->clear();
    if (!pid_ok || pid == 0 || !id_ok || id == XCB_WINDOW_NONE || !time_ok || timestamp == XCB_TIME_CURRENT_TIME
            || hostname.isEmpty() || caption.isEmpty() || appname.isEmpty()) {
        KCmdLineArgs::usageError(i18n("This helper utility is not supposed to be called directly."));
        return 1;
    }
    bool isLocal = hostname == QStringLiteral("localhost");

    caption = caption.toHtmlEscaped();
    appname = appname.toHtmlEscaped();
    hostname = hostname.toHtmlEscaped();
    QString pidString = QString::number(pid); // format pid ourself as it does not make sense to format an ID according to locale settings

    QString question = i18nc("@info", "<b>Application \"%1\" is not responding</b>", appname);
    question += isLocal
        ? i18nc("@info", "<para>You tried to close window \"%1\" from application \"%2\" (Process ID: %3) but the application is not responding.</para>",
            caption, appname, pidString)
        : i18nc("@info", "<para>You tried to close window \"%1\" from application \"%2\" (Process ID: %3), running on host \"%4\", but the application is not responding.</para>",
            caption, appname, pidString, hostname);
    question += i18nc("@info",
        "<para>Do you want to terminate this application?</para>"
        "<para><warning>Terminating the application will close all of its child windows. Any unsaved data will be lost.</warning></para>"
        );

    KGuiItem continueButton = KGuiItem(i18n("&Terminate Application %1", appname), QStringLiteral("edit-bomb"));
    KGuiItem cancelButton = KGuiItem(i18n("Wait Longer"), QStringLiteral("chronometer"));
    QX11Info::setAppUserTime(timestamp);
    if (KMessageBox::warningContinueCancelWId(id, question, QString(), continueButton, cancelButton) == KMessageBox::Continue) {
        if (!isLocal) {
            QStringList lst;
            lst << hostname << QStringLiteral("kill") << QString::number(pid);
            QProcess::startDetached(QStringLiteral("xon"), lst);
        } else {
            if (::kill(pid, SIGKILL) && errno == EPERM) {
                KAuth::Action killer(QStringLiteral("org.kde.ksysguard.processlisthelper.sendsignal"));
                killer.setHelperId(QStringLiteral("org.kde.ksysguard.processlisthelper"));
                killer.addArgument(QStringLiteral("pid0"), pid);
                killer.addArgument(QStringLiteral("pidcount"), 1);
                killer.addArgument(QStringLiteral("signal"), SIGKILL);
                if (killer.isValid()) {
                    qDebug() << "Using KAuth to kill pid: " << pid;
                    killer.execute();
                } else {
                    qDebug() << "KWin process killer action not valid";
                }
            }
        }
    }
}
