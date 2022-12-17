/*
    SPDX-FileCopyrightText: 2021 Dan Leinir Turthra Jensen <admin@leinir.dk>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kwindecorationsettings.h"

#include "decorationmodel.h"

#include <KLocalizedString>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QFileInfo>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int exitCode{0};
    QCoreApplication::setApplicationName(QStringLiteral("kwin-applywindowdecoration"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    KLocalizedString::setApplicationDomain("kwin-applywindowdecoration");

    QCommandLineParser *parser = new QCommandLineParser;
    parser->addHelpOption();
    parser->setApplicationDescription(i18n("This tool allows you to set the window decoration theme for the currently active session, without accidentally setting it to one that is either not available, or which is already set."));
    parser->addPositionalArgument(QStringLiteral("theme"), i18n("The name of the window decoration theme you wish to set for KWin. Passing a full path will attempt to find a theme in that directory, and then apply that if one can be deduced."));
    parser->addOption(QCommandLineOption(QStringLiteral("list-themes"), i18n("Show all the themes available on the system (and which is the current theme)")));
    parser->process(app);

    KDecoration2::Configuration::DecorationsModel *model = new KDecoration2::Configuration::DecorationsModel(&app);
    model->init();
    KWinDecorationSettings *settings = new KWinDecorationSettings(&app);
    QTextStream ts(stdout);
    if (!parser->positionalArguments().isEmpty()) {
        QString requestedTheme{parser->positionalArguments().constFirst()};
        if (requestedTheme.endsWith(QStringLiteral("/*"))) {
            // Themes installed through KNewStuff will commonly be given an installed files entry
            // which has the main directory name and an asterix to say the cursors are all in that directory,
            // and since one of the main purposes of this tool is to allow adopting things from a kns dialog,
            // we handle that little weirdness here.
            requestedTheme.remove(requestedTheme.length() - 2, 2);
        }

        bool themeResolved{true};
        if (requestedTheme.contains(QStringLiteral("/"))) {
            themeResolved = false;
            if (QFileInfo::exists(requestedTheme) && QFileInfo(requestedTheme).isDir()) {
                // Since this is the name of a directory, let's do a bit of checking to see
                // if we know enough about it to deduce that this is, in fact, a theme.
                QStringList splitTheme = requestedTheme.split(QStringLiteral("/"), Qt::SkipEmptyParts);
                if (splitTheme.count() > 3 && splitTheme[splitTheme.count() - 3] == QStringLiteral("aurorae") && splitTheme[splitTheme.count() - 2] == QStringLiteral("themes")) {
                    // We think this is an aurorae theme, but let's just make a little more certain...
                    QString file(QStringLiteral("aurorae/themes/%1/metadata.desktop").arg(splitTheme.last()));
                    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, file);
                    if (!path.isEmpty() && path == QStringLiteral("%1/metadata.desktop").arg(requestedTheme)) {
                        requestedTheme = QString("__aurorae__svg__").append(splitTheme.last());
                        themeResolved = true;
                        ts << i18n("Resolved %1 to the KWin Aurorae theme \"%2\", and will attempt to set that as your current theme.")
                                  .arg(parser->positionalArguments().first(), requestedTheme)
                           << Qt::endl;
                    }
                }
            } else {
                ts << i18n("You attempted to pass a file path, but this could not be resolved to a theme, and we will have to abort, due to having no theme to set") << Qt::endl;
                exitCode = -1;
            }
        }

        if (settings->theme() == requestedTheme) {
            ts << i18n("The requested theme \"%1\" is already set as the window decoration theme.", requestedTheme) << Qt::endl;
            // not an error condition, just nothing happens
        } else if (themeResolved) {
            int index{-1};
            QStringList availableThemes;
            for (int i = 0; i < model->rowCount(); ++i) {
                const QString themeName = model->data(model->index(i), KDecoration2::Configuration::DecorationsModel::ThemeNameRole).toString();
                if (requestedTheme == themeName) {
                    index = i;
                    break;
                }
                availableThemes << themeName;
            }
            if (index > -1) {
                settings->setTheme(model->data(model->index(index), KDecoration2::Configuration::DecorationsModel::ThemeNameRole).toString());
                settings->setPluginName(model->data(model->index(index), KDecoration2::Configuration::DecorationsModel::PluginNameRole).toString());
                if (settings->save()) {
                    // Send a signal to all kwin instances
                    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KWin"),
                                                                      QStringLiteral("org.kde.KWin"),
                                                                      QStringLiteral("reloadConfig"));
                    QDBusConnection::sessionBus().send(message);
                    ts << i18n("Successfully applied the cursor theme %1 to your current Plasma session",
                               model->data(model->index(index), KDecoration2::Configuration::DecorationsModel::ThemeNameRole).toString())
                       << Qt::endl;
                } else {
                    ts << i18n("Failed to save your theme settings - the reason is unknown, but this is an unrecoverable error. You may find that simply trying again will work.");
                    exitCode = -1;
                }
            } else {
                ts << i18n("Could not find theme \"%1\". The theme should be one of the following options: %2", requestedTheme, availableThemes.join(QStringLiteral(", "))) << Qt::endl;
                exitCode = -1;
            }
        }
    } else if (parser->isSet(QStringLiteral("list-themes"))) {
        ts << i18n("You have the following KWin window decoration themes on your system:") << Qt::endl;
        for (int i = 0; i < model->rowCount(); ++i) {
            const QString displayName = model->data(model->index(i), Qt::DisplayRole).toString();
            const QString themeName = model->data(model->index(i), KDecoration2::Configuration::DecorationsModel::ThemeNameRole).toString();
            if (settings->theme() == themeName) {
                ts << QStringLiteral(" * %1 (theme name: %2 - current theme for this Plasma session)").arg(displayName, themeName) << Qt::endl;
            } else {
                ts << QStringLiteral(" * %1 (theme name: %2)").arg(displayName, themeName) << Qt::endl;
            }
        }
    } else {
        parser->showHelp();
    }
    QTimer::singleShot(0, &app, [&app, &exitCode]() {
        app.exit(exitCode);
    });

    return app.exec();
}
