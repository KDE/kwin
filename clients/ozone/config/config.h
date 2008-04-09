/*
 * Oxygen KWin client configuration module
 *
 * Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>
 *
 * Based on the Quartz configuration module,
 *     Copyright (c) 2001 Karol Szwed <gallium@kde.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef OXYGEN_CONFIG_H
#define OXYGEN_CONFIG_H

#include <kconfig.h>

#include "ui_oxygenconfig.h"

namespace Oxygen {

class OxygenConfigUI : public QWidget, public Ui::OxygenConfigUI
{
public:
    OxygenConfigUI( QWidget *parent ) : QWidget( parent )
    {
        setupUi( this );
    }
};


class OxygenConfig: public QObject
{
    Q_OBJECT
public:
    OxygenConfig( KConfig* conf, QWidget* parent );
    ~OxygenConfig();
// These public signals/slots work similar to KCM modules
signals:
    void changed();
public slots:
    void load( const KConfigGroup& conf );	
    void save( KConfigGroup& conf );
    void defaults();
private:
    OxygenConfigUI *ui;
    KConfig *c;
};

} //namespace Oxygen

#endif
