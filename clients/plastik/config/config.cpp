/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qwhatsthis.h>

#include <kconfig.h>
#include <klocale.h>
#include <kglobal.h>

#include "config.h"
#include "configdialog.h"

PlastikConfig::PlastikConfig(KConfig* config, QWidget* parent)
    : QObject(parent), m_config(0), m_dialog(0)
{
    // create the configuration object
    m_config = new KConfig("kwinplastikrc");
    KGlobal::locale()->insertCatalogue("kwin_plastik_config");

    // create and show the configuration dialog
    m_dialog = new ConfigDialog(parent);
    m_dialog->show();

    // load the configuration
    load(config);

    // setup the connections
    connect(m_dialog->titleAlign, SIGNAL(clicked(int)),
            this, SIGNAL(changed()));
    connect(m_dialog->animateButtons, SIGNAL(toggled(bool)),
            this, SIGNAL(changed()));
    connect(m_dialog->titleShadow, SIGNAL(toggled(bool)),
            this, SIGNAL(changed()));
    connect(m_dialog->titlebarHeight, SIGNAL(valueChanged(int)),
            this, SIGNAL(changed()));

}

PlastikConfig::~PlastikConfig()
{
    if (m_dialog) delete m_dialog;
    if (m_config) delete m_config;
}

void PlastikConfig::load(KConfig*)
{
    m_config->setGroup("General");


    QString value = m_config->readEntry("TitleAlignment", "AlignHCenter");
    QRadioButton *button = (QRadioButton*)m_dialog->titleAlign->child(value);
    if (button) button->setChecked(true);
    bool animateButtons = m_config->readBoolEntry("AnimateButtons", true);
    m_dialog->animateButtons->setChecked(animateButtons);
    bool titleShadow = m_config->readBoolEntry("TitleShadow", true);
    m_dialog->titleShadow->setChecked(titleShadow);
    int titlebarHeight = m_config->readNumEntry("TitleHeightMin", 19);
    if(titlebarHeight <= 16)
    {
        m_dialog->titlebarHeight->setValue(1);
    }
    else if(titlebarHeight <= 19)
    {
        m_dialog->titlebarHeight->setValue(2);
    }
    else if(titlebarHeight <= 23)
    {
        m_dialog->titlebarHeight->setValue(3);
    }
    else if(titlebarHeight <= 27)
    {
        m_dialog->titlebarHeight->setValue(4);
    }
    else if(titlebarHeight <= 31)
    {
        m_dialog->titlebarHeight->setValue(5);
    }
    else
    {
        m_dialog->titlebarHeight->setValue(6);
    }
}

void PlastikConfig::save(KConfig*)
{
    m_config->setGroup("General");

    QRadioButton *button = (QRadioButton*)m_dialog->titleAlign->selected();
    if (button) m_config->writeEntry("TitleAlignment", QString(button->name()));
    m_config->writeEntry("AnimateButtons", m_dialog->animateButtons->isChecked() );
    m_config->writeEntry("TitleShadow", m_dialog->titleShadow->isChecked() );
    switch(m_dialog->titlebarHeight->value())
    {
        case 2:
            m_config->writeEntry("TitleHeightMin", 19 );
            break;
        case 3:
            m_config->writeEntry("TitleHeightMin", 23 );
            break;
        case 4:
            m_config->writeEntry("TitleHeightMin", 27 );
            break;
        case 5:
            m_config->writeEntry("TitleHeightMin", 31 );
            break;
        case 6:
            m_config->writeEntry("TitleHeightMin", 35 );
            break;
        case 1:
        default:
            m_config->writeEntry("TitleHeightMin", 16 );
    }
    m_config->sync();
}

void PlastikConfig::defaults()
{
    QRadioButton *button =
        (QRadioButton*)m_dialog->titleAlign->child("AlignHCenter");
    if (button) button->setChecked(true);
    m_dialog->animateButtons->setChecked(true);
    m_dialog->titleShadow->setChecked(true);
    m_dialog->titlebarHeight->setValue(2);
}

//////////////////////////////////////////////////////////////////////////////
// Plugin Stuff                                                             //
//////////////////////////////////////////////////////////////////////////////

extern "C"
{
    QObject* allocate_config(KConfig* config, QWidget* parent) {
        return (new PlastikConfig(config, parent));
    }
}

#include "config.moc"
