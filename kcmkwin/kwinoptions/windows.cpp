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

#include <QApplication>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>
#include <KComboBox>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDesktopWidget>
#include <QtDBus>
#include <QGroupBox>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include "windows.h"
#include "kwinoptions_settings.h"
#include <effect_builtins.h>
#include <kwin_effects_interface.h>

// kwin config keywords
#define KWIN_FOCUS                 "FocusPolicy"
#define KWIN_PLACEMENT             "Placement"
#define KWIN_AUTORAISE_INTERVAL    "AutoRaiseInterval"
#define KWIN_AUTORAISE             "AutoRaise"
#define KWIN_DELAYFOCUS_INTERVAL   "DelayFocusInterval"
#define KWIN_CLICKRAISE            "ClickRaise"
#define KWIN_SHADEHOVER            "ShadeHover"
#define KWIN_SHADEHOVER_INTERVAL   "ShadeHoverInterval"
#define KWIN_FOCUS_STEALING        "FocusStealingPreventionLevel"
#define KWIN_HIDE_UTILITY          "HideUtilityWindowsForInactive"
#define KWIN_INACTIVE_SKIP_TASKBAR "InactiveTabsSkipTaskbar"
#define KWIN_SEPARATE_SCREEN_FOCUS "SeparateScreenFocus"
#define KWIN_ACTIVE_MOUSE_SCREEN   "ActiveMouseScreen"

#define  CLICK_TO_FOCUS               0
#define  FOCUS_FOLLOWS_MOUSE          2
#define  FOCUS_UNDER_MOUSE            4
#define  FOCUS_STRICTLY_UNDER_MOUSE   5


KFocusConfig::~KFocusConfig()
{
    if (standAlone)
        delete config;
}

KWinFocusConfigForm::KWinFocusConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(parent);
}

// removed the LCD display over the slider - this is not good GUI design :) RNolden 051701
KFocusConfig::KFocusConfig(bool _standAlone, KConfig *_config, QWidget * parent)
    : KCModule(parent), config(_config), standAlone(_standAlone)
    , m_ui(new KWinFocusConfigForm(this))
{
    connect(m_ui->focusStealing, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->windowFocusPolicyCombo, SIGNAL(currentIndexChanged(int)), SLOT(changed()));
    connect(m_ui->windowFocusPolicyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(focusPolicyChanged()));
    connect(m_ui->windowFocusPolicyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setDelayFocusEnabled()));
    connect(m_ui->windowFocusPolicyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateActiveMouseScreen()));
    connect(m_ui->autoRaiseOn, SIGNAL(clicked()), SLOT(changed()));
    connect(m_ui->autoRaiseOn, SIGNAL(toggled(bool)), SLOT(autoRaiseOnTog(bool)));
    connect(m_ui->clickRaiseOn, SIGNAL(clicked()), SLOT(changed()));
    connect(m_ui->autoRaise, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->delayFocus, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->separateScreenFocus, SIGNAL(clicked()), SLOT(changed()));
    connect(m_ui->activeMouseScreen, SIGNAL(clicked()), SLOT(changed()));

    connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), SLOT(updateMultiScreen()));
    updateMultiScreen();

    load();
}

void KFocusConfig::updateMultiScreen()
{
    m_ui->multiscreenBehaviorLabel->setVisible(QApplication::screens().count() > 1);
    m_ui->activeMouseScreen->setVisible(QApplication::screens().count() > 1);
    m_ui->separateScreenFocus->setVisible(QApplication::screens().count() > 1);
}


int KFocusConfig::getFocus()
{
    int policy = m_ui->windowFocusPolicyCombo->currentIndex();
    if (policy == 1 || policy == 3)
        --policy; // fix the NextFocusPrefersMouse condition
    return policy;
}

void KFocusConfig::setFocus(int foc)
{
    m_ui->windowFocusPolicyCombo->setCurrentIndex(foc);

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
    switch (m_ui->windowFocusPolicyCombo->currentIndex()) {
    case 0:
        m_ui->windowFocusPolicyDescriptionLabel->setText(i18n("<em>Click to focus:</em> A window becomes active when you click into it. This behavior is common on other operating systems and likely what you want."));
        break;
    case 1:
        m_ui->windowFocusPolicyDescriptionLabel->setText(i18n("<em>Click to focus (mouse precedence):</em> Mostly the same as <em>Click to focus</em>. If an active window has to be chosen by the system (eg. because the currently active one was closed) the window under the mouse is the preferred candidate. Unusual, but possible variant of <em>Click to focus</em>."));
        break;
    case 2:
        m_ui->windowFocusPolicyDescriptionLabel->setText(i18n("<em>Focus follows mouse:</em> Moving the mouse onto a window will activate it. Eg. windows randomly appearing under the mouse will not gain the focus. <em>Focus stealing prevention</em> takes place as usual. Think as <em>Click to focus</em> just without having to actually click."));
        break;
    case 3:
        m_ui->windowFocusPolicyDescriptionLabel->setText(i18n("This is mostly the same as <em>Focus follows mouse</em>. If an active window has to be chosen by the system (eg. because the currently active one was closed) the window under the mouse is the preferred candidate. Choose this, if you want a hover controlled focus."));
        break;
    case 4:
        m_ui->windowFocusPolicyDescriptionLabel->setText(i18n("<em>Focus under mouse:</em> The focus always remains on the window under the mouse.<br/><strong>Warning:</strong> <em>Focus stealing prevention</em> and the <em>tabbox ('Alt+Tab')</em> contradict the activation policy and will not work. You very likely want to use <em>Focus follows mouse (mouse precedence)</em> instead!"));
        break;
    case 5:
        m_ui->windowFocusPolicyDescriptionLabel->setText(i18n("<em>Focus strictly under mouse:</em> The focus is always on the window under the mouse (in doubt nowhere) very much like the focus behavior in an unmanaged legacy X11 environment.<br/><strong>Warning:</strong> <em>Focus stealing prevention</em> and the <em>tabbox ('Alt+Tab')</em> contradict the activation policy and will not work. You very likely want to use <em>Focus follows mouse (mouse precedence)</em> instead!"));
        break;
    }

    int policyIndex = getFocus();

    // the auto raise related widgets are: autoRaise
    m_ui->autoRaiseOn->setEnabled(policyIndex != CLICK_TO_FOCUS);
    autoRaiseOnTog(policyIndex != CLICK_TO_FOCUS && m_ui->autoRaiseOn->isChecked());

    m_ui->focusStealing->setDisabled(policyIndex == FOCUS_UNDER_MOUSE || policyIndex == FOCUS_STRICTLY_UNDER_MOUSE);
    m_ui->focusStealingLabel->setEnabled(m_ui->focusStealing->isEnabled());

    setDelayFocusEnabled();

}

void KFocusConfig::setDelayFocusEnabled()
{
    int policyIndex = getFocus();

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
        setActiveMouseScreen(getFocus() != 0);
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

    const bool focusNextToMouse = cg.readEntry("NextFocusPrefersMouse", false);

    key = cg.readEntry(KWIN_FOCUS);
    if (key == "ClickToFocus")
        setFocus(CLICK_TO_FOCUS + focusNextToMouse);
    else if (key == "FocusFollowsMouse")
        setFocus(FOCUS_FOLLOWS_MOUSE + focusNextToMouse);
    else if (key == "FocusUnderMouse")
        setFocus(FOCUS_UNDER_MOUSE);
    else if (key == "FocusStrictlyUnderMouse")
        setFocus(FOCUS_STRICTLY_UNDER_MOUSE);

    int k = cg.readEntry(KWIN_AUTORAISE_INTERVAL, 750);
    setAutoRaiseInterval(k);

    k = cg.readEntry(KWIN_DELAYFOCUS_INTERVAL, 300);
    setDelayFocusInterval(k);

    setAutoRaise(cg.readEntry(KWIN_AUTORAISE, false));
    setClickRaise(cg.readEntry(KWIN_CLICKRAISE, true));
    focusPolicyChanged();      // this will disable/hide the auto raise delay widget if focus==click

    setSeparateScreenFocus(cg.readEntry(KWIN_SEPARATE_SCREEN_FOCUS, false));
    // on by default for non click to focus policies
    setActiveMouseScreen(cg.readEntry(KWIN_ACTIVE_MOUSE_SCREEN, getFocus() != 0));

//    setFocusStealing( cg.readEntry(KWIN_FOCUS_STEALING, 2 ));
    // TODO default to low for now
    setFocusStealing(cg.readEntry(KWIN_FOCUS_STEALING, 1));


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

    cg.writeEntry("NextFocusPrefersMouse", v != m_ui->windowFocusPolicyCombo->currentIndex());

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
    setActiveMouseScreen(getFocus() != 0);
    setDelayFocusEnabled();
    emit KCModule::changed(true);
}

KWinAdvancedConfigForm::KWinAdvancedConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(parent);
}

KAdvancedConfig::~KAdvancedConfig()
{
    if (standAlone)
        delete config;
}

KAdvancedConfig::KAdvancedConfig(bool _standAlone, KConfig *_config, QWidget *parent)
    : KCModule(parent), config(_config), standAlone(_standAlone)
    , m_ui(new KWinAdvancedConfigForm(this))
{
    m_ui->placementCombo->setItemData(0, "Smart");
    m_ui->placementCombo->setItemData(1, "Maximizing");
    m_ui->placementCombo->setItemData(2, "Cascade");
    m_ui->placementCombo->setItemData(3, "Random");
    m_ui->placementCombo->setItemData(4, "Centered");
    m_ui->placementCombo->setItemData(5, "ZeroCornered");
    m_ui->placementCombo->setItemData(6, "UnderMouse");

    connect(m_ui->shadeHoverOn, SIGNAL(toggled(bool)), this, SLOT(shadeHoverChanged(bool)));
    connect(m_ui->shadeHoverOn, SIGNAL(toggled(bool)), SLOT(changed()));
    connect(m_ui->shadeHover, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(m_ui->placementCombo, SIGNAL(activated(int)), SLOT(changed()));
    connect(m_ui->hideUtilityWindowsForInactive, SIGNAL(toggled(bool)), SLOT(changed()));
    load();

}

void KAdvancedConfig::setShadeHover(bool on)
{
    m_ui->shadeHoverOn->setChecked(on);
    m_ui->shadeHover->setEnabled(on);
}

void KAdvancedConfig::setShadeHoverInterval(int k)
{
    m_ui->shadeHover->setValue(k);
}

int KAdvancedConfig::getShadeHoverInterval()
{

    return m_ui->shadeHover->value();
}

void KAdvancedConfig::shadeHoverChanged(bool a)
{
    m_ui->shadeHover->setEnabled(a);
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
    key = cg.readEntry(KWIN_PLACEMENT);
    int idx = m_ui->placementCombo->findData(key);
    if (idx < 0)
        idx = m_ui->placementCombo->findData("Smart");
    m_ui->placementCombo->setCurrentIndex(idx);

    setHideUtilityWindowsForInactive(cg.readEntry(KWIN_HIDE_UTILITY, true));

    emit KCModule::changed(false);
}

void KAdvancedConfig::save(void)
{
    int v;

    KConfigGroup cg(config, "Windows");
    cg.writeEntry(KWIN_SHADEHOVER, m_ui->shadeHoverOn->isChecked());

    v = getShadeHoverInterval();
    if (v < 0) v = 0;
    cg.writeEntry(KWIN_SHADEHOVER_INTERVAL, v);
    cg.writeEntry(KWIN_PLACEMENT, m_ui->placementCombo->itemData(m_ui->placementCombo->currentIndex()).toString());
    cg.writeEntry(KWIN_HIDE_UTILITY, m_ui->hideUtilityWindowsForInactive->isChecked());

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
    m_ui->placementCombo->setCurrentIndex(0); // default to Smart
    setHideUtilityWindowsForInactive(true);
    emit KCModule::changed(true);
}


void KAdvancedConfig::setHideUtilityWindowsForInactive(bool s)
{
    m_ui->hideUtilityWindowsForInactive->setChecked(s);
}

KWinMovingConfigForm::KWinMovingConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(parent);
}

KMovingConfig::~KMovingConfig()
{
}

KMovingConfig::KMovingConfig(bool _standAlone, QWidget *parent)
    : KCModule(parent), m_config(KWinOptionsSettings::self()), standAlone(_standAlone)
    , m_ui(new KWinMovingConfigForm(this))
{
    addConfig(m_config, m_ui);
    load();
}

void KMovingConfig::showEvent(QShowEvent *ev)
{
    if (!standAlone) {
        QWidget::showEvent(ev);
        return;
    }
    KCModule::showEvent(ev);
}

void KMovingConfig::save(void)
{
    m_config->save();

    if (standAlone) {
        // Send signal to all kwin instances
        QDBusMessage message =
            QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
    // and reconfigure the effect
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    if (m_config->geometryTip()) {
        interface.loadEffect(KWin::BuiltInEffects::nameForEffect(KWin::BuiltInEffect::WindowGeometry));
    } else {
        interface.unloadEffect(KWin::BuiltInEffects::nameForEffect(KWin::BuiltInEffect::WindowGeometry));
    }
}
