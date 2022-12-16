/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
    SPDX-FileCopyrightText: 2015 Mika Allan Rauhala <mika.allan.rauhala@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KColorScheme>
#include <KConfigWatcher>
#include <KDecoration2/DecorationSettings>
#include <KSharedConfig>
#include <QFileSystemWatcher>
#include <QPalette>

#include <optional>

namespace KWin
{
namespace Decoration
{

class DecorationPalette : public QObject
{
    Q_OBJECT
public:
    DecorationPalette(const QString &colorScheme);

    bool isValid() const;

    QColor color(KDecoration2::ColorGroup group, KDecoration2::ColorRole role) const;
    QPalette palette() const;

Q_SIGNALS:
    void changed();

private:
    void update();

    QString m_colorScheme;
    KConfigWatcher::Ptr m_watcher;

    struct LegacyColors
    {
        QColor activeTitleBarColor;
        QColor inactiveTitleBarColor;

        QColor activeFrameColor;
        QColor inactiveFrameColor;

        QColor activeForegroundColor;
        QColor inactiveForegroundColor;
        QColor warningForegroundColor;
    };

    struct ModernColors
    {
        KColorScheme active;
        KColorScheme inactive;
    };

    KSharedConfig::Ptr m_colorSchemeConfig;
    QPalette m_palette;
    ModernColors m_colors;
    std::optional<LegacyColors> m_legacyColors;
};

}
}
