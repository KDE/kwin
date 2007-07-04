/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_INVERT_H
#define KWIN_INVERT_H

#include <kwinshadereffect.h>

namespace KWin
{

/**
 * Inverts desktop's colors
 **/
class InvertEffect : public QObject, public ShaderEffect
    {
    Q_OBJECT
    public:
        InvertEffect();

    public slots:
        void toggle();
    };

} // namespace

#endif
