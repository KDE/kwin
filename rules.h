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

class KConfig;

namespace KWinInternal
{

class Client;

enum SettingRule
    {
    DontCareRule,
    ForceRule,
    ApplyRule,
    RememberRule,
    LastRule = RememberRule
    };

class WindowRules
    {
    public:
        WindowRules();
        WindowRules( KConfig& );
        void write( KConfig& ) const;
        void update( Client* );
        bool match( const Client* c ) const;
        int checkDesktop( int desktop, bool init = false ) const;
        bool checkKeepAbove( bool above, bool init = false ) const;
        bool checkKeepBelow( bool above, bool init = false ) const;
        NET::WindowType checkType( NET::WindowType type ) const;
    private:
        static SettingRule readRule( KConfig&, const QString& key );
        static SettingRule readForceRule( KConfig&, const QString& key );
        static NET::WindowType readType( KConfig&, const QString& key );
        static bool checkRule( SettingRule rule, bool init );
        static bool checkForceRule( SettingRule rule );
        QCString wmclass;
        bool wmclassregexp;
        bool wmclasscomplete;
        QCString windowrole;
        bool windowroleregexp;
        QString title; // TODO "caption" ?
        bool titleregexp;
        QCString extrarole;
        bool extraroleregexp;
        QCString clientmachine;
        bool clientmachineregexp;
        int desktop;
        SettingRule desktoprule;
        NET::WindowType type;
        SettingRule typerule;
        bool above;
        SettingRule aboverule;
        bool below;
        SettingRule belowrule;
    };

inline
bool WindowRules::checkRule( SettingRule rule, bool init )
    {
    if( rule != DontCareRule )
        {
        if( rule == ForceRule || init )
            return true;
        }
    return false;
    }

inline
bool WindowRules::checkForceRule( SettingRule rule )
    {
    return rule == ForceRule;
    }

} // namespace

#endif
