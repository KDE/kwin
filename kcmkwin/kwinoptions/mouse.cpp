/*
 *
 * Copyright (c) 1998 Matthias Ettrich <ettrich@kde.org>
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

#include <qlabel.h>
#include <qcombobox.h>
#include <qwhatsthis.h>
#include <qlayout.h>
#include <qvgroupbox.h>
#include <qgrid.h>
#include <qsizepolicy.h>

#include <dcopclient.h>
#include <klocale.h>
#include <kconfig.h>
#include <kdialog.h>
#include <kglobalsettings.h>
#include <kseparator.h>
#include <kkeynative.h>	// For KKeyNative::keyboardHasWinKey()

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "mouse.h"
#include "mouse.moc"


KActionsConfig::~KActionsConfig ()
{

}

KActionsConfig::KActionsConfig (KConfig *_config, QWidget * parent, const char *name)
  : KCModule (parent, name), config(_config)
{
  QString strWin1, strWin2, strWin3, strAllKey, strAll1, strAll2, strAll3;
  QVBoxLayout *layout = new QVBoxLayout(this, KDialog::marginHint(), KDialog::spacingHint());
  QGrid *grid;
  QGroupBox *box;
  QLabel *label;
  QString strMouseButton1, strMouseButton3;
  QString txtButton1, txtButton3;
  QStringList items;
  bool leftHandedMouse = ( KGlobalSettings::mouseSettings().handed == KGlobalSettings::KMouseSettings::LeftHanded);

/** Titlebar doubleclick ************/

  QHBoxLayout *hlayout = new QHBoxLayout(layout);

  label = new QLabel(i18n("&Titlebar double-click:"), this);
  hlayout->addWidget(label);
  QWhatsThis::add( label, i18n("Here you can customize mouse click behavior when double clicking on the"
    " titlebar of a window.") );

  QComboBox* combo = new QComboBox(this);
  combo->insertItem(i18n("Maximize"));
  combo->insertItem(i18n("Maximize (vertical only)"));
  combo->insertItem(i18n("Maximize (horizontal only)"));
  combo->insertItem(i18n("Shade"));
  combo->insertItem(i18n("Lower"));
  combo->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  hlayout->addWidget(combo);
  coTiDbl = combo;
  QWhatsThis::add(combo, i18n("Behavior on <em>double</em> click into the titlebar."));

  label->setBuddy(combo);

/** Titlebar and frame  **************/

  box = new QVGroupBox( i18n("Titlebar and frame"), this, "Titlebar and frame");
  box->layout()->setMargin(KDialog::marginHint());
  box->layout()->setSpacing(KDialog::spacingHint());
  layout->addWidget(box);
  QWhatsThis::add( box, i18n("Here you can customize mouse click behavior when clicking on the"
                             " titlebar or the frame of a window.") );

  grid = new QGrid(4, Qt::Vertical, box);


  new QLabel(grid); // dummy

  strMouseButton1 = i18n("Left Button:");
  txtButton1 = i18n("In this row you can customize left click behavior when clicking into"
     " the titlebar or the frame.");

  strMouseButton3 = i18n("Right Button:");
  txtButton3 = i18n("In this row you can customize right click behavior when clicking into"
     " the titlebar or the frame." );

  if ( leftHandedMouse )
  {
     qSwap(strMouseButton1, strMouseButton3);
     qSwap(txtButton1, txtButton3);
  }

  label = new QLabel(strMouseButton1, grid);
  QWhatsThis::add( label, txtButton1);

  label = new QLabel(i18n("Middle Button:"), grid);
  QWhatsThis::add( label, i18n("In this row you can customize middle click behavior when clicking into"
    " the titlebar or the frame.") );

  label = new QLabel(strMouseButton3, grid);
  QWhatsThis::add( label, txtButton3);


  label = new QLabel(i18n("Active"), grid);
  label->setAlignment(AlignCenter);
  QWhatsThis::add( label, i18n("In this column you can customize mouse clicks into the titlebar"
                               " or the frame of an active window.") );

  // Titlebar and frame, active, mouse button 1
  combo = new QComboBox(grid);
  combo->insertItem(i18n("Raise"));
  combo->insertItem(i18n("Lower"));
  combo->insertItem(i18n("Operations menu"));
  combo->insertItem(i18n("Toggle raise and lower"));
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coTiAct1 = combo;

  txtButton1 = i18n("Behavior on <em>left</em> click into the titlebar or frame of an "
     "<em>active</em> window.");

  txtButton3 = i18n("Behavior on <em>right</em> click into the titlebar or frame of an "
     "<em>active</em> window.");

  // Be nice to left handed users
  if ( leftHandedMouse ) qSwap(txtButton1, txtButton3);

  QWhatsThis::add(combo, txtButton1);

  // Titlebar and frame, active, mouse button 2

  items << i18n("Raise")
        << i18n("Lower")
        << i18n("Operations menu")
        << i18n("Toggle raise and lower")
        << i18n("Nothing")
        << i18n("Shade");

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coTiAct2 = combo;
  QWhatsThis::add(combo, i18n("Behavior on <em>middle</em> click into the titlebar or frame of an <em>active</em> window."));

  // Titlebar and frame, active, mouse button 3
  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coTiAct3 =  combo;
  QWhatsThis::add(combo, txtButton3 );

  txtButton1 = i18n("Behavior on <em>left</em> click into the titlebar or frame of an "
     "<em>inactive</em> window.");

  txtButton3 = i18n("Behavior on <em>right</em> click into the titlebar or frame of an "
     "<em>inactive</em> window.");

  // Be nice to left handed users
  if ( leftHandedMouse ) qSwap(txtButton1, txtButton3);

  label = new QLabel(i18n("Inactive"), grid);
  label->setAlignment(AlignCenter);
  QWhatsThis::add( label, i18n("In this column you can customize mouse clicks into the titlebar"
                               " or the frame of an inactive window.") );

  items.clear();
  items  << i18n("Activate and raise")
         << i18n("Activate and lower")
         << i18n("Activate")
         << i18n("Shade");

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coTiInAct1 = combo;
  QWhatsThis::add(combo, txtButton1);

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coTiInAct2 = combo;
  QWhatsThis::add(combo, i18n("Behavior on <em>middle</em> click into the titlebar or frame of an <em>inactive</em> window."));

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coTiInAct3 = combo;
  QWhatsThis::add(combo, txtButton3);

/**  Inactive inner window ******************/

  box = new QVGroupBox(i18n("Inactive inner window"), this, "Inactive inner window");
  box->layout()->setMargin(KDialog::marginHint());
  box->layout()->setSpacing(KDialog::spacingHint());
  layout->addWidget(box);
  QWhatsThis::add( box, i18n("Here you can customize mouse click behavior when clicking on an inactive"
                             " inner window ('inner' means: not titlebar, not frame).") );

  grid = new QGrid(3, Qt::Vertical, box);

  strMouseButton1 = i18n("Left Button:");
  txtButton1 = i18n("In this row you can customize left click behavior when clicking into"
     " the titlebar or the frame.");

  strMouseButton3 = i18n("Right Button:");
  txtButton3 = i18n("In this row you can customize right click behavior when clicking into"
     " the titlebar or the frame." );

  if ( leftHandedMouse )
  {
     qSwap(strMouseButton1, strMouseButton3);
     qSwap(txtButton1, txtButton3);
  }

  strWin1 = i18n("In this row you can customize left click behavior when clicking into"
     " an inactive inner window ('inner' means: not titlebar, not frame).");

  strWin3 = i18n("In this row you can customize right click behavior when clicking into"
     " an inactive inner window ('inner' means: not titlebar, not frame).");

  // Be nice to lefties
  if ( leftHandedMouse ) qSwap(strWin1, strWin3);

  label = new QLabel(strMouseButton1, grid);
  QWhatsThis::add( label, strWin1 );

  label = new QLabel(i18n("Middle Button:"), grid);
  strWin2 = i18n("In this row you can customize middle click behavior when clicking into"
     " an inactive inner window ('inner' means: not titlebar, not frame).");
  QWhatsThis::add( label, strWin2 );

  label = new QLabel(strMouseButton3, grid);
  QWhatsThis::add( label, strWin3 );

  items.clear();
  items   << i18n("Activate, raise and pass click")
          << i18n("Activate and pass click")
          << i18n("Activate")
          << i18n("Activate and raise");

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coWin1 = combo;
  QWhatsThis::add( combo, strWin1 );

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coWin2 = combo;
  QWhatsThis::add( combo, strWin2 );

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coWin3 = combo;
  QWhatsThis::add( combo, strWin3 );


/** Inner window, titlebar and frame **************/

  box = new QVGroupBox(i18n("Inner window, titlebar and frame"), this, "Inner window, titlebar and frame");
  box->layout()->setMargin(KDialog::marginHint());
  box->layout()->setSpacing(KDialog::spacingHint());
  layout->addWidget(box);
  QWhatsThis::add( box, i18n("Here you can customize KDE's behavior when clicking somewhere into"
                             " a window while pressing a modifier key."));

  grid = new QGrid(4, Qt::Vertical, box);

  // Labels
  label = new QLabel(i18n("Modifier Key:"), grid);

  strAllKey = i18n("Here you select whether holding the Meta key or Alt key "
    "will allow you to perform the following actions.");
  QWhatsThis::add( label, strAllKey );

  label = new QLabel(i18n("Modifier Key + Middle Button:"), grid);
  strAll2 = i18n("Here you can customize KDE's behavior when middle clicking into a window"
                 " while pressing the modifier key.");
  QWhatsThis::add( label, strAll2 );


  strMouseButton1 = i18n("Modifier Key + Left Button:");
  strAll1 = i18n("In this row you can customize left click behavior when clicking into"
                 " the titlebar or the frame.");

  strMouseButton3 = i18n("Modifier Key + Right Button:");
  strAll3 = i18n("In this row you can customize right click behavior when clicking into"
                 " the titlebar or the frame." );

  if ( leftHandedMouse )
  {
     qSwap(strMouseButton1, strMouseButton3);
     qSwap(strAll1, strAll3);
  }

  label = new QLabel(strMouseButton1, grid);
  QWhatsThis::add( label, strAll1);

  label = new QLabel(strMouseButton3, grid);
  QWhatsThis::add( label, strAll3);

  // Combo's
  combo = new QComboBox(grid);
  combo->insertItem(i18n("Meta"));
  combo->insertItem(i18n("Alt"));
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coAllKey = combo;
  QWhatsThis::add( combo, strAllKey );

  items.clear();
  items << i18n("Move")
        << i18n("Toggle raise and lower")
        << i18n("Resize")
        << i18n("Raise")
        << i18n("Lower")
        << i18n("Nothing");

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coAll1 = combo;
  QWhatsThis::add( combo, strAll1 );

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coAll2 = combo;
  QWhatsThis::add( combo, strAll2 );

  combo = new QComboBox(grid);
  combo->insertStringList(items);
  connect(combo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  coAll3 =  combo;
  QWhatsThis::add( combo, strAll3 );

  layout->addStretch();

  load();
}

void KActionsConfig::setComboText(QComboBox* combo, const char* text){
  int i;
  QString s = i18n(text); // no problem. These are already translated!
  for (i=0;i<combo->count();i++){
    if (s==combo->text(i)){
      combo->setCurrentItem(i);
      return;
    }
  }
}

const char*  KActionsConfig::functionTiDbl(int i)
{
  switch (i){
  case 0: return "Maximize"; break;
  case 1: return "Maximize (vertical only)"; break;
  case 2: return "Maximize (horizontal only)"; break;
  case 3: return "Shade"; break;
  case 4: return "Lower"; break;
  }
  return "";
}

const char*  KActionsConfig::functionTiAc(int i)
{
  switch (i){
  case 0: return "Raise"; break;
  case 1: return "Lower"; break;
  case 2: return "Operations menu"; break;
  case 3: return "Toggle raise and lower"; break;
  case 4: return "Nothing"; break;
  case 5: return "Shade"; break;
  }
  return "";
}
const char*  KActionsConfig::functionTiInAc(int i)
{
  switch (i){
  case 0: return "Activate and raise"; break;
  case 1: return "Activate and lower"; break;
  case 2: return "Activate"; break;
  case 3: return "Shade"; break;
  case 4: return ""; break;
  case 5: return ""; break;
  }
  return "";
}
const char*  KActionsConfig::functionWin(int i)
{
  switch (i){
  case 0: return "Activate, raise and pass click"; break;
  case 1: return "Activate and pass click"; break;
  case 2: return "Activate"; break;
  case 3: return "Activate and raise"; break;
  case 4: return ""; break;
  case 5: return ""; break;
  }
  return "";
}
const char*  KActionsConfig::functionAllKey(int i)
{
  switch (i){
  case 0: return "Meta"; break;
  case 1: return "Alt"; break;
  }
  return "";
}
const char*  KActionsConfig::functionAll(int i)
{
  switch (i){
  case 0: return "Move"; break;
  case 1: return "Toggle raise and lower"; break;
  case 2: return "Resize"; break;
  case 3: return "Raise"; break;
  case 4: return "Lower"; break;
  case 5: return "Nothing"; break;
  }
  return "";
}


void KActionsConfig::load()
{
  config->setGroup("Windows");
  setComboText(coTiDbl, config->readEntry("TitlebarDoubleClickCommand","Shade").ascii());

  config->setGroup( "MouseBindings");
  setComboText(coTiAct1,config->readEntry("CommandActiveTitlebar1","Raise").ascii());
  setComboText(coTiAct2,config->readEntry("CommandActiveTitlebar2","Lower").ascii());
  setComboText(coTiAct3,config->readEntry("CommandActiveTitlebar3","Operations menu").ascii());
  setComboText(coTiInAct1,config->readEntry("CommandInactiveTitlebar1","Activate and raise").ascii());
  setComboText(coTiInAct2,config->readEntry("CommandInactiveTitlebar2","Activate and lower").ascii());
  setComboText(coTiInAct3,config->readEntry("CommandInactiveTitlebar3","Activate").ascii());
  setComboText(coWin1,config->readEntry("CommandWindow1","Activate, raise and pass click").ascii());
  setComboText(coWin2,config->readEntry("CommandWindow2","Activate and pass click").ascii());
  setComboText(coWin3,config->readEntry("CommandWindow3","Activate and pass click").ascii());
  setComboText(coAllKey,config->readEntry("CommandAllKey","Alt").ascii());
  setComboText(coAll1,config->readEntry("CommandAll1","Move").ascii());
  setComboText(coAll2,config->readEntry("CommandAll2","Toggle raise and lower").ascii());
  setComboText(coAll3,config->readEntry("CommandAll3","Resize").ascii());
}

// many widgets connect to this slot
void KActionsConfig::slotChanged()
{
  emit changed(true);
}

void KActionsConfig::save()
{
  config->setGroup("Windows");
  config->writeEntry("TitlebarDoubleClickCommand", functionTiDbl( coTiDbl->currentItem() ) );

  config->setGroup("MouseBindings");
  config->writeEntry("CommandActiveTitlebar1", functionTiAc(coTiAct1->currentItem()));
  config->writeEntry("CommandActiveTitlebar2", functionTiAc(coTiAct2->currentItem()));
  config->writeEntry("CommandActiveTitlebar3", functionTiAc(coTiAct3->currentItem()));
  config->writeEntry("CommandInactiveTitlebar1", functionTiInAc(coTiInAct1->currentItem()));
  config->writeEntry("CommandInactiveTitlebar2", functionTiInAc(coTiInAct2->currentItem()));
  config->writeEntry("CommandInactiveTitlebar3", functionTiInAc(coTiInAct3->currentItem()));
  config->writeEntry("CommandWindow1", functionWin(coWin1->currentItem()));
  config->writeEntry("CommandWindow2", functionWin(coWin2->currentItem()));
  config->writeEntry("CommandWindow3", functionWin(coWin3->currentItem()));
  config->writeEntry("CommandAllKey", functionAllKey(coAllKey->currentItem()));
  config->writeEntry("CommandAll1", functionAll(coAll1->currentItem()));
  config->writeEntry("CommandAll2", functionAll(coAll2->currentItem()));
  config->writeEntry("CommandAll3", functionAll(coAll3->currentItem()));
}

void KActionsConfig::defaults()
{
  setComboText(coTiAct1,"Raise");
  setComboText(coTiAct2,"Lower");
  setComboText(coTiAct3,"Operations menu");
  setComboText(coTiInAct1,"Activate and raise");
  setComboText(coTiInAct2,"Activate and lower");
  setComboText(coTiInAct3,"Activate");
  setComboText(coWin1,"Activate, raise and pass click");
  setComboText(coWin2,"Activate and pass click");
  setComboText(coWin3,"Activate and pass click");
  setComboText(coAllKey, KKeyNative::keyboardHasWinKey() ? "Meta" : "Alt");
  setComboText (coAll1,"Move");
  setComboText(coAll2,"Toggle raise and lower");
  setComboText(coAll3,"Resize");
}
