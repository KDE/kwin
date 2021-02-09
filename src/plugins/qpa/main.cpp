/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "integration.h"
#include <qpa/qplatformintegrationplugin.h>

#include <QCoreApplication>

class KWinIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "kwin.json")
public:
    using QPlatformIntegrationPlugin::create;
    QPlatformIntegration *create(const QString &system, const QStringList &paramList) override;
};

QPlatformIntegration *KWinIntegrationPlugin::create(const QString &system, const QStringList &paramList)
{
    Q_UNUSED(paramList)
    if (!QCoreApplication::applicationFilePath().endsWith(QLatin1String("kwin_wayland")) && !qEnvironmentVariableIsSet("KWIN_FORCE_OWN_QPA")) {
        // Not KWin
        return nullptr;
    }
    if (system.compare(QLatin1String("wayland-org.kde.kwin.qpa"), Qt::CaseInsensitive) == 0) {
        // create our integration
        return new KWin::QPA::Integration;
    }
    return nullptr;
}

#include "main.moc"
