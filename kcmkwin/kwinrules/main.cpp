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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <kcmdlineargs.h>
#include <kapplication.h>
#include <dcopclient.h>
#include <kconfig.h>
#include <klocale.h>
#include <kwin.h>

#include <X11/Xlib.h>
#include <fixx11h.h>

#include "ruleswidget.h"
#include "../../rules.h"

namespace KWinInternal
{

static void loadRules( QValueList< Rules* >& rules )
    {
    KConfig cfg( "kwinrulesrc", true );
    cfg.setGroup( "General" );
    int count = cfg.readNumEntry( "count" );
    for( int i = 1;
         i <= count;
         ++i )
        {
        cfg.setGroup( QString::number( i ));
        Rules* rule = new Rules( cfg );
        rules.append( rule );
        }
    }

static void saveRules( const QValueList< Rules* >& rules )
    {
    KConfig cfg( "kwinrulesrc" );
    cfg.setGroup( "General" );
    cfg.writeEntry( "count", rules.count());
    int i = 1;
    for( QValueList< Rules* >::ConstIterator it = rules.begin();
         it != rules.end();
         ++it )
        {
        cfg.setGroup( QString::number( i ));
        (*it)->write( cfg );
        ++i;
        }
    }

static Rules* findRule( const QValueList< Rules* >& rules, Window wid )
    {
    KWin::WindowInfo info = KWin::windowInfo( wid,
        NET::WMName | NET::WMWindowType,
        NET::WM2WindowClass | NET::WM2WindowRole | NET::WM2ClientMachine );
    if( !info.valid()) // shouldn't really happen
        return NULL;
    QCString wmclass_class = info.windowClassClass().lower();
    QCString wmclass_name = info.windowClassName().lower();
    QCString role = info.windowRole().lower();
    NET::WindowType type = info.windowType( NET::NormalMask | NET::DesktopMask | NET::DockMask
        | NET::ToolbarMask | NET::MenuMask | NET::DialogMask | NET::OverrideMask | NET::TopMenuMask
        | NET::UtilityMask | NET::SplashMask );
    QString title = info.name().lower();
//    QCString extrarole = ""; // TODO
    QCString machine = info.clientMachine().lower();
    Rules* best_match = NULL;
    int match_quality = 0; // 0 - no, 1 - generic windowrole or title, 2 - exact match
    for( QValueList< Rules* >::ConstIterator it = rules.begin();
         it != rules.end();
         ++it )
        {
        // try to find an exact match, i.e. not a generic rule
        Rules* rule = *it;
        int quality = 0;
        bool generic = true;
        if( rule->wmclassmatch != Rules::ExactMatch )
            continue; // too generic
        if( !rule->matchWMClass( wmclass_class, wmclass_name ))
            continue;
        // from now on, it matches the app - now try to match for a specific window
        if( rule->wmclasscomplete )
            {
            quality += 1;
            generic = false;  // this can be considered specific enough (old X apps)
            }
        if( rule->windowrolematch != Rules::UnimportantMatch )
            {
            quality += rule->windowrolematch == Rules::ExactMatch ? 5 : 1;
            generic = false;
            }
        if( rule->titlematch != Rules::UnimportantMatch )
            {
            quality += rule->titlematch == Rules::ExactMatch ? 3 : 1;
            generic = false;
            }
        if( rule->types != NET::AllTypesMask )
            {
            int bits = 0;
            for( int bit = 1;
                 bit < 1 << 31;
                 bit <<= 1 )
                if( rule->types & bit )
                    ++bits;
            if( bits == 1 )
                quality += 2;
            }
        if( generic )
            continue;
        if( !rule->matchType( type )
            || !rule->matchRole( role )
            || !rule->matchTitle( title )
            || !rule->matchClientMachine( machine ))
            continue;
        if( quality > match_quality )
            {
            best_match = rule;
            match_quality = quality;
            }
        }
    return best_match;
    }

static int edit( Window wid )
    {
    QValueList< Rules* > rules;
    loadRules( rules );
    Rules* orig_rule = findRule( rules, wid );
    RulesDialog dlg;
    // dlg.edit() creates new Rules instance if edited
    Rules* edited_rule = dlg.edit( orig_rule, wid );
    if( edited_rule == NULL ) // cancelled
        return 0;
    if( edited_rule->isEmpty())
        {
        if( orig_rule == NULL )
            { // new, without any settings
            delete edited_rule;
            return 0;
            }
        else
            { // removed all settings from already existing
            rules.remove( orig_rule );
            delete orig_rule;
            delete edited_rule;
            }
        }
    else if( edited_rule != orig_rule )
        {
        QValueList< Rules* >::Iterator pos = rules.find( orig_rule );
        if( pos != rules.end())
            *pos = edited_rule;
        else
            rules.prepend( edited_rule );
        delete orig_rule;
        }
    saveRules( rules );
    if( !kapp->dcopClient()->isAttached())
        kapp->dcopClient()->attach();
    kapp->dcopClient()->send("kwin*", "", "reconfigure()", "");
    return 0;
    }
    
} // namespace

static const KCmdLineOptions options[] =
    {
    // no need for I18N_NOOP(), this is not supposed to be used directly
        { "wid <wid>", "WId of the window for special window settings.", 0 },
        KCmdLineLastOption
    };

extern "C"
int kdemain( int argc, char* argv[] )
    {
    KLocale::setMainCatalogue( "kcmkwinrules" );
    KCmdLineArgs::init( argc, argv, "kwin_rules_dialog", I18N_NOOP( "KWin" ),
	I18N_NOOP( "KWin helper utility" ), "1.0" );
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    bool id_ok = false;
    Window id = args->getOption( "wid" ).toULong( &id_ok );
    args->clear();
    if( !id_ok || id == None )
        {
	KCmdLineArgs::usage( i18n( "This helper utility is not supposed to be called directly." ));
	return 1;
        }
    return KWinInternal::edit( id );
    }
