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


#ifndef __DETECTWIDGET_H__
#define __DETECTWIDGET_H__

#include "ui_detectwidgetbase.h"

#include <kdialog.h>
#include <kwm.h>

#include "../../rules.h"
//Added by qt3to4:
#include <QEvent>
#include <QByteArray>

namespace KWin
{

class DetectWidgetBase : public QWidget, public Ui::DetectWidgetBase
{
public:
  DetectWidgetBase( QWidget *parent ) : QWidget( parent ) {
    setupUi( this );
  }
};


class DetectWidget
    : public DetectWidgetBase
    {
    Q_OBJECT
    public:
        DetectWidget( QWidget* parent = NULL );
    };

class DetectDialog
    : public KDialog
    {
    Q_OBJECT
    public:
        DetectDialog( QWidget* parent = NULL, const char* name = NULL );
        void detect( WId window );
        QByteArray selectedClass() const;
        bool selectedWholeClass() const;
        QByteArray selectedRole() const;
        bool selectedWholeApp() const;
        NET::WindowType selectedType() const;
        QString selectedTitle() const;
        Rules::StringMatch titleMatch() const;
        QByteArray selectedMachine() const;
        const KWM::WindowInfo& windowInfo() const;
    signals:
        void detectionDone( bool );
    protected:
        virtual bool eventFilter( QObject* o, QEvent* e );
    private:
        void selectWindow();
        void readWindow( WId window );
        void executeDialog();
        WId findWindow();
        QByteArray wmclass_class;
        QByteArray wmclass_name;
        QByteArray role;
        NET::WindowType type;
        QString title;
        QByteArray extrarole;
        QByteArray machine;
        DetectWidget* widget;
        QDialog* grabber;
        KWM::WindowInfo info;
    };

inline
const KWM::WindowInfo& DetectDialog::windowInfo() const
    {
    return info;
    }

} // namespace

#endif
