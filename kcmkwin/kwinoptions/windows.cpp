/*
 * windows.cpp
 *
 * Copyright (c) 1997 Patrick Dowler dowler@morgul.fsh.uvic.ca
 * Copyright (c) 2001 Waldo Bastian bastian@kde.org
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
 *
 *
 */

#include <config.h>

#include <qlayout.h>
#include <qslider.h>
#include <qwhatsthis.h>
#include <qvbuttongroup.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qlabel.h>
#include <qcombobox.h>

#include <klocale.h>
#include <kconfig.h>
#include <knuminput.h>
#include <kapplication.h>
#include <kdialog.h>
#include <dcopclient.h>
#include <kglobal.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "windows.h"


// kwin config keywords
#define KWIN_FOCUS                 "FocusPolicy"
#define KWIN_PLACEMENT             "Placement"
#define KWIN_MOVE                  "MoveMode"
#define KWIN_MINIMIZE_ANIM         "AnimateMinimize"
#define KWIN_MINIMIZE_ANIM_SPEED   "AnimateMinimizeSpeed"
#define KWIN_RESIZE_OPAQUE         "ResizeMode"
#define KWIN_GEOMETRY		   "GeometryTip"
#define KWIN_AUTORAISE_INTERVAL    "AutoRaiseInterval"
#define KWIN_AUTORAISE             "AutoRaise"
#define KWIN_CLICKRAISE            "ClickRaise"
#define KWIN_ANIMSHADE             "AnimateShade"
#define KWIN_MOVE_RESIZE_MAXIMIZED "MoveResizeMaximizedWindows"
#define KWIN_ALTTABMODE            "AltTabStyle"
#define KWIN_TRAVERSE_ALL          "TraverseAll"
#define KWIN_SHOW_POPUP            "ShowPopup"
#define KWIN_ROLL_OVER_DESKTOPS    "RollOverDesktops"
#define KWIN_SHADEHOVER            "ShadeHover"
#define KWIN_SHADEHOVER_INTERVAL   "ShadeHoverInterval"
#define KWIN_FOCUS_STEALING        "FocusStealingPreventionLevel"

// kwm config keywords
#define KWM_ELECTRIC_BORDER                  "ElectricBorders"
#define KWM_ELECTRIC_BORDER_DELAY            "ElectricBorderDelay"

//CT 15mar 98 - magics
#define KWM_BRDR_SNAP_ZONE                   "BorderSnapZone"
#define KWM_BRDR_SNAP_ZONE_DEFAULT           10
#define KWM_WNDW_SNAP_ZONE                   "WindowSnapZone"
#define KWM_WNDW_SNAP_ZONE_DEFAULT           10

#define MAX_BRDR_SNAP                          100
#define MAX_WNDW_SNAP                          100
#define MAX_EDGE_RES                          1000


KFocusConfig::~KFocusConfig ()
{
    if (standAlone)
        delete config;
}

// removed the LCD display over the slider - this is not good GUI design :) RNolden 051701
KFocusConfig::KFocusConfig (bool _standAlone, KConfig *_config, QWidget * parent, const char *)
    : KCModule(parent, "kcmkwm"), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout (this, 0, KDialog::spacingHint());

    //iTLabel = new QLabel(i18n("  Allowed overlap:\n"
    //                         "(% of desktop space)"),
    //             plcBox);
    //iTLabel->setAlignment(AlignTop|AlignHCenter);
    //pLay->addWidget(iTLabel,1,1);

    //interactiveTrigger = new QSpinBox(0, 500, 1, plcBox);
    //pLay->addWidget(interactiveTrigger,1,2);

    //pLay->addRowSpacing(2,KDialog::spacingHint());

    //lay->addWidget(plcBox);

    // focus policy
    fcsBox = new QButtonGroup(i18n("Focus"),this);
    fcsBox->setColumnLayout( 0, Qt::Horizontal );

    QBoxLayout *fLay = new QVBoxLayout(fcsBox->layout(),
        KDialog::spacingHint());

    QBoxLayout *cLay = new QHBoxLayout(fLay);
    QLabel *fLabel = new QLabel(i18n("&Policy:"), fcsBox);
    cLay->addWidget(fLabel, 0);
    focusCombo =  new QComboBox(false, fcsBox);
    focusCombo->insertItem(i18n("Click to Focus"), CLICK_TO_FOCUS);
    focusCombo->insertItem(i18n("Focus Follows Mouse"), FOCUS_FOLLOWS_MOUSE);
    focusCombo->insertItem(i18n("Focus Under Mouse"), FOCUS_UNDER_MOUSE);
    focusCombo->insertItem(i18n("Focus Strictly Under Mouse"), FOCUS_STRICTLY_UNDER_MOUSE);
    cLay->addWidget(focusCombo,1 ,Qt::AlignLeft);
    fLabel->setBuddy(focusCombo);

    // FIXME, when more policies have been added to KWin
    wtstr = i18n("The focus policy is used to determine the active window, i.e."
                                      " the window you can work in. <ul>"
                                      " <li><em>Click to focus:</em> A window becomes active when you click into it. This is the behavior"
                                      " you might know from other operating systems.</li>"
                                      " <li><em>Focus follows mouse:</em> Moving the mouse pointer actively on to a"
                                      " normal window activates it. Very practical if you are using the mouse a lot.</li>"
                                      " <li><em>Focus under mouse:</em> The window that happens to be under the"
                                      " mouse pointer becomes active. If the mouse points nowhere, the last window"
                                      " that was under the mouse has focus. </li>"
                                      " <li><em>Focus strictly under mouse:</em> This is even worse than"
                                      " 'Focus under mouse'. Only the window under the mouse pointer is"
                                      " active. If the mouse points nowhere, nothing has focus."
                                      " </ul>"
                                      " Note that 'Focus under mouse' and 'Focus strictly under mouse' are not"
                                      " particularly useful. They are only provided for old-fashioned"
                                      " die-hard UNIX people ;-)"
                         );
    QWhatsThis::add( focusCombo, wtstr);
    QWhatsThis::add(fLabel, wtstr);

    connect(focusCombo, SIGNAL(activated(int)), this, SLOT(setAutoRaiseEnabled()) );

    // autoraise delay
    autoRaiseOn = new QCheckBox(i18n("Auto &raise"), fcsBox);
    fLay->addWidget(autoRaiseOn);
    connect(autoRaiseOn,SIGNAL(toggled(bool)), this, SLOT(autoRaiseOnTog(bool)));

    autoRaise = new KIntNumInput(500, fcsBox);
    autoRaise->setLabel(i18n("Dela&y:"), Qt::AlignVCenter|Qt::AlignLeft);
    autoRaise->setRange(0, 3000, 100, true);
    autoRaise->setSteps(100,100);
    autoRaise->setSuffix(i18n(" msec"));
    fLay->addWidget(autoRaise);

    clickRaiseOn = new QCheckBox(i18n("C&lick raise active window"), fcsBox);
    connect(clickRaiseOn,SIGNAL(toggled(bool)), this, SLOT(clickRaiseOnTog(bool)));
    fLay->addWidget(clickRaiseOn);

//     fLay->addColSpacing(0,QMAX(autoRaiseOn->sizeHint().width(),
//                                clickRaiseOn->sizeHint().width()) + 15);

    QWhatsThis::add( autoRaiseOn, i18n("When this option is enabled, a window in the background will automatically"
                                       " come to the front when the mouse pointer has been over it for some time.") );
    wtstr = i18n("This is the delay after which the window that the mouse pointer is over will automatically"
                 " come to the front.");
    QWhatsThis::add( autoRaise, wtstr );

    QWhatsThis::add( clickRaiseOn, i18n("When this option is enabled, the active window will be brought to the"
                                        " front when you click somewhere into the window contents. To change"
                                        " it for inactive windows, you need to change the settings"
                                        " in the Actions tab.") );

    lay->addWidget(fcsBox);

    kbdBox = new QButtonGroup(i18n("Navigation"), this);
    kbdBox->setColumnLayout( 0, Qt::Horizontal );
    QGridLayout *kLay = new QGridLayout(kbdBox->layout(), 4, 4,
                                        KDialog::spacingHint());
    QLabel *altTabLabel = new QLabel( i18n("Walk through windows mode:"), kbdBox);
    kLay->addWidget(altTabLabel, 1, 0);
    kdeMode = new QRadioButton(i18n("&KDE"), kbdBox);
    kLay->addWidget(kdeMode, 1, 1);
    cdeMode = new QRadioButton(i18n("CD&E"), kbdBox);
    kLay->addWidget(cdeMode, 1, 2);

    wtstr = i18n("Hold down the Alt key and press the Tab key repeatedly to walk"
                 " through the windows on the current desktop (the Alt+Tab"
                 " combination can be reconfigured). The two different modes mean:<ul>"
                 "<li><b>KDE</b>: a nice widget is shown, displaying the icons of all windows to"
                 " walk through and the title of the currently selected one;"
                 "<li><b>CDE</b>: the focus is passed to a new window each time Tab is pressed."
                 " No fancy widget.</li></ul>");
    QWhatsThis::add( altTabLabel, wtstr );
    QWhatsThis::add( kdeMode, wtstr );
    QWhatsThis::add( cdeMode, wtstr );

    traverseAll = new QCheckBox( i18n( "&Traverse windows on all desktops" ), kbdBox );
    kLay->addMultiCellWidget( traverseAll, 2, 2, 0, 2 );

    wtstr = i18n( "Leave this option disabled if you want to limit walking through"
                  " windows to the current desktop." );
    QWhatsThis::add( traverseAll, wtstr );

    rollOverDesktops = new QCheckBox( i18n("Desktop navi&gation wraps around"), kbdBox );
    kLay->addMultiCellWidget(rollOverDesktops, 3, 3, 0, 2);

    wtstr = i18n( "Enable this option if you want keyboard or active desktop border navigation beyond"
                  " the edge of a desktop to take you to the opposite edge of the new desktop." );
    QWhatsThis::add( rollOverDesktops, wtstr );

    showPopupinfo = new QCheckBox( i18n("Popup desktop name on desktop &switch"), kbdBox );
    kLay->addMultiCellWidget(showPopupinfo, 4, 4, 0, 2);

    wtstr = i18n( "Enable this option if you wish to see the current desktop"
                  " name popup whenever the current desktop is changed." );
    QWhatsThis::add( showPopupinfo, wtstr );

    lay->addWidget(kbdBox);

    lay->addStretch();

    // Any changes goes to slotChanged()
    connect(focusCombo, SIGNAL(activated(int)), SLOT(changed()));
    connect(fcsBox, SIGNAL(clicked(int)), SLOT(changed()));
    connect(autoRaise, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(kdeMode, SIGNAL(clicked()), SLOT(changed()));
    connect(cdeMode, SIGNAL(clicked()), SLOT(changed()));
    connect(traverseAll, SIGNAL(clicked()), SLOT(changed()));
    connect(rollOverDesktops, SIGNAL(clicked()), SLOT(changed()));
    connect(showPopupinfo, SIGNAL(clicked()), SLOT(changed()));

    load();
}


int KFocusConfig::getFocus()
{
    return focusCombo->currentItem();
}

void KFocusConfig::setFocus(int foc)
{
    focusCombo->setCurrentItem(foc);

    // this will disable/hide the auto raise delay widget if focus==click
    setAutoRaiseEnabled();
}

void KFocusConfig::setAutoRaiseInterval(int tb)
{
    autoRaise->setValue(tb);
}

int KFocusConfig::getAutoRaiseInterval()
{
    return autoRaise->value();
}

void KFocusConfig::setAutoRaise(bool on)
{
    autoRaiseOn->setChecked(on);
}

void KFocusConfig::setClickRaise(bool on)
{
    clickRaiseOn->setChecked(on);
}

void KFocusConfig::setAutoRaiseEnabled()
{
    // the auto raise related widgets are: autoRaise
    if ( focusCombo->currentItem() != CLICK_TO_FOCUS )
    {
        autoRaiseOn->setEnabled(true);
        autoRaiseOnTog(autoRaiseOn->isChecked());
    }
    else
    {
        autoRaiseOn->setEnabled(false);
        autoRaiseOnTog(false);
    }
}


void KFocusConfig::autoRaiseOnTog(bool a) {
    autoRaise->setEnabled(a);
    clickRaiseOn->setEnabled( !a );
    if ( a )
    {
        clickRaiseOn->setChecked( TRUE );
        if(getAutoRaiseInterval() == 0)
            setAutoRaiseInterval(750);
    }

}

void KFocusConfig::clickRaiseOnTog(bool ) {
}

void KFocusConfig::setAltTabMode(bool a) {
    kdeMode->setChecked(a);
    cdeMode->setChecked(!a);
}

void KFocusConfig::setTraverseAll(bool a) {
    traverseAll->setChecked(a);
}

void KFocusConfig::setRollOverDesktops(bool a) {
    rollOverDesktops->setChecked(a);
}

void KFocusConfig::setShowPopupinfo(bool a) {
    showPopupinfo->setChecked(a);
}

void KFocusConfig::load( void )
{
    QString key;

    config->setGroup( "Windows" );

    key = config->readEntry(KWIN_FOCUS);
    if( key == "ClickToFocus")
        setFocus(CLICK_TO_FOCUS);
    else if( key == "FocusFollowsMouse")
        setFocus(FOCUS_FOLLOWS_MOUSE);
    else if(key == "FocusUnderMouse")
        setFocus(FOCUS_UNDER_MOUSE);
    else if(key == "FocusStrictlyUnderMouse")
        setFocus(FOCUS_STRICTLY_UNDER_MOUSE);

    int k = config->readNumEntry(KWIN_AUTORAISE_INTERVAL,0);
    setAutoRaiseInterval(k);

    key = config->readEntry(KWIN_AUTORAISE);
    setAutoRaise(key == "on");
    key = config->readEntry(KWIN_CLICKRAISE);
    setClickRaise(key != "off");
    setAutoRaiseEnabled();      // this will disable/hide the auto raise delay widget if focus==click

    key = config->readEntry(KWIN_ALTTABMODE, "KDE");
    setAltTabMode(key == "KDE");

    setRollOverDesktops( config->readBoolEntry(KWIN_ROLL_OVER_DESKTOPS, true ));

    config->setGroup( "PopupInfo" );
    setShowPopupinfo( config->readBoolEntry(KWIN_SHOW_POPUP, false ));

    config->setGroup( "TabBox" );
    setTraverseAll( config->readBoolEntry(KWIN_TRAVERSE_ALL, false ));

    config->setGroup("Desktops");
    emit KCModule::changed(false);
}

void KFocusConfig::save( void )
{
    int v;

    config->setGroup( "Windows" );

    v = getFocus();
    if (v == CLICK_TO_FOCUS)
        config->writeEntry(KWIN_FOCUS,"ClickToFocus");
    else if (v == FOCUS_UNDER_MOUSE)
        config->writeEntry(KWIN_FOCUS,"FocusUnderMouse");
    else if (v == FOCUS_STRICTLY_UNDER_MOUSE)
        config->writeEntry(KWIN_FOCUS,"FocusStrictlyUnderMouse");
    else
        config->writeEntry(KWIN_FOCUS,"FocusFollowsMouse");

    v = getAutoRaiseInterval();
    if (v <0) v = 0;
    config->writeEntry(KWIN_AUTORAISE_INTERVAL,v);

    if (autoRaiseOn->isChecked())
        config->writeEntry(KWIN_AUTORAISE, "on");
    else
        config->writeEntry(KWIN_AUTORAISE, "off");

    if (clickRaiseOn->isChecked())
        config->writeEntry(KWIN_CLICKRAISE, "on");
    else
        config->writeEntry(KWIN_CLICKRAISE, "off");

    if (kdeMode->isChecked())
        config->writeEntry(KWIN_ALTTABMODE, "KDE");
    else
        config->writeEntry(KWIN_ALTTABMODE, "CDE");

    config->writeEntry( KWIN_ROLL_OVER_DESKTOPS, rollOverDesktops->isChecked());

    config->setGroup( "PopupInfo" );
    config->writeEntry( KWIN_SHOW_POPUP, showPopupinfo->isChecked());

    config->setGroup( "TabBox" );
    config->writeEntry( KWIN_TRAVERSE_ALL , traverseAll->isChecked());

    config->setGroup("Desktops");

    if (standAlone)
    {
        config->sync();
        if ( !kapp->dcopClient()->isAttached() )
            kapp->dcopClient()->attach();
        kapp->dcopClient()->send("kwin*", "", "reconfigure()", "");
    }
    emit KCModule::changed(false);
}

void KFocusConfig::defaults()
{
    setAutoRaiseInterval(0);
    setFocus(CLICK_TO_FOCUS);
    setAutoRaise(false);
    setClickRaise(true);
    setAltTabMode(true);
    setTraverseAll( false );
    setRollOverDesktops(true);
    setShowPopupinfo(false);
    emit KCModule::changed(true);
}

KAdvancedConfig::~KAdvancedConfig ()
{
    if (standAlone)
        delete config;
}

KAdvancedConfig::KAdvancedConfig (bool _standAlone, KConfig *_config, QWidget *parent, const char *)
    : KCModule(parent, "kcmkwm"), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout (this, 0, KDialog::spacingHint());

    //iTLabel = new QLabel(i18n("  Allowed overlap:\n"
    //                         "(% of desktop space)"),
    //             plcBox);
    //iTLabel->setAlignment(AlignTop|AlignHCenter);
    //pLay->addWidget(iTLabel,1,1);

    //interactiveTrigger = new QSpinBox(0, 500, 1, plcBox);
    //pLay->addWidget(interactiveTrigger,1,2);

    //pLay->addRowSpacing(2,KDialog::spacingHint());

    //lay->addWidget(plcBox);

    shBox = new QVButtonGroup(i18n("Shading"), this);

    animateShade = new QCheckBox(i18n("Anima&te"), shBox);
    QWhatsThis::add(animateShade, i18n("Animate the action of reducing the window to its titlebar (shading)"
                                       " as well as the expansion of a shaded window") );

    shadeHoverOn = new QCheckBox(i18n("&Enable hover"), shBox);

    connect(shadeHoverOn, SIGNAL(toggled(bool)), this, SLOT(shadeHoverChanged(bool)));

    shadeHover = new KIntNumInput(500, shBox);
    shadeHover->setLabel(i18n("Dela&y:"), Qt::AlignVCenter|Qt::AlignLeft);
    shadeHover->setRange(0, 3000, 100, true);
    shadeHover->setSteps(100, 100);
    shadeHover->setSuffix(i18n(" msec"));

    QWhatsThis::add(shadeHoverOn, i18n("If Shade Hover is enabled, a shaded window will un-shade automatically "
                                       "when the mouse pointer has been over the title bar for some time."));

    wtstr = i18n("Sets the time in milliseconds before the window unshades "
                "when the mouse pointer goes over the shaded window.");
    QWhatsThis::add(shadeHover, wtstr);

    lay->addWidget(shBox);

    // Any changes goes to slotChanged()
    connect(animateShade, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(shadeHoverOn, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(shadeHover, SIGNAL(valueChanged(int)), SLOT(changed()));

    electricBox = new QVButtonGroup(i18n("Active Desktop Borders"), this);
    electricBox->setMargin(15);

    QWhatsThis::add( electricBox, i18n("If this option is enabled, moving the mouse to a screen border"
       " will change your desktop. This is e.g. useful if you want to drag windows from one desktop"
       " to the other.") );
    active_disable = new QRadioButton(i18n("D&isabled"), electricBox);
    active_move    = new QRadioButton(i18n("Only &when moving windows"), electricBox);
    active_always  = new QRadioButton(i18n("A&lways enabled"), electricBox);

    delays = new KIntNumInput(10, electricBox);
    delays->setRange(0, MAX_EDGE_RES, 50, true);
    delays->setSuffix(i18n(" msec"));
    delays->setLabel(i18n("Desktop &switch delay:"));
    QWhatsThis::add( delays, i18n("Here you can set a delay for switching desktops using the active"
       " borders feature. Desktops will be switched after the mouse has been pushed against a screen border"
       " for the specified number of milliseconds.") );

    connect( electricBox, SIGNAL(clicked(int)), this, SLOT(setEBorders()));

    // Any changes goes to slotChanged()
    connect(electricBox, SIGNAL(clicked(int)), SLOT(changed()));
    connect(delays, SIGNAL(valueChanged(int)), SLOT(changed()));

    lay->addWidget(electricBox);

    QHBoxLayout* focusStealingLayout = new QHBoxLayout( lay,KDialog::spacingHint());
    QLabel* focusStealingLabel = new QLabel( i18n( "Focus stealing prevention level:" ), this );
    focusStealing = new QComboBox( this );
    focusStealing->insertItem( i18n( "Focus Stealing Prevention Level", "None" ));
    focusStealing->insertItem( i18n( "Focus Stealing Prevention Level", "Low" ));
    focusStealing->insertItem( i18n( "Focus Stealing Prevention Level", "Normal" ));
    focusStealing->insertItem( i18n( "Focus Stealing Prevention Level", "High" ));
    focusStealing->insertItem( i18n( "Focus Stealing Prevention Level", "Extreme" ));
    focusStealingLabel->setBuddy( focusStealing );
    focusStealingLayout->addWidget( focusStealingLabel );
    focusStealingLayout->addWidget( focusStealing, AlignLeft );
    wtstr = i18n( "This option specifies how much KWin will try to prevent unwanted focus stealing "
                  "caused by unexpected activation of new windows. (Note: This feature doesn't "
                  "work with the Focus Under Mouse or Focus Strictly Under Mouse focus policies.)"
                  "<ul>"
                  "<li><em>None:</em> The standard old behavior - prevention is turned off "
                  "and new windows always become activated.</li>"
                  "<li><em>Low:</em> Prevention is enabled; when some window doesn't have support "
                  "for the underlying mechanism and KWin cannot reliably decide whether to "
                  "activate the window or not, it will be activated. This setting may have both "
                  "worse and better results than normal level, depending on the applications.</li>"
                  "<li><em>Normal:</em> Prevention is enabled; the default setting.</li>"
                  "<li><em>High:</em> New windows get activated only if no window is currently active "
                  "or if they belong to the currently active application. This setting is probably "
                  "not really usable when not using mouse focus policy.</li>"
                  "<li><em>Extreme:</em> All windows must be explicitly activated by the user.</li>"
                  "</ul>" );
    QWhatsThis::add( focusStealing, wtstr );
    QWhatsThis::add( focusStealingLabel, wtstr );

    connect(focusStealing, SIGNAL(activated(int)), SLOT(changed()));

    lay->addStretch();
    load();

}

void KAdvancedConfig::setShadeHover(bool on) {
    shadeHoverOn->setChecked(on);
    shadeHover->setEnabled(on);
}

void KAdvancedConfig::setShadeHoverInterval(int k) {
    shadeHover->setValue(k);
}

int KAdvancedConfig::getShadeHoverInterval() {

    return shadeHover->value();
}

void KAdvancedConfig::shadeHoverChanged(bool a) {
    shadeHover->setEnabled(a);
}

void KAdvancedConfig::setAnimateShade(bool a) {
    animateShade->setChecked(a);
}

void KAdvancedConfig::setFocusStealing(int l) {
    l = KMAX( 0, KMIN( 4, l ));
    focusStealing->setCurrentItem(l);
}

void KAdvancedConfig::load( void )
{
    config->setGroup( "Windows" );

    setAnimateShade(config->readBoolEntry(KWIN_ANIMSHADE, true));
    setShadeHover(config->readBoolEntry(KWIN_SHADEHOVER, false));
    setShadeHoverInterval(config->readNumEntry(KWIN_SHADEHOVER_INTERVAL, 250));

    setElectricBorders(config->readNumEntry(KWM_ELECTRIC_BORDER, false));
    setElectricBorderDelay(config->readNumEntry(KWM_ELECTRIC_BORDER_DELAY, 150));

//    setFocusStealing( config->readNumEntry(KWIN_FOCUS_STEALING, 2 ));
    // TODO default to low for now
    setFocusStealing( config->readNumEntry(KWIN_FOCUS_STEALING, 1 ));

    emit KCModule::changed(false);
}

void KAdvancedConfig::save( void )
{
    int v;

    config->setGroup( "Windows" );
    config->writeEntry(KWIN_ANIMSHADE, animateShade->isChecked());
    if (shadeHoverOn->isChecked())
        config->writeEntry(KWIN_SHADEHOVER, "on");
    else
        config->writeEntry(KWIN_SHADEHOVER, "off");

    v = getShadeHoverInterval();
    if (v<0) v = 0;
    config->writeEntry(KWIN_SHADEHOVER_INTERVAL, v);

    config->writeEntry(KWM_ELECTRIC_BORDER, getElectricBorders());
    config->writeEntry(KWM_ELECTRIC_BORDER_DELAY,getElectricBorderDelay());

    config->writeEntry(KWIN_FOCUS_STEALING, focusStealing->currentItem());

    if (standAlone)
    {
        config->sync();
        if ( !kapp->dcopClient()->isAttached() )
            kapp->dcopClient()->attach();
        kapp->dcopClient()->send("kwin*", "", "reconfigure()", "");
    }
    emit KCModule::changed(false);
}

void KAdvancedConfig::defaults()
{
    setAnimateShade(true);
    setShadeHover(false);
    setShadeHoverInterval(250);
    setElectricBorders(0);
    setElectricBorderDelay(150);
//    setFocusStealing(2);
    // TODO default to low for now
    setFocusStealing(1);
    emit KCModule::changed(true);
}

void KAdvancedConfig::setEBorders()
{
    delays->setEnabled(!active_disable->isChecked());
}

int KAdvancedConfig::getElectricBorders()
{
    if (active_move->isChecked())
       return 1;
    if (active_always->isChecked())
       return 2;
    return 0;
}

int KAdvancedConfig::getElectricBorderDelay()
{
    return delays->value();
}

void KAdvancedConfig::setElectricBorders(int i){
    switch(i)
    {
      case 1: active_move->setChecked(true); break;
      case 2: active_always->setChecked(true); break;
      default: active_disable->setChecked(true); break;
    }
    setEBorders();
}

void KAdvancedConfig::setElectricBorderDelay(int delay)
{
    delays->setValue(delay);
}


KMovingConfig::~KMovingConfig ()
{
    if (standAlone)
        delete config;
}

KMovingConfig::KMovingConfig (bool _standAlone, KConfig *_config, QWidget *parent, const char *)
    : KCModule(parent, "kcmkwm"), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout (this, 0, KDialog::spacingHint());

    windowsBox = new QButtonGroup(i18n("Windows"), this);
    windowsBox->setColumnLayout( 0, Qt::Horizontal );

    QBoxLayout *wLay = new QVBoxLayout (windowsBox->layout(), KDialog::spacingHint());

    QBoxLayout *bLay = new QVBoxLayout;
    wLay->addLayout(bLay);

    opaque = new QCheckBox(i18n("Di&splay content in moving windows"), windowsBox);
    bLay->addWidget(opaque);
    QWhatsThis::add( opaque, i18n("Enable this option if you want a window's content to be fully shown"
                                  " while moving it, instead of just showing a window 'skeleton'. The result may not be satisfying"
                                  " on slow machines without graphic acceleration.") );

    resizeOpaqueOn = new QCheckBox(i18n("Display content in &resizing windows"), windowsBox);
    bLay->addWidget(resizeOpaqueOn);
    QWhatsThis::add( resizeOpaqueOn, i18n("Enable this option if you want a window's content to be shown"
                                          " while resizing it, instead of just showing a window 'skeleton'. The result may not be satisfying"
                                          " on slow machines.") );

    geometryTipOn = new QCheckBox(i18n("Display window &geometry when moving or resizing"), windowsBox);
    bLay->addWidget(geometryTipOn);
    QWhatsThis::add(geometryTipOn, i18n("Enable this option if you want a window's geometry to be displayed"
			    		" while it is being moved or resized. The window position relative"
					" to the top-left corner of the screen is displayed together with"
					" its size."));

    QGridLayout *rLay = new QGridLayout(2,3);
    bLay->addLayout(rLay);
    rLay->setColStretch(0,0);
    rLay->setColStretch(1,1);

    minimizeAnimOn = new QCheckBox(i18n("Animate minimi&ze and restore"),
                                   windowsBox);
    QWhatsThis::add( minimizeAnimOn, i18n("Enable this option if you want an animation shown when"
                                          " windows are minimized or restored." ) );
    rLay->addWidget(minimizeAnimOn,0,0);

    minimizeAnimSlider = new QSlider(0,10,10,0,QSlider::Horizontal, windowsBox);
    minimizeAnimSlider->setSteps(1, 1);
    // QSlider::Below clashes with a X11/X.h #define
    #undef Below
    minimizeAnimSlider->setTickmarks(QSlider::Below);
    rLay->addMultiCellWidget(minimizeAnimSlider,0,0,1,2);

    connect(minimizeAnimOn, SIGNAL(toggled(bool)), this, SLOT(setMinimizeAnim(bool)));
    connect(minimizeAnimSlider, SIGNAL(valueChanged(int)), this, SLOT(setMinimizeAnimSpeed(int)));

    minimizeAnimSlowLabel= new QLabel(i18n("Slow"),windowsBox);
    minimizeAnimSlowLabel->setAlignment(Qt::AlignTop|Qt::AlignLeft);
    rLay->addWidget(minimizeAnimSlowLabel,1,1);

    minimizeAnimFastLabel= new QLabel(i18n("Fast"),windowsBox);
    minimizeAnimFastLabel->setAlignment(Qt::AlignTop|Qt::AlignRight);
    rLay->addWidget(minimizeAnimFastLabel,1,2);

    wtstr = i18n("Here you can set the speed of the animation shown when windows are"
                 " minimized and restored. ");
    QWhatsThis::add( minimizeAnimSlider, wtstr );
    QWhatsThis::add( minimizeAnimSlowLabel, wtstr );
    QWhatsThis::add( minimizeAnimFastLabel, wtstr );

    moveResizeMaximized = new QCheckBox( i18n("Allow moving and resizing o&f maximized windows"), windowsBox);
    bLay->addWidget(moveResizeMaximized);
    QWhatsThis::add(moveResizeMaximized, i18n("When enabled, this feature activates the border of maximized windows"
                                              " and allows you to move or resize them,"
                                              " just like for normal windows"));

    QBoxLayout *vLay = new QHBoxLayout(bLay);

    QLabel *plcLabel = new QLabel(i18n("&Placement:"),windowsBox);

    placementCombo = new QComboBox(false, windowsBox);
    placementCombo->insertItem(i18n("Smart"), SMART_PLACEMENT);
    placementCombo->insertItem(i18n("Cascade"), CASCADE_PLACEMENT);
    placementCombo->insertItem(i18n("Random"), RANDOM_PLACEMENT);
    placementCombo->insertItem(i18n("Centered"), CENTERED_PLACEMENT);
    placementCombo->insertItem(i18n("Zero-Cornered"), ZEROCORNERED_PLACEMENT);
    // CT: disabling is needed as long as functionality misses in kwin
    //placementCombo->insertItem(i18n("Interactive"), INTERACTIVE_PLACEMENT);
    //placementCombo->insertItem(i18n("Manual"), MANUAL_PLACEMENT);
    placementCombo->setCurrentItem(SMART_PLACEMENT);

    // FIXME, when more policies have been added to KWin
    wtstr = i18n("The placement policy determines where a new window"
                 " will appear on the desktop. For now, there are three different policies:"
                 " <ul><li><em>Smart</em> will try to achieve a minimum overlap of windows</li>"
                 " <li><em>Cascade</em> will cascade the windows</li>"
                 " <li><em>Random</em> will use a random position</li></ul>") ;

    QWhatsThis::add( plcLabel, wtstr);
    QWhatsThis::add( placementCombo, wtstr);

    plcLabel->setBuddy(placementCombo);
    vLay->addWidget(plcLabel, 0);
    vLay->addWidget(placementCombo, 1, Qt::AlignLeft);

    bLay->addSpacing(10);

    lay->addWidget(windowsBox);

    //iTLabel = new QLabel(i18n("  Allowed overlap:\n"
    //                         "(% of desktop space)"),
    //             plcBox);
    //iTLabel->setAlignment(AlignTop|AlignHCenter);
    //pLay->addWidget(iTLabel,1,1);

    //interactiveTrigger = new QSpinBox(0, 500, 1, plcBox);
    //pLay->addWidget(interactiveTrigger,1,2);

    //pLay->addRowSpacing(2,KDialog::spacingHint());

    //lay->addWidget(plcBox);


    //CT 15mar98 - add EdgeResistance, BorderAttractor, WindowsAttractor config
    MagicBox = new QVButtonGroup(i18n("Snap Zones"), this);
    MagicBox->setMargin(15);

    BrdrSnap = new KIntNumInput(10, MagicBox);
    BrdrSnap->setSpecialValueText( i18n("none") );
    BrdrSnap->setRange( 0, MAX_BRDR_SNAP);
    BrdrSnap->setLabel(i18n("&Border snap zone:"));
    BrdrSnap->setSuffix(i18n(" pixels"));
    BrdrSnap->setSteps(1,10);
    QWhatsThis::add( BrdrSnap, i18n("Here you can set the snap zone for screen borders, i.e."
                                    " the 'strength' of the magnetic field which will make windows snap to the border when"
                                    " moved near it.") );

    WndwSnap = new KIntNumInput(10, MagicBox);
    WndwSnap->setSpecialValueText( i18n("none") );
    WndwSnap->setRange( 0, MAX_WNDW_SNAP);
    WndwSnap->setLabel(i18n("&Window snap zone:"));
    WndwSnap->setSuffix( i18n(" pixels"));
    BrdrSnap->setSteps(1,10);
    QWhatsThis::add( WndwSnap, i18n("Here you can set the snap zone for windows, i.e."
                                    " the 'strength' of the magnetic field which will make windows snap to each other when"
                                    " they're moved near another window.") );

    OverlapSnap=new QCheckBox(i18n("Snap windows onl&y when overlapping"),MagicBox);
    QWhatsThis::add( OverlapSnap, i18n("Here you can set that windows will be only"
                                       " snapped if you try to overlap them, i.e. they won't be snapped if the windows"
                                       " comes only near another window or border.") );

    lay->addWidget(MagicBox);
    lay->addStretch();

    load();

    // Any changes goes to slotChanged()
    connect( opaque, SIGNAL(clicked()), SLOT(changed()));
    connect( resizeOpaqueOn, SIGNAL(clicked()), SLOT(changed()));
    connect( geometryTipOn, SIGNAL(clicked()), SLOT(changed()));
    connect( minimizeAnimOn, SIGNAL(clicked() ), SLOT(changed()));
    connect( minimizeAnimSlider, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect( moveResizeMaximized, SIGNAL(toggled(bool)), SLOT(changed()));
    connect( placementCombo, SIGNAL(activated(int)), SLOT(changed()));
    connect( BrdrSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect( WndwSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect( OverlapSnap, SIGNAL(clicked()), SLOT(changed()));
}

int KMovingConfig::getMove()
{
    return (opaque->isChecked())? OPAQUE : TRANSPARENT;
}

void KMovingConfig::setMove(int trans)
{
    opaque->setChecked(trans == OPAQUE);
}

void KMovingConfig::setGeometryTip(bool showGeometryTip)
{
    geometryTipOn->setChecked(showGeometryTip);
}

bool KMovingConfig::getGeometryTip()
{
    return geometryTipOn->isChecked();
}

// placement policy --- CT 31jan98 ---
int KMovingConfig::getPlacement()
{
    return placementCombo->currentItem();
}

void KMovingConfig::setPlacement(int plac)
{
    placementCombo->setCurrentItem(plac);
}

bool KMovingConfig::getMinimizeAnim()
{
    return minimizeAnimOn->isChecked();
}

int KMovingConfig::getMinimizeAnimSpeed()
{
    return minimizeAnimSlider->value();
}

void KMovingConfig::setMinimizeAnim(bool anim)
{
    minimizeAnimOn->setChecked( anim );
    minimizeAnimSlider->setEnabled( anim );
    minimizeAnimSlowLabel->setEnabled( anim );
    minimizeAnimFastLabel->setEnabled( anim );
}

void KMovingConfig::setMinimizeAnimSpeed(int speed)
{
    minimizeAnimSlider->setValue(speed);
}

int KMovingConfig::getResizeOpaque()
{
    return (resizeOpaqueOn->isChecked())? RESIZE_OPAQUE : RESIZE_TRANSPARENT;
}

void KMovingConfig::setResizeOpaque(int opaque)
{
    resizeOpaqueOn->setChecked(opaque == RESIZE_OPAQUE);
}

void KMovingConfig::setMoveResizeMaximized(bool a) {
    moveResizeMaximized->setChecked(a);
}

void KMovingConfig::load( void )
{
    QString key;

    config->setGroup( "Windows" );

    key = config->readEntry(KWIN_MOVE, "Opaque");
    if( key == "Transparent")
        setMove(TRANSPARENT);
    else if( key == "Opaque")
        setMove(OPAQUE);

    //CT 17Jun1998 - variable animation speed from 0 (none!!) to 10 (max)
    bool anim = config->readBoolEntry(KWIN_MINIMIZE_ANIM, true );
    int animSpeed = config->readNumEntry(KWIN_MINIMIZE_ANIM_SPEED, 5);
    if( animSpeed < 1 ) animSpeed = 0;
    if( animSpeed > 10 ) animSpeed = 10;
    setMinimizeAnim( anim );
    setMinimizeAnimSpeed( animSpeed );

    // DF: please keep the default consistent with kwin (options.cpp line 145)
    key = config->readEntry(KWIN_RESIZE_OPAQUE, "Opaque");
    if( key == "Opaque")
        setResizeOpaque(RESIZE_OPAQUE);
    else if ( key == "Transparent")
        setResizeOpaque(RESIZE_TRANSPARENT);

    //KS 10Jan2003 - Geometry Tip during window move/resize
    bool showGeomTip = config->readBoolEntry(KWIN_GEOMETRY, false);
    setGeometryTip( showGeomTip );

    // placement policy --- CT 19jan98 ---
    key = config->readEntry(KWIN_PLACEMENT);
    //CT 13mar98 interactive placement
//   if( key.left(11) == "interactive") {
//     setPlacement(INTERACTIVE_PLACEMENT);
//     int comma_pos = key.find(',');
//     if (comma_pos < 0)
//       interactiveTrigger->setValue(0);
//     else
//       interactiveTrigger->setValue (key.right(key.length()
//                           - comma_pos).toUInt(0));
//     iTLabel->setEnabled(true);
//     interactiveTrigger->show();
//   }
//   else {
//     interactiveTrigger->setValue(0);
//     iTLabel->setEnabled(false);
//     interactiveTrigger->hide();
    if( key == "Random")
        setPlacement(RANDOM_PLACEMENT);
    else if( key == "Cascade")
        setPlacement(CASCADE_PLACEMENT); //CT 31jan98
    //CT 31mar98 manual placement
    else if( key == "manual")
        setPlacement(MANUAL_PLACEMENT);
    else if( key == "Centered")
        setPlacement(CENTERED_PLACEMENT);
    else if( key == "ZeroCornered")
        setPlacement(ZEROCORNERED_PLACEMENT);

    else
        setPlacement(SMART_PLACEMENT);
//  }

    setMoveResizeMaximized(config->readBoolEntry(KWIN_MOVE_RESIZE_MAXIMIZED, true));

    int v;

    v = config->readNumEntry(KWM_BRDR_SNAP_ZONE, KWM_BRDR_SNAP_ZONE_DEFAULT);
    if (v > MAX_BRDR_SNAP) setBorderSnapZone(MAX_BRDR_SNAP);
    else if (v < 0) setBorderSnapZone (0);
    else setBorderSnapZone(v);

    v = config->readNumEntry(KWM_WNDW_SNAP_ZONE, KWM_WNDW_SNAP_ZONE_DEFAULT);
    if (v > MAX_WNDW_SNAP) setWindowSnapZone(MAX_WNDW_SNAP);
    else if (v < 0) setWindowSnapZone (0);
    else setWindowSnapZone(v);

    OverlapSnap->setChecked(config->readBoolEntry("SnapOnlyWhenOverlapping",false));
    emit KCModule::changed(false);
}

void KMovingConfig::save( void )
{
    int v;

    config->setGroup( "Windows" );

    v = getMove();
    if (v == TRANSPARENT)
        config->writeEntry(KWIN_MOVE,"Transparent");
    else
        config->writeEntry(KWIN_MOVE,"Opaque");

    config->writeEntry(KWIN_GEOMETRY, getGeometryTip());

    // placement policy --- CT 31jan98 ---
    v =getPlacement();
    if (v == RANDOM_PLACEMENT)
        config->writeEntry(KWIN_PLACEMENT, "Random");
    else if (v == CASCADE_PLACEMENT)
        config->writeEntry(KWIN_PLACEMENT, "Cascade");
    else if (v == CENTERED_PLACEMENT)
        config->writeEntry(KWIN_PLACEMENT, "Centered");
    else if (v == ZEROCORNERED_PLACEMENT)
        config->writeEntry(KWIN_PLACEMENT, "ZeroCornered");
//CT 13mar98 manual and interactive placement
//   else if (v == MANUAL_PLACEMENT)
//     config->writeEntry(KWIN_PLACEMENT, "Manual");
//   else if (v == INTERACTIVE_PLACEMENT) {
//       QString tmpstr = QString("Interactive,%1").arg(interactiveTrigger->value());
//       config->writeEntry(KWIN_PLACEMENT, tmpstr);
//   }
    else
        config->writeEntry(KWIN_PLACEMENT, "Smart");

    config->writeEntry(KWIN_MINIMIZE_ANIM, getMinimizeAnim());
    config->writeEntry(KWIN_MINIMIZE_ANIM_SPEED, getMinimizeAnimSpeed());

    v = getResizeOpaque();
    if (v == RESIZE_OPAQUE)
        config->writeEntry(KWIN_RESIZE_OPAQUE, "Opaque");
    else
        config->writeEntry(KWIN_RESIZE_OPAQUE, "Transparent");

    config->writeEntry(KWIN_MOVE_RESIZE_MAXIMIZED, moveResizeMaximized->isChecked());


    config->writeEntry(KWM_BRDR_SNAP_ZONE,getBorderSnapZone());
    config->writeEntry(KWM_WNDW_SNAP_ZONE,getWindowSnapZone());
    config->writeEntry("SnapOnlyWhenOverlapping",OverlapSnap->isChecked());

    if (standAlone)
    {
        config->sync();
        if ( !kapp->dcopClient()->isAttached() )
            kapp->dcopClient()->attach();
        kapp->dcopClient()->send("kwin*", "", "reconfigure()", "");
    }
    emit KCModule::changed(false);
}

void KMovingConfig::defaults()
{
    setMove(OPAQUE);
    setResizeOpaque(RESIZE_TRANSPARENT);
    setGeometryTip(false);
    setPlacement(SMART_PLACEMENT);
    setMoveResizeMaximized(true);

    //copied from kcontrol/konq/kwindesktop, aleXXX
    setWindowSnapZone(KWM_WNDW_SNAP_ZONE_DEFAULT);
    setBorderSnapZone(KWM_BRDR_SNAP_ZONE_DEFAULT);
    OverlapSnap->setChecked(false);

    setMinimizeAnim( true );
    setMinimizeAnimSpeed( 5 );
    emit KCModule::changed(true);
}

int KMovingConfig::getBorderSnapZone() {
  return BrdrSnap->value();
}

void KMovingConfig::setBorderSnapZone(int pxls) {
  BrdrSnap->setValue(pxls);
}

int KMovingConfig::getWindowSnapZone() {
  return WndwSnap->value();
}

void KMovingConfig::setWindowSnapZone(int pxls) {
  WndwSnap->setValue(pxls);
}

#include "windows.moc"
