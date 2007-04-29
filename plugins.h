/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_PLUGINS_H
#define KWIN_PLUGINS_H

#include <kdecoration_plugins_p.h>

namespace KWin
{

class PluginMgr
    : public KDecorationPlugins
    {
    public:
        PluginMgr();
        virtual bool provides( Requirement );
    protected:
        virtual void error( const QString& error_msg );
    };

} // namespace

#endif
