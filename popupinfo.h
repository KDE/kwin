/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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


#ifndef KWIN_POPUPINFO_H
#define KWIN_POPUPINFO_H
#include <QWidget>
#include <QTimer>

namespace KWin
{

class Workspace;

class PopupInfo : public QWidget
    {
    Q_OBJECT
    public:
        explicit PopupInfo( Workspace* ws, const char *name=0 );
        ~PopupInfo();

        void reset();
        void hide();
        void showInfo(const QString &infoString);

        void reconfigure();

    protected:
        void paintEvent( QPaintEvent* );
        void paintContents();

    private:
        QTimer m_delayedHideTimer;
        int m_delayTime;
        bool m_show;
        bool m_shown;
        QString m_infoString;
        Workspace* workspace;
    };

} // namespace

#endif
