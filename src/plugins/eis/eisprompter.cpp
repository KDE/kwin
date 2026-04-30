/*
    SPDX-FileCopyrightText: 2026 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "config-kwin.h"
#include "kwinxwaylandsettings.h"

#include <KContextualHelpButton>
#include <KLocalizedString>

#include <QApplication>
#include <QCheckBox>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <print>

int main(int argc, char **argv)
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kwin"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("kde")));
    QCoreApplication::setApplicationName(QStringLiteral("kwin_eis_prompter"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    QApplication::setApplicationDisplayName(QString());
    QCoreApplication::setApplicationVersion(KWIN_VERSION_STRING);
    QApplication::setDesktopFileName(QStringLiteral("org.kde.kwin.eisprompter"));
    QApplication app(argc, argv);

    QCommandLineOption clientOption(QStringLiteral("client"), QStringLiteral("name of the eis client"), QStringLiteral("client"));
    QCommandLineParser parser;
    parser.addVersionOption();
    parser.addOption(clientOption);
    parser.process(app);

    const QString clientName = parser.value(clientOption);
    if (clientName.isEmpty()) {
        std::println(stderr, "No client name");
        return 1;
    }

    // Try to be close to the strings of xdg-desktop-portal-kde
    const QString title = i18nc("@title:window", "Remote Control");
    const QString text = xi18nc("@info", "<application>%1</application> is asking to control input devices", clientName);
    QDialog dialog;
    QVBoxLayout layout(&dialog);
    QHBoxLayout mainLayout;
    QHBoxLayout rememberLayout;
    layout.addLayout(&mainLayout);
    layout.addLayout(&rememberLayout);
    dialog.setWindowTitle(title);
    auto icon = new QLabel;
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("krfb")).pixmap(QApplication::style()->pixelMetric(QStyle::PM_MessageBoxIconSize)));
    mainLayout.addWidget(icon);
    mainLayout.addWidget(new QLabel(text));
    auto rememberCheckBox = new QCheckBox(i18nc("@option:check", "Always allow apps claiming to be %1", clientName));
    rememberLayout.addWidget(rememberCheckBox);
    auto helpButton = new KContextualHelpButton(xi18nc("@info:tooltip", "Identities of legacy X11 apps cannot be verified. Any app claiming to be <application>%1</application> will be able to control input devices. You can revoke this permission later on <application>System Settings</application>’ “Legacy X11 App Support” page.", clientName), nullptr, nullptr);
    rememberLayout.addWidget(helpButton);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel);
    buttons->button(QDialogButtonBox::StandardButton::Ok)->setText(i18nc("@action:button Allow control of input devices", "Allow"));
    buttons->button(QDialogButtonBox::StandardButton::Cancel)->setText(i18nc("@action:button Deny control of input devices", "Deny"));
    layout.addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(&dialog, &QDialog::finished, &dialog, [clientName, rememberCheckBox](int result) {
        if (result == QDialogButtonBox::AcceptRole) {
            if (rememberCheckBox->isChecked()) {
                XwaylandSettings settings;
                settings.setXwaylandEisNoPromptApps(settings.xwaylandEisNoPromptApps() += clientName);
                settings.save();
            }
            QCoreApplication::quit();
        } else {
            QCoreApplication::exit(1);
        }
    });
    dialog.show();

    return app.exec();
}
