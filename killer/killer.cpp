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
#include <kapplication.h>
#include <kmessagebox.h>
#include <KDE/KIcon>
#include <KDE/KLocalizedString>
#include <kauth.h>
#include <kdebug.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <QX11Info>
#include <QProcess>
// TODO: remove with Qt 5, only for HTML escaping the caption
#include <QTextDocument>
#include <signal.h>
#include <errno.h>

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
    KApplication app;
    KApplication::setWindowIcon(KIcon("kwin"));
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    QString hostname = args->getOption("hostname");
    bool pid_ok = false;
    pid_t pid = QString(args->getOption("pid")).toULong(&pid_ok);
    QString caption = args->getOption("windowname");
    QString appname = args->getOption("applicationname");
    bool id_ok = false;
    Window id = QString(args->getOption("wid")).toULong(&id_ok);
    bool time_ok = false;
    Time timestamp = QString(args->getOption("timestamp")).toULong(&time_ok);
    args->clear();
    if (!pid_ok || pid == 0 || !id_ok || id == None || !time_ok || timestamp == CurrentTime
            || hostname.isEmpty() || caption.isEmpty() || appname.isEmpty()) {
        KCmdLineArgs::usageError(i18n("This helper utility is not supposed to be called directly."));
        return 1;
    }
    bool isLocal = hostname == "localhost";

    caption = Qt::escape(caption);
    appname = Qt::escape(appname);
    hostname = Qt::escape(hostname);
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

    KGuiItem continueButton = KGuiItem(i18n("&Terminate Application %1", appname), "edit-bomb");
    KGuiItem cancelButton = KGuiItem(i18n("Wait Longer"), "chronometer");
    app.updateUserTimestamp(timestamp);
    if (KMessageBox::warningContinueCancelWId(id, question, QString(), continueButton, cancelButton) == KMessageBox::Continue) {
        if (!isLocal) {
            QStringList lst;
            lst << hostname << "kill" << QString::number(pid);
            QProcess::startDetached("xon", lst);
        } else {
            if (::kill(pid, SIGKILL) && errno == EPERM) {
                KAuth::Action killer("org.kde.ksysguard.processlisthelper.sendsignal");
                killer.setHelperID("org.kde.ksysguard.processlisthelper");
                killer.addArgument("pid0", pid);
                killer.addArgument("pidcount", 1);
                killer.addArgument("signal", SIGKILL);
                if (killer.isValid()) {
                    kDebug(1212) << "Using KAuth to kill pid: " << pid;
                    killer.execute();
                } else {
                    kDebug(1212) << "KWin process killer action not valid";
                }
            }
        }
    }
}
