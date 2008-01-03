/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::META + Qt::Key_S));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    }

void SharpenEffect::toggle()
    {
    setEnabled( !isEnabled());
    }

} // namespace

#include "sharpen.moc"
