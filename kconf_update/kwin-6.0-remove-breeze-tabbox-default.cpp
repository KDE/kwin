/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KConfigGroup>
#include <KSharedConfig>

int main()
{
    KConfig config(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kdedefaults/kwinrc"), KConfig::SimpleConfig);

    KConfigGroup windows = config.group(QStringLiteral("TabBox"));
    bool needsSync = false;

    if (!windows.exists()) {
        return EXIT_SUCCESS;
    }

    if (windows.hasKey(QStringLiteral("LayoutName")) && windows.readEntry(QStringLiteral("LayoutName"), QString()) == QString("org.kde.breeze.desktop")) {
        windows.deleteEntry(QStringLiteral("LayoutName"));
        needsSync = true;
    }

    if (windows.hasKey(QStringLiteral("DesktopListLayout"))) {
        windows.deleteEntry(QStringLiteral("DesktopListLayout"));
        needsSync = true;
    }
    if (windows.hasKey(QStringLiteral("DesktopLayout"))) {
        windows.deleteEntry(QStringLiteral("DesktopLayout"));
        needsSync = true;
    }

    if (needsSync) {
        return windows.sync() ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}
