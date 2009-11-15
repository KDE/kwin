/********************************************************************
Tabstrip KWin window decoration
This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef TABSTRIPCONFIG_H
#define TABSTRIPCONFIG_H

#include "ui_tabstripconfig.h"

#include <KConfig>
#include <QObject>

class TabstripConfigDialog : public QWidget, public Ui::TabstripConfigUi
    {
    public:
        TabstripConfigDialog( QWidget *parent ) : QWidget( parent )
            {
            setupUi( this );
            }
    };

class TabstripConfig : public QObject
    {
    Q_OBJECT
    public:
        TabstripConfig( KConfig *c, QWidget *parent );
        ~TabstripConfig();
    signals:
        void changed();
    public slots:
        void load( KConfigGroup &c );
        void save( KConfigGroup &c );
        void defaults();
    private:
        KConfig *config;
        TabstripConfigDialog *ui;
    };

#endif
