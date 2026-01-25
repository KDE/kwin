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

#include "debug.h"

using namespace Qt::StringLiterals;

static QString shortcut(const QString &name)
{
    const auto shortcuts = KGlobalAccel::self()->globalShortcut(u"kwin"_s, name);
    return !shortcuts.isEmpty() ? shortcuts.first().toString(QKeySequence::NativeText) : QString();
}

int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kwin"));
    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme(u"kwin"_s));
    QCoreApplication::setApplicationName(u"kwin_dialog_helper"_s);
    QCoreApplication::setOrganizationDomain(u"kde.org"_s);
    QApplication::setApplicationDisplayName(i18n("Window Manager"));
    QCoreApplication::setApplicationVersion(KWIN_VERSION_STRING);
    QApplication::setDesktopFileName(u"org.kde.kwin.dialoghelper"_s);

    QCommandLineOption widOption(u"wid"_s,
                                 i18n("ID of resource belonging to the application"), i18n("id"));
    QCommandLineOption timestampOption(u"timestamp"_s,
                                       i18n("Time of user action causing termination"), i18n("time"));
    QCommandLineOption messageOption(u"message"_s, i18n("The message to show (fullscreen, noborder)"), u"message"_s);
    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("KWin helper utility"));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(widOption);
    parser.addOption(timestampOption);
    parser.addOption(messageOption);

    parser.process(app);

    const bool isX11 = app.platformName() == "xcb"_L1;

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

    if (message == "fullscreen"_L1) {
        if (const QString fullscreenShortcut = shortcut(u"Window Fullscreen"_s); !fullscreenShortcut.isEmpty()) {
            prompt = i18n("You have selected to show a window in fullscreen mode.\n"
                          "If the application itself does not have an option to turn the fullscreen "
                          "mode off you will not be able to disable it "
                          "again using the mouse: use the %1 keyboard shortcut instead.",
                          fullscreenShortcut);
        } else if (const QString windowMenuShortcut = shortcut(u"Window Operations Menu"_s); !windowMenuShortcut.isEmpty()) {
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

        revertButton = KGuiItem(i18nc("@action:button Un-fullscreen", "Undo Fullscreen"), u"view-restore"_s);
    } else if (message == "noborder"_L1) {
        if (const QString noBorderShortcut = shortcut(u"Window No Border"_s); !noBorderShortcut.isEmpty()) {
            prompt = i18n("You have selected to show a window without its border.\n"
                          "Without the border, you will not be able to enable the border "
                          "again using the mouse: use the %1 keyboard shortcut instead.",
                          noBorderShortcut);
        } else if (const QString windowMenuShortcut = shortcut(u"Window Operations Menu"_s); !windowMenuShortcut.isEmpty()) {
            prompt = i18n("You have selected to show a window without its border.\n"
                          "Without the border, you will not be able to enable the border "
                          "again using the mouse: use the window menu instead, "
                          "activated using the %1 keyboard shortcut.",
                          windowMenuShortcut);
        } else {
            prompt = i18n("You have selected to show a window without its border.\n"
                          "Without the border, you will not be able to enable the border again using the mouse.");
        }

        revertButton = KGuiItem(i18nc("@action:button", "Restore Border"), u"edit-undo"_s);
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
            KConfig cfg(u"kwin_dialogsrc"_s);
            KMessageBox::setDontShowAgainConfig(&cfg);
            const QString dontAgainKey = u"altf3warning"_s;
            KMessageBox::saveDontShowAgainTwoActions(dontAgainKey, KMessageBox::PrimaryAction);
        }

        // Exit code 2 tells kwin to undo the action.
        qApp->exit(result == KMessageDialog::ButtonType::SecondaryAction ? 2 : 0);
    });

    dialog->show();

    return app.exec();
}
