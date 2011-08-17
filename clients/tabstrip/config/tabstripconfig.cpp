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

#include "tabstripconfig.h"

#include "tabstripconfig.moc"

#include <KConfig>
#include <KGlobal>
#include <KLocale>

#include <QString>
#include <QRadioButton>

extern "C"
    {
    KDE_EXPORT QObject* allocate_config( KConfig* conf, QWidget* parent )
        {
        return new TabstripConfig( conf, parent );
        }
    }

TabstripConfig::TabstripConfig( KConfig *c, QWidget *parent )
    {
    Q_UNUSED(c);
    KGlobal::locale()->insertCatalog( "kwin_tabstrip_config" );
    config = new KConfig( "tabstriprc" );
    KConfigGroup cg( config, "General" );
    ui = new TabstripConfigDialog( parent );
    connect( ui->left, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui->center, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui->right, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui->showIcon, SIGNAL(clicked()), SIGNAL(changed()) );
    load( cg );
    ui->show();
    }

TabstripConfig::~TabstripConfig()
    {
    delete ui;
    delete config;
    }

void TabstripConfig::load( KConfigGroup &c )
    {
    c = KConfigGroup( config, "General" );
    QString align = c.readEntry( "TitleAlignment", "Center" );
    ui->left->setChecked( align == "Left" );
    ui->center->setChecked( align == "Center" );
    ui->right->setChecked( align == "Right" );
    ui->showIcon->setChecked( c.readEntry( "ShowIcon", true ) );
    }

void TabstripConfig::save( KConfigGroup &c )
    {
    c = KConfigGroup( config, "General" );
    if( ui->left->isChecked() )
        c.writeEntry( "TitleAlignment", "Left" );
    else if( ui->center->isChecked() )
        c.writeEntry( "TitleAlignment", "Center" );
    else
        c.writeEntry( "TitleAlignment", "Right" );
    c.writeEntry( "ShowIcon", ui->showIcon->isChecked() );
    config->sync();
    }

void TabstripConfig::defaults()
    {
    ui->left->setChecked( false );
    ui->center->setChecked( true );
    ui->right->setChecked( false );
    ui->showIcon->setChecked( true );
    emit changed();
    }
