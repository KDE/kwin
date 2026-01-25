/*
 * SPDX-FileCopyrightText: 2026 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: MIT
 */

#include "config-kwin.h"

#include <KConfig>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageDialog>
#include <KWindowSystem>

#include <QApplication>
#include <QCommandLineParser>
#include <QKeySequence>

#include <qpa/qplatformwindow_p.h>

#include <private/qtx11extras_p.h>
#include <xcb/xcb.h>

static QString shortcut(const QString &name)
{
    const auto shortcuts = KGlobalAccel::self()->globalShortcut(QStringLiteral("kwin"), name);
    return !shortcuts.isEmpty() ? shortcuts.first().toString(QKeySequence::NativeText) : QString();
}

int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kwin"));
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("kwin")));
    QCoreApplication::setApplicationName(QStringLiteral("kwin_dialog_helper"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    QApplication::setApplicationDisplayName(i18n("Window Manager"));
    QCoreApplication::setApplicationVersion(KWIN_VERSION_STRING);
    QApplication::setDesktopFileName(QStringLiteral("org.kde.kwin.dialoghelper"));

    QCommandLineOption widOption(QStringLiteral("wid"),
                                 i18n("ID of resource belonging to the application"), i18n("id"));
    QCommandLineOption timestampOption(QStringLiteral("timestamp"),
                                       i18n("Time of user action causing termination"), i18n("time"));
    QCommandLineOption messageOption(QStringLiteral("message"), i18n("The message to show (fullscreen, noborder)"), QStringLiteral("message"));
    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("KWin helper utility"));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(widOption);
    parser.addOption(timestampOption);
    parser.addOption(messageOption);

    parser.process(app);

    const bool isX11 = app.platformName() == QLatin1StringView("xcb");

    QString windowHandle = parser.value(widOption);

    // on Wayland XDG_ACTIVATION_TOKEN is set in the environment.
    bool time_ok = false;
    xcb_timestamp_t timestamp = parser.value(timestampOption).toULong(&time_ok);

    if (windowHandle.isEmpty()
        || (isX11 && (!time_ok || timestamp == XCB_CURRENT_TIME))) {
        fprintf(stdout, "%s\n", qPrintable(i18n("This helper utility is not supposed to be called directly.")));
        parser.showHelp(1);
    }

    const QString message = parser.value(messageOption);

    QString prompt;
    KGuiItem revertButton;

    if (message == QLatin1StringView("fullscreen")) {
        if (const QString fullscreenShortcut = shortcut(QStringLiteral("Window Fullscreen")); !fullscreenShortcut.isEmpty()) {
            prompt = i18n("You have selected to show a window in fullscreen mode.\n"
                          "If the application itself does not have an option to turn the fullscreen "
                          "mode off you will not be able to disable it "
                          "again using the mouse: use the %1 keyboard shortcut instead.",
                          fullscreenShortcut);
        } else if (const QString windowMenuShortcut = shortcut(QStringLiteral("Window Operations Menu")); !windowMenuShortcut.isEmpty()) {
            prompt = i18n("You have selected to show a window in fullscreen mode.\n"
                          "If the application itself does not have an option to turn the fullscreen "
                          "mode off you will not be able to disable it "
                          "again using the mouse: use the window menu instead, "
                          "activated using the %1 keyboard shortcut.",
                          windowMenuShortcut);
        } else {
            // User is screwed, no shortcut.
            prompt = i18n("You have selected to show a window in fullscreen mode.\n"
                          "If the application itself does not have an option to turn the fullscreen "
                          "mode off you will not be able to disable it again using the mouse.");
        }

        revertButton = KGuiItem(i18nc("@action:button Un-fullscreen", "Undo Fullscreen"), QStringLiteral("view-restore"));
    } else if (message == QLatin1StringView("noborder")) {
        if (const QString noBorderShortcut = shortcut(QStringLiteral("Window No Border")); !noBorderShortcut.isEmpty()) {
            prompt = i18n("You have selected to show a window without its border.\n"
                          "Without the border, you will not be able to enable the border "
                          "again using the mouse: use the %1 keyboard shortcut instead.",
                          noBorderShortcut);
        } else if (const QString windowMenuShortcut = shortcut(QStringLiteral("Window Operations Menu")); !windowMenuShortcut.isEmpty()) {
            prompt = i18n("You have selected to show a window without its border.\n"
                          "Without the border, you will not be able to enable the border "
                          "again using the mouse: use the window menu instead, "
                          "activated using the %1 keyboard shortcut.",
                          windowMenuShortcut);
        } else {
            prompt = i18n("You have selected to show a window without its border.\n"
                          "Without the border, you will not be able to enable the border again using the mouse.");
        }

        revertButton = KGuiItem(i18nc("@action:button", "Restore Border"), QStringLiteral("edit-undo"));
    } else {
        fprintf(stdout, "%s\n", qPrintable(i18n("This helper utility is not supposed to be called directly.")));
        parser.showHelp(1);
    }

    if (isX11) {
        QX11Info::setAppUserTime(timestamp);
    }

    // KMessageDialog::Information doesn't let us add additional buttons...
    auto *dialog = new KMessageDialog(KMessageDialog::QuestionTwoActions, prompt);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    dialog->setIcon(app.windowIcon());
    dialog->setButtons(KStandardGuiItem::ok(), revertButton);

    dialog->setDontAskAgainText(i18n("Do not show again"));

    dialog->winId();

    KWindowSystem::setMainWindow(dialog->windowHandle(), windowHandle);

    QObject::connect(dialog, &QDialog::finished, &app, [dialog](int result) {
        if (result == KMessageDialog::ButtonType::PrimaryAction && dialog->isDontAskAgainChecked()) {
            KConfig cfg(QStringLiteral("kwin_dialogsrc"));
            KMessageBox::setDontShowAgainConfig(&cfg);
            const QString dontAgainKey = QStringLiteral("altf3warning");
            KMessageBox::saveDontShowAgainTwoActions(dontAgainKey, KMessageBox::PrimaryAction);
        }

        // Exit code 2 tells kwin to undo the action.
        qApp->exit(result == KMessageDialog::ButtonType::SecondaryAction ? 2 : 0);
    });

    dialog->show();

    return app.exec();
}
