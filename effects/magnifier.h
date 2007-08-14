/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_MAGNIFIER_H
#define KWIN_MAGNIFIER_H

#include <kwineffects.h>

namespace KWin
{

class MagnifierEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        MagnifierEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void mouseChanged( const QPoint& pos, const QPoint& old,
            Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );
    private slots:
        void zoomIn();
        void zoomOut();
        void toggle();
    private:
        QRect magnifierArea( QPoint pos = cursorPos()) const;
        float zoom;
        float target_zoom;
        QSize magnifier_size;
    };

} // namespace

#endif
