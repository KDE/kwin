/*
    SPDX-FileCopyrightText: 2025 Oliver Beard <olib141@outlook.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>

#include <KConfigGroup>
#include <KGlobalAccel>
#include <KSharedConfig>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    // We removed activating the showpaint effect — it is always running when
    // loaded. Previously, when loaded, it was initially inactive until toggled,
    // so now we must disable it or it'll start appearing without being invoked.

    // Remove old toggle action shortcut (that activates the effect)
    QAction toggleAction;
    toggleAction.setObjectName(QStringLiteral("Toggle"));
    toggleAction.setProperty("componentName", QStringLiteral("kwin"));
    toggleAction.setProperty("componentDisplayName", QStringLiteral("KWin"));

    KGlobalAccel::self()->setShortcut(&toggleAction, {QKeySequence()}, KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->removeAllShortcuts(&toggleAction);

    // Disable effect
    auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup pluginsGroup = config->group(QStringLiteral("Plugins"));

    if (pluginsGroup.exists() && pluginsGroup.hasKey(QStringLiteral("showpaintEnabled"))) {
        pluginsGroup.deleteEntry(QStringLiteral("showpaintEnabled"));
        pluginsGroup.sync();
    }

    QDBusMessage reloadMessage =
        QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                       QStringLiteral("/Effects"),
                                       QStringLiteral("org.kde.kwin.Effects"),
                                       QStringLiteral("unloadEffect"));
    reloadMessage.setArguments({QStringLiteral("showpaint")});
    QDBusConnection::sessionBus().call(reloadMessage);

    return EXIT_SUCCESS;
}
