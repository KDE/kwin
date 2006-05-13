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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <QLabel>
#include <QComboBox>

#include <QLayout>
#include <q3grid.h>
#include <QSizePolicy>
#include <QBitmap>
#include <QToolTip>
#include <Q3GroupBox>
//Added by qt3to4:
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <dcopclient.h>
#include <klocale.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kdialog.h>
#include <kglobalsettings.h>
#include <kseparator.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdlib.h>

#include "mouse.h"
#include "mouse.moc"


namespace {

char const * const cnf_Max[] = {
  "MaximizeButtonLeftClickCommand",
  "MaximizeButtonMiddleClickCommand",
  "MaximizeButtonRightClickCommand",
};

char const * const tbl_Max[] = {
  "Maximize",
  "Maximize (vertical only)",
  "Maximize (horizontal only)",
  "" };

QPixmap maxButtonPixmaps[3];

void createMaxButtonPixmaps()
{
  char const * maxButtonXpms[][3 + 13] = {
    {0, 0, 0,
    "...............",
    ".......#.......",
    "......###......",
    ".....#####.....",
    "..#....#....#..",
    ".##....#....##.",
    "###############",
    ".##....#....##.",
    "..#....#....#..",
    ".....#####.....",
    "......###......",
    ".......#.......",
    "..............."},
    {0, 0, 0,
    "...............",
    ".......#.......",
    "......###......",
    ".....#####.....",
    ".......#.......",
    ".......#.......",
    ".......#.......",
    ".......#.......",
    ".......#.......",
    ".....#####.....",
    "......###......",
    ".......#.......",
    "..............."},
    {0, 0, 0,
    "...............",
    "...............",
    "...............",
    "...............",
    "..#.........#..",
    ".##.........##.",
    "###############",
    ".##.........##.",
    "..#.........#..",
    "...............",
    "...............",
    "...............",
    "..............."},
  };

  QString baseColor(". c " + KGlobalSettings::baseColor().name());
  QString textColor("# c " + KGlobalSettings::textColor().name());
  for (int t = 0; t < 3; ++t)
  {
    maxButtonXpms[t][0] = "15 13 2 1";
    maxButtonXpms[t][1] = baseColor.toAscii();
    maxButtonXpms[t][2] = textColor.toAscii();
    maxButtonPixmaps[t] = QPixmap(maxButtonXpms[t]);
    maxButtonPixmaps[t].setMask(maxButtonPixmaps[t].createHeuristicMask());
  }
}

} // namespace

void KTitleBarActionsConfig::paletteChanged()
{
  createMaxButtonPixmaps();
  for (int b = 0; b < 3; ++b)
    for (int t = 0; t < 3; ++t)
      coMax[b]->setItemIcon(t, maxButtonPixmaps[t]);

}

KTitleBarActionsConfig::KTitleBarActionsConfig (bool _standAlone, KConfig *_config, KInstance *inst, QWidget * parent)
  : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
  QString strWin1, strWin2, strWin3, strAllKey, strAll1, strAll2, strAll3;
  Q3Grid *grid;
  Q3GroupBox *box;
  QLabel *label;
  QString strMouseButton1, strMouseButton3, strMouseWheel;
  QString txtButton1, txtButton3, txtButton4;
  QStringList items;
  bool leftHandedMouse = ( KGlobalSettings::mouseSettings().handed == KGlobalSettings::KMouseSettings::LeftHanded);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(KDialog::spacingHint());

/** Titlebar doubleclick ************/

  QHBoxLayout *hlayout = new QHBoxLayout();
  layout->addLayout( hlayout );

  label = new QLabel(i18n("&Titlebar double-click:"), this);
  hlayout->addWidget(label);
  label->setWhatsThis( i18n("Here you can customize mouse click behavior when double clicking on the"
    " titlebar of a window.") );

  QComboBox* combo = new QComboBox(this);
  combo->addItem(i18n("Maximize"));
  combo->addItem(i18n("Maximize (vertical only)"));
  combo->addItem(i18n("Maximize (horizontal only)"));
  combo->addItem(i18n("Minimize"));
  combo->addItem(i18n("Shade"));
  combo->addItem(i18n("Lower"));
  combo->addItem(i18n("On All Desktops"));
  combo->addItem(i18n("Nothing"));
  combo->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  hlayout->addWidget(combo);
  coTiDbl = combo;
  combo->setWhatsThis( i18n("Behavior on <em>double</em> click into the titlebar."));

  label->setBuddy(combo);

/** Mouse Wheel Events  **************/
  QHBoxLayout *hlayoutW = new QHBoxLayout();
  layout->addLayout( hlayoutW );
  strMouseWheel = i18n("Titlebar wheel event:");
  label = new QLabel(strMouseWheel, this);
  hlayoutW->addWidget(label);
  txtButton4 = i18n("Handle mouse wheel events");
  label->setWhatsThis( txtButton4);
  
  // Titlebar and frame mouse Wheel  
  QComboBox* comboW = new QComboBox(this);
  comboW->addItem(i18n("Raise/Lower"));
  comboW->addItem(i18n("Shade/Unshade"));
  comboW->addItem(i18n("Maximize/Restore"));
  comboW->addItem(i18n("Keep Above/Below"));  
  comboW->addItem(i18n("Move to Previous/Next Desktop"));  
  comboW->addItem(i18n("Change Opacity"));  
  comboW->addItem(i18n("Nothing"));  
  comboW->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
  connect(comboW, SIGNAL(activated(int)), SLOT(changed()));
  hlayoutW->addWidget(comboW);
  coTiAct4 = comboW;
  comboW->setWhatsThis( txtButton4);
  label->setBuddy(comboW);
  
/** Titlebar and frame  **************/

  box = new Q3GroupBox( 1, Qt::Horizontal, i18n("Titlebar && Frame"), this, "Titlebar and Frame");
  box->layout()->setMargin(KDialog::marginHint());
  box->layout()->setSpacing(KDialog::spacingHint());
  layout->addWidget(box);
  box->setWhatsThis( i18n("Here you can customize mouse click behavior when clicking on the"
                             " titlebar or the frame of a window.") );

  grid = new Q3Grid(4, Qt::Vertical, box);


  new QLabel(grid); // dummy

  strMouseButton1 = i18n("Left button:");
  txtButton1 = i18n("In this row you can customize left click behavior when clicking into"
     " the titlebar or the frame.");

  strMouseButton3 = i18n("Right button:");
  txtButton3 = i18n("In this row you can customize right click behavior when clicking into"
     " the titlebar or the frame." );

  if ( leftHandedMouse )
  {
     qSwap(strMouseButton1, strMouseButton3);
     qSwap(txtButton1, txtButton3);
  }

  label = new QLabel(strMouseButton1, grid);
  label->setWhatsThis( txtButton1);

  label = new QLabel(i18n("Middle button:"), grid);
  label->setWhatsThis( i18n("In this row you can customize middle click behavior when clicking into"
    " the titlebar or the frame.") );

  label = new QLabel(strMouseButton3, grid);
  label->setWhatsThis( txtButton3);


  label = new QLabel(i18n("Active"), grid);
  label->setAlignment(Qt::AlignCenter);
  label->setWhatsThis( i18n("In this column you can customize mouse clicks into the titlebar"
                               " or the frame of an active window.") );

  // Titlebar and frame, active, mouse button 1
  combo = new QComboBox(grid);
  combo->addItem(i18n("Raise"));
  combo->addItem(i18n("Lower"));
  combo->addItem(i18n("Operations Menu"));
  combo->addItem(i18n("Toggle Raise & Lower"));
  combo->addItem(i18n("Nothing"));
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coTiAct1 = combo;

  txtButton1 = i18n("Behavior on <em>left</em> click into the titlebar or frame of an "
     "<em>active</em> window.");

  txtButton3 = i18n("Behavior on <em>right</em> click into the titlebar or frame of an "
     "<em>active</em> window.");

  // Be nice to left handed users
  if ( leftHandedMouse ) qSwap(txtButton1, txtButton3);

  combo->setWhatsThis( txtButton1);

  // Titlebar and frame, active, mouse button 2

  items << i18n("Raise")
        << i18n("Lower")
        << i18n("Operations Menu")
        << i18n("Toggle Raise & Lower")
        << i18n("Nothing")
        << i18n("Shade");

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coTiAct2 = combo;
  combo->setWhatsThis( i18n("Behavior on <em>middle</em> click into the titlebar or frame of an <em>active</em> window."));

  // Titlebar and frame, active, mouse button 3
  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coTiAct3 =  combo;
  combo->setWhatsThis( txtButton3 );

  txtButton1 = i18n("Behavior on <em>left</em> click into the titlebar or frame of an "
     "<em>inactive</em> window.");

  txtButton3 = i18n("Behavior on <em>right</em> click into the titlebar or frame of an "
     "<em>inactive</em> window.");

  // Be nice to left handed users
  if ( leftHandedMouse ) qSwap(txtButton1, txtButton3);

  label = new QLabel(i18n("Inactive"), grid);
  label->setAlignment(Qt::AlignCenter);
  label->setWhatsThis( i18n("In this column you can customize mouse clicks into the titlebar"
                               " or the frame of an inactive window.") );

  items.clear();
  items  << i18n("Activate & Raise")
         << i18n("Activate & Lower")
         << i18n("Activate")
         << i18n("Shade")
         << i18n("Operations Menu")
         << i18n("Raise")
         << i18n("Lower")
         << i18n("Nothing");

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coTiInAct1 = combo;
  combo->setWhatsThis( txtButton1);

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coTiInAct2 = combo;
  combo->setWhatsThis( i18n("Behavior on <em>middle</em> click into the titlebar or frame of an <em>inactive</em> window."));

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coTiInAct3 = combo;
  combo->setWhatsThis( txtButton3);

/**  Maximize Button ******************/

  box = new Q3GroupBox(1, Qt::Vertical, i18n("Maximize Button"), this, "Maximize Button");
  box->layout()->setMargin(KDialog::marginHint());
  box->layout()->setSpacing(KDialog::spacingHint());
  layout->addWidget(box);
  box->setWhatsThis(
    i18n("Here you can customize behavior when clicking on the maximize button.") );

  QString strMouseButton[] = {
    i18n("Left button:"),
    i18n("Middle button:"),
    i18n("Right button:")};

  QString txtButton[] = {
    i18n("Behavior on <em>left</em> click onto the maximize button." ),
    i18n("Behavior on <em>middle</em> click onto the maximize button." ),
    i18n("Behavior on <em>right</em> click onto the maximize button." )};

  if ( leftHandedMouse ) // Be nice to lefties
  {
     qSwap(strMouseButton[0], strMouseButton[2]);
     qSwap(txtButton[0], txtButton[2]);
  }

  createMaxButtonPixmaps();
  for (int b = 0; b < 3; ++b)
  {
    if (b != 0) new QWidget(box); // Spacer

    QLabel * label = new QLabel(strMouseButton[b], box);
    label->setWhatsThis(    txtButton[b] );
    label   ->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum ));

    coMax[b] = new ToolTipComboBox(box, tbl_Max);
    for (int t = 0; t < 3; ++t) coMax[b]->addItem(maxButtonPixmaps[t], QString());
    connect(coMax[b], SIGNAL(activated(int)), SLOT(changed()));
    connect(coMax[b], SIGNAL(activated(int)), coMax[b], SLOT(changed()));
    coMax[b]->setWhatsThis( txtButton[b] );
    coMax[b]->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum ));
  }

  connect(kapp, SIGNAL(kdisplayPaletteChanged()), SLOT(paletteChanged()));

  layout->addStretch();

  load();
}

KTitleBarActionsConfig::~KTitleBarActionsConfig()
{
  if (standAlone)
    delete config;
}

// do NOT change the texts below, they are written to config file
// and are not shown in the GUI
// they have to match the order of items in GUI elements though
const char* const tbl_TiDbl[] = {
    "Maximize",
    "Maximize (vertical only)",
    "Maximize (horizontal only)",
    "Minimize",
    "Shade",
    "Lower",
    "OnAllDesktops",
    "Nothing",
    "" };

const char* const tbl_TiAc[] = {
    "Raise",
    "Lower",
    "Operations menu",
    "Toggle raise and lower",
    "Nothing",
    "Shade",
    "" };

const char* const tbl_TiInAc[] = {
    "Activate and raise",
    "Activate and lower",
    "Activate",
    "Shade",
    "Operations menu",
    "Raise",
    "Lower",
    "Nothing",
    "" };

const char* const tbl_Win[] = {
    "Activate, raise and pass click",
    "Activate and pass click",
    "Activate",
    "Activate and raise",
    "" };

const char* const tbl_AllKey[] = {
    "Meta",
    "Alt",
    "" };

const char* const tbl_All[] = {
    "Move",
    "Activate, raise and move",
    "Toggle raise and lower",
    "Resize",
    "Raise",
    "Lower",
    "Minimize",
    "Nothing",
    "" };

const char* tbl_TiWAc[] = {
    "Raise/Lower",
    "Shade/Unshade",
    "Maximize/Restore",
    "Above/Below",
    "Previous/Next Desktop",
    "Change Opacity",
    "Nothing",
    "" };

const char* tbl_AllW[] = {
    "Raise/Lower",
    "Shade/Unshade",
    "Maximize/Restore",
    "Above/Below",
    "Previous/Next Desktop",
    "Change Opacity",
    "Nothing",
    "" };

static const char* tbl_num_lookup( const char* const arr[], int pos )
{
    for( int i = 0;
         arr[ i ][ 0 ] != '\0' && pos >= 0;
         ++i )
    {
        if( pos == 0 )
            return arr[ i ];
        --pos;
    }
    abort(); // should never happen this way
}

static int tbl_txt_lookup( const char* const arr[], const char* txt )
{
    int pos = 0;
    for( int i = 0;
         arr[ i ][ 0 ] != '\0';
         ++i )
    {
        if( qstricmp( txt, arr[ i ] ) == 0 )
            return pos;
        ++pos;
    }
    return 0;
}

void KTitleBarActionsConfig::setComboText( QComboBox* combo, const char*txt )
{
    if( combo == coTiDbl )
        combo->setCurrentIndex( tbl_txt_lookup( tbl_TiDbl, txt ));
    else if( combo == coTiAct1 || combo == coTiAct2 || combo == coTiAct3 )
        combo->setCurrentIndex( tbl_txt_lookup( tbl_TiAc, txt ));
    else if( combo == coTiInAct1 || combo == coTiInAct2 || combo == coTiInAct3 )
        combo->setCurrentIndex( tbl_txt_lookup( tbl_TiInAc, txt ));
    else if( combo == coTiAct4 )
        combo->setCurrentIndex( tbl_txt_lookup( tbl_TiWAc, txt ));	
    else if( combo == coMax[0] || combo == coMax[1] || combo == coMax[2] )
    {
        combo->setCurrentIndex( tbl_txt_lookup( tbl_Max, txt ));
        static_cast<ToolTipComboBox *>(combo)->changed();
    }
    else
        abort();
}

const char* KTitleBarActionsConfig::functionTiDbl( int i )
{
    return tbl_num_lookup( tbl_TiDbl, i );
}

const char* KTitleBarActionsConfig::functionTiAc( int i )
{
    return tbl_num_lookup( tbl_TiAc, i );
}

const char* KTitleBarActionsConfig::functionTiInAc( int i )
{
    return tbl_num_lookup( tbl_TiInAc, i );
}

const char* KTitleBarActionsConfig::functionTiWAc(int i)
{
    return tbl_num_lookup( tbl_TiWAc, i );
}

const char* KTitleBarActionsConfig::functionMax( int i )
{
    return tbl_num_lookup( tbl_Max, i );
}

void KTitleBarActionsConfig::load()
{
  config->setGroup("Windows");
  setComboText(coTiDbl, config->readEntry("TitlebarDoubleClickCommand","Shade").toAscii());
  for (int t = 0; t < 3; ++t)
    setComboText(coMax[t],config->readEntry(cnf_Max[t], tbl_Max[t]).toAscii());

  config->setGroup( "MouseBindings");
  setComboText(coTiAct1,config->readEntry("CommandActiveTitlebar1","Raise").toAscii());
  setComboText(coTiAct2,config->readEntry("CommandActiveTitlebar2","Lower").toAscii());
  setComboText(coTiAct3,config->readEntry("CommandActiveTitlebar3","Operations menu").toAscii());
  setComboText(coTiAct4,config->readEntry("CommandTitlebarWheel","Nothing").toAscii());  
  setComboText(coTiInAct1,config->readEntry("CommandInactiveTitlebar1","Activate and raise").toAscii());
  setComboText(coTiInAct2,config->readEntry("CommandInactiveTitlebar2","Activate and lower").toAscii());
  setComboText(coTiInAct3,config->readEntry("CommandInactiveTitlebar3","Operations menu").toAscii());
}

void KTitleBarActionsConfig::save()
{
  config->setGroup("Windows");
  config->writeEntry("TitlebarDoubleClickCommand", functionTiDbl( coTiDbl->currentIndex() ) );
  for (int t = 0; t < 3; ++t)
    config->writeEntry(cnf_Max[t], functionMax(coMax[t]->currentIndex()));

  config->setGroup("MouseBindings");
  config->writeEntry("CommandActiveTitlebar1", functionTiAc(coTiAct1->currentIndex()));
  config->writeEntry("CommandActiveTitlebar2", functionTiAc(coTiAct2->currentIndex()));
  config->writeEntry("CommandActiveTitlebar3", functionTiAc(coTiAct3->currentIndex()));
  config->writeEntry("CommandInactiveTitlebar1", functionTiInAc(coTiInAct1->currentIndex()));
  config->writeEntry("CommandTitlebarWheel", functionTiWAc(coTiAct4->currentIndex()));  
  config->writeEntry("CommandInactiveTitlebar2", functionTiInAc(coTiInAct2->currentIndex()));
  config->writeEntry("CommandInactiveTitlebar3", functionTiInAc(coTiInAct3->currentIndex()));
  
  if (standAlone)
  {
    config->sync();
    if ( !kapp->dcopClient()->isAttached() )
      kapp->dcopClient()->attach();
    kapp->dcopClient()->send("kwin*", "", "reconfigure()", QByteArray());
  }
}

void KTitleBarActionsConfig::defaults()
{
  setComboText(coTiDbl, "Shade");
  setComboText(coTiAct1,"Raise");
  setComboText(coTiAct2,"Lower");
  setComboText(coTiAct3,"Operations menu");
  setComboText(coTiAct4,"Nothing");    
  setComboText(coTiInAct1,"Activate and raise");
  setComboText(coTiInAct2,"Activate and lower");
  setComboText(coTiInAct3,"Operations menu");
  for (int t = 0; t < 3; ++t)
    setComboText(coMax[t], tbl_Max[t]);
}


KWindowActionsConfig::KWindowActionsConfig (bool _standAlone, KConfig *_config, KInstance *inst, QWidget * parent)
  : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
  QString strWin1, strWin2, strWin3, strAllKey, strAll1, strAll2, strAll3, strAllW;
  Q3Grid *grid;
  Q3GroupBox *box;
  QLabel *label;
  QString strMouseButton1, strMouseButton3;
  QString txtButton1, txtButton3;
  QStringList items;
  bool leftHandedMouse = ( KGlobalSettings::mouseSettings().handed == KGlobalSettings::KMouseSettings::LeftHanded);
  
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(KDialog::spacingHint());

/**  Inactive inner window ******************/

  box = new Q3GroupBox(1, Qt::Horizontal, i18n("Inactive Inner Window"), this, "Inactive Inner Window");
  box->layout()->setMargin(KDialog::marginHint());
  box->layout()->setSpacing(KDialog::spacingHint());
  layout->addWidget(box);
  box->setWhatsThis( i18n("Here you can customize mouse click behavior when clicking on an inactive"
                             " inner window ('inner' means: not titlebar, not frame).") );

  grid = new Q3Grid(3, Qt::Vertical, box);

  strMouseButton1 = i18n("Left button:");
  txtButton1 = i18n("In this row you can customize left click behavior when clicking into"
     " the titlebar or the frame.");

  strMouseButton3 = i18n("Right button:");
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
  label->setWhatsThis( strWin1 );

  label = new QLabel(i18n("Middle button:"), grid);
  strWin2 = i18n("In this row you can customize middle click behavior when clicking into"
     " an inactive inner window ('inner' means: not titlebar, not frame).");
  label->setWhatsThis( strWin2 );

  label = new QLabel(strMouseButton3, grid);
  label->setWhatsThis( strWin3 );

  items.clear();
  items   << i18n("Activate, Raise & Pass Click")
          << i18n("Activate & Pass Click")
          << i18n("Activate")
          << i18n("Activate & Raise");

  QComboBox* combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coWin1 = combo;
  combo->setWhatsThis( strWin1 );

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coWin2 = combo;
  combo->setWhatsThis( strWin2 );

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coWin3 = combo;
  combo->setWhatsThis( strWin3 );


/** Inner window, titlebar and frame **************/

  box = new Q3GroupBox(1, Qt::Horizontal, i18n("Inner Window, Titlebar && Frame"), this, "Inner Window, Titlebar and Frame");
  box->layout()->setMargin(KDialog::marginHint());
  box->layout()->setSpacing(KDialog::spacingHint());
  layout->addWidget(box);
  box->setWhatsThis( i18n("Here you can customize KDE's behavior when clicking somewhere into"
                             " a window while pressing a modifier key."));

  grid = new Q3Grid(5, Qt::Vertical, box);

  // Labels
  label = new QLabel(i18n("Modifier key:"), grid);

  strAllKey = i18n("Here you select whether holding the Meta key or Alt key "
    "will allow you to perform the following actions.");
  label->setWhatsThis( strAllKey );


  strMouseButton1 = i18n("Modifier key + left button:");
  strAll1 = i18n("In this row you can customize left click behavior when clicking into"
                 " the titlebar or the frame.");

  strMouseButton3 = i18n("Modifier key + right button:");
  strAll3 = i18n("In this row you can customize right click behavior when clicking into"
                 " the titlebar or the frame." );

  if ( leftHandedMouse )
  {
     qSwap(strMouseButton1, strMouseButton3);
     qSwap(strAll1, strAll3);
  }

  label = new QLabel(strMouseButton1, grid);
  label->setWhatsThis( strAll1);

  label = new QLabel(i18n("Modifier key + middle button:"), grid);
  strAll2 = i18n("Here you can customize KDE's behavior when middle clicking into a window"
                 " while pressing the modifier key.");
  label->setWhatsThis( strAll2 );

  label = new QLabel(strMouseButton3, grid);
  label->setWhatsThis( strAll3);

  label = new QLabel(i18n("Modifier key + mouse wheel:"), grid);
  strAllW = i18n("Here you can customize KDE's behavior when scrolling with the mouse wheel"
      "  in a window while pressing the modifier key.");
  label->setWhatsThis( strAllW);

  // Combo's
  combo = new QComboBox(grid);
  combo->addItem(i18n("Meta"));
  combo->addItem(i18n("Alt"));
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coAllKey = combo;
  combo->setWhatsThis( strAllKey );

  items.clear();
  items << i18n("Move")
        << i18n("Activate, Raise and Move")
        << i18n("Toggle Raise & Lower")
        << i18n("Resize")
        << i18n("Raise")
        << i18n("Lower")
        << i18n("Minimize")
        << i18n("Nothing");

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coAll1 = combo;
  combo->setWhatsThis( strAll1 );

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coAll2 = combo;
  combo->setWhatsThis( strAll2 );

  combo = new QComboBox(grid);
  combo->addItems(items);
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coAll3 =  combo;
  combo->setWhatsThis( strAll3 );

  combo = new QComboBox(grid);
  combo->addItem(i18n("Raise/Lower"));
  combo->addItem(i18n("Shade/Unshade"));
  combo->addItem(i18n("Maximize/Restore"));
  combo->addItem(i18n("Keep Above/Below"));  
  combo->addItem(i18n("Move to Previous/Next Desktop"));  
  combo->addItem(i18n("Change Opacity"));  
  combo->addItem(i18n("Nothing"));  
  connect(combo, SIGNAL(activated(int)), SLOT(changed()));
  coAllW =  combo;
  combo->setWhatsThis( strAllW );

  layout->addStretch();

  load();
}

KWindowActionsConfig::~KWindowActionsConfig()
{
  if (standAlone)
    delete config;
}

void KWindowActionsConfig::setComboText( QComboBox* combo, const char*txt )
{
    if( combo == coWin1 || combo == coWin2 || combo == coWin3 )
        combo->setCurrentIndex( tbl_txt_lookup( tbl_Win, txt ));
    else if( combo == coAllKey )
        combo->setCurrentIndex( tbl_txt_lookup( tbl_AllKey, txt ));
    else if( combo == coAll1 || combo == coAll2 || combo == coAll3 )
        combo->setCurrentIndex( tbl_txt_lookup( tbl_All, txt ));
    else if( combo == coAllW )
        combo->setCurrentIndex( tbl_txt_lookup( tbl_AllW, txt ));	
    else
        abort();
}

const char* KWindowActionsConfig::functionWin( int i )
{
    return tbl_num_lookup( tbl_Win, i );
}

const char* KWindowActionsConfig::functionAllKey( int i )
{
    return tbl_num_lookup( tbl_AllKey, i );
}

const char* KWindowActionsConfig::functionAll( int i )
{
    return tbl_num_lookup( tbl_All, i );
}

const char* KWindowActionsConfig::functionAllW(int i)
{
    return tbl_num_lookup( tbl_AllW, i );
}

void KWindowActionsConfig::load()
{
  config->setGroup( "MouseBindings");
  setComboText(coWin1,config->readEntry("CommandWindow1","Activate, raise and pass click").toAscii());
  setComboText(coWin2,config->readEntry("CommandWindow2","Activate and pass click").toAscii());
  setComboText(coWin3,config->readEntry("CommandWindow3","Activate and pass click").toAscii());
  setComboText(coAllKey,config->readEntry("CommandAllKey","Alt").toAscii());
  setComboText(coAll1,config->readEntry("CommandAll1","Move").toAscii());
  setComboText(coAll2,config->readEntry("CommandAll2","Toggle raise and lower").toAscii());
  setComboText(coAll3,config->readEntry("CommandAll3","Resize").toAscii());
  setComboText(coAllW,config->readEntry("CommandAllWheel","Nothing").toAscii());
}

void KWindowActionsConfig::save()
{
  config->setGroup("MouseBindings");
  config->writeEntry("CommandWindow1", functionWin(coWin1->currentIndex()));
  config->writeEntry("CommandWindow2", functionWin(coWin2->currentIndex()));
  config->writeEntry("CommandWindow3", functionWin(coWin3->currentIndex()));
  config->writeEntry("CommandAllKey", functionAllKey(coAllKey->currentIndex()));
  config->writeEntry("CommandAll1", functionAll(coAll1->currentIndex()));
  config->writeEntry("CommandAll2", functionAll(coAll2->currentIndex()));
  config->writeEntry("CommandAll3", functionAll(coAll3->currentIndex()));
  config->writeEntry("CommandAllWheel", functionAllW(coAllW->currentIndex()));
  
  if (standAlone)
  {
    config->sync();
    if ( !kapp->dcopClient()->isAttached() )
      kapp->dcopClient()->attach();
    kapp->dcopClient()->send("kwin*", "", "reconfigure()", QByteArray());
  }
}

void KWindowActionsConfig::defaults()
{
  setComboText(coWin1,"Activate, raise and pass click");
  setComboText(coWin2,"Activate and pass click");
  setComboText(coWin3,"Activate and pass click");
  setComboText(coAllKey,"Alt");
  setComboText (coAll1,"Move");
  setComboText(coAll2,"Toggle raise and lower");
  setComboText(coAll3,"Resize");
  setComboText(coAllW,"Nothing");
}
