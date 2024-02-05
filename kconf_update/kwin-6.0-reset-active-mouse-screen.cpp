/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KConfigGroup>
#include <KSharedConfig>

int main()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));

    KConfigGroup windows = config->group(QStringLiteral("Windows"));
    if (!windows.exists()) {
        return EXIT_SUCCESS;
    }

    if (!windows.hasKey(QStringLiteral("ActiveMouseScreen"))) {
        return EXIT_SUCCESS;
    }

    windows.deleteEntry(QStringLiteral("ActiveMouseScreen"));

    return windows.sync() ? EXIT_SUCCESS : EXIT_FAILURE;
}
