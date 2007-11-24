/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_MOUSEMARK_H
#define KWIN_MOUSEMARK_H

#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

class MouseMarkEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        MouseMarkEffect();
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void mouseChanged( const QPoint& pos, const QPoint& old,
            Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );
    private slots:
        void clear();
    private:
        typedef QVector< QPoint > Mark;
        QVector< Mark > marks;
        Mark drawing;
        int width;
        QColor color;
    };

} // namespace

#endif
