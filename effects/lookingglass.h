/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_LOOKINGGLASS_H
#define KWIN_LOOKINGGLASS_H

#include <kwinshadereffect.h>

namespace KWin
{

/**
 * Enhanced magnifier
 **/
class LookingGlassEffect : public QObject, public ShaderEffect
    {
    Q_OBJECT
    public:
        LookingGlassEffect();

        virtual void mouseChanged( const QPoint& pos, const QPoint& old,
            Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );

    public slots:
        void toggle();
        void zoomIn();
        void zoomOut();

    private:
        float zoom;
        float target_zoom;
        int radius;
    };

} // namespace

#endif
