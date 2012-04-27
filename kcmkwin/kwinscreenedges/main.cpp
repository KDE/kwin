/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Martin Gräßlin <kde@martin-graesslin.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "kdebug.h"

#include "main.h"

#include <kservicetypetrader.h>

#include <KPluginFactory>
#include <KPluginLoader>
#include <QtDBus/QtDBus>

K_PLUGIN_FACTORY(KWinScreenEdgesConfigFactory, registerPlugin<KWin::KWinScreenEdgesConfig>();)
K_EXPORT_PLUGIN(KWinScreenEdgesConfigFactory("kcmkwinscreenedges"))

namespace KWin
{

KWinScreenEdgesConfigForm::KWinScreenEdgesConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KWinScreenEdgesConfig::KWinScreenEdgesConfig(QWidget* parent, const QVariantList& args)
    : KCModule(KWinScreenEdgesConfigFactory::componentData(), parent, args)
    , m_config(KSharedConfig::openConfig("kwinrc"))
{
    m_ui = new KWinScreenEdgesConfigForm(this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    monitorInit();

    connect(m_ui->monitor, SIGNAL(changed()), this, SLOT(changed()));

    connect(m_ui->desktopSwitchCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->activationDelaySpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->triggerCooldownSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->quickMaximizeBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->quickTileBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));

    // Visual feedback of action group conflicts
    connect(m_ui->desktopSwitchCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(groupChanged()));
    connect(m_ui->quickMaximizeBox, SIGNAL(stateChanged(int)), this, SLOT(groupChanged()));
    connect(m_ui->quickTileBox, SIGNAL(stateChanged(int)), this, SLOT(groupChanged()));

    load();
}

KWinScreenEdgesConfig::~KWinScreenEdgesConfig()
{
}

void KWinScreenEdgesConfig::groupChanged()
{
    // Monitor conflicts
    bool hide = false;
    if (m_ui->desktopSwitchCombo->currentIndex() == 2)
        hide = true;
    monitorHideEdge(ElectricTop, hide);
    monitorHideEdge(ElectricRight, hide);
    monitorHideEdge(ElectricBottom, hide);
    monitorHideEdge(ElectricLeft, hide);
}

void KWinScreenEdgesConfig::load()
{
    KCModule::load();

    monitorLoad();

    KConfigGroup config(m_config, "Windows");

    m_ui->desktopSwitchCombo->setCurrentIndex(config.readEntry("ElectricBorders", 0));
    m_ui->activationDelaySpin->setValue(config.readEntry("ElectricBorderDelay", 150));
    m_ui->triggerCooldownSpin->setValue(config.readEntry("ElectricBorderCooldown", 350));
    m_ui->quickMaximizeBox->setChecked(config.readEntry("ElectricBorderMaximize", true));
    m_ui->quickTileBox->setChecked(config.readEntry("ElectricBorderTiling", true));

    emit changed(false);
}

void KWinScreenEdgesConfig::save()
{
    KCModule::save();

    monitorSave();

    KConfigGroup config(m_config, "Windows");

    config.writeEntry("ElectricBorders", m_ui->desktopSwitchCombo->currentIndex());
    config.writeEntry("ElectricBorderDelay", m_ui->activationDelaySpin->value());
    config.writeEntry("ElectricBorderCooldown", m_ui->triggerCooldownSpin->value());
    config.writeEntry("ElectricBorderMaximize", m_ui->quickMaximizeBox->isChecked());
    config.writeEntry("ElectricBorderTiling", m_ui->quickTileBox->isChecked());

    config.sync();

    // Reload KWin.
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);

    emit changed(false);
}

void KWinScreenEdgesConfig::defaults()
{
    monitorDefaults();

    m_ui->desktopSwitchCombo->setCurrentIndex(0);
    m_ui->activationDelaySpin->setValue(150);
    m_ui->triggerCooldownSpin->setValue(350);
    m_ui->quickMaximizeBox->setChecked(true);
    m_ui->quickTileBox->setChecked(true);

    emit changed(true);
}

void KWinScreenEdgesConfig::showEvent(QShowEvent* e)
{
    KCModule::showEvent(e);

    monitorShowEvent();
}

// Copied from kcmkwin/kwincompositing/main.cpp
bool KWinScreenEdgesConfig::effectEnabled(const QString& effect, const KConfigGroup& cfg) const
{
    KService::List services = KServiceTypeTrader::self()->query(
                                  "KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + '\'');
    if (services.isEmpty())
        return false;
    QVariant v = services.first()->property("X-KDE-PluginInfo-EnabledByDefault");
    return cfg.readEntry("kwin4_effect_" + effect + "Enabled", v.toBool());
}

//-----------------------------------------------------------------------------
// Monitor

void KWinScreenEdgesConfig::monitorAddItem(const QString& item)
{
    for (int i = 0; i < 8; i++)
        m_ui->monitor->addEdgeItem(i, item);
}

void KWinScreenEdgesConfig::monitorItemSetEnabled(int index, bool enabled)
{
    for (int i = 0; i < 8; i++)
        m_ui->monitor->setEdgeItemEnabled(i, index, enabled);
}

void KWinScreenEdgesConfig::monitorInit()
{
    monitorAddItem(i18n("No Action"));
    monitorAddItem(i18n("Show Dashboard"));
    monitorAddItem(i18n("Show Desktop"));
    monitorAddItem(i18n("Lock Screen"));
    monitorAddItem(i18n("Prevent Screen Locking"));
    //Prevent Screen Locking is not supported on some edges
    m_ui->monitor->setEdgeItemEnabled(int(Monitor::Top), 4, false);
    m_ui->monitor->setEdgeItemEnabled(int(Monitor::Left), 4, false);
    m_ui->monitor->setEdgeItemEnabled(int(Monitor::Right), 4, false);
    m_ui->monitor->setEdgeItemEnabled(int(Monitor::Bottom), 4, false);

    // Search the effect names
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_presentwindows'");
    if (services.isEmpty()) {
        // adding empty strings in case the effect is not found
        // TODO: after string freeze add a info that the effect is missing
        monitorAddItem(QString());
        monitorAddItem(QString());
    } else {
        monitorAddItem(services.first()->name() + " - " + i18n("All Desktops"));
        monitorAddItem(services.first()->name() + " - " + i18n("Current Desktop"));
        monitorAddItem(services.first()->name() + " - " + i18n("Current Application"));
    }
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_desktopgrid'");
    if (services.isEmpty()) {
        // adding empty strings in case the effect is not found
        // TODO: after string freeze add a info that the effect is missing
        monitorAddItem(QString());
    } else {
        monitorAddItem(services.first()->name());
    }
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_cube'");
    if (services.isEmpty()) {
        // adding empty strings in case the effect is not found
        // TODO: after string freeze add a info that the effect is missing
        monitorAddItem(QString());
        monitorAddItem(QString());
        monitorAddItem(QString());
    } else {
        monitorAddItem(services.first()->name() + " - " + i18n("Cube"));
        monitorAddItem(services.first()->name() + " - " + i18n("Cylinder"));
        monitorAddItem(services.first()->name() + " - " + i18n("Sphere"));
    }
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_flipswitch'");
    if (services.isEmpty()) {
        // adding empty strings in case the effect is not found
        // TODO: after string freeze add a info that the effect is missing
        monitorAddItem(QString());
        monitorAddItem(QString());
    } else {
        monitorAddItem(services.first()->name() + " - " + i18n("All Desktops"));
        monitorAddItem(services.first()->name() + " - " + i18n("Current Desktop"));
    }

    monitorShowEvent();
}

void KWinScreenEdgesConfig::monitorLoadAction(ElectricBorder edge, const QString& configName)
{
    KConfigGroup config(m_config, "ElectricBorders");
    QString lowerName = config.readEntry(configName, "None").toLower();
    if (lowerName == "dashboard") monitorChangeEdge(edge, int(ElectricActionDashboard));
    else if (lowerName == "showdesktop") monitorChangeEdge(edge, int(ElectricActionShowDesktop));
    else if (lowerName == "lockscreen") monitorChangeEdge(edge, int(ElectricActionLockScreen));
    else if (lowerName == "preventscreenlocking") monitorChangeEdge(edge, int(ElectricActionPreventScreenLocking));
}

void KWinScreenEdgesConfig::monitorLoad()
{
    // Load ElectricBorderActions
    monitorLoadAction(ElectricTop,         "Top");
    monitorLoadAction(ElectricTopRight,    "TopRight");
    monitorLoadAction(ElectricRight,       "Right");
    monitorLoadAction(ElectricBottomRight, "BottomRight");
    monitorLoadAction(ElectricBottom,      "Bottom");
    monitorLoadAction(ElectricBottomLeft,  "BottomLeft");
    monitorLoadAction(ElectricLeft,        "Left");
    monitorLoadAction(ElectricTopLeft,     "TopLeft");

    // Load effect-specific actions:

    // Present Windows
    KConfigGroup presentWindowsConfig(m_config, "Effect-PresentWindows");
    QList<int> list = QList<int>();
    // PresentWindows BorderActivateAll
    list.append(int(ElectricTopLeft));
    list = presentWindowsConfig.readEntry("BorderActivateAll", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(PresentWindowsAll));
    }
    // PresentWindows BorderActivate
    list.clear();
    list.append(int(ElectricNone));
    list = presentWindowsConfig.readEntry("BorderActivate", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(PresentWindowsCurrent));
    }
    // PresentWindows BorderActivateClass
    list.clear();
    list.append(int(ElectricNone));
    list = presentWindowsConfig.readEntry("BorderActivateClass", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(PresentWindowsClass));
    }

    // Desktop Grid
    KConfigGroup gridConfig(m_config, "Effect-DesktopGrid");
    list.clear();
    list.append(int(ElectricNone));
    list = gridConfig.readEntry("BorderActivate", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(DesktopGrid));
    }

    // Desktop Cube
    KConfigGroup cubeConfig(m_config, "Effect-Cube");
    list.clear();
    list.append(int(ElectricNone));
    list = cubeConfig.readEntry("BorderActivate", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(Cube));
    }
    list.clear();
    list.append(int(ElectricNone));
    list = cubeConfig.readEntry("BorderActivateCylinder", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(Cylinder));
    }
    list.clear();
    list.append(int(ElectricNone));
    list = cubeConfig.readEntry("BorderActivateSphere", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(Sphere));
    }

    // Flip Switch
    KConfigGroup flipSwitchConfig(m_config, "Effect-FlipSwitch");
    list.clear();
    // FlipSwitch BorderActivateAll
    list.append(int(ElectricNone));
    list = flipSwitchConfig.readEntry("BorderActivateAll", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(FlipSwitchAll));
    }
    // FlipSwitch BorderActivate
    list.clear();
    list.append(int(ElectricNone));
    list = flipSwitchConfig.readEntry("BorderActivate", list);
    foreach (int i, list) {
        monitorChangeEdge(ElectricBorder(i), int(FlipSwitchCurrent));
    }
}

void KWinScreenEdgesConfig::monitorSaveAction(int edge, const QString& configName)
{
    KConfigGroup config(m_config, "ElectricBorders");
    int item = m_ui->monitor->selectedEdgeItem(edge);
    if (item == 1)   // Plasma dashboard
        config.writeEntry(configName, "Dashboard");
    else if (item == 2)
        config.writeEntry(configName, "ShowDesktop");
    else if (item == 3)
        config.writeEntry(configName, "LockScreen");
    else if (item == 4)
        config.writeEntry(configName, "PreventScreenLocking");
    else // Anything else
        config.writeEntry(configName, "None");

    if ((edge == int(Monitor::TopRight)) ||
            (edge == int(Monitor::BottomRight)) ||
            (edge == int(Monitor::BottomLeft)) ||
            (edge == int(Monitor::TopLeft))) {
        KConfig scrnConfig("kscreensaverrc");
        KConfigGroup scrnGroup = scrnConfig.group("ScreenSaver");
        scrnGroup.writeEntry("Action" + configName, (item == 4) ? 2 /* Prevent Screen Locking */ : 0 /* None */);
        scrnGroup.sync();
    }
}

void KWinScreenEdgesConfig::monitorSave()
{
    // Save ElectricBorderActions
    monitorSaveAction(int(Monitor::Top),         "Top");
    monitorSaveAction(int(Monitor::TopRight),    "TopRight");
    monitorSaveAction(int(Monitor::Right),       "Right");
    monitorSaveAction(int(Monitor::BottomRight), "BottomRight");
    monitorSaveAction(int(Monitor::Bottom),      "Bottom");
    monitorSaveAction(int(Monitor::BottomLeft),  "BottomLeft");
    monitorSaveAction(int(Monitor::Left),        "Left");
    monitorSaveAction(int(Monitor::TopLeft),     "TopLeft");

    // Save effect-specific actions:

    // Present Windows
    KConfigGroup presentWindowsConfig(m_config, "Effect-PresentWindows");
    presentWindowsConfig.writeEntry("BorderActivateAll",
                                    monitorCheckEffectHasEdge(int(PresentWindowsAll)));
    presentWindowsConfig.writeEntry("BorderActivate",
                                    monitorCheckEffectHasEdge(int(PresentWindowsCurrent)));
    presentWindowsConfig.writeEntry("BorderActivateClass",
                                    monitorCheckEffectHasEdge(int(PresentWindowsClass)));

    // Desktop Grid
    KConfigGroup gridConfig(m_config, "Effect-DesktopGrid");
    gridConfig.writeEntry("BorderActivate",
                          monitorCheckEffectHasEdge(int(DesktopGrid)));

    // Desktop Cube
    KConfigGroup cubeConfig(m_config, "Effect-Cube");
    cubeConfig.writeEntry("BorderActivate",
                          monitorCheckEffectHasEdge(int(Cube)));
    cubeConfig.writeEntry("BorderActivateCylinder",
                          monitorCheckEffectHasEdge(int(Cylinder)));
    cubeConfig.writeEntry("BorderActivateSphere",
                          monitorCheckEffectHasEdge(int(Sphere)));

    // Flip Switch
    KConfigGroup flipSwitchConfig(m_config, "Effect-FlipSwitch");
    flipSwitchConfig.writeEntry("BorderActivateAll",
                                monitorCheckEffectHasEdge(int(FlipSwitchAll)));
    flipSwitchConfig.writeEntry("BorderActivate",
                                monitorCheckEffectHasEdge(int(FlipSwitchCurrent)));
}

void KWinScreenEdgesConfig::monitorDefaults()
{
    // Clear all edges
    for (int i = 0; i < 8; i++)
        m_ui->monitor->selectEdgeItem(i, 0);

    // Present windows = Top-left
    m_ui->monitor->selectEdgeItem(int(Monitor::TopLeft), int(PresentWindowsAll));
}

void KWinScreenEdgesConfig::monitorShowEvent()
{
    // Check if they are enabled
    KConfigGroup config(m_config, "Compositing");
    if (config.readEntry("Enabled", true)) {
        // Compositing enabled
        config = KConfigGroup(m_config, "Plugins");

        // Present Windows
        bool enabled = effectEnabled("presentwindows", config);
        monitorItemSetEnabled(int(PresentWindowsCurrent), enabled);
        monitorItemSetEnabled(int(PresentWindowsAll), enabled);

        // Desktop Grid
        enabled = effectEnabled("desktopgrid", config);
        monitorItemSetEnabled(int(DesktopGrid), enabled);

        // Desktop Cube
        enabled = effectEnabled("cube", config);
        monitorItemSetEnabled(int(Cube), enabled);
        monitorItemSetEnabled(int(Cylinder), enabled);
        monitorItemSetEnabled(int(Sphere), enabled);

        // Flip Switch
        enabled = effectEnabled("flipswitch", config);
        monitorItemSetEnabled(int(FlipSwitchAll), enabled);
        monitorItemSetEnabled(int(FlipSwitchCurrent), enabled);
    } else { // Compositing disabled
        monitorItemSetEnabled(int(PresentWindowsCurrent), false);
        monitorItemSetEnabled(int(PresentWindowsAll), false);
        monitorItemSetEnabled(int(DesktopGrid), false);
        monitorItemSetEnabled(int(Cube), false);
        monitorItemSetEnabled(int(Cylinder), false);
        monitorItemSetEnabled(int(Sphere), false);
        monitorItemSetEnabled(int(FlipSwitchAll), false);
        monitorItemSetEnabled(int(FlipSwitchCurrent), false);
    }
}

void KWinScreenEdgesConfig::monitorChangeEdge(ElectricBorder border, int index)
{
    switch(border) {
    case ElectricTop:
        m_ui->monitor->selectEdgeItem(int(Monitor::Top), index);
        break;
    case ElectricTopRight:
        m_ui->monitor->selectEdgeItem(int(Monitor::TopRight), index);
        break;
    case ElectricRight:
        m_ui->monitor->selectEdgeItem(int(Monitor::Right), index);
        break;
    case ElectricBottomRight:
        m_ui->monitor->selectEdgeItem(int(Monitor::BottomRight), index);
        break;
    case ElectricBottom:
        m_ui->monitor->selectEdgeItem(int(Monitor::Bottom), index);
        break;
    case ElectricBottomLeft:
        m_ui->monitor->selectEdgeItem(int(Monitor::BottomLeft), index);
        break;
    case ElectricLeft:
        m_ui->monitor->selectEdgeItem(int(Monitor::Left), index);
        break;
    case ElectricTopLeft:
        m_ui->monitor->selectEdgeItem(int(Monitor::TopLeft), index);
        break;
    default: // Nothing
        break;
    }
}

void KWinScreenEdgesConfig::monitorHideEdge(ElectricBorder border, bool hidden)
{
    switch(border) {
    case ElectricTop:
        m_ui->monitor->setEdgeHidden(int(Monitor::Top), hidden);
        break;
    case ElectricTopRight:
        m_ui->monitor->setEdgeHidden(int(Monitor::TopRight), hidden);
        break;
    case ElectricRight:
        m_ui->monitor->setEdgeHidden(int(Monitor::Right), hidden);
        break;
    case ElectricBottomRight:
        m_ui->monitor->setEdgeHidden(int(Monitor::BottomRight), hidden);
        break;
    case ElectricBottom:
        m_ui->monitor->setEdgeHidden(int(Monitor::Bottom), hidden);
        break;
    case ElectricBottomLeft:
        m_ui->monitor->setEdgeHidden(int(Monitor::BottomLeft), hidden);
        break;
    case ElectricLeft:
        m_ui->monitor->setEdgeHidden(int(Monitor::Left), hidden);
        break;
    case ElectricTopLeft:
        m_ui->monitor->setEdgeHidden(int(Monitor::TopLeft), hidden);
        break;
    default: // Nothing
        break;
    }
}

QList<int> KWinScreenEdgesConfig::monitorCheckEffectHasEdge(int index) const
{
    QList<int> list = QList<int>();
    if (m_ui->monitor->selectedEdgeItem(int(Monitor::Top)) == index)
        list.append(int(ElectricTop));
    if (m_ui->monitor->selectedEdgeItem(int(Monitor::TopRight)) == index)
        list.append(int(ElectricTopRight));
    if (m_ui->monitor->selectedEdgeItem(int(Monitor::Right)) == index)
        list.append(int(ElectricRight));
    if (m_ui->monitor->selectedEdgeItem(int(Monitor::BottomRight)) == index)
        list.append(int(ElectricBottomRight));
    if (m_ui->monitor->selectedEdgeItem(int(Monitor::Bottom)) == index)
        list.append(int(ElectricBottom));
    if (m_ui->monitor->selectedEdgeItem(int(Monitor::BottomLeft)) == index)
        list.append(int(ElectricBottomLeft));
    if (m_ui->monitor->selectedEdgeItem(int(Monitor::Left)) == index)
        list.append(int(ElectricLeft));
    if (m_ui->monitor->selectedEdgeItem(int(Monitor::TopLeft)) == index)
        list.append(int(ElectricTopLeft));

    if (list.isEmpty())
        list.append(int(ElectricNone));
    return list;
}

} // namespace

#include "main.moc"
