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
#include <QtGui/QDesktopWidget>
#include <QtDBus/QtDBus>

#include <KButtonGroup>
#include <klocale.h>
#include <knuminput.h>
#include <kdialog.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "windows.h"

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

KWinFocusConfigForm::KWinFocusConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

// removed the LCD display over the slider - this is not good GUI design :) RNolden 051701
KFocusConfig::KFocusConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget * parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
    , m_ui(new KWinFocusConfigForm(this))
{
    connect(m_ui->focusStealing, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->focusCombo, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->focusCombo, SIGNAL(activated(int)), this, SLOT(focusPolicyChanged()));
    connect(m_ui->focusCombo, SIGNAL(activated(int)), this, SLOT(setDelayFocusEnabled()));
    connect(m_ui->focusCombo, SIGNAL(activated(int)), this, SLOT(updateActiveMouseScreen()));
    connect(m_ui->autoRaiseOn, SIGNAL(clicked()), SLOT(changed()));
    connect(m_ui->autoRaiseOn, SIGNAL(toggled(bool)), SLOT(autoRaiseOnTog(bool)));
    connect(m_ui->clickRaiseOn, SIGNAL(clicked()), SLOT(changed()));
    connect(m_ui->autoRaise, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->delayFocus, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->separateScreenFocus, SIGNAL(clicked()), SLOT(changed()));
    connect(m_ui->activeMouseScreen, SIGNAL(clicked()), SLOT(changed()));
    connect(m_ui->focusNextToMouse, SIGNAL(clicked()), SLOT(changed()));

    load();
}


int KFocusConfig::getFocus()
{
    return m_ui->focusCombo->currentIndex();
}

void KFocusConfig::setFocus(int foc)
{
    m_ui->focusCombo->setCurrentIndex(foc);

    // this will disable/hide the auto raise delay widget if focus==click
    focusPolicyChanged();
}

void KFocusConfig::setAutoRaiseInterval(int tb)
{
    m_ui->autoRaise->setValue(tb);
}

void KFocusConfig::setDelayFocusInterval(int tb)
{
    m_ui->delayFocus->setValue(tb);
}

int KFocusConfig::getAutoRaiseInterval()
{
    return m_ui->autoRaise->value();
}

int KFocusConfig::getDelayFocusInterval()
{
    return m_ui->delayFocus->value();
}

void KFocusConfig::setAutoRaise(bool on)
{
    m_ui->autoRaiseOn->setChecked(on);
}

void KFocusConfig::setClickRaise(bool on)
{
    m_ui->clickRaiseOn->setChecked(on);
}

void KFocusConfig::focusPolicyChanged()
{
    int policyIndex = m_ui->focusCombo->currentIndex();

    // the auto raise related widgets are: autoRaise
    m_ui->autoRaiseOn->setEnabled(policyIndex != CLICK_TO_FOCUS);
    autoRaiseOnTog(policyIndex != CLICK_TO_FOCUS && m_ui->autoRaiseOn->isChecked());

    m_ui->focusStealing->setDisabled(policyIndex == FOCUS_UNDER_MOUSE || policyIndex == FOCUS_STRICTLY_UNDER_MOUSE);

    m_ui->focusNextToMouse->setDisabled(policyIndex == FOCUS_UNDER_MOUSE || policyIndex == FOCUS_STRICTLY_UNDER_MOUSE);

}

void KFocusConfig::setDelayFocusEnabled()
{
    int policyIndex = m_ui->focusCombo->currentIndex();

    // the delayed focus related widgets are: delayFocus
    m_ui->delayFocusOnLabel->setEnabled(policyIndex != CLICK_TO_FOCUS);
    delayFocusOnTog(policyIndex != CLICK_TO_FOCUS);
}

void KFocusConfig::autoRaiseOnTog(bool a)
{
    m_ui->autoRaise->setEnabled(a);
    m_ui->clickRaiseOn->setEnabled(!a);
}

void KFocusConfig::delayFocusOnTog(bool a)
{
    m_ui->delayFocus->setEnabled(a);
}

void KFocusConfig::setFocusStealing(int l)
{
    l = qMax(0, qMin(4, l));
    m_ui->focusStealing->setCurrentIndex(l);
}

void KFocusConfig::setSeparateScreenFocus(bool s)
{
    m_ui->separateScreenFocus->setChecked(s);
}

void KFocusConfig::setActiveMouseScreen(bool a)
{
    m_ui->activeMouseScreen->setChecked(a);
}

void KFocusConfig::updateActiveMouseScreen()
{
    // on by default for non click to focus policies
    KConfigGroup cfg(config, "Windows");
    if (!cfg.hasKey(KWIN_ACTIVE_MOUSE_SCREEN))
        setActiveMouseScreen(m_ui->focusCombo->currentIndex() != 0);
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
    setActiveMouseScreen(cg.readEntry(KWIN_ACTIVE_MOUSE_SCREEN, m_ui->focusCombo->currentIndex() != 0));

//    setFocusStealing( cg.readEntry(KWIN_FOCUS_STEALING, 2 ));
    // TODO default to low for now
    setFocusStealing(cg.readEntry(KWIN_FOCUS_STEALING, 1));

    m_ui->focusNextToMouse->setChecked(cg.readEntry("NextFocusPrefersMouse", false));

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

    cg.writeEntry(KWIN_AUTORAISE, m_ui->autoRaiseOn->isChecked());

    cg.writeEntry(KWIN_CLICKRAISE, m_ui->clickRaiseOn->isChecked());

    cg.writeEntry(KWIN_SEPARATE_SCREEN_FOCUS, m_ui->separateScreenFocus->isChecked());
    cg.writeEntry(KWIN_ACTIVE_MOUSE_SCREEN, m_ui->activeMouseScreen->isChecked());

    cg.writeEntry(KWIN_FOCUS_STEALING, m_ui->focusStealing->currentIndex());

    cg.writeEntry(KWIN_SEPARATE_SCREEN_FOCUS, m_ui->separateScreenFocus->isChecked());
    cg.writeEntry(KWIN_ACTIVE_MOUSE_SCREEN, m_ui->activeMouseScreen->isChecked());

    cg.writeEntry("NextFocusPrefersMouse", m_ui->focusNextToMouse->isChecked());

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
    setActiveMouseScreen(m_ui->focusCombo->currentIndex() != 0);
    setDelayFocusEnabled();
    m_ui->focusNextToMouse->setChecked(false);
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
    placementCombo->addItem(i18n("Smart"), "Smart");
    placementCombo->addItem(i18n("Maximizing"), "Maximizing");
    placementCombo->addItem(i18n("Cascade"), "Cascade");
    placementCombo->addItem(i18n("Random"), "Random");
    placementCombo->addItem(i18n("Centered"), "Centered");
    placementCombo->addItem(i18n("Zero-Cornered"), "ZeroCornered");
    placementCombo->addItem(i18n("Under Mouse"), "UnderMouse");
    // CT: disabling is needed as long as functionality misses in kwin
    //placementCombo->addItem(i18n("Interactive"), INTERACTIVE_PLACEMENT);
    //placementCombo->addItem(i18n("Manual"), MANUAL_PLACEMENT);
    placementCombo->setCurrentIndex(0); // default to "Smart"

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
                 " <li><em>Under Mouse</em> will place the window under the pointer</li>"
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
    int idx = placementCombo->findData(key);
    if (idx < 0)
        idx = placementCombo->findData("Smart");
    placementCombo->setCurrentIndex(idx);
//  }

    setHideUtilityWindowsForInactive(cg.readEntry(KWIN_HIDE_UTILITY, true));
    setInactiveTabsSkipTaskbar(cg.readEntry(KWIN_INACTIVE_SKIP_TASKBAR, false));
    setAutogroupSimilarWindows(cg.readEntry(KWIN_AUTOGROUP_SIMILAR, false));
    setAutogroupInForeground(cg.readEntry(KWIN_AUTOGROUP_FOREGROUND, true));

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
    cg.writeEntry(KWIN_PLACEMENT, placementCombo->itemData(placementCombo->currentIndex()).toString());
//CT 13mar98 manual and interactive placement
//   else if (v == MANUAL_PLACEMENT)
//     cg.writeEntry(KWIN_PLACEMENT, "Manual");
//   else if (v == INTERACTIVE_PLACEMENT) {
//       QString tmpstr = QString("Interactive,%1").arg(interactiveTrigger->value());
//       cg.writeEntry(KWIN_PLACEMENT, tmpstr);
//   }

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
    emit KCModule::changed(false);
}

void KAdvancedConfig::defaults()
{
    setShadeHover(false);
    setShadeHoverInterval(250);
    placementCombo->setCurrentIndex(0); // default to Smart
    setHideUtilityWindowsForInactive(true);
    setInactiveTabsSkipTaskbar(false);
    setAutogroupSimilarWindows(false);
    setAutogroupInForeground(true);
    emit KCModule::changed(true);
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

KWinMovingConfigForm::KWinMovingConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KMovingConfig::~KMovingConfig()
{
    if (standAlone)
        delete config;
}

KMovingConfig::KMovingConfig(bool _standAlone, KConfig *_config, const KComponentData &inst, QWidget *parent)
    : KCModule(inst, parent), config(_config), standAlone(_standAlone)
    , m_ui(new KWinMovingConfigForm(this))
{
    // Any changes goes to slotChanged()
    connect(m_ui->geometryTipOn, SIGNAL(clicked()), SLOT(changed()));
    connect(m_ui->moveResizeMaximized, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(m_ui->borderSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->windowSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->centerSnap, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->OverlapSnap, SIGNAL(clicked()), SLOT(changed()));

    load();
}

void KMovingConfig::setGeometryTip(bool showGeometryTip)
{
    m_ui->geometryTipOn->setChecked(showGeometryTip);
}

bool KMovingConfig::getGeometryTip()
{
    return m_ui->geometryTipOn->isChecked();
}

void KMovingConfig::setMoveResizeMaximized(bool a)
{
    m_ui->moveResizeMaximized->setChecked(a);
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

    m_ui->OverlapSnap->setChecked(cg.readEntry("SnapOnlyWhenOverlapping", false));
    emit KCModule::changed(false);
}

void KMovingConfig::save(void)
{
    KConfigGroup cg(config, "Windows");
    cg.writeEntry(KWIN_GEOMETRY, getGeometryTip());
    cg.writeEntry(KWIN_MOVE_RESIZE_MAXIMIZED, m_ui->moveResizeMaximized->isChecked());


    cg.writeEntry(KWM_BRDR_SNAP_ZONE, getBorderSnapZone());
    cg.writeEntry(KWM_WNDW_SNAP_ZONE, getWindowSnapZone());
    cg.writeEntry(KWM_CNTR_SNAP_ZONE, getCenterSnapZone());
    cg.writeEntry("SnapOnlyWhenOverlapping", m_ui->OverlapSnap->isChecked());

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
    m_ui->OverlapSnap->setChecked(false);

    emit KCModule::changed(true);
}

int KMovingConfig::getBorderSnapZone()
{
    return m_ui->borderSnap->value();
}

void KMovingConfig::setBorderSnapZone(int pxls)
{
    m_ui->borderSnap->setValue(pxls);
}

int KMovingConfig::getWindowSnapZone()
{
    return m_ui->windowSnap->value();
}

void KMovingConfig::setWindowSnapZone(int pxls)
{
    m_ui->windowSnap->setValue(pxls);
}

int KMovingConfig::getCenterSnapZone()
{
    return m_ui->centerSnap->value();
}

void KMovingConfig::setCenterSnapZone(int pxls)
{
    m_ui->centerSnap->setValue(pxls);
}

#include "windows.moc"
