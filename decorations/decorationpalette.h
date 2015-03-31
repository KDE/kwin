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

#ifndef KWIN_DECORATION_PALETTE_H
#define KWIN_DECORATION_PALETTE_H

#include <KDecoration2/DecorationSettings>
#include <QFileSystemWatcher>
#include <QPalette>

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
    QFileSystemWatcher m_watcher;

    QPalette m_palette;

    QColor m_activeTitleBarColor;
    QColor m_inactiveTitleBarColor;

    QColor m_activeFrameColor;
    QColor m_inactiveFrameColor;

    QColor m_activeForegroundColor;
    QColor m_inactiveForegroundColor;
    QColor m_warningForegroundColor;
};

}
}

#endif
