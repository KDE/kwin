/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2004 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "rules.h"

#include <kconfig.h>
#include <qregexp.h>

#include "client.h"
#include "workspace.h"

namespace KWinInternal
{

static WindowRules dummyRules; // dummy used to avoid NULL checks

WindowRules::WindowRules()
    : wmclassregexp( false )
    , windowroleregexp( false )
    , titleregexp( false )
    , extraroleregexp( false )
    , desktoprule( DontCareRule )
    , aboverule( DontCareRule )
    , belowrule( DontCareRule )
    {
    }
    
WindowRules::WindowRules( KConfig& cfg )
    : wmclassregexp( false )
    , windowroleregexp( false )
    , titleregexp( false )
    , extraroleregexp( false )
    , desktoprule( DontCareRule )
    , aboverule( DontCareRule )
    , belowrule( DontCareRule )
    {
    wmclass = cfg.readEntry( "wmclass" ).lower().latin1();
    wmclassregexp = cfg.readBoolEntry( "wmclassregexp" );
    windowrole = cfg.readEntry( "windowrole" ).lower().latin1();
    windowroleregexp = cfg.readBoolEntry( "windowroleregexp" );
    title = cfg.readEntry( "title" );
    titleregexp = cfg.readBoolEntry( "titleregexp" );
    extrarole = cfg.readEntry( "extrarole" ).lower().latin1();
    extraroleregexp = cfg.readBoolEntry( "extraroleregexp" );
    desktop = cfg.readNumEntry( "desktop" );
    desktoprule = readRule( cfg, "desktoprule" );
    above = cfg.readBoolEntry( "above" );
    aboverule = readRule( cfg, "aboverule" );
    below = cfg.readBoolEntry( "below" );
    belowrule = readRule( cfg, "belowrule" );
    kdDebug() << "READ RULE:" << wmclass << endl;
    }

#define WRITE_MATCH_STRING( var, cast ) \
    if( !var.isEmpty()) \
        { \
        cfg.writeEntry( #var, cast var ); \
        cfg.writeEntry( #var "regexp", var##regexp ); \
        } \
    else \
        { \
        cfg.deleteEntry( #var ); \
        cfg.deleteEntry( #var "regexp" ); \
        }

#define WRITE_SET_RULE( var ) \
    if( var##rule != DontCareRule ) \
        { \
        cfg.writeEntry( #var, var ); \
        cfg.writeEntry( #var "rule", var##rule ); \
        } \
    else \
        { \
        cfg.deleteEntry( #var ); \
        cfg.deleteEntry( #var "rule" ); \
        }

void WindowRules::write( KConfig& cfg ) const
    {
    // always write wmclass
    cfg.writeEntry( "wmclass", ( const char* )wmclass );
    cfg.writeEntry( "wmclassregexp", wmclassregexp );
    WRITE_MATCH_STRING( windowrole, (const char*) );
    WRITE_MATCH_STRING( title, );
    WRITE_MATCH_STRING( extrarole, (const char*) );
    WRITE_SET_RULE( desktop );
    WRITE_SET_RULE( above );
    WRITE_SET_RULE( below );
    }
    
#undef WRITE_MATCH_STRING
#undef WRITE_SET_RULE

SettingRule WindowRules::readRule( KConfig& cfg, const QString& key )
    {
    int v = cfg.readNumEntry( key );
    if( v >= DontCareRule && v <= LastRule )
        return static_cast< SettingRule >( v );
    return DontCareRule;
    }

void WindowRules::update( Client* c )
    {
    // TODO check this setting is for this client ?
    if( desktoprule == RememberRule )
        desktop = c->desktop();
    if( aboverule == RememberRule )
        above = c->keepAbove();
    if( belowrule == RememberRule )
        below = c->keepBelow();
    }

bool WindowRules::match( const Client* c ) const
    {
    // TODO exactMatch() for regexp?
    if( !wmclass.isEmpty())
        { // TODO optimize?
        if( wmclassregexp && !QRegExp( wmclass ).exactMatch( c->resourceClass()))
            return false;
        if( !wmclassregexp && wmclass != c->resourceClass())
            return false;
        }
    if( !windowrole.isEmpty())
        {
        if( windowroleregexp && QRegExp( windowrole ).exactMatch( c->windowRole()))
            return false;
        if( !windowroleregexp && windowrole != c->windowRole())
            return false;
        }
    if( !title.isEmpty())
        {
        if( titleregexp && QRegExp( title ).exactMatch( c->caption( false )))
            return false;
        if( !titleregexp && title != c->caption( false ))
            return false;
        }
    // TODO extrarole
    return true;
    }

int WindowRules::checkDesktop( int req_desktop, bool init ) const
    {
    // TODO chaining?
    return checkRule( desktoprule, init ) ? desktop : req_desktop;
    }

bool WindowRules::checkKeepAbove( bool req_above, bool init ) const
    {
    return checkRule( aboverule, init ) ? above : req_above;
    }

bool WindowRules::checkKeepBelow( bool req_below, bool init ) const
    {
    return checkRule( belowrule, init ) ? below : req_below;
    }

// Client

void Client::initWindowRules()
    {
    client_rules = workspace()->findWindowRules( this );
    }

void Client::updateWindowRules()
    {
    client_rules->update( this );
    }

// Workspace

WindowRules* Workspace::findWindowRules( const Client* c ) const
    {
    if( c->isTopMenu()) // TODO cannot have restrictions
        return &dummyRules;
    for( QValueList< WindowRules* >::ConstIterator it = windowRules.begin();
         it != windowRules.end();
         ++it )
        {
        // chaining for wildcard matches
        if( (*it)->match( c ))
            {
            kdDebug() << "RULE FOUND:" << c << endl;
            return *it;
            }
        }
    kdDebug() << "RULE DUMMY:" << c << endl;
    return &dummyRules;
    }

void Workspace::editWindowRules( Client* c )
    {
    // TODO
    }

void Workspace::loadWindowRules()
    {
    while( !windowRules.isEmpty())
        {
        delete windowRules.front();
        windowRules.pop_front();
        }
    KConfig cfg( "kwinrulesrc", true );
    cfg.setGroup( "General" );
    int count = cfg.readNumEntry( "count" );
    kdDebug() << "RULES:" << count << endl;
    for( int i = 1;
         i <= count;
         ++i )
        {
        cfg.setGroup( QString::number( i ));
        WindowRules* setting = new WindowRules( cfg );
        windowRules.append( setting );
        }
    }

void Workspace::writeWindowRules()
    {
    KConfig cfg( "kwinrulesrc" );
    cfg.setGroup( "General" );
    cfg.writeEntry( "count", windowRules.count());
    int i = 1;
    for( QValueList< WindowRules* >::ConstIterator it = windowRules.begin();
         it != windowRules.end();
         ++it, ++i )
        {
        cfg.setGroup( QString::number( i ));
        (*it)->write( cfg );
        }
    }

} // namespace
