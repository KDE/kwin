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

class KConfig;

namespace KWinInternal
{

class Client;

enum SettingRule
    {
    DontCareRule,
    ApplyRule,
    ForceRule,
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
    private:
        static SettingRule readRule( KConfig&, const QString& key );
        static bool checkRule( SettingRule rule, bool init );
        QCString wmclass;
        bool wmclassregexp;
        // TODO bool wmclasscomplete - class+name
        QCString windowrole;
        bool windowroleregexp;
        QString title; // TODO "caption" ?
        bool titleregexp;
        QCString extrarole;
        bool extraroleregexp;
        // TODO window type? both to which it applies and to which value to force it
        int desktop;
        SettingRule desktoprule;
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

} // namespace

#endif
