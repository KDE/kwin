/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

/*

 Testing of painting a window more than once.

*/

#ifndef KWIN_THUMBNAILASIDE_H
#define KWIN_THUMBNAILASIDE_H

#include <kwineffects.h>

#include <qhash.h>

namespace KWin
{

class ThumbnailAsideEffect
    : public QObject
    , public Effect
    {
    Q_OBJECT
    public:
        ThumbnailAsideEffect();
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void windowDamaged( EffectWindow* w, const QRect& damage );
        virtual void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );
        virtual void windowClosed( EffectWindow* w );
    private slots:
        void toggleCurrentThumbnail();
    private:
        void addThumbnail( EffectWindow* w );
        void removeThumbnail( EffectWindow* w );
        void arrange();
        void repaintAll();
        struct Data
            {
            EffectWindow* window; // the same like the key in the hash (makes code simpler)
            int index;
            QRect rect;
            };
        QHash< EffectWindow*, Data > windows;
        int maxwidth;
        int spacing;
        float opacity;
    };

} // namespace

#endif
