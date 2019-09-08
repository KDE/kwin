/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
