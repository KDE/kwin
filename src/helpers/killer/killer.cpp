/*
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: MIT

*/

#include <KAuth/Action>
#include <KIconUtils>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageDialog>
#include <KService>

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QProcess>
#include <QWaylandClientExtensionTemplate>
#include <QWindow>

#include <qpa/qplatformwindow_p.h>

#include <private/qtx11extras_p.h>
#include <xcb/xcb.h>

#include <cerrno>
#include <csignal>
#include <memory>

#include "qwayland-xdg-foreign-unstable-v2.h"

class XdgImported : public QtWayland::zxdg_imported_v2
{
public:
    XdgImported(::zxdg_imported_v2 *object)
        : QtWayland::zxdg_imported_v2(object)
    {
    }
    ~XdgImported() override
    {
        destroy();
    }
};

class XdgImporter : public QWaylandClientExtensionTemplate<XdgImporter>, public QtWayland::zxdg_importer_v2
{
public:
    XdgImporter()
        : QWaylandClientExtensionTemplate(1)
    {
    }
    ~XdgImporter() override
    {
        if (isActive()) {
            destroy();
        }
    }
    XdgImported *import(const QString &handle)
    {
        return new XdgImported(import_toplevel(handle));
    }
};

int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kwin"));
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("tools-report-bug")));
    QCoreApplication::setApplicationName(QStringLiteral("kwin_killer_helper"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    QApplication::setApplicationDisplayName(i18n("Window Manager"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));
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

    const bool isX11 = app.platformName() == QLatin1String("xcb");

    QString hostname = parser.value(hostNameOption);
    bool pid_ok = false;
    pid_t pid = parser.value(pidOption).toULong(&pid_ok);
    QString caption = parser.value(windowNameOption);
    QString appname = parser.value(applicationNameOption);
    bool id_ok = false;
    xcb_window_t wid = XCB_WINDOW_NONE;
    QString windowHandle;
    if (isX11) {
        wid = parser.value(widOption).toULong(&id_ok);
    } else {
        windowHandle = parser.value(widOption);
    }

    // on Wayland XDG_ACTIVATION_TOKEN is set in the environment.
    bool time_ok = false;
    xcb_timestamp_t timestamp = parser.value(timestampOption).toULong(&time_ok);

    if (!pid_ok || pid == 0 || ((!id_ok || wid == XCB_WINDOW_NONE) && windowHandle.isEmpty())
        || (isX11 && (!time_ok || timestamp == XCB_CURRENT_TIME))
        || hostname.isEmpty() || caption.isEmpty() || appname.isEmpty()) {
        fprintf(stdout, "%s\n", qPrintable(i18n("This helper utility is not supposed to be called directly.")));
        parser.showHelp(1);
    }
    bool isLocal = hostname == QStringLiteral("localhost");

    const auto service = KService::serviceByDesktopName(appname);
    if (service) {
        appname = service->name();
        QApplication::setApplicationDisplayName(appname);
    }

    // Drop redundant application name, cf. QXcbWindow::setWindowTitle.
    const QString titleSeparator = QString::fromUtf8(" \xe2\x80\x94 "); // // U+2014, EM DASH
    caption.remove(titleSeparator + appname);
    caption.remove(QStringLiteral(" – ") + appname); // EN dash (Firefox)
    caption.remove(QStringLiteral(" - ") + appname); // classic minus :-)

    caption = caption.toHtmlEscaped();
    appname = appname.toHtmlEscaped();
    hostname = hostname.toHtmlEscaped();
    QString pidString = QString::number(pid); // format pid ourself as it does not make sense to format an ID according to locale settings

    QString question = (caption == appname) ? xi18nc("@info", "<para><application>%1</application> is not responding. Do you want to terminate this application?</para>",
                                                     appname)
                                            : xi18nc("@info \"window title\" of application name is not responding.", "<para>\"%1\" of <application>%2</application> is not responding. Do you want to terminate this application?</para>",
                                                     caption, appname);
    question += xi18nc("@info",
                       "<para><emphasis strong='true'>Terminating this application will close all of its windows. Any unsaved data will be lost.</emphasis></para>");

    KGuiItem continueButton = KGuiItem(i18nc("@action:button Terminate app", "&Terminate %1", appname), QStringLiteral("application-exit"));
    KGuiItem cancelButton = KGuiItem(i18nc("@action:button Wait for frozen app to maybe respond again", "&Wait Longer"), QStringLiteral("chronometer"));

    if (isX11) {
        QX11Info::setAppUserTime(timestamp);
    }

    auto *dialog = new KMessageDialog(KMessageDialog::WarningContinueCancel, question);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCaption(i18nc("@title:window", "Not Responding"));

    QIcon icon;
    if (service) {
        const QIcon appIcon = QIcon::fromTheme(service->icon());
        if (!appIcon.isNull()) {
            // emblem-warning is non-standard, fall back to emblem-important if necessary.
            const QIcon warningBadge = QIcon::fromTheme(QStringLiteral("emblem-warning"), QIcon::fromTheme(QStringLiteral("emblem-important")));

            icon = KIconUtils::addOverlay(appIcon, warningBadge, qApp->isRightToLeft() ? Qt::BottomLeftCorner : Qt::BottomRightCorner);
        }
    }
    dialog->setIcon(icon); // null icon will result in default warning icon.
    dialog->setButtons(continueButton, KGuiItem(), cancelButton);

    QStringList details{
        i18nc("@info", "Process ID: %1", pidString)};
    if (!isLocal) {
        details << i18nc("@info", "Host name: %1", hostname);
    }
    dialog->setDetails(details.join(QLatin1Char('\n')));
    dialog->winId();

    std::unique_ptr<XdgImporter> xdgImporter;
    std::unique_ptr<XdgImported> importedParent;

    if (isX11) {
        if (QWindow *foreignParent = QWindow::fromWinId(wid)) {
            dialog->windowHandle()->setTransientParent(foreignParent);
        }
    } else {
        xdgImporter = std::make_unique<XdgImporter>();
    }

    QObject::connect(dialog, &QDialog::finished, &app, [pid, hostname, isLocal](int result) {
        if (result == KMessageBox::PrimaryAction) {
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

        qApp->quit();
    });

    dialog->show();

    auto setTransientParent = [&xdgImporter, &importedParent, dialog, windowHandle] {
        if (xdgImporter->isActive()) {
            if (auto *waylandWindow = dialog->windowHandle()->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
                importedParent.reset(xdgImporter->import(windowHandle));
                if (auto *surface = waylandWindow->surface()) {
                    importedParent->set_parent_of(surface);
                }
            }
        }
    };

    if (xdgImporter) {
        QObject::connect(xdgImporter.get(), &XdgImporter::activeChanged, dialog, setTransientParent);
        setTransientParent();
    }

    dialog->windowHandle()->requestActivate();

    return app.exec();
}
