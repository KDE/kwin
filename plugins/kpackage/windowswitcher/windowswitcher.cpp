/******************************************************************************
*   Copyright 2017 by Marco Martin <mart@kde.org>                             *
*                                                                             *
*   This library is free software; you can redistribute it and/or             *
*   modify it under the terms of the GNU Library General Public               *
*   License as published by the Free Software Foundation; either              *
*   version 2 of the License, or (at your option) any later version.          *
*                                                                             *
*   This library is distributed in the hope that it will be useful,           *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          *
*   Library General Public License for more details.                          *
*                                                                             *
*   You should have received a copy of the GNU Library General Public License *
*   along with this library; see the file COPYING.LIB.  If not, write to      *
*   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
*   Boston, MA 02110-1301, USA.                                               *
*******************************************************************************/

#include "windowswitcher.h"

#include <KLocalizedString>

void SwitcherPackage::initPackage(KPackage::Package *package)
{
    package->setDefaultPackageRoot(QStringLiteral("kwin/tabbox/"));

    package->addDirectoryDefinition("config", QStringLiteral("config"), i18n("Configuration Definitions"));
    QStringList mimetypes;
    mimetypes << QStringLiteral("text/xml");
    package->setMimeTypes("config", mimetypes);

    package->addDirectoryDefinition("ui", QStringLiteral("ui"), i18n("User Interface"));

    package->addDirectoryDefinition("code", QStringLiteral("code"), i18n("Executable windowswitcher"));

    package->addFileDefinition("mainscript", QStringLiteral("ui/main.qml"), i18n("Main Script File"));
    package->setRequired("mainscript", true);

    mimetypes.clear();
    mimetypes << QStringLiteral("text/plain");
    package->setMimeTypes("windowswitcher", mimetypes);
}

void SwitcherPackage::pathChanged(KPackage::Package *package)
{
    if (package->path().isEmpty()) {
        return;
    }

    KPluginMetaData md(package->metadata().metaDataFileName());
    QString mainScript = md.value("X-Plasma-MainScript");

    if (!mainScript.isEmpty()) {
        package->addFileDefinition("mainscript", mainScript, i18n("Main Script File"));
    }
}

K_EXPORT_KPACKAGE_PACKAGE_WITH_JSON(SwitcherPackage, "kwin-packagestructure-windowswitcher.json")

#include "windowswitcher.moc"

