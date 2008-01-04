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
    a->setText( i18n("Toggle Invert Effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::META + Qt::Key_I));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    }

void InvertEffect::toggle()
    {
    setEnabled( !isEnabled());
    }

} // namespace

#include "invert.moc"
