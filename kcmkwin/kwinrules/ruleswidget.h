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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef __RULESWIDGET_H__
#define __RULESWIDGET_H__

#include <kdialog.h>
#include <kwin.h>
//#include <kshortcutdialog.h>

#include "ruleswidgetbase.h"
#include "editshortcutbase.h"

namespace KWinInternal
{

class Rules;
class DetectDialog;

class RulesWidget
    : public RulesWidgetBase
    {
    Q_OBJECT
    public:
        RulesWidget( QWidget* parent = NULL );
        void setRules( Rules* r );
        Rules* rules() const;
        bool finalCheck();
        void prepareWindowSpecific( WId window );
    signals:
        void changed( bool state );
    protected slots:
        virtual void detectClicked();
        virtual void wmclassMatchChanged();
        virtual void roleMatchChanged();
        virtual void titleMatchChanged();
        virtual void extraMatchChanged();
        virtual void machineMatchChanged();
        virtual void shortcutEditClicked();
    private slots:
        // geometry tab
        void updateEnableposition();
        void updateEnablesize();
        void updateEnabledesktop();
        void updateEnablemaximizehoriz();
        void updateEnablemaximizevert();
        void updateEnableminimize();
        void updateEnableshade();
        void updateEnablefullscreen();
        void updateEnableplacement();
        // preferences tab
        void updateEnableabove();
        void updateEnablebelow();
        void updateEnablenoborder();
        void updateEnableskiptaskbar();
        void updateEnableskippager();
        void updateEnableacceptfocus();
        void updateEnablecloseable();
        void updateEnableopacityactive();
        void updateEnableopacityinactive();
        // workarounds tab
        void updateEnablefsplevel();
        void updateEnablemoveresizemode();
        void updateEnabletype();
        void updateEnableignoreposition();
        void updateEnableminsize();
        void updateEnablemaxsize();
        void updateEnablestrictgeometry();
        void updateEnableshortcut();
        void updateEnabledisableglobalshortcuts();
        // internal
        void detected( bool );
    private:
        int desktopToCombo( int d ) const;
        int comboToDesktop( int val ) const;
        void prefillUnusedValues( const KWin::WindowInfo& info );
        DetectDialog* detect_dlg;
        bool detect_dlg_ok;
    };

class RulesDialog
    : public KDialog
    {
    Q_OBJECT
    public:
        RulesDialog( QWidget* parent = NULL, const char* name = NULL );
        Rules* edit( Rules* r, WId window, bool show_hints );
    protected:
        virtual void accept();
    private slots:
        void displayHints();
    private:
        RulesWidget* widget;
        Rules* rules;
    };

#ifdef __GNUC__
#warning KShortcutDialog is gone
#endif //__GNUC__
#if 0
class EditShortcut
    : public EditShortcutBase
    {
    Q_OBJECT
    public:
        EditShortcut( QWidget* parent = NULL );
    protected:
        void editShortcut();
        void clearShortcut();
    };

class EditShortcutDialog
    : public KDialog
    {
    Q_OBJECT
    public:
        EditShortcutDialog( QWidget* parent = NULL, const char* name = NULL );
        void setShortcut( const QString& cut );
        QString shortcut() const;
    private:
        EditShortcut* widget;
    };

// slightly duped from utils.cpp
class ShortcutDialog
    : public KShortcutDialog
    {
    Q_OBJECT
    public:
        ShortcutDialog( const KShortcut& cut, QWidget* parent = NULL );
        virtual void accept();
    };
#endif //0
} // namespace

#endif
