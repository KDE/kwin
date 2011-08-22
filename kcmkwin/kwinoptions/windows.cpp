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

#include <config-workspace.h>

#include <QApplication>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>
#include <KComboBox>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QtDBus/QtDBus>

#include <KButtonGroup>
#include <klocale.h>
#include <knuminput.h>
#include <kdialog.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "windows.h"

#include <kephal/screens.h>

// kwin config keywords
#define KWIN_FOCUS                 "FocusPolicy"
#define KWIN_PLACEMENT             "Placement"
#define KWIN_GEOMETRY              "GeometryTip"
#define KWIN_AUTORAISE_INTERVAL    "AutoRaiseInterval"
#define KWIN_AUTORAISE             "AutoRaise"
#define KWIN_DELAYFOCUS_INTERVAL   "DelayFocusInterval"
#define KWIN_CLICKRAISE            "ClickRaise"
#define KWIN_MOVE_RESIZE_MAXIMIZED "MoveResizeMaximizedWindows"
#define KWIN_SHADEHOVER            "ShadeHover"
#define KWIN_SHADEHOVER_INTERVAL   "ShadeHoverInterval"
#define KWIN_FOCUS_STEALING        "FocusStealingPreventionLevel"
#define KWIN_HIDE_UTILITY          "HideUtilityWindowsForInactive"
#define KWIN_INACTIVE_SKIP_TASKBAR "InactiveTabsSkipTaskbar"
#define KWIN_AUTOGROUP_SIMILAR     "AutogroupSimilarWindows"
#define KWIN_AUTOGROUP_FOREGROUND  "AutogroupInForeground"
#define KWIN_SEPARATE_SCREEN_FOCUS "SeparateScreenFocus"
#define KWIN_ACTIVE_MOUSE_SCREEN   "ActiveMouseScreen"
#define KWIN_TILINGON              "TilingOn"
#define KWIN_TILING_DEFAULT_LAYOUT "TilingDefaultLayout"
#define KWIN_TILING_RAISE_POLICY   "TilingRaisePolicy"

//CT 15mar 98 - magics
#define KWM_BRDR_SNAP_ZONE                   "BorderSnapZone"
#define KWM_BRDR_SNAP_ZONE_DEFAULT           10
#define KWM_WNDW_SNAP_ZONE                   "WindowSnapZone"
#define KWM_WNDW_SNAP_ZONE_DEFAULT           10
#define KWM_CNTR_SNAP_ZONE                   "CenterSnapZone"
#define KWM_CNTR_SNAP_ZONE_DEFAULT           0

#define MAX_BRDR_SNAP                          100
#define MAX_WNDW_SNAP                          100
#define MAX_CNTR_SNAP                          100
#define MAX_EDGE_RES                          1000


KFocusConfig::~KFocusConfig()
{
    if (standAlone)
        delete config;
}

// removed the LCD display over the slider - this is not good GUI design :) RNolden 051701
KFocusConfig::KFocusConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget * parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout(this);
    QLabel *label;

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
    //fcsBox = new QGroupBox(i18n("Focus"),this);
    fcsBox = new QWidget(this);

    QGridLayout *gLay = new QGridLayout();

    fcsBox->setLayout(gLay);

    focusCombo =  new KComboBox(fcsBox);
    focusCombo->setEditable(false);
    focusCombo->addItem(i18n("Click to Focus"), CLICK_TO_FOCUS);
    focusCombo->addItem(i18n("Focus Follows Mouse"), FOCUS_FOLLOWS_MOUSE);
    focusCombo->addItem(i18n("Focus Under Mouse"), FOCUS_UNDER_MOUSE);
    focusCombo->addItem(i18n("Focus Strictly Under Mouse"), FOCUS_STRICTLY_UNDER_MOUSE);
    focusCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(i18n("&Policy:"), this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(focusCombo);
    gLay->addWidget(label, 0, 0, 1, 2);
    gLay->addWidget(focusCombo, 0, 2);


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
    " active. If the mouse points nowhere, nothing has focus.</li>"
    " </ul>"
    "Note that 'Focus under mouse' and 'Focus strictly under mouse' prevent certain"
    " features such as the Alt+Tab walk through windows dialog in the KDE mode"
    " from working properly."
    );
    focusCombo->setWhatsThis(wtstr);

    connect(focusCombo, SIGNAL(activated(int)), this, SLOT(focusPolicyChanged()));

    focusStealing = new KComboBox(this);
    focusStealing->addItem(i18nc("Focus Stealing Prevention Level", "None"));
    focusStealing->addItem(i18nc("Focus Stealing Prevention Level", "Low"));
    focusStealing->addItem(i18nc("Focus Stealing Prevention Level", "Medium"));
    focusStealing->addItem(i18nc("Focus Stealing Prevention Level", "High"));
    focusStealing->addItem(i18nc("Focus Stealing Prevention Level", "Extreme"));
    wtstr = i18n("<p>This option specifies how much KWin will try to prevent unwanted focus stealing "
                 "caused by unexpected activation of new windows. (Note: This feature does not "
                 "work with the Focus Under Mouse or Focus Strictly Under Mouse focus policies.)"
                 "<ul>"
                 "<li><em>None:</em> Prevention is turned off "
                 "and new windows always become activated.</li>"
                 "<li><em>Low:</em> Prevention is enabled; when some window does not have support "
                 "for the underlying mechanism and KWin cannot reliably decide whether to "
                 "activate the window or not, it will be activated. This setting may have both "
                 "worse and better results than the medium level, depending on the applications.</li>"
                 "<li><em>Medium:</em> Prevention is enabled.</li>"
                 "<li><em>High:</em> New windows get activated only if no window is currently active "
                 "or if they belong to the currently active application. This setting is probably "
                 "not really usable when not using mouse focus policy.</li>"
                 "<li><em>Extreme:</em> All windows must be explicitly activated by the user.</li>"
                 "</ul></p>"
                 "<p>Windows that are prevented from stealing focus are marked as demanding attention, "
                 "which by default means their taskbar entry will be highlighted. This can be changed "
                 "in the Notifications control module.</p>");
    focusStealing->setWhatsThis(wtstr);
    connect(focusStealing, SIGNAL(activated(int)), SLOT(changed()));
    focusStealing->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(i18n("Focus stealing prevention level:"), this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(focusStealing);
    gLay->addWidget(label, 1, 0, 1, 2);
    gLay->addWidget(focusStealing, 1, 2);

    focusNextToMouse = new QCheckBox(/*TODO 4.9 i__18n*/("When the active window disappears, pass focus to window under mouse"), this);
    gLay->addWidget(focusNextToMouse, 2, 2, 1, 1);
    focusNextToMouse->hide();

    // autoraise delay
    autoRaiseOn = new QCheckBox(fcsBox);
    connect(autoRaiseOn, SIGNAL(toggled(bool)), this, SLOT(autoRaiseOnTog(bool)));
    autoRaise = new KIntNumInput(500, fcsBox);
    autoRaise->setRange(0, 3000, 100);
    autoRaise->setSteps(100, 100);
    autoRaise->setSuffix(i18n(" ms"));
    autoRaise->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    autoRaiseOnLabel = new QLabel(i18n("&Raise, with the following delay:"), this);
    autoRaiseOnLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    autoRaiseOnLabel->setBuddy(autoRaise);
    gLay->addWidget(autoRaiseOn, 3, 0);
    gLay->addWidget(autoRaiseOnLabel, 3, 1);
    gLay->addWidget(autoRaise, 3, 2);

    connect(focusCombo, SIGNAL(activated(int)), this, SLOT(setDelayFocusEnabled()));

    delayFocus = new KIntNumInput(500, fcsBox);
    delayFocus->setRange(0, 3000, 100);
    delayFocus->setSteps(100, 100);
    delayFocus->setSuffix(i18n(" ms"));
    delayFocus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    delayFocusOnLabel = new QLabel(i18n("Delay focus by:"), this);
    delayFocusOnLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    delayFocusOnLabel->setBuddy(delayFocus);

    gLay->addWidget(delayFocusOnLabel, 4, 1);
    gLay->addWidget(delayFocus, 4, 2);

    clickRaiseOn = new QCheckBox(i18n("C&lick raises active window"), fcsBox);
    connect(clickRaiseOn, SIGNAL(toggled(bool)), this, SLOT(clickRaiseOnTog(bool)));
    gLay->addWidget(clickRaiseOn, 5, 0, 1, 3);

    autoRaiseOn->setWhatsThis(i18n("When this option is enabled, a window in the background will automatically"
                                   " come to the front when the mouse pointer has been over it for some time."));
    wtstr = i18n("This is the delay after which the window that the mouse pointer is over will automatically"
                 " come to the front.");
    autoRaise->setWhatsThis(wtstr);

    clickRaiseOn->setWhatsThis(i18n("When this option is enabled, the active window will be brought to the"
                                    " front when you click somewhere into the window contents. To change"
                                    " it for inactive windows, you need to change the settings"
                                    " in the Actions tab."));

    delayFocus->setWhatsThis(i18n("This is the delay after which the window the mouse pointer is over"
                                  " will automatically receive focus."));

    separateScreenFocus = new QCheckBox(i18n("S&eparate screen focus"), fcsBox);
    gLay->addWidget(separateScreenFocus, 6, 0, 1, 3);
    wtstr = i18n("When this option is enabled, focus operations are limited only to the active Xinerama screen");
    separateScreenFocus->setWhatsThis(wtstr);

    activeMouseScreen = new QCheckBox(i18n("Active screen follows &mouse"), fcsBox);
    gLay->addWidget(activeMouseScreen, 7, 0, 1, 3);
    wtstr = i18n("When this option is enabled, the active Xinerama screen (where new windows appear, for example)"
                 " is the screen containing the mouse pointer. When disabled, the active Xinerama screen is the "
                 " screen containing the focused window. By default this option is disabled for Click to focus and"
                 " enabled for other focus policies.");
    activeMouseScreen->setWhatsThis(wtstr);
    connect(focusCombo, SIGNAL(activated(int)), this, SLOT(updateActiveMouseScreen()));

    if (Kephal::ScreenUtils::numScreens() == 1) { // No Ximerama
        separateScreenFocus->hide();
        activeMouseScreen->hide();
    }

    lay->addWidget(fcsBox);

    lay->addStretch();

    // Any changes goes to slotChanged()
    connect(focusCombo, SIGNAL(activated(int)), SLOT(changed()));
    connect(autoRaiseOn, SIGNAL(clicked()), SLOT(changed()));
    connect(clickRaiseOn, SIGNAL(clicked()), SLOT(changed()));
    connect(autoRaise, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(delayFocus, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(separateScreenFocus, SIGNAL(clicked()), SLOT(changed()));
    connect(activeMouseScreen, SIGNAL(clicked()), SLOT(changed()));
    connect(focusNextToMouse, SIGNAL(clicked()), SLOT(changed()));

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
    focusPolicyChanged();
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

void KFocusConfig::setClickRaise(bool on)
{
    clickRaiseOn->setChecked(on);
}

void KFocusConfig::focusPolicyChanged()
{
    int policyIndex = focusCombo->currentIndex();

    // the auto raise related widgets are: autoRaise
    autoRaiseOn->setEnabled(policyIndex != CLICK_TO_FOCUS);
    autoRaiseOnLabel->setEnabled(policyIndex != CLICK_TO_FOCUS);
    autoRaiseOnTog(policyIndex != CLICK_TO_FOCUS && autoRaiseOn->isChecked());

    focusStealing->setDisabled(policyIndex == FOCUS_UNDER_MOUSE || policyIndex == FOCUS_STRICTLY_UNDER_MOUSE);

    focusNextToMouse->setDisabled(policyIndex == FOCUS_UNDER_MOUSE || policyIndex == FOCUS_STRICTLY_UNDER_MOUSE);

}

void KFocusConfig::setDelayFocusEnabled()
{
    int policyIndex = focusCombo->currentIndex();

    // the delayed focus related widgets are: delayFocus
    delayFocusOnLabel->setEnabled(policyIndex != CLICK_TO_FOCUS);
    delayFocusOnTog(policyIndex != CLICK_TO_FOCUS);
}

void KFocusConfig::autoRaiseOnTog(bool a)
{
    autoRaise->setEnabled(a);
    clickRaiseOn->setEnabled(!a);
}

void KFocusConfig::delayFocusOnTog(bool a)
{
    delayFocus->setEnabled(a);
}

void KFocusConfig::clickRaiseOnTog(bool)
{
}

void KFocusConfig::setFocusStealing(int l)
{
    l = qMax(0, qMin(4, l));
    focusStealing->setCurrentIndex(l);
}

void KFocusConfig::setSeparateScreenFocus(bool s)
{
    separateScreenFocus->setChecked(s);
}

void KFocusConfig::setActiveMouseScreen(bool a)
{
    activeMouseScreen->setChecked(a);
}

void KFocusConfig::updateActiveMouseScreen()
{
    // on by default for non click to focus policies
    KConfigGroup cfg(config, "Windows");
    if (!cfg.hasKey(KWIN_ACTIVE_MOUSE_SCREEN))
        setActiveMouseScreen(focusCombo->currentIndex() != 0);
}

void KFocusConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
}

void KFocusConfig::load(void)
{
    QString key;

    KConfigGroup cg(config, "Windows");

    key = cg.readEntry(KWIN_FOCUS);
    if (key == "ClickToFocus")
        setFocus(CLICK_TO_FOCUS);
    else if (key == "FocusFollowsMouse")
        setFocus(FOCUS_FOLLOWS_MOUSE);
    else if (key == "FocusUnderMouse")
        setFocus(FOCUS_UNDER_MOUSE);
    else if (key == "FocusStrictlyUnderMouse")
        setFocus(FOCUS_STRICTLY_UNDER_MOUSE);

    int k = cg.readEntry(KWIN_AUTORAISE_INTERVAL, 750);
    setAutoRaiseInterval(k);

    k = cg.readEntry(KWIN_DELAYFOCUS_INTERVAL, 750);
    setDelayFocusInterval(k);

    setAutoRaise(cg.readEntry(KWIN_AUTORAISE, false));
    setClickRaise(cg.readEntry(KWIN_CLICKRAISE, true));
    focusPolicyChanged();      // this will disable/hide the auto raise delay widget if focus==click
    setDelayFocusEnabled();

    setSeparateScreenFocus(cg.readEntry(KWIN_SEPARATE_SCREEN_FOCUS, false));
    // on by default for non click to focus policies
    setActiveMouseScreen(cg.readEntry(KWIN_ACTIVE_MOUSE_SCREEN, focusCombo->currentIndex() != 0));

//    setFocusStealing( cg.readEntry(KWIN_FOCUS_STEALING, 2 ));
    // TODO default to low for now
    setFocusStealing(cg.readEntry(KWIN_FOCUS_STEALING, 1));

    focusNextToMouse->setChecked(cg.readEntry("NextFocusPrefersMouse", false));

    emit KCModule::changed(false);
}

void KFocusConfig::save(void)
{
    int v;

    KConfigGroup cg(config, "Windows");

    v = getFocus();
    if (v == CLICK_TO_FOCUS)
        cg.writeEntry(KWIN_FOCUS, "ClickToFocus");
    else if (v == FOCUS_UNDER_MOUSE)
        cg.writeEntry(KWIN_FOCUS, "FocusUnderMouse");
    else if (v == FOCUS_STRICTLY_UNDER_MOUSE)
        cg.writeEntry(KWIN_FOCUS, "FocusStrictlyUnderMouse");
    else
        cg.writeEntry(KWIN_FOCUS, "FocusFollowsMouse");

    v = getAutoRaiseInterval();
    if (v < 0) v = 0;
    cg.writeEntry(KWIN_AUTORAISE_INTERVAL, v);

    v = getDelayFocusInterval();
    if (v < 0) v = 0;
    cg.writeEntry(KWIN_DELAYFOCUS_INTERVAL, v);

    cg.writeEntry(KWIN_AUTORAISE, autoRaiseOn->isChecked());

    cg.writeEntry(KWIN_CLICKRAISE, clickRaiseOn->isChecked());

    cg.writeEntry(KWIN_SEPARATE_SCREEN_FOCUS, separateScreenFocus->isChecked());
    cg.writeEntry(KWIN_ACTIVE_MOUSE_SCREEN, activeMouseScreen->isChecked());

    cg.writeEntry(KWIN_FOCUS_STEALING, focusStealing->currentIndex());

    cg.writeEntry(KWIN_SEPARATE_SCREEN_FOCUS, separateScreenFocus->isChecked());
    cg.writeEntry(KWIN_ACTIVE_MOUSE_SCREEN, activeMouseScreen->isChecked());

    cg.writeEntry("NextFocusPrefersMouse", focusNextToMouse->isChecked());

    if (standAlone) {
        config->sync();
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
    emit KCModule::changed(false);
}

void KFocusConfig::defaults()
{
    setAutoRaiseInterval(0);
    setDelayFocusInterval(0);
    setFocus(CLICK_TO_FOCUS);
    setAutoRaise(false);
    setClickRaise(true);
    setSeparateScreenFocus(false);

//    setFocusStealing(2);
    // TODO default to low for now
    setFocusStealing(1);

    // on by default for non click to focus policies
    setActiveMouseScreen(focusCombo->currentIndex() != 0);
    setDelayFocusEnabled();
    focusNextToMouse->setChecked(false);
    emit KCModule::changed(true);
}

KAdvancedConfig::~KAdvancedConfig()
{
    if (standAlone)
        delete config;
}

KAdvancedConfig::KAdvancedConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget *parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QLabel *label;
    QVBoxLayout *lay = new QVBoxLayout(this);

    //iTLabel = new QLabel(i18n("  Allowed overlap:\n"
    //                         "(% of desktop space)"),
    //             plcBox);
    //iTLabel->setAlignment(AlignTop|AlignHCenter);
    //pLay->addWidget(iTLabel,1,1);

    //interactiveTrigger = new QSpinBox(0, 500, 1, plcBox);
    //pLay->addWidget(interactiveTrigger,1,2);

    //pLay->addRowSpacing(2,KDialog::spacingHint());

    //lay->addWidget(plcBox);

    shBox = new KButtonGroup(this);
    shBox->setTitle(i18n("Shading"));
    QGridLayout *kLay = new QGridLayout(shBox);

    shadeHoverOn = new QCheckBox(i18n("&Enable hover"), shBox);

    connect(shadeHoverOn, SIGNAL(toggled(bool)), this, SLOT(shadeHoverChanged(bool)));
    kLay->addWidget(shadeHoverOn, 0, 0, 1, 2);

    shadeHover = new KIntNumInput(500, shBox);
    shadeHover->setRange(0, 3000, 100);
    shadeHover->setSteps(100, 100);
    shadeHover->setSuffix(i18n(" ms"));

    shadeHoverOn->setWhatsThis(i18n("If Shade Hover is enabled, a shaded window will un-shade automatically "
                                    "when the mouse pointer has been over the title bar for some time."));

    wtstr = i18n("Sets the time in milliseconds before the window unshades "
                 "when the mouse pointer goes over the shaded window.");
    shadeHover->setWhatsThis(wtstr);
    shadeHover->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    shadeHoverLabel = new QLabel(i18n("Dela&y:"), this);
    shadeHoverLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    shadeHoverLabel->setBuddy(shadeHover);
    kLay->addWidget(shadeHoverLabel, 1, 0);
    kLay->addWidget(shadeHover, 1, 1);

    lay->addWidget(shBox);

    //----------------
    // Window tabbing

    wtBox = new KButtonGroup(this);
    wtBox->setTitle(i18n("Window Tabbing"));
    QVBoxLayout *wtLay = new QVBoxLayout(wtBox);

    inactiveTabsSkipTaskbar = new QCheckBox(i18n("Hide inactive window tabs from the taskbar"), this);
    inactiveTabsSkipTaskbar->setVisible(false);   // TODO: We want translations in case this is fixed...
    inactiveTabsSkipTaskbar->setWhatsThis(
        i18n("When turned on hide all tabs that are not active from the taskbar."));
    connect(inactiveTabsSkipTaskbar, SIGNAL(toggled(bool)), SLOT(changed()));
    wtLay->addWidget(inactiveTabsSkipTaskbar);

    autogroupSimilarWindows = new QCheckBox(i18n("Automatically group similar windows"), this);
    autogroupSimilarWindows->setWhatsThis(
        i18n("When turned on attempt to automatically detect when a newly opened window is related"
             " to an existing one and place them in the same window group."));
    connect(autogroupSimilarWindows, SIGNAL(toggled(bool)), SLOT(changed()));
    wtLay->addWidget(autogroupSimilarWindows);

    autogroupInForeground = new QCheckBox(i18n("Switch to automatically grouped windows immediately"), this);
    autogroupInForeground->setWhatsThis(
        i18n("When turned on immediately switch to any new window tabs that were automatically added"
             " to the current group."));
    connect(autogroupInForeground, SIGNAL(toggled(bool)), SLOT(changed()));
    wtLay->addWidget(autogroupInForeground);

    lay->addWidget(wtBox);

    //----------------

    // Any changes goes to slotChanged()
    connect(shadeHoverOn, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(shadeHover, SIGNAL(valueChanged(int)), SLOT(changed()));

    QGridLayout *vLay = new QGridLayout();
    lay->addLayout(vLay);

    placementCombo = new KComboBox(this);
    placementCombo->setEditable(false);
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

    placementCombo->setWhatsThis(wtstr);

    placementCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label = new QLabel(i18n("&Placement:"), this);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label->setBuddy(placementCombo);
    vLay->addWidget(label, 0, 0);
    vLay->addWidget(placementCombo, 0, 1);

    connect(placementCombo, SIGNAL(activated(int)), SLOT(changed()));

    hideUtilityWindowsForInactive = new QCheckBox(i18n("Hide utility windows for inactive applications"), this);
    hideUtilityWindowsForInactive->setWhatsThis(
        i18n("When turned on, utility windows (tool windows, torn-off menus,...) of inactive applications will be"
             " hidden and will be shown only when the application becomes active. Note that applications"
             " have to mark the windows with the proper window type for this feature to work."));
    connect(hideUtilityWindowsForInactive, SIGNAL(toggled(bool)), SLOT(changed()));
    vLay->addWidget(hideUtilityWindowsForInactive, 1, 0, 1, 2);

    tilBox = new KButtonGroup(this);
    tilBox->setTitle(i18n("Tiling"));
    QGridLayout *tilBoxLay = new QGridLayout(tilBox);

    tilingOn = new QCheckBox(i18n("Enable Tiling"), tilBox);
    tilingOn->setWhatsThis(
        i18n("A tiling window manager lays out all the windows in a non-overlapping manner."
             " This way all windows are always visible."));
    tilBoxLay->addWidget(tilingOn);
    connect(tilingOn, SIGNAL(toggled(bool)), SLOT(tilingOnChanged(bool)));
    connect(tilingOn, SIGNAL(toggled(bool)), SLOT(changed()));

    tilingLayoutLabel = new QLabel(i18n("Default Tiling &Layout"), tilBox);
    tilBoxLay->addWidget(tilingLayoutLabel, 1, 0);

    tilingLayoutCombo = new KComboBox(tilBox);

    // NOTE: add your layout to the bottom of this list
    tilingLayoutCombo->addItem(i18nc("Spiral tiling layout", "Spiral"));
    tilingLayoutCombo->addItem(i18nc("Two-column horizontal tiling layout", "Columns"));
    tilingLayoutCombo->addItem(i18nc("Floating layout, windows aren't tiled at all", "Floating"));

    tilingLayoutLabel->setBuddy(tilingLayoutCombo);
    connect(tilingLayoutCombo, SIGNAL(activated(int)), SLOT(changed()));
    tilBoxLay->addWidget(tilingLayoutCombo, 1, 1);

    tilingRaiseLabel = new QLabel(i18n("Floating &Windows Raising"), tilBox);
    tilBoxLay->addWidget(tilingRaiseLabel, 2, 0);

    tilingRaiseCombo = new KComboBox(tilBox);
    // when a floating window is activated, all other floating
    // windows are also brought to the front, above the tiled windows
    // when a tiled window is focused, all floating windows go to the back.
    // NOTE: If the user has explicitly set a client to "keep above others", that will be respected.
    tilingRaiseCombo->addItem(i18nc("Window Raising Policy", "Raise/Lower all floating windows"));
    tilingRaiseCombo->addItem(i18nc("Window Raising Policy", "Raise/Lower current window only"));
    tilingRaiseCombo->addItem(i18nc("Window Raising Policy", "Floating windows are always on top"));
    wtstr = i18n("The window raising policy determines how floating windows are stacked"
                 " <ul>"
                 " <li><em>Raise/Lower all</em> will raise all floating windows when a"
                 " floating window is activated.</li>"
                 " <li><em>Raise/Lower current</em> will raise only the current window.</li>"
                 " <li><em>Floating windows on top</em> will always keep floating windows on top, even"
                 " when a tiled window is activated."
                 "</ul>") ;
    tilingRaiseCombo->setWhatsThis(wtstr);
    connect(tilingRaiseCombo, SIGNAL(activated(int)), SLOT(changed()));
    tilingRaiseLabel->setBuddy(tilingRaiseCombo);
    tilBoxLay->addWidget(tilingRaiseCombo, 2, 1);

    lay->addWidget(tilBox);

    lay->addStretch();
    load();

}

void KAdvancedConfig::setShadeHover(bool on)
{
    shadeHoverOn->setChecked(on);
    shadeHoverLabel->setEnabled(on);
    shadeHover->setEnabled(on);
}

void KAdvancedConfig::setShadeHoverInterval(int k)
{
    shadeHover->setValue(k);
}

int KAdvancedConfig::getShadeHoverInterval()
{

    return shadeHover->value();
}

void KAdvancedConfig::shadeHoverChanged(bool a)
{
    shadeHoverLabel->setEnabled(a);
    shadeHover->setEnabled(a);
}

void KAdvancedConfig::setTilingOn(bool on)
{
    tilingOn->setChecked(on);
    tilingLayoutLabel->setEnabled(on);
    tilingLayoutCombo->setEnabled(on);
    tilingRaiseLabel->setEnabled(on);
    tilingRaiseCombo->setEnabled(on);
}

void KAdvancedConfig::setTilingLayout(int l)
{
    tilingLayoutCombo->setCurrentIndex(l);
}

void KAdvancedConfig::setTilingRaisePolicy(int l)
{
    tilingRaiseCombo->setCurrentIndex(l);
}

void KAdvancedConfig::tilingOnChanged(bool a)
{
    tilingLayoutLabel->setEnabled(a);
    tilingLayoutCombo->setEnabled(a);
    tilingRaiseLabel->setEnabled(a);
    tilingRaiseCombo->setEnabled(a);
}

void KAdvancedConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
}

void KAdvancedConfig::load(void)
{
    KConfigGroup cg(config, "Windows");

    setShadeHover(cg.readEntry(KWIN_SHADEHOVER, false));
    setShadeHoverInterval(cg.readEntry(KWIN_SHADEHOVER_INTERVAL, 250));

    QString key;
    // placement policy --- CT 19jan98 ---
    key = cg.readEntry(KWIN_PLACEMENT);
    //CT 13mar98 interactive placement
//   if ( key.left(11) == "interactive") {
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
    if (key == "Random")
        setPlacement(RANDOM_PLACEMENT);
    else if (key == "Cascade")
        setPlacement(CASCADE_PLACEMENT); //CT 31jan98
    //CT 31mar98 manual placement
    else if (key == "manual")
        setPlacement(MANUAL_PLACEMENT);
    else if (key == "Centered")
        setPlacement(CENTERED_PLACEMENT);
    else if (key == "ZeroCornered")
        setPlacement(ZEROCORNERED_PLACEMENT);
    else if (key == "Maximizing")
        setPlacement(MAXIMIZING_PLACEMENT);
    else
        setPlacement(SMART_PLACEMENT);
//  }

    setHideUtilityWindowsForInactive(cg.readEntry(KWIN_HIDE_UTILITY, true));
    setInactiveTabsSkipTaskbar(cg.readEntry(KWIN_INACTIVE_SKIP_TASKBAR, false));
    setAutogroupSimilarWindows(cg.readEntry(KWIN_AUTOGROUP_SIMILAR, false));
    setAutogroupInForeground(cg.readEntry(KWIN_AUTOGROUP_FOREGROUND, true));

    setTilingOn(cg.readEntry(KWIN_TILINGON, false));
    setTilingLayout(cg.readEntry(KWIN_TILING_DEFAULT_LAYOUT, 0));
    setTilingRaisePolicy(cg.readEntry(KWIN_TILING_RAISE_POLICY, 0));

    emit KCModule::changed(false);
}

void KAdvancedConfig::save(void)
{
    int v;

    KConfigGroup cg(config, "Windows");
    cg.writeEntry(KWIN_SHADEHOVER, shadeHoverOn->isChecked());

    v = getShadeHoverInterval();
    if (v < 0) v = 0;
    cg.writeEntry(KWIN_SHADEHOVER_INTERVAL, v);

    // placement policy --- CT 31jan98 ---
    v = getPlacement();
    if (v == RANDOM_PLACEMENT)
        cg.writeEntry(KWIN_PLACEMENT, "Random");
    else if (v == CASCADE_PLACEMENT)
        cg.writeEntry(KWIN_PLACEMENT, "Cascade");
    else if (v == CENTERED_PLACEMENT)
        cg.writeEntry(KWIN_PLACEMENT, "Centered");
    else if (v == ZEROCORNERED_PLACEMENT)
        cg.writeEntry(KWIN_PLACEMENT, "ZeroCornered");
    else if (v == MAXIMIZING_PLACEMENT)
        cg.writeEntry(KWIN_PLACEMENT, "Maximizing");
//CT 13mar98 manual and interactive placement
//   else if (v == MANUAL_PLACEMENT)
//     cg.writeEntry(KWIN_PLACEMENT, "Manual");
//   else if (v == INTERACTIVE_PLACEMENT) {
//       QString tmpstr = QString("Interactive,%1").arg(interactiveTrigger->value());
//       cg.writeEntry(KWIN_PLACEMENT, tmpstr);
//   }
    else
        cg.writeEntry(KWIN_PLACEMENT, "Smart");

    cg.writeEntry(KWIN_HIDE_UTILITY, hideUtilityWindowsForInactive->isChecked());
    cg.writeEntry(KWIN_INACTIVE_SKIP_TASKBAR, inactiveTabsSkipTaskbar->isChecked());
    cg.writeEntry(KWIN_AUTOGROUP_SIMILAR, autogroupSimilarWindows->isChecked());
    cg.writeEntry(KWIN_AUTOGROUP_FOREGROUND, autogroupInForeground->isChecked());

    if (standAlone) {
        config->sync();
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);

    }

    cg.writeEntry(KWIN_TILINGON, tilingOn->isChecked());
    cg.writeEntry(KWIN_TILING_DEFAULT_LAYOUT, tilingLayoutCombo->currentIndex());
    cg.writeEntry(KWIN_TILING_RAISE_POLICY, tilingRaiseCombo->currentIndex());
    emit KCModule::changed(false);
}

void KAdvancedConfig::defaults()
{
    setShadeHover(false);
    setShadeHoverInterval(250);
    setPlacement(SMART_PLACEMENT);
    setHideUtilityWindowsForInactive(true);
    setTilingOn(false);
    setTilingLayout(0);
    setTilingRaisePolicy(0);
    setInactiveTabsSkipTaskbar(false);
    setAutogroupSimilarWindows(false);
    setAutogroupInForeground(true);
    emit KCModule::changed(true);
}

// placement policy --- CT 31jan98 ---
int KAdvancedConfig::getPlacement()
{
    return placementCombo->currentIndex();
}

void KAdvancedConfig::setPlacement(int plac)
{
    placementCombo->setCurrentIndex(plac);
}


void KAdvancedConfig::setHideUtilityWindowsForInactive(bool s)
{
    hideUtilityWindowsForInactive->setChecked(s);
}

void KAdvancedConfig::setInactiveTabsSkipTaskbar(bool s)
{
    inactiveTabsSkipTaskbar->setChecked(s);
}

void KAdvancedConfig::setAutogroupSimilarWindows(bool s)
{
    autogroupSimilarWindows->setChecked(s);
}

void KAdvancedConfig::setAutogroupInForeground(bool s)
{
    autogroupInForeground->setChecked(s);
}

KMovingConfig::~KMovingConfig()
{
    if (standAlone)
        delete config;
}

KMovingConfig::KMovingConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget *parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout(this);

    windowsBox = new KButtonGroup(this);
    windowsBox->setTitle(i18n("Windows"));

    QBoxLayout *wLay = new QVBoxLayout(windowsBox);

    QBoxLayout *bLay = new QVBoxLayout;
    wLay->addLayout(bLay);

    geometryTipOn = new QCheckBox(i18n("Display window &geometry when moving or resizing"), windowsBox);
    bLay->addWidget(geometryTipOn);
    geometryTipOn->setWhatsThis(i18n("Enable this option if you want a window's geometry to be displayed"
                                     " while it is being moved or resized. The window position relative"
                                     " to the top-left corner of the screen is displayed together with"
                                     " its size."));

    moveResizeMaximized = new QCheckBox(i18n("Display borders on &maximized windows"), windowsBox);
    bLay->addWidget(moveResizeMaximized);
    moveResizeMaximized->setWhatsThis(i18n("When enabled, this feature activates the border of maximized windows"
                                           " and allows you to move or resize them,"
                                           " just like for normal windows"));


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
    MagicBox = new KButtonGroup(this);
    MagicBox->setTitle(i18n("Snap Zones"));
    QGridLayout *kLay = new QGridLayout(MagicBox);

    BrdrSnap = new KIntNumInput(10, MagicBox);
    BrdrSnap->setSpecialValueText(i18nc("no border snap zone", "none"));
    BrdrSnap->setRange(0, MAX_BRDR_SNAP);
    BrdrSnap->setSteps(1, 10);
    BrdrSnap->setWhatsThis(i18n("Here you can set the snap zone for screen borders, i.e."
                                " the 'strength' of the magnetic field which will make windows snap to the border when"
                                " moved near it."));
    BrdrSnap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    BrdrSnapLabel = new QLabel(i18n("&Border snap zone:"), this);
    BrdrSnapLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    BrdrSnapLabel->setBuddy(BrdrSnap);
    kLay->addWidget(BrdrSnapLabel, 0, 0);
    kLay->addWidget(BrdrSnap, 0, 1);

    WndwSnap = new KIntNumInput(10, MagicBox);
    WndwSnap->setSpecialValueText(i18nc("no window snap zone", "none"));
    WndwSnap->setRange(0, MAX_WNDW_SNAP);
    WndwSnap->setSteps(1, 10);
    WndwSnap->setWhatsThis(i18n("Here you can set the snap zone for windows, i.e."
                                " the 'strength' of the magnetic field which will make windows snap to each other when"
                                " they are moved near another window."));
    BrdrSnap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    WndwSnapLabel = new QLabel(i18n("&Window snap zone:"), this);
    WndwSnapLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    WndwSnapLabel->setBuddy(WndwSnap);
    kLay->addWidget(WndwSnapLabel, 1, 0);
    kLay->addWidget(WndwSnap, 1, 1);

    CntrSnap = new KIntNumInput(10, MagicBox);
    CntrSnap->setSpecialValueText(i18nc("no center snap zone", "none"));
    CntrSnap->setRange(0, MAX_CNTR_SNAP);
    CntrSnap->setSteps(1, 10);
    CntrSnap->setWhatsThis(i18n("Here you can set the snap zone for the screen center, i.e."
                                " the 'strength' of the magnetic field which will make windows snap to the center of"
                                " the screen when moved near it."));
    BrdrSnap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    CntrSnapLabel = new QLabel(i18n("&Center snap zone:"), this);
    CntrSnapLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    CntrSnapLabel->setBuddy(CntrSnap);
    kLay->addWidget(CntrSnapLabel, 2, 0);
    kLay->addWidget(CntrSnap, 2, 1);

    OverlapSnap = new QCheckBox(i18n("Snap windows onl&y when overlapping"), MagicBox);
    OverlapSnap->setWhatsThis(i18n("Here you can set that windows will be only"
                                   " snapped if you try to overlap them, i.e. they will not be snapped if the windows"
                                   " comes only near another window or border."));
    kLay->addWidget(OverlapSnap, 3, 0, 1, 2);

    lay->addWidget(MagicBox);
    lay->addStretch();

    load();

    // Any changes goes to slotChanged()
    connect(geometryTipOn, SIGNAL(clicked()), SLOT(changed()));
    connect(moveResizeMaximized, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(BrdrSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(BrdrSnap, SIGNAL(valueChanged(int)), SLOT(slotBrdrSnapChanged(int)));
    connect(WndwSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(WndwSnap, SIGNAL(valueChanged(int)), SLOT(slotWndwSnapChanged(int)));
    connect(CntrSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(CntrSnap, SIGNAL(valueChanged(int)), SLOT(slotCntrSnapChanged(int)));
    connect(OverlapSnap, SIGNAL(clicked()), SLOT(changed()));

    // To get suffix to BrdrSnap, WndwSnap and CntrSnap inputs with default values.
    slotBrdrSnapChanged(BrdrSnap->value());
    slotWndwSnapChanged(WndwSnap->value());
    slotCntrSnapChanged(CntrSnap->value());
}

void KMovingConfig::setGeometryTip(bool showGeometryTip)
{
    geometryTipOn->setChecked(showGeometryTip);
}

bool KMovingConfig::getGeometryTip()
{
    return geometryTipOn->isChecked();
}

void KMovingConfig::setMoveResizeMaximized(bool a)
{
    moveResizeMaximized->setChecked(a);
}

void KMovingConfig::slotBrdrSnapChanged(int value)
{
    BrdrSnap->setSuffix(i18np(" pixel", " pixels", value));
}

void KMovingConfig::slotWndwSnapChanged(int value)
{
    WndwSnap->setSuffix(i18np(" pixel", " pixels", value));
}

void KMovingConfig::slotCntrSnapChanged(int value)
{
    CntrSnap->setSuffix(i18np(" pixel", " pixels", value));
}

void KMovingConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
}

void KMovingConfig::load(void)
{
    QString key;

    KConfigGroup cg(config, "Windows");

    //KS 10Jan2003 - Geometry Tip during window move/resize
    bool showGeomTip = cg.readEntry(KWIN_GEOMETRY, false);
    setGeometryTip(showGeomTip);


    setMoveResizeMaximized(cg.readEntry(KWIN_MOVE_RESIZE_MAXIMIZED, false));

    int v;

    v = cg.readEntry(KWM_BRDR_SNAP_ZONE, KWM_BRDR_SNAP_ZONE_DEFAULT);
    if (v > MAX_BRDR_SNAP) setBorderSnapZone(MAX_BRDR_SNAP);
    else if (v < 0) setBorderSnapZone(0);
    else setBorderSnapZone(v);

    v = cg.readEntry(KWM_WNDW_SNAP_ZONE, KWM_WNDW_SNAP_ZONE_DEFAULT);
    if (v > MAX_WNDW_SNAP) setWindowSnapZone(MAX_WNDW_SNAP);
    else if (v < 0) setWindowSnapZone(0);
    else setWindowSnapZone(v);

    v = cg.readEntry(KWM_CNTR_SNAP_ZONE, KWM_CNTR_SNAP_ZONE_DEFAULT);
    if (v > MAX_CNTR_SNAP) setCenterSnapZone(MAX_CNTR_SNAP);
    else if (v < 0) setCenterSnapZone(0);
    else setCenterSnapZone(v);

    OverlapSnap->setChecked(cg.readEntry("SnapOnlyWhenOverlapping", false));
    emit KCModule::changed(false);
}

void KMovingConfig::save(void)
{
    KConfigGroup cg(config, "Windows");

    cg.writeEntry(KWIN_MOVE_RESIZE_MAXIMIZED, moveResizeMaximized->isChecked());


    cg.writeEntry(KWM_BRDR_SNAP_ZONE, getBorderSnapZone());
    cg.writeEntry(KWM_WNDW_SNAP_ZONE, getWindowSnapZone());
    cg.writeEntry(KWM_CNTR_SNAP_ZONE, getCenterSnapZone());
    cg.writeEntry("SnapOnlyWhenOverlapping", OverlapSnap->isChecked());

    KConfigGroup(config, "Plugins").writeEntry("kwin4_effect_windowgeometryEnabled", getGeometryTip());

    if (standAlone) {
        config->sync();
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
    emit KCModule::changed(false);
}

void KMovingConfig::defaults()
{
    setGeometryTip(false);
    setMoveResizeMaximized(false);

    //copied from kcontrol/konq/kwindesktop, aleXXX
    setWindowSnapZone(KWM_WNDW_SNAP_ZONE_DEFAULT);
    setBorderSnapZone(KWM_BRDR_SNAP_ZONE_DEFAULT);
    setCenterSnapZone(KWM_CNTR_SNAP_ZONE_DEFAULT);
    OverlapSnap->setChecked(false);

    emit KCModule::changed(true);
}

int KMovingConfig::getBorderSnapZone()
{
    return BrdrSnap->value();
}

void KMovingConfig::setBorderSnapZone(int pxls)
{
    BrdrSnap->setValue(pxls);
}

int KMovingConfig::getWindowSnapZone()
{
    return WndwSnap->value();
}

void KMovingConfig::setWindowSnapZone(int pxls)
{
    WndwSnap->setValue(pxls);
}

int KMovingConfig::getCenterSnapZone()
{
    return CntrSnap->value();
}

void KMovingConfig::setCenterSnapZone(int pxls)
{
    CntrSnap->setValue(pxls);
}

#include "windows.moc"
