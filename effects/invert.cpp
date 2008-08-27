/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include <kwinglutils.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>
#include <kdebug.h>
#include <kwinshadereffect.h>
#include <KStandardDirs>

namespace KWin
{

KWIN_EFFECT( invert, InvertEffect )
KWIN_EFFECT_SUPPORTED( invert, ShaderEffect::supported() )

InvertEffect::InvertEffect()
    :   m_inited( false ),
        m_valid( true ),
        m_shader( NULL ),
        m_allWindows( false )
    {
    KActionCollection* actionCollection = new KActionCollection( this );

    KAction* a = (KAction*)actionCollection->addAction( "Invert" );
    a->setText( i18n( "Toggle Invert Effect" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::META + Qt::Key_I ));
    connect(a, SIGNAL( triggered(bool) ), this, SLOT( toggle() ));

    KAction* b = (KAction*)actionCollection->addAction( "InvertWindow" );
    b->setText( i18n( "Toggle Invert Effect On Window" ));
    b->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::META + Qt::Key_U ));
    connect(b, SIGNAL( triggered(bool) ), this, SLOT( toggleWindow() ));
    }

InvertEffect::~InvertEffect()
    {
    delete m_shader;
    }

bool InvertEffect::loadData()
{
    m_inited = true;

    QString fragmentshader =  KGlobal::dirs()->findResource("data", "kwin/invert.frag");
    QString vertexshader =  KGlobal::dirs()->findResource("data", "kwin/invert.vert");
    if(fragmentshader.isEmpty() || vertexshader.isEmpty())
        {
        kError(1212) << "Couldn't locate shader files" << endl;
        return false;
        }

    m_shader = new GLShader(vertexshader, fragmentshader);
    if( !m_shader->isValid() )
        {
        kError(1212) << "The shader failed to load!" << endl;
        return false;
        }
    else
        {
        m_shader->bind();
        m_shader->setUniform("winTexture", 0);
        m_shader->unbind();
        }

    return true;
    }

void InvertEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    // Load if we haven't already
    if( m_valid && !m_inited )
        m_valid = loadData();

    bool useShader = m_valid && ( m_allWindows != m_windows.contains( w ));
    if( useShader )
        {
        m_shader->bind();

        int texw = w->width();
        int texh = w->height();
        if( !GLTexture::NPOTTextureSupported() )
            {
            kWarning( 1212 ) << "NPOT textures not supported, wasting some memory" ;
            texw = nearestPowerOfTwo(texw);
            texh = nearestPowerOfTwo(texh);
            }
        m_shader->setUniform("textureWidth", (float)texw);
        m_shader->setUniform("textureHeight", (float)texh);

        data.shader = m_shader;
        }

    effects->paintWindow( w, mask, region, data );

    if( useShader )
        m_shader->unbind();
    }

void InvertEffect::windowClosed( EffectWindow* w )
    {
    m_windows.removeOne( w );
    }

void InvertEffect::toggle()
    {
    m_allWindows = !m_allWindows;
    effects->addRepaintFull();
    }

void InvertEffect::toggleWindow()
    {
    if( !m_windows.contains( effects->activeWindow() ))
        m_windows.append( effects->activeWindow() );
    else
        m_windows.removeOne( effects->activeWindow() );
    effects->activeWindow()->addRepaintFull();
    }

} // namespace

#include "invert.moc"
