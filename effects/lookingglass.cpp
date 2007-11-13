/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/


#include "lookingglass.h"

#include <kwinglutils.h>

#include <kactioncollection.h>
#include <kaction.h>
#include <kconfiggroup.h>
#include <klocale.h>
#include <kdebug.h>

#include <kmessagebox.h>

namespace KWin
{

KWIN_EFFECT( lookingglass, LookingGlassEffect )
KWIN_EFFECT_SUPPORTED( lookingglass, ShaderEffect::supported() )


LookingGlassEffect::LookingGlassEffect() : QObject(), ShaderEffect("lookingglass")
    {
    zoom = 1.0f;
    target_zoom = 1.0f;

    KConfigGroup conf = EffectsHandler::effectConfig("LookingGlass");
    actionCollection = new KActionCollection( this );
    actionCollection->setConfigGlobal(true);
    actionCollection->setConfigGroup("LookingGlass");

    KAction* a;
    a = static_cast< KAction* >( actionCollection->addAction( KStandardAction::ZoomIn, this, SLOT( zoomIn())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Plus));
    a = static_cast< KAction* >( actionCollection->addAction( KStandardAction::ZoomOut, this, SLOT( zoomOut())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));
    a = static_cast< KAction* >( actionCollection->addAction( KStandardAction::ActualSize, this, SLOT( toggle())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));
    radius = conf.readEntry("Radius", 200);

    kDebug(1212) << QString("Radius from config: %1").arg(radius) << endl;

    actionCollection->readSettings();
    }

LookingGlassEffect::~LookingGlassEffect()
    {
    }

void LookingGlassEffect::toggle()
    {
    if( target_zoom == 1.0f )
        target_zoom = 2.0f;
    else
        target_zoom = 1.0f;
    setEnabled( true );
    }

void LookingGlassEffect::zoomIn()
    {
    target_zoom = qMin(7.0, target_zoom + 0.5);
    setEnabled( true );
    effects->addRepaint( cursorPos().x() - radius, cursorPos().y() - radius, 2*radius, 2*radius );
    }

void LookingGlassEffect::zoomOut()
    {
    target_zoom -= 0.5;
    if( target_zoom < 1 )
        {
        target_zoom = 1;
        setEnabled( false );
        }
    effects->addRepaint( cursorPos().x() - radius, cursorPos().y() - radius, 2*radius, 2*radius );
    }

void LookingGlassEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( zoom != target_zoom )
        {
        double diff = time / 500.0;
        if( target_zoom > zoom )
            zoom = qMin( zoom * qMax( 1.0 + diff, 1.2 ), target_zoom );
        else
            zoom = qMax( zoom * qMin( 1.0 - diff, 0.8 ), target_zoom );
        kDebug() << "zoom is now " << zoom;
        radius = qBound(200.0, 200.0 * zoom, 500.0);

        if( zoom > 1.0f )
            {
            shader()->bind();
            shader()->setUniform("zoom", (float)zoom);
            shader()->setUniform("radius", (float)radius);
            shader()->unbind();
            }
        else
            {
            setEnabled( false );
            }

        effects->addRepaint( cursorPos().x() - radius, cursorPos().y() - radius, 2*radius, 2*radius );
        }

    ShaderEffect::prePaintScreen( data, time );
    }

void LookingGlassEffect::mouseChanged( const QPoint& pos, const QPoint& old, Qt::MouseButtons, Qt::KeyboardModifiers )
    {
    if( pos != old && isEnabled() )
        {
        effects->addRepaint( pos.x() - radius, pos.y() - radius, 2*radius, 2*radius );
        effects->addRepaint( old.x() - radius, old.y() - radius, 2*radius, 2*radius );
        }
    }

} // namespace

#include "lookingglass.moc"
