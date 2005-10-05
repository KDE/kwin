/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.
 */

#include "kcm.h"

#include <kglobal.h>
#include <qlayout.h>
//Added by qt3to4:
#include <QVBoxLayout>
#include <klocale.h>
#include <kapplication.h>
#include <dcopclient.h>
#include <kaboutdata.h>

#include "ruleslist.h"

extern "C"
    KDE_EXPORT KCModule *create_kwinrules( QWidget *parent, const char *name )
    {
    //CT there's need for decision: kwm or kwin?
    KGlobal::locale()->insertCatalog( "kcmkwinrules" );
    return new KWinInternal::KCMRules( parent, name );
    }

namespace KWinInternal
{

KCMRules::KCMRules( QWidget *parent, const char *name )
: KCModule( parent, name )
, config( "kwinrulesrc" )
    {
    QVBoxLayout *layout = new QVBoxLayout( this );
    widget = new KCMRulesList( this );
    layout->addWidget( widget );
    connect( widget, SIGNAL( changed( bool )), SLOT( moduleChanged( bool )));
    KAboutData *about = new KAboutData(I18N_NOOP( "kcmkwinrules" ),
        I18N_NOOP( "Window-Specific Settings Configuration Module" ),
        0, 0, KAboutData::License_GPL, I18N_NOOP( "(c) 2004 KWin and KControl Authors" ));
    about->addAuthor("Lubos Lunak",0,"l.lunak@kde.org");
    setAboutData(about);
    }

void KCMRules::load()
    {
    config.reparseConfiguration();
    widget->load();
    emit KCModule::changed( false );
    }

void KCMRules::save()
    {
    widget->save();
    emit KCModule::changed( false );
    // Send signal to kwin
    config.sync();
    if( !kapp->dcopClient()->isAttached())
        kapp->dcopClient()->attach();
    kapp->dcopClient()->send("kwin*", "", "reconfigure()", QByteArray());
    }

void KCMRules::defaults()
    {
    widget->defaults();
    }

QString KCMRules::quickHelp() const
    {
    return i18n("<h1>Window-specific Settings</h1> Here you can customize window settings specifically only"
        " for some windows."
        " <p>Please note that this configuration will not take effect if you do not use"
        " KWin as your window manager. If you do use a different window manager, please refer to its documentation"
        " for how to customize window behavior.");
    }

void KCMRules::moduleChanged( bool state )
    {
    emit KCModule::changed( state );
    }

}

// i18n freeze :-/
#if 0
I18N_NOOP("Remember settings separately for every window")
I18N_NOOP("Show internal settings for remembering")
I18N_NOOP("Internal setting for remembering")
#endif


#include "kcm.moc"
