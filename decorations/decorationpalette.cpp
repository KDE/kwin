/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2014  Hugo Pereira Da Costa <hugo.pereira@free.fr>
Copyright 2015  Mika Allan Rauhala <mika.allan.rauhala@gmail.com>

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

#include "decorationpalette.h"
#include "decorations_logging.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KColorScheme>

#include <QPalette>
#include <QFileInfo>
#include <QStandardPaths>

namespace KWin
{
namespace Decoration
{

DecorationPalette::DecorationPalette(const QString &colorScheme)
    : m_colorScheme(QFileInfo(colorScheme).isAbsolute()
                    ? colorScheme
                    : QStandardPaths::locate(QStandardPaths::GenericConfigLocation, colorScheme))
{
    if (!m_colorScheme.startsWith(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)) && colorScheme == QStringLiteral("kdeglobals")) {
        // kdeglobals doesn't exist so create it. This is needed to monitor it using QFileSystemWatcher.
        auto config = KSharedConfig::openConfig(colorScheme, KConfig::SimpleConfig);
        KConfigGroup wmConfig(config, QStringLiteral("WM"));
        wmConfig.writeEntry("FakeEntryToKeepThisGroup", true);
        config->sync();

        m_colorScheme = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, colorScheme);
    }
    m_watcher.addPath(m_colorScheme);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, [this]() {
        m_watcher.addPath(m_colorScheme);
        update();
        emit changed();
    });

    update();
}

bool DecorationPalette::isValid() const
{
    return m_activeTitleBarColor.isValid();
}

QColor DecorationPalette::color(KDecoration2::ColorGroup group, KDecoration2::ColorRole role) const
{
    using KDecoration2::ColorRole;
    using KDecoration2::ColorGroup;

    switch (role) {
        case ColorRole::Frame:
            switch (group) {
                case ColorGroup::Active:
                    return m_activeFrameColor;
                case ColorGroup::Inactive:
                    return m_inactiveFrameColor;
                default:
                    return QColor();
            }
        case ColorRole::TitleBar:
            switch (group) {
                case ColorGroup::Active:
                    return m_activeTitleBarColor;
                case ColorGroup::Inactive:
                    return m_inactiveTitleBarColor;
                default:
                    return QColor();
            }
        case ColorRole::Foreground:
            switch (group) {
                case ColorGroup::Active:
                    return m_activeForegroundColor;
                case ColorGroup::Inactive:
                    return m_inactiveForegroundColor;
                case ColorGroup::Warning:
                    return m_warningForegroundColor;
                default:
                    return QColor();
            }
        default:
            return QColor();
    }
}

QPalette DecorationPalette::palette() const
{
    return m_palette;
}

void DecorationPalette::update()
{
    auto config = KSharedConfig::openConfig(m_colorScheme, KConfig::SimpleConfig);
    KConfigGroup wmConfig(config, QStringLiteral("WM"));

    if (!wmConfig.exists() && !m_colorScheme.endsWith(QStringLiteral("/kdeglobals"))) {
        qCWarning(KWIN_DECORATIONS) << "Invalid color scheme" << m_colorScheme << "lacks WM group";
        return;
    }

    m_palette = KColorScheme::createApplicationPalette(config);

    m_activeFrameColor        = wmConfig.readEntry("frame", m_palette.color(QPalette::Active, QPalette::Window));
    m_inactiveFrameColor      = wmConfig.readEntry("inactiveFrame", m_activeFrameColor);
    m_activeTitleBarColor     = wmConfig.readEntry("activeBackground", m_palette.color(QPalette::Active, QPalette::Highlight));
    m_inactiveTitleBarColor   = wmConfig.readEntry("inactiveBackground", m_inactiveFrameColor);
    m_activeForegroundColor   = wmConfig.readEntry("activeForeground", m_palette.color(QPalette::Active, QPalette::HighlightedText));
    m_inactiveForegroundColor = wmConfig.readEntry("inactiveForeground", m_activeForegroundColor.dark());

    KConfigGroup windowColorsConfig(config, QStringLiteral("Colors:Window"));
    m_warningForegroundColor = windowColorsConfig.readEntry("ForegroundNegative", QColor(237, 21, 2));

}

}
}
