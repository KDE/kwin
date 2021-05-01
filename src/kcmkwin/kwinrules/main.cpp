/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2018 Nicolas Fella <nicolas.fella@gmx.de>
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QIcon>
#include <QUuid>

#include <KCMultiDialog>
#include <KLocalizedString>
#include <KPluginMetaData>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("kcm_kwinrules");

    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    app.setApplicationName("kwin_rules_dialog");
    app.setWindowIcon(QIcon::fromTheme("preferences-system-windows-actions"));
    app.setApplicationVersion("2.0");
    app.setDesktopFileName(QStringLiteral("org.kde.kwin_rules_dialog"));

    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("KWinRules KCM launcher"));
    parser.addOption(QCommandLineOption("uuid", i18n("KWin id of the window for special window settings."), "uuid"));
    parser.addOption(QCommandLineOption("whole-app", i18n("Whether the settings should affect all windows of the application.")));
    parser.process(app);

    const QUuid uuid = QUuid::fromString(parser.value("uuid"));
    const bool whole_app = parser.isSet("whole-app");

    if (uuid.isNull()) {
        printf("%s\n", qPrintable(i18n("This helper utility is not supposed to be called directly.")));
        return 1;
    }

    app.setApplicationDisplayName((whole_app) ? i18nc("Window caption for the application wide rules dialog", "Edit Application-Specific Settings")
                                              : i18n("Edit Window-Specific Settings"));

    QStringList kcm_args;
    kcm_args << QStringLiteral("uuid=%1").arg(uuid.toString());
    if (whole_app) {
        kcm_args << QStringLiteral("whole-app");
    }

    // Try to reach any open instance of the KCM and send the petition there
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin.KCMKWinRules"),
                                                          QStringLiteral("/KCMKWinRules"),
                                                          QStringLiteral("org.kde.KWin.KCMKWinRules"),
                                                          QStringLiteral("updateArguments"));
    message.setArguments({kcm_args});
    const QDBusMessage response = QDBusConnection::sessionBus().call(message, QDBus::Block, 1);
    if (response.type() == QDBusMessage::ReplyMessage) {
        // A previous instance of the KCM has already handled the message.
        return 0;
    }

    // Open a new KCM instance
    KPluginMetaData pluginData = KPluginMetaData(QStringLiteral("kcms/kcm_kwinrules"));

    KCMultiDialog *dialog = new KCMultiDialog;
    dialog->addModule(pluginData, kcm_args);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();

    app.setQuitOnLastWindowClosed(true);

    return app.exec();
}
