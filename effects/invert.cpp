/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "invert.h"

#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>


namespace KWin
{

KWIN_EFFECT( invert, InvertEffect )
KWIN_EFFECT_SUPPORTED( invert, ShaderEffect::supported() )


InvertEffect::InvertEffect() : QObject(), ShaderEffect("invert")
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "Invert" );
    a->setText( i18n("Toggle Invert effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F6));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    }

void InvertEffect::toggle()
    {
    setEnabled( !isEnabled());
    }

} // namespace

#include "invert.moc"
