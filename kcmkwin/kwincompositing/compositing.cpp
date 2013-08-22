/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
*                                                                        *
* This program is free software; you can redistribute it and/or modify   *
* it under the terms of the GNU General Public License as published by   *
* the Free Software Foundation; either version 2 of the License, or      *
* (at your option) any later version.                                    *
*                                                                        *
* This program is distributed in the hope that it will be useful,        *
* but WITHOUT ANY WARRANTY; without even the implied warranty of         *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
* GNU General Public License for more details.                           *
*                                                                        *
* You should have received a copy of the GNU General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
**************************************************************************/

#include "compositing.h"

#include <KDE/KCModuleProxy>
#include <KConfigGroup>
#include <KDE/KSharedConfig>

#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

namespace KWin {
namespace Compositing {

Compositing::Compositing(QObject *parent)
    : QObject(parent)
{
}

bool Compositing::OpenGLIsUnsafe() {
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    return kwinConfig.readEntry("OpenGLIsUnsafe", true);
}

bool Compositing::OpenGLIsBroken() {
    QDBusInterface interface(QStringLiteral("org.kde.kwin"), QStringLiteral("/Compositing"));
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");

    QString oldBackend = kwinConfig.readEntry("Backend", "OpenGL");
    kwinConfig.writeEntry("Backend", "OpenGL");
    kwinConfig.sync();
    QDBusReply<bool> OpenGLIsBrokenReply = interface.call("OpenGLIsBroken");

    if (OpenGLIsBrokenReply.value()) {
        kwinConfig.writeEntry("Backend", oldBackend);
        kwinConfig.sync();
        return true;
    }

    kwinConfig.writeEntry("OpenGLIsUnsafe", false);
    kwinConfig.sync();
    return false;
}

void Compositing::syncConfig(int openGLType, int graphicsSystem) {
    QString graphicsSystemType;
    QString backend;
    bool glLegacy;
    bool glCore;


    switch (openGLType) {
        case OPENGL31_INDEX:
            backend = "OpenGL";
            glLegacy = false;
            glCore = true;
            break;
        case OPENGL20_INDEX:
            backend = "OpenGL";
            glLegacy = false;
            glCore = false;
            break;
        case OPENGL12_INDEX:
            backend = "OpenGL";
            glLegacy = true;
            glCore = false;
            break;
        case XRENDER_INDEX:
            backend = "XRender";
            glLegacy = false;
            glCore = false;
            break;
    }

    graphicsSystem == 0 ? graphicsSystemType = "native" : graphicsSystemType = "raster";

    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    kwinConfig.writeEntry("Backend", backend);
    kwinConfig.writeEntry("GLLegacy", glLegacy);
    kwinConfig.writeEntry("GLCore", glCore);
    kwinConfig.writeEntry("GraphicsSystem", graphicsSystemType);
    kwinConfig.sync();
}

int Compositing::currentOpenGLType() {
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    QString backend = kwinConfig.readEntry("Backend", "OpenGL");
    bool glLegacy = kwinConfig.readEntry("GLLegacy", false);
    bool glCore = kwinConfig.readEntry("GLCore", false);
    int currentIndex = OPENGL20_INDEX;

    if (backend == "OpenGL") {
        if (glLegacy) {
            currentIndex = OPENGL12_INDEX;
        } else if (glCore) {
            currentIndex = OPENGL31_INDEX;
        } else {
            currentIndex = OPENGL20_INDEX;
        }
    } else {
        currentIndex = XRENDER_INDEX;
    }

    return currentIndex;
}

int Compositing::currentGraphicsSystem() {
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    QString graphicsSystem = kwinConfig.readEntry("GraphicsSystem", "native");

    return graphicsSystem == "native" ? 0 : 1;
}

}//end namespace Compositing
}//end namespace KWin
