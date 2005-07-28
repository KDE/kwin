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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Cambridge, MA 02110-1301, USA.
 */


#ifndef __DETECTWIDGET_H__
#define __DETECTWIDGET_H__

#include "detectwidgetbase.h"

#include <kdialogbase.h>
#include <kwin.h>
#include <q3cstring.h>

#include "../../rules.h"
//Added by qt3to4:
#include <QEvent>
#include <Q3CString>

namespace KWinInternal
{

class DetectWidget
    : public DetectWidgetBase
    {
    Q_OBJECT
    public:
        DetectWidget( QWidget* parent = NULL, const char* name = NULL );
    };

class DetectDialog
    : public KDialogBase
    {
    Q_OBJECT
    public:
        DetectDialog( QWidget* parent = NULL, const char* name = NULL );
        void detect( WId window );
        Q3CString selectedClass() const;
        bool selectedWholeClass() const;
        Q3CString selectedRole() const;
        bool selectedWholeApp() const;
        NET::WindowType selectedType() const;
        QString selectedTitle() const;
        Rules::StringMatch titleMatch() const;
        Q3CString selectedMachine() const;
        const KWin::WindowInfo& windowInfo() const;
    signals:
        void detectionDone( bool );
    protected:
        virtual bool eventFilter( QObject* o, QEvent* e );
    private:
        void selectWindow();
        void readWindow( WId window );
        void executeDialog();
        WId findWindow();
        Q3CString wmclass_class;
        Q3CString wmclass_name;
        Q3CString role;
        NET::WindowType type;
        QString title;
        Q3CString extrarole;
        Q3CString machine;
        DetectWidget* widget;
        QDialog* grabber;
        KWin::WindowInfo info;
    };

inline
const KWin::WindowInfo& DetectDialog::windowInfo() const
    {
    return info;
    }

} // namespace

#endif
