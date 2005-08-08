/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2004 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_RULES_H
#define KWIN_RULES_H

#include <qstring.h>
#include <netwm_def.h>
#include <qrect.h>
#include <kdebug.h>

#include "placement.h"
#include "lib/kdecoration.h"
#include "options.h"
#include "utils.h"

class KConfig;

namespace KWinInternal
{

class Client;
class Rules;

#ifndef KCMRULES // only for kwin core

class WindowRules
    : public KDecorationDefines
    {
    public:
        WindowRules( const QVector< Rules* >& rules );
        WindowRules();
        void update( Client* );
        void discardTemporary();
        bool contains( const Rules* rule ) const;
        void remove( Rules* rule );
        Placement::Policy checkPlacement( Placement::Policy placement ) const;
        QRect checkGeometry( QRect rect, bool init = false ) const;
        // use 'invalidPoint' with checkPosition, unlike QSize() and QRect(), QPoint() is a valid point
        QPoint checkPosition( QPoint pos, bool init = false ) const;
        QSize checkSize( QSize s, bool init = false ) const;
        QSize checkMinSize( QSize s ) const;
        QSize checkMaxSize( QSize s ) const;
        int checkOpacityActive(int s) const;
        int checkOpacityInactive(int s) const;
        bool checkIgnoreGeometry( bool ignore ) const;
        int checkDesktop( int desktop, bool init = false ) const;
        NET::WindowType checkType( NET::WindowType type ) const;
        MaximizeMode checkMaximize( MaximizeMode mode, bool init = false ) const;
        bool checkMinimize( bool minimized, bool init = false ) const;
        ShadeMode checkShade( ShadeMode shade, bool init = false ) const;
        bool checkSkipTaskbar( bool skip, bool init = false ) const;
        bool checkSkipPager( bool skip, bool init = false ) const;
        bool checkKeepAbove( bool above, bool init = false ) const;
        bool checkKeepBelow( bool below, bool init = false ) const;
        bool checkFullScreen( bool fs, bool init = false ) const;
        bool checkNoBorder( bool noborder, bool init = false ) const;
        int checkFSP( int fsp ) const;
        bool checkAcceptFocus( bool focus ) const;
        Options::MoveResizeMode checkMoveResizeMode( Options::MoveResizeMode mode ) const;
        bool checkCloseable( bool closeable ) const;
        bool checkStrictGeometry( bool strict ) const;
        QString checkShortcut( QString s, bool init = false ) const;
        bool checkDisableGlobalShortcuts( bool disable ) const;
        bool checkIgnorePosition( bool ignore ) const; // obsolete
    private:
        MaximizeMode checkMaximizeVert( MaximizeMode mode, bool init ) const;
        MaximizeMode checkMaximizeHoriz( MaximizeMode mode, bool init ) const;
        QVector< Rules* > rules;
    };
#endif

class Rules
    : public KDecorationDefines
    {
    public:
        Rules();
        Rules( KConfig& );
        Rules( const QString&, bool temporary );
        void write( KConfig& ) const;
        bool isEmpty() const;
#ifndef KCMRULES
        void discardUsed( bool withdrawn );
        bool match( const Client* c ) const;
        bool update( Client* );
        bool isTemporary() const;
        bool discardTemporary( bool force ); // removes if temporary and forced or too old
        bool applyPlacement( Placement::Policy& placement ) const;
        bool applyGeometry( QRect& rect, bool init ) const;
        // use 'invalidPoint' with applyPosition, unlike QSize() and QRect(), QPoint() is a valid point
        bool applyPosition( QPoint& pos, bool init ) const;
        bool applySize( QSize& s, bool init ) const;
        bool applyMinSize( QSize& s ) const;
        bool applyMaxSize( QSize& s ) const;
        bool applyOpacityActive(int& s) const;
        bool applyOpacityInactive(int& s) const;
        bool applyIgnoreGeometry( bool& ignore ) const;
        bool applyDesktop( int& desktop, bool init ) const;
        bool applyType( NET::WindowType& type ) const;
        bool applyMaximizeVert( MaximizeMode& mode, bool init ) const;
        bool applyMaximizeHoriz( MaximizeMode& mode, bool init ) const;
        bool applyMinimize( bool& minimized, bool init ) const;
        bool applyShade( ShadeMode& shade, bool init ) const;
        bool applySkipTaskbar( bool& skip, bool init ) const;
        bool applySkipPager( bool& skip, bool init ) const;
        bool applyKeepAbove( bool& above, bool init ) const;
        bool applyKeepBelow( bool& below, bool init ) const;
        bool applyFullScreen( bool& fs, bool init ) const;
        bool applyNoBorder( bool& noborder, bool init ) const;
        bool applyFSP( int& fsp ) const;
        bool applyAcceptFocus( bool& focus ) const;
        bool applyMoveResizeMode( Options::MoveResizeMode& mode ) const;
        bool applyCloseable( bool& closeable ) const;
        bool applyStrictGeometry( bool& strict ) const;
        bool applyShortcut( QString& shortcut, bool init ) const;
        bool applyDisableGlobalShortcuts( bool& disable ) const;
        bool applyIgnorePosition( bool& ignore ) const; // obsolete
    private:
#endif
        bool matchType( NET::WindowType match_type ) const;
        bool matchWMClass( const QByteArray& match_class, const QByteArray& match_name ) const;
        bool matchRole( const QByteArray& match_role ) const;
        bool matchTitle( const QString& match_title ) const;
        bool matchClientMachine( const QByteArray& match_machine ) const;
        // All these values are saved to the cfg file, and are also used in kstart!
        enum
            {
            Unused = 0,
            DontAffect, // use the default value
            Force,      // force the given value
            Apply,      // apply only after initial mapping
            Remember,   // like apply, and remember the value when the window is withdrawn
            ApplyNow,   // apply immediatelly, then forget the setting
            ForceTemporarily // apply and force until the window is withdrawn
            };
        enum SetRule
            {
            UnusedSetRule = Unused,
            SetRuleDummy = 256   // so that it's at least short int
            };
        enum ForceRule
            {
            UnusedForceRule = Unused,
            ForceRuleDummy = 256   // so that it's at least short int
            };
        enum StringMatch
            {
            FirstStringMatch,
            UnimportantMatch = FirstStringMatch,
            ExactMatch,
            SubstringMatch,
            RegExpMatch,
            LastStringMatch = RegExpMatch
            };
        void readFromCfg( KConfig& cfg );
        static SetRule readSetRule( KConfig&, const QString& key );
        static ForceRule readForceRule( KConfig&, const QString& key );
        static NET::WindowType readType( KConfig&, const QString& key );
#ifndef KCMRULES
        static bool checkSetRule( SetRule rule, bool init );
        static bool checkForceRule( ForceRule rule );
        static bool checkSetStop( SetRule rule );
        static bool checkForceStop( ForceRule rule );
#endif
        int temporary_state; // e.g. for kstart
        QString description;
        QByteArray wmclass;
        StringMatch wmclassmatch;
        bool wmclasscomplete;
        QByteArray windowrole;
        StringMatch windowrolematch;
        QString title; // TODO "caption" ?
        StringMatch titlematch;
        QByteArray extrarole;
        StringMatch extrarolematch;
        QByteArray clientmachine;
        StringMatch clientmachinematch;
        unsigned long types; // types for matching
        Placement::Policy placement;
        ForceRule placementrule;
        QPoint position;
        SetRule positionrule;
        QSize size;
        SetRule sizerule;
        QSize minsize;
        ForceRule minsizerule;
        QSize maxsize;
        ForceRule maxsizerule;
        int opacityactive;
        ForceRule opacityactiverule;
        int opacityinactive;
        ForceRule opacityinactiverule;
        bool ignoreposition;
        ForceRule ignorepositionrule;
        int desktop;
        SetRule desktoprule;
        NET::WindowType type; // type for setting
        ForceRule typerule;
        bool maximizevert;
        SetRule maximizevertrule;
        bool maximizehoriz;
        SetRule maximizehorizrule;
        bool minimize;
        SetRule minimizerule;
        bool shade;
        SetRule shaderule;
        bool skiptaskbar;
        SetRule skiptaskbarrule;
        bool skippager;
        SetRule skippagerrule;
        bool above;
        SetRule aboverule;
        bool below;
        SetRule belowrule;
        bool fullscreen;
        SetRule fullscreenrule;
        bool noborder;
        SetRule noborderrule;
        int fsplevel;
        ForceRule fsplevelrule;
        bool acceptfocus;
        ForceRule acceptfocusrule;
        Options::MoveResizeMode moveresizemode;
        ForceRule moveresizemoderule;
        bool closeable;
        ForceRule closeablerule;
        bool strictgeometry;
        ForceRule strictgeometryrule;
        QString shortcut;
        SetRule shortcutrule;
        bool disableglobalshortcuts;
        ForceRule disableglobalshortcutsrule;
        friend kdbgstream& operator<<( kdbgstream& stream, const Rules* );
    };

#ifndef KCMRULES
inline
bool Rules::checkSetRule( SetRule rule, bool init )
    {
    if( rule > ( SetRule )DontAffect) // Unused or DontAffect
        {
        if( rule == ( SetRule )Force || rule == ( SetRule ) ApplyNow
            || rule == ( SetRule ) ForceTemporarily || init )
            return true;
        }
    return false;
    }

inline
bool Rules::checkForceRule( ForceRule rule )
    {
    return rule == ( ForceRule )Force || rule == ( ForceRule ) ForceTemporarily;
    }

inline
bool Rules::checkSetStop( SetRule rule )
    {
    return rule != UnusedSetRule;
    }
    
inline
bool Rules::checkForceStop( ForceRule rule )
    {
    return rule != UnusedForceRule;
    }

inline
WindowRules::WindowRules( const QVector< Rules* >& r )
    : rules( r )
    {
    }

inline
WindowRules::WindowRules()
    {
    }

inline
bool WindowRules::contains( const Rules* rule ) const
    {
    return qFind( rules.begin(), rules.end(), rule ) != rules.end();
    }
    
inline
void WindowRules::remove( Rules* rule )
    {
    QVector< Rules* >::Iterator pos = qFind( rules.begin(), rules.end(), rule );
    if( pos != rules.end())
        rules.erase( pos );
    }

#endif

#ifdef NDEBUG
inline
kndbgstream& operator<<( kndbgstream& stream, const Rules* ) { return stream; }
#else
kdbgstream& operator<<( kdbgstream& stream, const Rules* );
#endif

} // namespace

#endif
