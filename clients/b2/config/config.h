/********************************************************************
 	This file contains the B2 configuration widget
 
 	Copyright (c) 2001
 		Karol Szwed <gallium@kde.org>
 		http://gallium.n3.net/
 	Copyright (c) 2007
 		Luciano Montanaro <mikelima@cirulla.net>
 	Automove titlebar bits Copyright (c) 2009 Jussi Kekkonen <tmt@ubuntu.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef _KDE_B2CONFIG_H
#define _KDE_B2CONFIG_H

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <KComboBox>
#include <kconfig.h>

class B2Config: public QObject
{
    Q_OBJECT

public:
    B2Config(KConfig *conf, QWidget *parent);
    ~B2Config();

    // These public signals/slots work similar to KCM modules
signals:
    void changed();

public slots:
    void load(const KConfigGroup &conf);	
    void save(KConfigGroup &conf);
    void defaults();

protected slots:
    void slotSelectionChanged(); // Internal use

private:
    KConfig *b2Config;
    QCheckBox *cbColorBorder;
    QCheckBox *showGrabHandleCb;
    QCheckBox *autoMoveTitlebarCb;
    QGroupBox *actionsGB;
    KComboBox *menuDblClickOp;
    QWidget *gb;
};

#endif

// vi: sw=4 ts=8
