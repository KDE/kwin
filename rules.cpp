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
    , wmclasscomplete( false )
    , windowroleregexp( false )
    , titleregexp( false )
    , extraroleregexp( false )
    , clientmachineregexp( false )
    , types( NET::AllTypesMask )
    , placementrule( DontCareRule )
    , positionrule( DontCareRule )
    , sizerule( DontCareRule )
    , minsizerule( DontCareRule )
    , maxsizerule( DontCareRule )
    , desktoprule( DontCareRule )
    , typerule( DontCareRule )
    , maximizevertrule( DontCareRule )
    , maximizehorizrule( DontCareRule )
    , minimizerule( DontCareRule )
    , shaderule( DontCareRule )
    , skiptaskbarrule( DontCareRule )
    , skippagerrule( DontCareRule )
    , aboverule( DontCareRule )
    , belowrule( DontCareRule )
    , fullscreenrule( DontCareRule )
    , noborderrule( DontCareRule )
    {
    }

#define READ_MATCH_STRING( var, func ) \
    var = cfg.readEntry( #var ) func; \
    var##regexp = cfg.readBoolEntry( #var "regexp" );
    
#define READ_SET_RULE( var, type, func ) \
    var = func ( cfg.read##type##Entry( #var )); \
    var##rule = readRule( cfg, #var "rule" );
    
#define READ_SET_RULE_2( var, type, func, funcarg ) \
    var = func ( cfg.read##type##Entry( #var ), funcarg ); \
    var##rule = readRule( cfg, #var "rule" );
    
WindowRules::WindowRules( KConfig& cfg )
    {
    wmclass = cfg.readEntry( "wmclass" ).lower().latin1();
    wmclassregexp = cfg.readBoolEntry( "wmclassregexp" );
    wmclasscomplete = cfg.readBoolEntry( "wmclasscomplete" );
    READ_MATCH_STRING( windowrole, .lower().latin1() );
    READ_MATCH_STRING( title, );
    READ_MATCH_STRING( extrarole, .lower().latin1() );
    READ_MATCH_STRING( clientmachine, .lower().latin1() );
    types = cfg.readUnsignedLongNumEntry( "types", NET::AllTypesMask );
    READ_SET_RULE_2( placement,, Placement::policyFromString, false );
    READ_SET_RULE( position, Point, );
    READ_SET_RULE( size, Size, );
    if( size.isEmpty())
        sizerule = DontCareRule;
    READ_SET_RULE( minsize, Size, );
    if( !minsize.isValid())
        minsizerule = DontCareRule;
    READ_SET_RULE( maxsize, Size, );
    if( maxsize.isEmpty())
        maxsizerule = DontCareRule;
    READ_SET_RULE( desktop, Num, );
    type = readType( cfg, "type" );
    typerule = type != NET::Unknown ? readForceRule( cfg, "typerule" ) : DontCareRule;
    READ_SET_RULE( maximizevert, Bool, );
    READ_SET_RULE( maximizehoriz, Bool, );
    READ_SET_RULE( minimize, Bool, );
    READ_SET_RULE( shade, Bool, );
    READ_SET_RULE( skiptaskbar, Bool, );
    READ_SET_RULE( skippager, Bool, );
    READ_SET_RULE( above, Bool, );
    READ_SET_RULE( below, Bool, );
    READ_SET_RULE( fullscreen, Bool, );
    READ_SET_RULE( noborder, Bool, );
    kdDebug() << "READ RULE:" << wmclass << endl;
    }

#undef READ_MATCH_STRING
#undef READ_SET_RULE
#undef READ_SET_RULE_2

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

#define WRITE_SET_RULE( var, func ) \
    if( var##rule != DontCareRule ) \
        { \
        cfg.writeEntry( #var, func ( var )); \
        cfg.writeEntry( #var "rule", var##rule ); \
        } \
    else \
        { \
        cfg.deleteEntry( #var ); \
        cfg.deleteEntry( #var "rule" ); \
        }

#define WRITE_WITH_DEFAULT( var, default ) \
    if( var != default ) \
        cfg.writeEntry( #var, var ); \
    else \
        cfg.deleteEntry( #var );


void WindowRules::write( KConfig& cfg ) const
    {
    // always write wmclass
    cfg.writeEntry( "wmclass", ( const char* )wmclass );
    cfg.writeEntry( "wmclassregexp", wmclassregexp );
    cfg.writeEntry( "wmclasscomplete", wmclasscomplete );
    WRITE_MATCH_STRING( windowrole, (const char*) );
    WRITE_MATCH_STRING( title, );
    WRITE_MATCH_STRING( extrarole, (const char*) );
    WRITE_MATCH_STRING( clientmachine, (const char*) );
    WRITE_WITH_DEFAULT( types, NET::AllTypesMask );
    WRITE_SET_RULE( placement, Placement::policyToString );
    WRITE_SET_RULE( position, );
    WRITE_SET_RULE( size, );
    WRITE_SET_RULE( minsize, );
    WRITE_SET_RULE( maxsize, );
    WRITE_SET_RULE( desktop, );
    WRITE_SET_RULE( type, );
    WRITE_SET_RULE( maximizevert, );
    WRITE_SET_RULE( maximizehoriz, );
    WRITE_SET_RULE( minimize, );
    WRITE_SET_RULE( shade, );
    WRITE_SET_RULE( skiptaskbar, );
    WRITE_SET_RULE( skippager, );
    WRITE_SET_RULE( above, );
    WRITE_SET_RULE( below, );
    WRITE_SET_RULE( fullscreen, );
    WRITE_SET_RULE( noborder, );
    }
    
#undef WRITE_MATCH_STRING
#undef WRITE_SET_RULE
#undef WRITE_WITH_DEFAULT

SettingRule WindowRules::readRule( KConfig& cfg, const QString& key )
    {
    int v = cfg.readNumEntry( key );
    if( v >= DontCareRule && v <= LastRule )
        return static_cast< SettingRule >( v );
    return DontCareRule;
    }

SettingRule WindowRules::readForceRule( KConfig& cfg, const QString& key )
    {
    int v = cfg.readNumEntry( key );
    if( v == DontCareRule || v == ForceRule )
        return static_cast< SettingRule >( v );
    return DontCareRule;
    }

NET::WindowType WindowRules::readType( KConfig& cfg, const QString& key )
    {
    int v = cfg.readNumEntry( key );
    if( v >= NET::Normal && v <= NET::Splash )
        return static_cast< NET::WindowType >( v );
    return NET::Unknown;
    }

bool WindowRules::match( const Client* c ) const
    {
    if( types != NET::AllTypesMask )
        {
        if( !NET::typeMatchesMask( c->windowType( true ), types )) // direct
            return false;
        }
    // TODO exactMatch() for regexp?
    if( !wmclass.isEmpty())
        { // TODO optimize?
        QCString cwmclass = wmclasscomplete
            ? c->resourceClass() + ' ' + c->resourceName() : c->resourceClass();
        if( wmclassregexp && !QRegExp( wmclass ).exactMatch( cwmclass ))
            return false;
        if( !wmclassregexp && wmclass != cwmclass )
            return false;
        }
    if( !windowrole.isEmpty())
        {
        if( windowroleregexp && !QRegExp( windowrole ).exactMatch( c->windowRole()))
            return false;
        if( !windowroleregexp && windowrole != c->windowRole())
            return false;
        }
    if( !title.isEmpty())
        {
        if( titleregexp && !QRegExp( title ).exactMatch( c->caption( false )))
            return false;
        if( !titleregexp && title != c->caption( false ))
            return false;
        }
    // TODO extrarole
    if( !clientmachine.isEmpty())
        {
        if( clientmachineregexp
            && !QRegExp( clientmachine ).exactMatch( c->wmClientMachine( true ))
            && !QRegExp( clientmachine ).exactMatch( c->wmClientMachine( false )))
            return false;
        if( !clientmachineregexp
            && clientmachine != c->wmClientMachine( true )
            && clientmachine != c->wmClientMachine( false ))
            return false;
        }
    return true;
    }

void WindowRules::update( Client* c )
    {
    // TODO check this setting is for this client ?
    if( positionrule == RememberRule )
        position = c->pos();
    if( sizerule == RememberRule )
        size = c->size();
    if( desktoprule == RememberRule )
        desktop = c->desktop();
    if( maximizevertrule == RememberRule )
        maximizevert = c->maximizeMode() & MaximizeVertical;
    if( maximizehorizrule == RememberRule )
        maximizehoriz = c->maximizeMode() & MaximizeHorizontal;
    if( minimizerule == RememberRule )
        minimize = c->isMinimized();
    if( shaderule == RememberRule )
        shade = c->shadeMode() != Client::ShadeNone;
    if( skiptaskbarrule == RememberRule )
        skiptaskbar = c->skipTaskbar();
    if( skippagerrule == RememberRule )
        skippager = c->skipPager();
    if( aboverule == RememberRule )
        above = c->keepAbove();
    if( belowrule == RememberRule )
        below = c->keepBelow();
    if( fullscreenrule == RememberRule )
        fullscreen = c->isFullScreen();
    if( noborderrule == RememberRule )
        noborder = c->isUserNoBorder();
    }

Placement::Policy WindowRules::checkPlacement( Placement::Policy placement ) const
    {
    return checkForceRule( placementrule ) ? this->placement : placement;
    }

// TODO at to porad jeste kontroluje min/max size , + udelat override pro min/max?
QRect WindowRules::checkGeometry( const QRect& rect, bool init ) const
    {
    return QRect( checkRule( positionrule, init ) ? this->position : rect.topLeft(),
        checkRule( sizerule, init ) ? this->size : rect.size());
    }

QPoint WindowRules::checkPosition( const QPoint& pos, bool init ) const
    {
    return checkRule( positionrule, init ) ? this->position : pos;
    }

QSize WindowRules::checkSize( const QSize& s, bool init ) const
    {
    return checkRule( sizerule, init ) ? this->size : s;
    }

QSize WindowRules::checkMinSize( const QSize& s ) const
    {
    return checkForceRule( minsizerule ) ? this->minsize : s;
    }

QSize WindowRules::checkMaxSize( const QSize& s ) const
    {
    return checkForceRule( maxsizerule ) ? this->maxsize : s;
    }

int WindowRules::checkDesktop( int req_desktop, bool init ) const
    {
    // TODO chaining?
    return checkRule( desktoprule, init ) ? this->desktop : req_desktop;
    }

NET::WindowType WindowRules::checkType( NET::WindowType req_type ) const
    {
    return checkForceRule( typerule ) ? this->type : req_type;
    }

KDecorationDefines::MaximizeMode WindowRules::checkMaximize( MaximizeMode mode, bool init ) const
    {
    bool vert = checkRule( maximizevertrule, init ) ? this->maximizevert : bool( mode & MaximizeVertical );
    bool horiz = checkRule( maximizehorizrule, init ) ? this->maximizehoriz : bool( mode & MaximizeHorizontal );
    return static_cast< MaximizeMode >(( vert ? MaximizeVertical : 0 ) | ( horiz ? MaximizeHorizontal : 0 ));
    }

bool WindowRules::checkMinimize( bool minimize, bool init ) const
    {
    return checkRule( minimizerule, init ) ? this->minimize : minimize;
    }

Client::ShadeMode WindowRules::checkShade( Client::ShadeMode shade, bool init ) const
    {
    if( checkRule( shaderule, init ))
        {
        if( !this->shade )
            return Client::ShadeNone;
        if( this->shade && shade == Client::ShadeNone )
            return Client::ShadeNormal;
        }
    return shade;
    }

bool WindowRules::checkSkipTaskbar( bool skip, bool init ) const
    {
    return checkRule( skiptaskbarrule, init ) ? this->skiptaskbar : skip;
    }

bool WindowRules::checkSkipPager( bool skip, bool init ) const
    {
    return checkRule( skippagerrule, init ) ? this->skippager : skip;
    }

bool WindowRules::checkKeepAbove( bool above, bool init ) const
    {
    return checkRule( aboverule, init ) ? this->above : above;
    }

bool WindowRules::checkKeepBelow( bool below, bool init ) const
    {
    return checkRule( belowrule, init ) ? this->below : below;
    }

bool WindowRules::checkFullScreen( bool fs, bool init ) const
    {
    return checkRule( fullscreenrule, init ) ? this->fullscreen : fs;
    }

bool WindowRules::checkNoBorder( bool noborder, bool init ) const
    {
    return checkRule( noborderrule, init ) ? this->noborder : noborder;
    }


// Client

void Client::setupWindowRules()
    {
    client_rules = workspace()->findWindowRules( this );
    // check only after getting the rules, because there may be a rule forcing window type
    if( isTopMenu()) // TODO cannot have restrictions
        client_rules = &dummyRules;
    }

void Client::updateWindowRules()
    {
    client_rules->update( this );
    }

void Client::finishWindowRules()
    {
    updateWindowRules();
    client_rules = &dummyRules;
    }

// Workspace

WindowRules* Workspace::findWindowRules( const Client* c ) const
    {
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
