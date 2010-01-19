/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#ifndef KWIN_AURORAEPREVIEW_H
#define KWIN_AURORAEPREVIEW_H
#include <QObject>
#include <QPixmap>
#include <QHash>

namespace Aurorae
{
class ThemeConfig;
}

namespace Plasma
{
class FrameSvg;
}

namespace KWin
{

class AuroraePreview : public QObject
{
    public:
        AuroraePreview( const QString& name, const QString& packageName,
                        const QString& themeRoot, QObject* parent = NULL );
        ~AuroraePreview();

        QPixmap preview( const QSize& size, bool custom, const QString& left, const QString& right ) const;

    private:
        void initButtonFrame( const QString &button, const QString &themeName );
        void paintDeco( QPainter *painter, bool active, const QRect &rect,
                        int leftMargin, int topMargin, int rightMargin, int bottomMargin,
                        bool custom, const QString& left, const QString& right ) const;

        Plasma::FrameSvg *m_svg;
        Aurorae::ThemeConfig *m_themeConfig;
        QHash<QString, Plasma::FrameSvg*> m_buttons;
        QString m_title;
};

} // namespace KWin

#endif // KWIN_AURORAEPREVIEW_H
