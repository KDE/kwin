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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 */

#include <config.h>

#include <QDir>
#include <QLayout>
#include <QSlider>

#include <Q3ButtonGroup>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>
#include <QComboBox>
//Added by qt3to4:
#include <QGridLayout>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QVBoxLayout>
#include <kmessagebox.h>

#include <kactivelabel.h>
#include <klocale.h>
#include <kcolorbutton.h>
#include <kconfig.h>
#include <knuminput.h>
#include <kapplication.h>
#include <kdialog.h>
#include <kglobal.h>
#include <kprocess.h>
#include <QTabWidget>
#include <dbus/qdbus.h>

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
#define KWIN_GEOMETRY              "GeometryTip"
#define KWIN_AUTORAISE_INTERVAL    "AutoRaiseInterval"
#define KWIN_AUTORAISE             "AutoRaise"
#define KWIN_DELAYFOCUS_INTERVAL   "DelayFocusInterval"
#define KWIN_DELAYFOCUS            "DelayFocus"
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
#define KWIN_HIDE_UTILITY          "HideUtilityWindowsForInactive"

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
KFocusConfig::KFocusConfig (bool _standAlone, KConfig *_config, KInstance *inst, QWidget * parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout(this);
    lay->setMargin(0);
    lay->setSpacing(KDialog::spacingHint());

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
    fcsBox = new Q3ButtonGroup(i18n("Focus"),this);
    fcsBox->setColumnLayout( 0, Qt::Horizontal );

    QBoxLayout *fLay = new QVBoxLayout();
    fLay->setSpacing(KDialog::spacingHint());
    fcsBox->layout()->addItem( fLay );

    QBoxLayout *cLay = new QHBoxLayout();
    fLay->addLayout( cLay );
    QLabel *fLabel = new QLabel(i18n("&Policy:"), fcsBox);
    cLay->addWidget(fLabel, 0);
    focusCombo =  new QComboBox(fcsBox);
    focusCombo->setEditable( false );
    focusCombo->addItem(i18n("Click to Focus"), CLICK_TO_FOCUS);
    focusCombo->addItem(i18n("Focus Follows Mouse"), FOCUS_FOLLOWS_MOUSE);
    focusCombo->addItem(i18n("Focus Under Mouse"), FOCUS_UNDER_MOUSE);
    focusCombo->addItem(i18n("Focus Strictly Under Mouse"), FOCUS_STRICTLY_UNDER_MOUSE);
    cLay->addWidget(focusCombo,1 ,Qt::AlignLeft);
    fLabel->setBuddy(focusCombo);

    // FIXME, when more policies have been added to KWin
    wtstr = i18n("The focus policy is used to determine the active window, i.e."
                                      " the window you can work in. <ul>"
                                      " <li><em>Click to focus:</em> A window becomes active when you click into it."
                                      " This is the behavior you might know from other operating systems.</li>"
                                      " <li><em>Focus follows mouse:</em> Moving the mouse pointer actively on to a"
                                      " normal window activates it. New windows will receive the focus,"
                                      " without you having to point the mouse at them explicitly."
                                      " Very practical if you are using the mouse a lot.</li>"
                                      " <li><em>Focus under mouse:</em> The window that happens to be under the"
                                      " mouse pointer is active. If the mouse points nowhere, the last window"
                                      " that was under the mouse has focus."
                                      " New windows will not automatically receive the focus.</li>"
                                      " <li><em>Focus strictly under mouse:</em> Only the window under the mouse pointer is"
                                      " active. If the mouse points nowhere, nothing has focus."
                                      " </ul>"
                                      "Note that 'Focus under mouse' and 'Focus strictly under mouse' prevent certain"
                                      " features such as the Alt+Tab walk through windows dialog in the KDE mode"
                                      " from working properly."
                         );
    focusCombo->setWhatsThis( wtstr);
    fLabel->setWhatsThis( wtstr);

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

    connect(focusCombo, SIGNAL(activated(int)), this, SLOT(setDelayFocusEnabled()) );

    delayFocusOn = new QCheckBox(i18n("Delay focus"), fcsBox);
    fLay->addWidget(delayFocusOn);
    connect(delayFocusOn,SIGNAL(toggled(bool)), this, SLOT(delayFocusOnTog(bool)));

    delayFocus = new KIntNumInput(500, fcsBox);
    delayFocus->setLabel(i18n("Dela&y:"), Qt::AlignVCenter|Qt::AlignLeft);
    delayFocus->setRange(0, 3000, 100, true);
    delayFocus->setSteps(100,100);
    delayFocus->setSuffix(i18n(" msec"));
    fLay->addWidget(delayFocus);

    clickRaiseOn = new QCheckBox(i18n("C&lick raise active window"), fcsBox);
    connect(clickRaiseOn,SIGNAL(toggled(bool)), this, SLOT(clickRaiseOnTog(bool)));
    fLay->addWidget(clickRaiseOn);

//     fLay->addColSpacing(0,qMax(autoRaiseOn->sizeHint().width(),
//                                clickRaiseOn->sizeHint().width()) + 15);

    autoRaiseOn->setWhatsThis( i18n("When this option is enabled, a window in the background will automatically"
                                       " come to the front when the mouse pointer has been over it for some time.") );
    wtstr = i18n("This is the delay after which the window that the mouse pointer is over will automatically"
                 " come to the front.");
    autoRaise->setWhatsThis( wtstr );

    clickRaiseOn->setWhatsThis( i18n("When this option is enabled, the active window will be brought to the"
                                        " front when you click somewhere into the window contents. To change"
                                        " it for inactive windows, you need to change the settings"
                                        " in the Actions tab.") );

    delayFocusOn->setWhatsThis( i18n("When this option is enabled, there will be a delay after which the"
                                        " window the mouse pointer is over will become active (receive focus).") );
    delayFocus->setWhatsThis( i18n("This is the delay after which the window the mouse pointer is over"
                                       " will automatically receive focus.") );

    lay->addWidget(fcsBox);

    kbdBox = new Q3ButtonGroup(i18n("Navigation"), this);
    kbdBox->setColumnLayout( 0, Qt::Horizontal );
    QVBoxLayout *kLay = new QVBoxLayout();
    kLay->setSpacing(KDialog::spacingHint());
    kbdBox->layout()->addItem( kLay );

    altTabPopup = new QCheckBox( i18n("Show window list while switching windows"), kbdBox );
    kLay->addWidget( altTabPopup );

    wtstr = i18n("Hold down the Alt key and press the Tab key repeatedly to walk"
                 " through the windows on the current desktop (the Alt+Tab"
                 " combination can be reconfigured).\n\n"
                 "If this checkbox is checked"
                 " a popup widget is shown, displaying the icons of all windows to"
                 " walk through and the title of the currently selected one.\n\n"
                 "Otherwise, the focus is passed to a new window each time Tab"
                 " is pressed, with no popup widget.  In addition, the previously"
                 " activated window will be sent to the back in this mode.");
    altTabPopup->setWhatsThis( wtstr );
    connect(focusCombo, SIGNAL(activated(int)), this, SLOT(updateAltTabMode()));

    traverseAll = new QCheckBox( i18n( "&Traverse windows on all desktops" ), kbdBox );
    kLay->addWidget( traverseAll );

    wtstr = i18n( "Leave this option disabled if you want to limit walking through"
                  " windows to the current desktop." );
    traverseAll->setWhatsThis( wtstr );

    rollOverDesktops = new QCheckBox( i18n("Desktop navi&gation wraps around"), kbdBox );
    kLay->addWidget(rollOverDesktops);

    wtstr = i18n( "Enable this option if you want keyboard or active desktop border navigation beyond"
                  " the edge of a desktop to take you to the opposite edge of the new desktop." );
    rollOverDesktops->setWhatsThis( wtstr );

    showPopupinfo = new QCheckBox( i18n("Popup desktop name on desktop &switch"), kbdBox );
    kLay->addWidget(showPopupinfo);

    wtstr = i18n( "Enable this option if you wish to see the current desktop"
                  " name popup whenever the current desktop is changed." );
    showPopupinfo->setWhatsThis( wtstr );

    lay->addWidget(kbdBox);

    lay->addStretch();

    // Any changes goes to slotChanged()
    connect(focusCombo, SIGNAL(activated(int)), SLOT(changed()));
    connect(fcsBox, SIGNAL(clicked(int)), SLOT(changed()));
    connect(autoRaise, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(delayFocus, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(altTabPopup, SIGNAL(clicked()), SLOT(changed()));
    connect(traverseAll, SIGNAL(clicked()), SLOT(changed()));
    connect(rollOverDesktops, SIGNAL(clicked()), SLOT(changed()));
    connect(showPopupinfo, SIGNAL(clicked()), SLOT(changed()));

    load();
}


int KFocusConfig::getFocus()
{
    return focusCombo->currentIndex();
}

void KFocusConfig::setFocus(int foc)
{
    focusCombo->setCurrentIndex(foc);

    // this will disable/hide the auto raise delay widget if focus==click
    setAutoRaiseEnabled();
    updateAltTabMode();
}

void KFocusConfig::updateAltTabMode()
{
    // not KDE-style Alt+Tab with unreasonable focus policies
    altTabPopup->setEnabled( focusCombo->currentIndex() == 0 || focusCombo->currentIndex() == 1 );
}

void KFocusConfig::setAutoRaiseInterval(int tb)
{
    autoRaise->setValue(tb);
}

void KFocusConfig::setDelayFocusInterval(int tb)
{
    delayFocus->setValue(tb);
}

int KFocusConfig::getAutoRaiseInterval()
{
    return autoRaise->value();
}

int KFocusConfig::getDelayFocusInterval()
{
    return delayFocus->value();
}

void KFocusConfig::setAutoRaise(bool on)
{
    autoRaiseOn->setChecked(on);
}

void KFocusConfig::setDelayFocus(bool on)
{
    delayFocusOn->setChecked(on);
}

void KFocusConfig::setClickRaise(bool on)
{
    clickRaiseOn->setChecked(on);
}

void KFocusConfig::setAutoRaiseEnabled()
{
    // the auto raise related widgets are: autoRaise
    if ( focusCombo->currentIndex() != CLICK_TO_FOCUS )
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

void KFocusConfig::setDelayFocusEnabled()
{
    // the delayed focus related widgets are: delayFocus
    if ( focusCombo->currentIndex() != CLICK_TO_FOCUS )
    {
        delayFocusOn->setEnabled(true);
        delayFocusOnTog(delayFocusOn->isChecked());
    }
    else
    {
        delayFocusOn->setEnabled(false);
        delayFocusOnTog(false);
    }
}

void KFocusConfig::autoRaiseOnTog(bool a) {
    autoRaise->setEnabled(a);
    clickRaiseOn->setEnabled( !a );
}

void KFocusConfig::delayFocusOnTog(bool a) {
    delayFocus->setEnabled(a);
}

void KFocusConfig::clickRaiseOnTog(bool ) {
}

void KFocusConfig::setAltTabMode(bool a) {
    altTabPopup->setChecked(a);
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

    int k = config->readEntry(KWIN_AUTORAISE_INTERVAL,750);
    setAutoRaiseInterval(k);

    k = config->readEntry(KWIN_DELAYFOCUS_INTERVAL,750);
    setDelayFocusInterval(k);

    key = config->readEntry(KWIN_AUTORAISE);
    setAutoRaise(key == "on");
    key = config->readEntry(KWIN_DELAYFOCUS);
    setDelayFocus(key == "on");
    key = config->readEntry(KWIN_CLICKRAISE);
    setClickRaise(key != "off");
    setAutoRaiseEnabled();      // this will disable/hide the auto raise delay widget if focus==click
    setDelayFocusEnabled();

    key = config->readEntry(KWIN_ALTTABMODE, "KDE");
    setAltTabMode(key == "KDE");

    setRollOverDesktops( config->readEntry(KWIN_ROLL_OVER_DESKTOPS, QVariant(true )).toBool());

    config->setGroup( "PopupInfo" );
    setShowPopupinfo( config->readEntry(KWIN_SHOW_POPUP, QVariant(false )).toBool());

    config->setGroup( "TabBox" );
    setTraverseAll( config->readEntry(KWIN_TRAVERSE_ALL, QVariant(false )).toBool());

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

    v = getDelayFocusInterval();
    if (v <0) v = 0;
    config->writeEntry(KWIN_DELAYFOCUS_INTERVAL,v);

    if (autoRaiseOn->isChecked())
        config->writeEntry(KWIN_AUTORAISE, "on");
    else
        config->writeEntry(KWIN_AUTORAISE, "off");

    if (delayFocusOn->isChecked())
        config->writeEntry(KWIN_DELAYFOCUS, "on");
    else
        config->writeEntry(KWIN_DELAYFOCUS, "off");

    if (clickRaiseOn->isChecked())
        config->writeEntry(KWIN_CLICKRAISE, "on");
    else
        config->writeEntry(KWIN_CLICKRAISE, "off");

    if (altTabPopup->isChecked())
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
        QDBusInterfacePtr kwin( "org.kde.kwin", "/KWin", "org.kde.KWin" );
        kwin->call( "reconfigure" );
    }
    emit KCModule::changed(false);
}

void KFocusConfig::defaults()
{
    setAutoRaiseInterval(0);
    setDelayFocusInterval(0);
    setFocus(CLICK_TO_FOCUS);
    setAutoRaise(false);
    setDelayFocus(false);
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

KAdvancedConfig::KAdvancedConfig (bool _standAlone, KConfig *_config, KInstance *inst, QWidget *parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout (this);
    lay->setMargin(0);
    lay->setSpacing(KDialog::spacingHint());

    //iTLabel = new QLabel(i18n("  Allowed overlap:\n"
    //                         "(% of desktop space)"),
    //             plcBox);
    //iTLabel->setAlignment(AlignTop|AlignHCenter);
    //pLay->addWidget(iTLabel,1,1);

    //interactiveTrigger = new QSpinBox(0, 500, 1, plcBox);
    //pLay->addWidget(interactiveTrigger,1,2);

    //pLay->addRowSpacing(2,KDialog::spacingHint());

    //lay->addWidget(plcBox);

    shBox = new Q3VButtonGroup(i18n("Shading"), this);

    animateShade = new QCheckBox(i18n("Anima&te"), shBox);
    animateShade->setWhatsThis( i18n("Animate the action of reducing the window to its titlebar (shading)"
                                       " as well as the expansion of a shaded window") );

    shadeHoverOn = new QCheckBox(i18n("&Enable hover"), shBox);

    connect(shadeHoverOn, SIGNAL(toggled(bool)), this, SLOT(shadeHoverChanged(bool)));

    shadeHover = new KIntNumInput(500, shBox);
    shadeHover->setLabel(i18n("Dela&y:"), Qt::AlignVCenter|Qt::AlignLeft);
    shadeHover->setRange(0, 3000, 100, true);
    shadeHover->setSteps(100, 100);
    shadeHover->setSuffix(i18n(" msec"));

    shadeHoverOn->setWhatsThis( i18n("If Shade Hover is enabled, a shaded window will un-shade automatically "
                                       "when the mouse pointer has been over the title bar for some time."));

    wtstr = i18n("Sets the time in milliseconds before the window unshades "
                "when the mouse pointer goes over the shaded window.");
    shadeHover->setWhatsThis( wtstr);

    lay->addWidget(shBox);

    // Any changes goes to slotChanged()
    connect(animateShade, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(shadeHoverOn, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(shadeHover, SIGNAL(valueChanged(int)), SLOT(changed()));

    electricBox = new Q3VButtonGroup(i18n("Active Desktop Borders"), this);
    electricBox->layout()->setMargin(15);

    electricBox->setWhatsThis( i18n("If this option is enabled, moving the mouse to a screen border"
       " will change your desktop. This is e.g. useful if you want to drag windows from one desktop"
       " to the other.") );
    active_disable = new QRadioButton(i18n("D&isabled"), electricBox);
    active_move    = new QRadioButton(i18n("Only &when moving windows"), electricBox);
    active_always  = new QRadioButton(i18n("A&lways enabled"), electricBox);

    delays = new KIntNumInput(10, electricBox);
    delays->setRange(0, MAX_EDGE_RES, 50, true);
    delays->setSuffix(i18n(" msec"));
    delays->setLabel(i18n("Desktop &switch delay:"));
    delays->setWhatsThis( i18n("Here you can set a delay for switching desktops using the active"
       " borders feature. Desktops will be switched after the mouse has been pushed against a screen border"
       " for the specified number of milliseconds.") );

    connect( electricBox, SIGNAL(clicked(int)), this, SLOT(setEBorders()));

    // Any changes goes to slotChanged()
    connect(electricBox, SIGNAL(clicked(int)), SLOT(changed()));
    connect(delays, SIGNAL(valueChanged(int)), SLOT(changed()));

    lay->addWidget(electricBox);

    QHBoxLayout* focusStealingLayout = new QHBoxLayout();
    focusStealingLayout->setSpacing(KDialog::spacingHint());
    lay->addLayout( focusStealingLayout );
    QLabel* focusStealingLabel = new QLabel( i18n( "Focus stealing prevention level:" ), this );
    focusStealing = new QComboBox( this );
    focusStealing->addItem( i18nc( "Focus Stealing Prevention Level", "None" ));
    focusStealing->addItem( i18nc( "Focus Stealing Prevention Level", "Low" ));
    focusStealing->addItem( i18nc( "Focus Stealing Prevention Level", "Normal" ));
    focusStealing->addItem( i18nc( "Focus Stealing Prevention Level", "High" ));
    focusStealing->addItem( i18nc( "Focus Stealing Prevention Level", "Extreme" ));
    focusStealingLabel->setBuddy( focusStealing );
    focusStealingLayout->addWidget( focusStealingLabel );
    focusStealingLayout->addWidget( focusStealing, Qt::AlignLeft );
    wtstr = i18n( "<p>This option specifies how much KWin will try to prevent unwanted focus stealing "
                  "caused by unexpected activation of new windows. (Note: This feature does not "
                  "work with the Focus Under Mouse or Focus Strictly Under Mouse focus policies.)"
                  "<ul>"
                  "<li><em>None:</em> Prevention is turned off "
                  "and new windows always become activated.</li>"
                  "<li><em>Low:</em> Prevention is enabled; when some window does not have support "
                  "for the underlying mechanism and KWin cannot reliably decide whether to "
                  "activate the window or not, it will be activated. This setting may have both "
                  "worse and better results than normal level, depending on the applications.</li>"
                  "<li><em>Normal:</em> Prevention is enabled.</li>"
                  "<li><em>High:</em> New windows get activated only if no window is currently active "
                  "or if they belong to the currently active application. This setting is probably "
                  "not really usable when not using mouse focus policy.</li>"
                  "<li><em>Extreme:</em> All windows must be explicitly activated by the user.</li>"
                  "</ul></p>"
                  "<p>Windows that are prevented from stealing focus are marked as demanding attention, "
                  "which by default means their taskbar entry will be highlighted. This can be changed "
                  "in the Notifications control module.</p>" );
    focusStealing->setWhatsThis( wtstr );
    focusStealingLabel->setWhatsThis( wtstr );
    connect(focusStealing, SIGNAL(activated(int)), SLOT(changed()));
    
    hideUtilityWindowsForInactive = new QCheckBox( i18n( "Hide utility windows for inactive applications" ), this );
    hideUtilityWindowsForInactive->setWhatsThis(
        i18n( "When turned on, utility windows (tool windows, torn-off menus,...) of inactive applications will be"
              " hidden and will be shown only when the application becomes active. Note that applications"
              " have to mark the windows with the proper window type for this feature to work." ));
    connect(hideUtilityWindowsForInactive, SIGNAL(toggled(bool)), SLOT(changed()));
    lay->addWidget( hideUtilityWindowsForInactive );

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
    l = qMax( 0, qMin( 4, l ));
    focusStealing->setCurrentIndex(l);
}

void KAdvancedConfig::setHideUtilityWindowsForInactive(bool s) {
    hideUtilityWindowsForInactive->setChecked( s );
}

void KAdvancedConfig::load( void )
{
    config->setGroup( "Windows" );

    setAnimateShade(config->readEntry(KWIN_ANIMSHADE, QVariant(true)).toBool());
    setShadeHover(config->readEntry(KWIN_SHADEHOVER, QVariant(false)).toBool());
    setShadeHoverInterval(config->readEntry(KWIN_SHADEHOVER_INTERVAL, 250));

    setElectricBorders(config->readEntry(KWM_ELECTRIC_BORDER, false));
    setElectricBorderDelay(config->readEntry(KWM_ELECTRIC_BORDER_DELAY, 150));

//    setFocusStealing( config->readEntry(KWIN_FOCUS_STEALING, 2 ));
    // TODO default to low for now
    setFocusStealing( config->readEntry(KWIN_FOCUS_STEALING, 1 ));
    setHideUtilityWindowsForInactive( config->readEntry( KWIN_HIDE_UTILITY, QVariant(true )).toBool());

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

    config->writeEntry(KWIN_FOCUS_STEALING, focusStealing->currentIndex());
    config->writeEntry(KWIN_HIDE_UTILITY, hideUtilityWindowsForInactive->isChecked());

    if (standAlone)
    {
        config->sync();
        QDBusInterfacePtr kwin( "org.kde.kwin", "/KWin", "org.kde.KWin" );
        kwin->call( "reconfigure" );
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
    setHideUtilityWindowsForInactive( true );
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

KMovingConfig::KMovingConfig (bool _standAlone, KConfig *_config, KInstance *inst, QWidget *parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout (this);
    lay->setMargin(0);
    lay->setSpacing(KDialog::spacingHint());

    windowsBox = new Q3ButtonGroup(i18n("Windows"), this);
    windowsBox->setColumnLayout( 0, Qt::Horizontal );

    QBoxLayout *wLay = new QVBoxLayout ();
    wLay->setSpacing(KDialog::spacingHint());
    windowsBox->layout()->addItem( wLay );

    QBoxLayout *bLay = new QVBoxLayout;
    wLay->addLayout(bLay);

    opaque = new QCheckBox(i18n("Di&splay content in moving windows"), windowsBox);
    bLay->addWidget(opaque);
    opaque->setWhatsThis( i18n("Enable this option if you want a window's content to be fully shown"
                                  " while moving it, instead of just showing a window 'skeleton'. The result may not be satisfying"
                                  " on slow machines without graphic acceleration.") );

    resizeOpaqueOn = new QCheckBox(i18n("Display content in &resizing windows"), windowsBox);
    bLay->addWidget(resizeOpaqueOn);
    resizeOpaqueOn->setWhatsThis( i18n("Enable this option if you want a window's content to be shown"
                                          " while resizing it, instead of just showing a window 'skeleton'. The result may not be satisfying"
                                          " on slow machines.") );

    geometryTipOn = new QCheckBox(i18n("Display window &geometry when moving or resizing"), windowsBox);
    bLay->addWidget(geometryTipOn);
    geometryTipOn->setWhatsThis( i18n("Enable this option if you want a window's geometry to be displayed"
                                        " while it is being moved or resized. The window position relative"
                                        " to the top-left corner of the screen is displayed together with"
                                        " its size."));

    QGridLayout *rLay = new QGridLayout();
    bLay->addLayout(rLay);
    rLay->setColumnStretch(0,0);
    rLay->setColumnStretch(1,1);

    minimizeAnimOn = new QCheckBox(i18n("Animate minimi&ze and restore"),
                                   windowsBox);
    minimizeAnimOn->setWhatsThis( i18n("Enable this option if you want an animation shown when"
                                          " windows are minimized or restored." ) );
    rLay->addWidget(minimizeAnimOn,0,0);

    minimizeAnimSlider = new QSlider(windowsBox);
    minimizeAnimSlider->setRange( 0, 10 );
    minimizeAnimSlider->setSingleStep( 1 );
    minimizeAnimSlider->setPageStep( 1 );
    minimizeAnimSlider->setValue( 0 );
    minimizeAnimSlider->setOrientation( Qt::Horizontal );
    minimizeAnimSlider->setTickPosition(QSlider::TicksBelow);
    rLay->addWidget(minimizeAnimSlider,0,0,1,2);

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
    minimizeAnimSlider->setWhatsThis( wtstr );
    minimizeAnimSlowLabel->setWhatsThis( wtstr );
    minimizeAnimFastLabel->setWhatsThis( wtstr );

    moveResizeMaximized = new QCheckBox( i18n("Allow moving and resizing o&f maximized windows"), windowsBox);
    bLay->addWidget(moveResizeMaximized);
    moveResizeMaximized->setWhatsThis( i18n("When enabled, this feature activates the border of maximized windows"
                                              " and allows you to move or resize them,"
                                              " just like for normal windows"));

    QBoxLayout *vLay = new QHBoxLayout();
    bLay->addLayout( vLay );

    QLabel *plcLabel = new QLabel(i18n("&Placement:"),windowsBox);

    placementCombo = new QComboBox(windowsBox);
    placementCombo->setEditable( false );
    placementCombo->addItem(i18n("Smart"), SMART_PLACEMENT);
    placementCombo->addItem(i18n("Maximizing"), MAXIMIZING_PLACEMENT);
    placementCombo->addItem(i18n("Cascade"), CASCADE_PLACEMENT);
    placementCombo->addItem(i18n("Random"), RANDOM_PLACEMENT);
    placementCombo->addItem(i18n("Centered"), CENTERED_PLACEMENT);
    placementCombo->addItem(i18n("Zero-Cornered"), ZEROCORNERED_PLACEMENT);
    // CT: disabling is needed as long as functionality misses in kwin
    //placementCombo->addItem(i18n("Interactive"), INTERACTIVE_PLACEMENT);
    //placementCombo->addItem(i18n("Manual"), MANUAL_PLACEMENT);
    placementCombo->setCurrentIndex(SMART_PLACEMENT);

    // FIXME, when more policies have been added to KWin
    wtstr = i18n("The placement policy determines where a new window"
                 " will appear on the desktop."
                 " <ul>"
                 " <li><em>Smart</em> will try to achieve a minimum overlap of windows</li>"
                 " <li><em>Maximizing</em> will try to maximize every window to fill the whole screen."
                 " It might be useful to selectively affect placement of some windows using"
                 " the window-specific settings.</li>"
                 " <li><em>Cascade</em> will cascade the windows</li>"
                 " <li><em>Random</em> will use a random position</li>"
                 " <li><em>Centered</em> will place the window centered</li>"
                 " <li><em>Zero-Cornered</em> will place the window in the top-left corner</li>"
                 "</ul>") ;

    plcLabel->setWhatsThis( wtstr);
    placementCombo->setWhatsThis( wtstr);

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
    MagicBox = new Q3VButtonGroup(i18n("Snap Zones"), this);
    MagicBox->layout()->setMargin(15);

    BrdrSnap = new KIntNumInput(10, MagicBox);
    BrdrSnap->setSpecialValueText( i18n("none") );
    BrdrSnap->setRange( 0, MAX_BRDR_SNAP);
    BrdrSnap->setLabel(i18n("&Border snap zone:"));
    BrdrSnap->setSteps(1,10);
    BrdrSnap->setWhatsThis( i18n("Here you can set the snap zone for screen borders, i.e."
                                    " the 'strength' of the magnetic field which will make windows snap to the border when"
                                    " moved near it.") );

    WndwSnap = new KIntNumInput(10, MagicBox);
    WndwSnap->setSpecialValueText( i18n("none") );
    WndwSnap->setRange( 0, MAX_WNDW_SNAP);
    WndwSnap->setLabel(i18n("&Window snap zone:"));
    BrdrSnap->setSteps(1,10);
    WndwSnap->setWhatsThis( i18n("Here you can set the snap zone for windows, i.e."
                                    " the 'strength' of the magnetic field which will make windows snap to each other when"
                                    " they're moved near another window.") );

    OverlapSnap=new QCheckBox(i18n("Snap windows onl&y when overlapping"),MagicBox);
    OverlapSnap->setWhatsThis( i18n("Here you can set that windows will be only"
                                       " snapped if you try to overlap them, i.e. they will not be snapped if the windows"
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
    connect( BrdrSnap, SIGNAL(valueChanged(int)), SLOT(slotBrdrSnapChanged(int)));
    connect( WndwSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect( WndwSnap, SIGNAL(valueChanged(int)), SLOT(slotWndwSnapChanged(int)));
    connect( OverlapSnap, SIGNAL(clicked()), SLOT(changed()));

    // To get suffix to BrdrSnap and WndwSnap inputs with default values.
    slotBrdrSnapChanged(BrdrSnap->value());
    slotWndwSnapChanged(WndwSnap->value());
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
    return placementCombo->currentIndex();
}

void KMovingConfig::setPlacement(int plac)
{
    placementCombo->setCurrentIndex(plac);
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

void KMovingConfig::slotBrdrSnapChanged(int value) {
    BrdrSnap->setSuffix(i18np(" pixel", " pixels", value));
}

void KMovingConfig::slotWndwSnapChanged(int value) {
    WndwSnap->setSuffix(i18np(" pixel", " pixels", value));
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
    bool anim = config->readEntry(KWIN_MINIMIZE_ANIM, QVariant(true )).toBool();
    int animSpeed = config->readEntry(KWIN_MINIMIZE_ANIM_SPEED, 5);
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
    bool showGeomTip = config->readEntry(KWIN_GEOMETRY, QVariant(false)).toBool();
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
    else if( key == "Maximizing")
        setPlacement(MAXIMIZING_PLACEMENT);
    else
        setPlacement(SMART_PLACEMENT);
//  }

    setMoveResizeMaximized(config->readEntry(KWIN_MOVE_RESIZE_MAXIMIZED, QVariant(false)).toBool());

    int v;

    v = config->readEntry(KWM_BRDR_SNAP_ZONE, KWM_BRDR_SNAP_ZONE_DEFAULT);
    if (v > MAX_BRDR_SNAP) setBorderSnapZone(MAX_BRDR_SNAP);
    else if (v < 0) setBorderSnapZone (0);
    else setBorderSnapZone(v);

    v = config->readEntry(KWM_WNDW_SNAP_ZONE, KWM_WNDW_SNAP_ZONE_DEFAULT);
    if (v > MAX_WNDW_SNAP) setWindowSnapZone(MAX_WNDW_SNAP);
    else if (v < 0) setWindowSnapZone (0);
    else setWindowSnapZone(v);

    OverlapSnap->setChecked(config->readEntry("SnapOnlyWhenOverlapping", QVariant(false)).toBool());
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
    else if (v == MAXIMIZING_PLACEMENT)
        config->writeEntry(KWIN_PLACEMENT, "Maximizing");
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
        QDBusInterfacePtr kwin( "org.kde.kwin", "/KWin", "org.kde.KWin" );
        kwin->call( "reconfigure" );
    }
    emit KCModule::changed(false);
}

void KMovingConfig::defaults()
{
    setMove(OPAQUE);
    setResizeOpaque(RESIZE_TRANSPARENT);
    setGeometryTip(false);
    setPlacement(SMART_PLACEMENT);
    setMoveResizeMaximized(false);

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

KTranslucencyConfig::~KTranslucencyConfig ()
{
    if (standAlone)
        delete config;
    if (kompmgr)
        kompmgr->detach();
}

KTranslucencyConfig::KTranslucencyConfig (bool _standAlone, KConfig *_config, KInstance *inst, QWidget *parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
  kompmgr = 0L;
  resetKompmgr_ = false;
  QVBoxLayout *lay = new QVBoxLayout (this);
  kompmgrAvailable_ = kompmgrAvailable();
  if (!kompmgrAvailable_){
  KActiveLabel *label = new KActiveLabel(i18n("<qt><b>It seems that alpha channel support is not available.</b><br><br>"
                                 "Please make sure you have "
                                 "<a href=\"http://www.freedesktop.org/\">Xorg &ge; 6.8</a>,"
                                 " and have installed the kompmgr that came with kwin.<br>"
                                 "Also, make sure you have the following entries in your XConfig (e.g. /etc/X11/xorg.conf):<br><br>"
                                 "<i>Section \"Extensions\"<br>"
                                 "Option \"Composite\" \"Enable\"<br>"
                                 "EndSection</i><br><br>"
                                 "And if your GPU provides hardware-accelerated Xrender support (mainly nVidia cards):<br><br>"
                                 "<i>Option     \"RenderAccel\" \"true\"</i><br>"
                                 "In <i>Section \"Device\"</i></qt>"), this);
  lay->addWidget(label);
  }
  else
  {
  QTabWidget *tabW = new QTabWidget(this);
  QWidget *tGroup = new QWidget(tabW);
  QVBoxLayout *vLay = new QVBoxLayout (tGroup);
  vLay->setMargin(KDialog::marginHint());
  vLay->setSpacing(KDialog::spacingHint());
  vLay->addSpacing(11); // to get the proper gb top offset
  
  onlyDecoTranslucent = new QCheckBox(i18n("Apply translucency only to decoration"),tGroup);
  vLay->addWidget(onlyDecoTranslucent);
  
  vLay->addSpacing(11);
  
  QGridLayout *gLay = new QGridLayout();
  gLay->setSpacing(KDialog::spacingHint());
  gLay->setColumnStretch(1,1);
  vLay->addLayout( gLay );

  activeWindowTransparency = new QCheckBox(i18n("Active windows:"),tGroup);
  gLay->addWidget(activeWindowTransparency,0,0);
  activeWindowOpacity = new KIntNumInput(100, tGroup);
  activeWindowOpacity->setRange(0,100);
  activeWindowOpacity->setSuffix("%");
  gLay->addWidget(activeWindowOpacity,0,1);

  inactiveWindowTransparency = new QCheckBox(i18n("Inactive windows:"),tGroup);
  gLay->addWidget(inactiveWindowTransparency,1,0);
  inactiveWindowOpacity = new KIntNumInput(100, tGroup);
  inactiveWindowOpacity->setRange(0,100);
  inactiveWindowOpacity->setSuffix("%");
  gLay->addWidget(inactiveWindowOpacity,1,1);

  movingWindowTransparency = new QCheckBox(i18n("Moving windows:"),tGroup);
  gLay->addWidget(movingWindowTransparency,2,0);
  movingWindowOpacity = new KIntNumInput(100, tGroup);
  movingWindowOpacity->setRange(0,100);
  movingWindowOpacity->setSuffix("%");
  gLay->addWidget(movingWindowOpacity,2,1);

  dockWindowTransparency = new QCheckBox(i18n("Dock windows:"),tGroup);
  gLay->addWidget(dockWindowTransparency,3,0);
  dockWindowOpacity = new KIntNumInput(100, tGroup);
  dockWindowOpacity->setRange(0,100);
  dockWindowOpacity->setSuffix("%");
  gLay->addWidget(dockWindowOpacity,3,1);
  
  vLay->addSpacing(11);

  keepAboveAsActive = new QCheckBox(i18n("Treat 'keep above' windows as active ones"),tGroup);
  vLay->addWidget(keepAboveAsActive);

  disableARGB = new QCheckBox(i18n("Disable ARGB windows (ignores window alpha maps, fixes gtk1 apps)"),tGroup);
  vLay->addWidget(disableARGB);

  vLay->addStretch();
  tabW->addTab(tGroup, i18n("Opacity"));

  QWidget *sGroup = new QWidget(tabW);
//   sGroup->setCheckable(true);
  QVBoxLayout *vLay2 = new QVBoxLayout (sGroup);
  vLay2->setMargin(11);
  vLay2->setSpacing(6);
  vLay2->addSpacing(11); // to get the proper gb top offset
  useShadows = new QCheckBox(i18n("Use shadows"),sGroup);
  vLay2->addWidget(useShadows);
  
  vLay2->addSpacing(11);

  QGridLayout *gLay2 = new QGridLayout();
  gLay2->setColumnStretch(1,1);
  vLay2->addLayout( gLay2 );

  QLabel *label1 = new QLabel(i18n("Active window size:"),sGroup);
  gLay2->addWidget(label1,0,0);
  activeWindowShadowSize = new KIntNumInput(12,sGroup);
  activeWindowShadowSize->setRange(0,32);
//   activeWindowShadowSize->setSuffix("px");
  gLay2->addWidget(activeWindowShadowSize,0,1);

  QLabel *label2 = new QLabel(i18n("Inactive window size:"),sGroup);
  gLay2->addWidget(label2,1,0);
  inactiveWindowShadowSize = new KIntNumInput(6,sGroup);
  inactiveWindowShadowSize->setRange(0,32);
//   inactiveWindowShadowSize->setSuffix("px");
  gLay2->addWidget(inactiveWindowShadowSize,1,1);

  QLabel *label3 = new QLabel(i18n("Dock window size:"),sGroup);
  gLay2->addWidget(label3,2,0);
  dockWindowShadowSize = new KIntNumInput(6,sGroup);
  dockWindowShadowSize->setRange(0,32);
//   dockWindowShadowSize->setSuffix("px");
  gLay2->addWidget(dockWindowShadowSize,2,1);

  QLabel *label4 = new QLabel(i18n("Vertical offset:"),sGroup);
  gLay2->addWidget(label4,3,0);
  shadowTopOffset = new KIntNumInput(80,sGroup);
  shadowTopOffset->setSuffix("%");
  shadowTopOffset->setRange(-200,200);
  gLay2->addWidget(shadowTopOffset,3,1);

  QLabel *label5 = new QLabel(i18n("Horizontal offset:"),sGroup);
  gLay2->addWidget(label5,4,0);
  shadowLeftOffset = new KIntNumInput(0,sGroup);
  shadowLeftOffset->setSuffix("%");
  shadowLeftOffset->setRange(-200,200);
  gLay2->addWidget(shadowLeftOffset,4,1);

  QLabel *label6 = new QLabel(i18n("Shadow color:"),sGroup);
  gLay2->addWidget(label6,5,0);
  shadowColor = new KColorButton(Qt::black,sGroup);
  gLay2->addWidget(shadowColor,5,1);
  gLay2->setColumnStretch(1,1);
  vLay2->addSpacing(11);
  removeShadowsOnMove = new QCheckBox(i18n("Remove shadows on move"),sGroup);
  vLay2->addWidget(removeShadowsOnMove);
  removeShadowsOnResize = new QCheckBox(i18n("Remove shadows on resize"),sGroup);
  vLay2->addWidget(removeShadowsOnResize);
  vLay2->addStretch();
  tabW->addTab(sGroup, i18n("Shadows"));

  QWidget *eGroup = new QWidget(this);
  QVBoxLayout *vLay3 = new QVBoxLayout (eGroup);
  vLay3->setMargin( 11 );
  vLay3->setSpacing( 6 );

  fadeInWindows = new QCheckBox(i18n("Fade-in windows (including popups)"),eGroup);
  fadeOnOpacityChange = new QCheckBox(i18n("Fade between opacity changes"),eGroup);
  fadeInSpeed = new KIntNumInput(100, eGroup);
  fadeInSpeed->setRange(1,100);
  fadeInSpeed->setLabel("Fade-in speed:");
  fadeOutSpeed = new KIntNumInput(100, eGroup);
  fadeOutSpeed->setRange(1,100);
  fadeOutSpeed->setLabel("Fade-out speed:");
  vLay3->addWidget(fadeInWindows);
  vLay3->addWidget(fadeOnOpacityChange);
  vLay3->addWidget(fadeInSpeed);
  vLay3->addWidget(fadeOutSpeed);
  vLay3->addStretch();

  tabW->addTab(eGroup, i18n("Effects"));

  useTranslucency = new QCheckBox(i18n("Use translucency/shadows"),this);
  lay->addWidget(useTranslucency);
  lay->addWidget(tabW);

  connect(useTranslucency, SIGNAL(toggled(bool)), tabW, SLOT(setEnabled(bool)));

  connect(activeWindowTransparency, SIGNAL(toggled(bool)), activeWindowOpacity, SLOT(setEnabled(bool)));
  connect(inactiveWindowTransparency, SIGNAL(toggled(bool)), inactiveWindowOpacity, SLOT(setEnabled(bool)));
  connect(movingWindowTransparency, SIGNAL(toggled(bool)), movingWindowOpacity, SLOT(setEnabled(bool)));
  connect(dockWindowTransparency, SIGNAL(toggled(bool)), dockWindowOpacity, SLOT(setEnabled(bool)));

  connect(useTranslucency, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(onlyDecoTranslucent, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(activeWindowTransparency, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(inactiveWindowTransparency, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(movingWindowTransparency, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(dockWindowTransparency, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(keepAboveAsActive, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(disableARGB, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(useShadows, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(removeShadowsOnResize, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(removeShadowsOnMove, SIGNAL(toggled(bool)), SLOT(changed()));

  connect(activeWindowOpacity, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(inactiveWindowOpacity, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(movingWindowOpacity, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(dockWindowOpacity, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(dockWindowShadowSize, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(activeWindowShadowSize, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(inactiveWindowShadowSize, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(shadowTopOffset, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(shadowLeftOffset, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(shadowColor, SIGNAL(changed(const QColor&)), SLOT(changed()));
  connect(fadeInWindows, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(fadeOnOpacityChange, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(fadeInSpeed, SIGNAL(valueChanged(int)), SLOT(changed()));
  connect(fadeOutSpeed, SIGNAL(valueChanged(int)), SLOT(changed()));

  connect(useShadows, SIGNAL(toggled(bool)), dockWindowShadowSize, SLOT(setEnabled(bool)));
  connect(useShadows, SIGNAL(toggled(bool)), activeWindowShadowSize, SLOT(setEnabled(bool)));
  connect(useShadows, SIGNAL(toggled(bool)), inactiveWindowShadowSize, SLOT(setEnabled(bool)));
  connect(useShadows, SIGNAL(toggled(bool)), shadowTopOffset, SLOT(setEnabled(bool)));
  connect(useShadows, SIGNAL(toggled(bool)), shadowLeftOffset, SLOT(setEnabled(bool)));
  connect(useShadows, SIGNAL(toggled(bool)), shadowColor, SLOT(setEnabled(bool)));

  load();

  tabW->setEnabled(useTranslucency->isChecked());

  connect(useTranslucency, SIGNAL(toggled(bool)), this, SLOT(showWarning(bool)));

  // handle kompmgr restarts if necessary
  connect(useTranslucency, SIGNAL(toggled(bool)), SLOT(resetKompmgr()));
  connect(disableARGB, SIGNAL(toggled(bool)), SLOT(resetKompmgr()));
  connect(useShadows, SIGNAL(toggled(bool)), SLOT(resetKompmgr()));
  connect(inactiveWindowShadowSize, SIGNAL(valueChanged(int)), SLOT(resetKompmgr()));
  connect(shadowTopOffset, SIGNAL(valueChanged(int)), SLOT(resetKompmgr()));
  connect(shadowLeftOffset, SIGNAL(valueChanged(int)), SLOT(resetKompmgr()));
  connect(shadowColor, SIGNAL(changed(const QColor&)), SLOT(resetKompmgr()));
  connect(fadeInWindows, SIGNAL(toggled(bool)), SLOT(resetKompmgr()));
  connect(fadeOnOpacityChange, SIGNAL(toggled(bool)), SLOT(resetKompmgr()));
  connect(fadeInSpeed, SIGNAL(valueChanged(int)), SLOT(resetKompmgr()));
  connect(fadeOutSpeed, SIGNAL(valueChanged(int)), SLOT(resetKompmgr()));

  }
}

void KTranslucencyConfig::resetKompmgr()
{
    resetKompmgr_ = true;
}
void KTranslucencyConfig::load( void )
{

    if (!kompmgrAvailable_)
        return;
  config->setGroup( "Notification Messages" );
  useTranslucency->setChecked(config->readEntry("UseTranslucency", QVariant(false)).toBool());

  config->setGroup( "Translucency" );
  activeWindowTransparency->setChecked(config->readEntry("TranslucentActiveWindows", QVariant(false)).toBool());
  inactiveWindowTransparency->setChecked(config->readEntry("TranslucentInactiveWindows", QVariant(true)).toBool());
  movingWindowTransparency->setChecked(config->readEntry("TranslucentMovingWindows", QVariant(false)).toBool());
  removeShadowsOnMove->setChecked(config->readEntry("RemoveShadowsOnMove", QVariant(false)).toBool());
  removeShadowsOnResize->setChecked(config->readEntry("RemoveShadowsOnResize", QVariant(false)).toBool());
  dockWindowTransparency->setChecked(config->readEntry("TranslucentDocks", QVariant(true)).toBool());
  keepAboveAsActive->setChecked(config->readEntry("TreatKeepAboveAsActive", QVariant(true)).toBool());
  onlyDecoTranslucent->setChecked(config->readEntry("OnlyDecoTranslucent", QVariant(false)).toBool());

  activeWindowOpacity->setValue(config->readEntry("ActiveWindowOpacity",100));
  inactiveWindowOpacity->setValue(config->readEntry("InactiveWindowOpacity",75));
  movingWindowOpacity->setValue(config->readEntry("MovingWindowOpacity",25));
  dockWindowOpacity->setValue(config->readEntry("DockOpacity",80));

  int ass, iss, dss;
  dss = config->readEntry("DockShadowSize", 33);
  ass = config->readEntry("ActiveWindowShadowSize", 133);
  iss = config->readEntry("InactiveWindowShadowSize", 67);

  activeWindowOpacity->setEnabled(activeWindowTransparency->isChecked());
  inactiveWindowOpacity->setEnabled(inactiveWindowTransparency->isChecked());
  movingWindowOpacity->setEnabled(movingWindowTransparency->isChecked());
  dockWindowOpacity->setEnabled(dockWindowTransparency->isChecked());

  KConfig conf_(QDir::homePath() + "/.xcompmgrrc");
  conf_.setGroup("xcompmgr");
  
  disableARGB->setChecked(conf_.readEntry("DisableARGB", QVariant(false)).toBool());

  useShadows->setChecked(conf_.readEntry("Compmode","CompClientShadows").compare("CompClientShadows") == 0);
  shadowTopOffset->setValue(-1*(conf_.readEntry("ShadowOffsetY",-80)));
  shadowLeftOffset->setValue(-1*(conf_.readEntry("ShadowOffsetX",0)));

  int ss =  conf_.readEntry("ShadowRadius",6);
  dockWindowShadowSize->setValue((int)(dss*ss/100.0));
  activeWindowShadowSize->setValue((int)(ass*ss/100.0));
  inactiveWindowShadowSize->setValue((int)(iss*ss/100.0));

  QString hex = conf_.readEntry("ShadowColor","#000000");
  uint r, g, b;
  r = g = b = 256;

  if (sscanf(hex.toLatin1(), "0x%02x%02x%02x", &r, &g, &b)!=3 || r > 255 || g > 255 || b > 255)
    shadowColor->setColor(Qt::black);
  else
    shadowColor->setColor(QColor(r,g,b));

  fadeInWindows->setChecked(conf_.readEntry("FadeWindows", QVariant(true)).toBool());
  fadeOnOpacityChange->setChecked(conf_.readEntry("FadeTrans", QVariant(false)).toBool());
  fadeInSpeed->setValue((int)(conf_.readEntry("FadeInStep",0.020)*1000.0));
  fadeOutSpeed->setValue((int)(conf_.readEntry("FadeOutStep",0.070)*1000.0));

  emit KCModule::changed(false);
}

void KTranslucencyConfig::save( void )
{
    if (!kompmgrAvailable_)
        return;
  config->setGroup( "Notification Messages" );
  config->writeEntry("UseTranslucency",useTranslucency->isChecked());

  config->setGroup( "Translucency" );
  config->writeEntry("TranslucentActiveWindows",activeWindowTransparency->isChecked());
  config->writeEntry("TranslucentInactiveWindows",inactiveWindowTransparency->isChecked());
  config->writeEntry("TranslucentMovingWindows",movingWindowTransparency->isChecked());
  config->writeEntry("TranslucentDocks",dockWindowTransparency->isChecked());
  config->writeEntry("TreatKeepAboveAsActive",keepAboveAsActive->isChecked());
  config->writeEntry("ActiveWindowOpacity",activeWindowOpacity->value());
  config->writeEntry("InactiveWindowOpacity",inactiveWindowOpacity->value());
  config->writeEntry("MovingWindowOpacity",movingWindowOpacity->value());
  config->writeEntry("DockOpacity",dockWindowOpacity->value());
  // for simplification:
  // xcompmgr supports a general shadow radius and additionally lets external apps set a multiplicator for each window
  // (speed reasons, so the shadow matrix hasn't to be recreated for every window)
  // we set inactive windows to 100%, the radius to the inactive window value and adjust the multiplicators for docks and active windows
  // this way the user can set the three values without caring about the radius/multiplicator stuff
   // additionally we find a value between big and small values to have a more smooth appereance
   config->writeEntry("DockShadowSize",(int)(200.0 * dockWindowShadowSize->value() / (activeWindowShadowSize->value() + inactiveWindowShadowSize->value())));
   config->writeEntry("ActiveWindowShadowSize",(int)(200.0 * activeWindowShadowSize->value() / (activeWindowShadowSize->value() + inactiveWindowShadowSize->value())));
   config->writeEntry("InctiveWindowShadowSize",(int)(200.0 * inactiveWindowShadowSize->value() / (activeWindowShadowSize->value() + inactiveWindowShadowSize->value())));
   
  config->writeEntry("RemoveShadowsOnMove",removeShadowsOnMove->isChecked());
  config->writeEntry("RemoveShadowsOnResize",removeShadowsOnResize->isChecked());
  config->writeEntry("OnlyDecoTranslucent", onlyDecoTranslucent->isChecked());
  config->writeEntry("ResetKompmgr",resetKompmgr_);

  KConfig *conf_ = new KConfig(QDir::homePath() + "/.xcompmgrrc");
  conf_->setGroup("xcompmgr");

  conf_->writeEntry("Compmode",useShadows->isChecked()?"CompClientShadows":"");
  conf_->writeEntry("DisableARGB",disableARGB->isChecked());
  conf_->writeEntry("ShadowOffsetY",-1*shadowTopOffset->value());
  conf_->writeEntry("ShadowOffsetX",-1*shadowLeftOffset->value());


  int r, g, b;
  shadowColor->color().getRgb( &r, &g, &b );
  QString hex;
  hex.sprintf("0x%02X%02X%02X", r,g,b);
  conf_->writeEntry("ShadowColor",hex);
  conf_->writeEntry("ShadowRadius",(activeWindowShadowSize->value() + inactiveWindowShadowSize->value()) / 2);
  conf_->writeEntry("FadeWindows",fadeInWindows->isChecked());
  conf_->writeEntry("FadeTrans",fadeOnOpacityChange->isChecked());
  conf_->writeEntry("FadeInStep",fadeInSpeed->value()/1000.0);
  conf_->writeEntry("FadeOutStep",fadeOutSpeed->value()/1000.0);

  delete conf_;

  if (standAlone)
  {
    config->sync();
    QDBusInterfacePtr kwin( "org.kde.kwin", "/KWin", "org.kde.KWin" );
    kwin->call( "reconfigure" );
  }
  emit KCModule::changed(false);
}

void KTranslucencyConfig::defaults()
{
    if (!kompmgrAvailable_)
        return;
  useTranslucency->setChecked(false);
  onlyDecoTranslucent->setChecked(false);
  activeWindowTransparency->setChecked(false);
  inactiveWindowTransparency->setChecked(true);
  movingWindowTransparency->setChecked(false);
  dockWindowTransparency->setChecked(true);
  keepAboveAsActive->setChecked(true);
  disableARGB->setChecked(false);

  activeWindowOpacity->setValue(100);
  inactiveWindowOpacity->setValue(75);
  movingWindowOpacity->setValue(25);
  dockWindowOpacity->setValue(80);

  dockWindowShadowSize->setValue(6);
  activeWindowShadowSize->setValue(12);
  inactiveWindowShadowSize->setValue(6);
  shadowTopOffset->setValue(80);
  shadowLeftOffset->setValue(0);

  activeWindowOpacity->setEnabled(false);
  inactiveWindowOpacity->setEnabled(true);
  movingWindowOpacity->setEnabled(false);
  dockWindowOpacity->setEnabled(true);
  useShadows->setChecked(true);
  removeShadowsOnMove->setChecked(false);
  removeShadowsOnResize->setChecked(false);
  shadowColor->setColor(Qt::black);
  fadeInWindows->setChecked(true);
  fadeOnOpacityChange->setChecked(false);
  fadeInSpeed->setValue(70);
  fadeOutSpeed->setValue(20);
  emit KCModule::changed(true);
}


bool KTranslucencyConfig::kompmgrAvailable()
{
    bool ret;
    KProcess proc;
    proc << "kompmgr" << "-v";
    ret = proc.start(KProcess::DontCare, KProcess::AllOutput);
    proc.detach();
    return ret;
}

void KTranslucencyConfig::showWarning(bool alphaActivated)
{
    if (alphaActivated)
        KMessageBox::information(this, i18n("<qt>Translucency support is new and may cause problems<br> including crashes (sometimes the translucency engine, seldom even X).</qt>"), i18n("Warning"));
}

#include "windows.moc"
