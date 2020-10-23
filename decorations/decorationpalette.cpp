/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
    SPDX-FileCopyrightText: 2015 Mika Allan Rauhala <mika.allan.rauhala@gmail.com>
    SPDX-FileCopyrightText: 2020 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "decorationpalette.h"
#include "decorations_logging.h"

#include <KConfigGroup>

#include <QFileInfo>
#include <QStandardPaths>

namespace KWin
{
namespace Decoration
{

DecorationPalette::DecorationPalette(const QString &colorScheme)
    : m_colorScheme(colorScheme != QStringLiteral("kdeglobals") ? colorScheme : QString() )
{
    if (m_colorScheme.isEmpty()) {
        m_colorSchemeConfig = KSharedConfig::openConfig(m_colorScheme, KConfig::FullConfig);
    } else {
        m_colorSchemeConfig = KSharedConfig::openConfig(m_colorScheme, KConfig::SimpleConfig);
    }
    m_watcher = KConfigWatcher::create(m_colorSchemeConfig);

    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, &DecorationPalette::update);

    update();
}

bool DecorationPalette::isValid() const
{
    return true;
}

QColor DecorationPalette::color(KDecoration2::ColorGroup group, KDecoration2::ColorRole role) const
{
    using KDecoration2::ColorRole;
    using KDecoration2::ColorGroup;

    if (m_legacyPalette.has_value()) {
        switch (role) {
            case ColorRole::Frame:
                switch (group) {
                    case ColorGroup::Active:
                        return m_legacyPalette->activeFrameColor;
                    case ColorGroup::Inactive:
                        return m_legacyPalette->inactiveFrameColor;
                    default:
                        return QColor();
                }
            case ColorRole::TitleBar:
                switch (group) {
                    case ColorGroup::Active:
                        return m_legacyPalette->activeTitleBarColor;
                    case ColorGroup::Inactive:
                        return m_legacyPalette->inactiveTitleBarColor;
                    default:
                        return QColor();
                }
            case ColorRole::Foreground:
                switch (group) {
                    case ColorGroup::Active:
                        return m_legacyPalette->activeForegroundColor;
                    case ColorGroup::Inactive:
                        return m_legacyPalette->inactiveForegroundColor;
                    case ColorGroup::Warning:
                        return m_legacyPalette->warningForegroundColor;
                    default:
                        return QColor();
                }
            default:
                return QColor();
        }
    }

    switch (role) {
        case ColorRole::Frame:
            switch (group) {
                case ColorGroup::Active:
                    return m_palette.active.background().color();
                case ColorGroup::Inactive:
                    return m_palette.inactive.background().color();
                default:
                    return QColor();
            }
        case ColorRole::TitleBar:
            switch (group) {
                case ColorGroup::Active:
                    return m_palette.active.background().color();
                case ColorGroup::Inactive:
                    return m_palette.inactive.background().color();
                default:
                    return QColor();
            }
        case ColorRole::Foreground:
            switch (group) {
                case ColorGroup::Active:
                    return m_palette.active.foreground().color();
                case ColorGroup::Inactive:
                    return m_palette.inactive.foreground().color();
                case ColorGroup::Warning:
                    return m_palette.inactive.foreground(KColorScheme::ForegroundRole::NegativeText).color();
                default:
                    return QColor();
            }
        default:
            return QColor();
    }
}

QPalette DecorationPalette::palette() const
{
    return m_legacyPalette ? m_legacyPalette->palette : KColorScheme::createApplicationPalette(m_colorSchemeConfig);
}

void DecorationPalette::update()
{
    m_colorSchemeConfig->sync();

    if (!KColorScheme::isColorSetSupported(m_colorSchemeConfig, KColorScheme::Header)) {
        KConfigGroup wmConfig(m_colorSchemeConfig, QStringLiteral("WM"));

        if (!wmConfig.exists()) {
            m_palette.active = KColorScheme(QPalette::Normal, KColorScheme::Header, m_colorSchemeConfig);
            m_palette.inactive = KColorScheme(QPalette::Inactive, KColorScheme::Header, m_colorSchemeConfig);
            m_legacyPalette.reset();
            return;
        }

        m_legacyPalette = LegacyPalette{};
        m_legacyPalette->palette = KColorScheme::createApplicationPalette(m_colorSchemeConfig);
        m_legacyPalette->activeFrameColor        = wmConfig.readEntry("frame", m_legacyPalette->palette.color(QPalette::Active, QPalette::Window));
        m_legacyPalette->inactiveFrameColor      = wmConfig.readEntry("inactiveFrame", m_legacyPalette->activeFrameColor);
        m_legacyPalette->activeTitleBarColor     = wmConfig.readEntry("activeBackground", m_legacyPalette->palette.color(QPalette::Active, QPalette::Highlight));
        m_legacyPalette->inactiveTitleBarColor   = wmConfig.readEntry("inactiveBackground", m_legacyPalette->inactiveTitleBarColor);
        m_legacyPalette->activeForegroundColor   = wmConfig.readEntry("activeForeground", m_legacyPalette->palette.color(QPalette::Active, QPalette::HighlightedText));
        m_legacyPalette->inactiveForegroundColor = wmConfig.readEntry("inactiveForeground", m_legacyPalette->activeForegroundColor.darker());

        KConfigGroup windowColorsConfig(m_colorSchemeConfig, QStringLiteral("Colors:Window"));
        m_legacyPalette->warningForegroundColor = windowColorsConfig.readEntry("ForegroundNegative", QColor(237, 21, 2));
    } else {
        m_palette.active = KColorScheme(QPalette::Normal, KColorScheme::Header, m_colorSchemeConfig);
        m_palette.inactive = KColorScheme(QPalette::Inactive, KColorScheme::Header, m_colorSchemeConfig);
        m_legacyPalette.reset();
    }

    Q_EMIT changed();
}

}
}
