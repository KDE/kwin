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


#ifndef __RULESWIDGET_H__
#define __RULESWIDGET_H__

#include <kdialogbase.h>
#include <kwin.h>

#include "ruleswidgetbase.h"

namespace KWinInternal
{

class Rules;
class DetectDialog;

class RulesWidget
    : public RulesWidgetBase
    {
    Q_OBJECT
    public:
        RulesWidget( QWidget* parent = NULL, const char* name = NULL );
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
        // workarounds tab
        void updateEnablefsplevel();
        void updateEnablemoveresizemode();
        void updateEnabletype();
        void updateEnableignoreposition();
        void updateEnableminsize();
        void updateEnablemaxsize();
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
    : public KDialogBase
    {
    Q_OBJECT
    public:
        RulesDialog( QWidget* parent = NULL, const char* name = NULL );
        Rules* edit( Rules* r, WId window );
    protected:
        virtual void accept();
    private:
        RulesWidget* widget;
        Rules* rules;
    };

} // namespace

#endif
