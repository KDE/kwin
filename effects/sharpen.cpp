/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "sharpen.h"

#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>


namespace KWin
{

KWIN_EFFECT( sharpen, SharpenEffect )
KWIN_EFFECT_SUPPORTED( sharpen, ShaderEffect::supported() )


SharpenEffect::SharpenEffect() : QObject(), ShaderEffect("sharpen")
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "Sharpen" );
    a->setText( i18n("Toggle Sharpen effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F7));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    }

void SharpenEffect::toggle()
    {
    setEnabled( !isEnabled());
    }

} // namespace

#include "sharpen.moc"
