/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_DEMO_SHOWPICTURE_H
#define KWIN_DEMO_SHOWPICTURE_H

#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

class ShowPictureEffect
    : public Effect
    {
    public:
        ShowPictureEffect();
        virtual ~ShowPictureEffect();
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
    private:
        void loadPicture();
        bool init;
        GLTexture* picture;
        QRect pictureRect;
    };

} // namespace

#endif
